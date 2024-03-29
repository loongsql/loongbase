/* Copyright (c) 2024 LoongSQL, Inc.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include "Engine.h"
#include "Dbb.h"
#include "DeferredIndex.h"
#include "DeferredIndexWalker.h"
#include "Index.h"
#include "IndexKey.h"
#include "Sync.h"
#include "Bitmap.h"
#include "Database.h"
#include "Btn.h"
#include "Transaction.h"
#include "Log.h"
#include "LogLock.h"
#include "Configuration.h"
#include "StreamLogWindow.h"

namespace Changjiang {

static const uint MIDPOINT = DEFERRED_INDEX_FANOUT / 2;
static char printable[256];

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

DeferredIndex::DeferredIndex(Index *idx, Transaction *trans) {
  index = idx;
  transaction = trans;
  transactionId = transaction->transactionId;
  initializeSpace();
  levels = 0;
  DILeaf *leaf = (DILeaf *)alloc(sizeof(DILeaf));
  leaf->count = 0;
  root = leaf;
  count = 0;
  sizeEstimate = 10;  // How much space this will take in the stream log
  virtualOffset = 0;
  virtualOffsetAtEnd = 0;
  minValue = NULL;
  maxValue = NULL;
  haveMinValue = true;
  haveMaxValue = true;
  window = NULL;
  syncObject.setName("DeferredIndex::syncObject");
  useCount = 1;  // the transaction that created it
}

DeferredIndex::~DeferredIndex(void) {
  if (window) window->clearInterest();

  ASSERT(index == NULL && transaction == NULL);
  freeHunks();
}

void DeferredIndex::freeHunks(void) {
  for (DIHunk *hunk; (hunk = hunks);) {
    hunks = hunk->next;
    delete hunk;
  }
}

void DeferredIndex::initializeSpace(void) {
  hunks = NULL;
  base = initialSpace;
  currentHunkOffset = sizeof(initialSpace);
}

void *DeferredIndex::alloc(uint length) {
  uint len = ROUNDUP(length, sizeof(void *));

  if (len > currentHunkOffset) {
    DIHunk *hunk = new DIHunk;
    hunk->next = hunks;
    hunks = hunk;
    currentHunkOffset = sizeof(hunk->space);  // DEFERRED_INDEX_HUNK_SIZE;
    base = hunks->space;
  }

  ASSERT(len <= currentHunkOffset);
  currentHunkOffset -= len;

  return base + currentHunkOffset;
}

void DeferredIndex::addNode(IndexKey *indexKey, int32 recordNumber) {
  bool doingDIHash = ((index->database->configuration->useDeferredIndexHash) &&
                      (INDEX_IS_UNIQUE(index->type)));

  Sync syncHash(&index->syncDIHash, "DeferredIndex::addNode");
  if (doingDIHash) syncHash.lock(Exclusive);

  Sync sync(&syncObject, "DeferredIndex::addNode");
  sync.lock(Exclusive);
  DINode *node;
  DIUniqueNode *uniqueNode = NULL;

  if (doingDIHash) {
    int nodeSize = sizeof(DIUniqueNode) + indexKey->keyLength - 1;
    uniqueNode = (DIUniqueNode *)alloc(nodeSize);
    uniqueNode->collision = NULL;
    node = &uniqueNode->node;
  } else {
    int nodeSize = sizeof(DINode) + indexKey->keyLength - 1;
    node = (DINode *)alloc(nodeSize);
    ;
  }

  node->recordNumber = recordNumber;
  node->keyLength = indexKey->keyLength;
  memcpy(node->key, indexKey->key, node->keyLength);

  if (doingDIHash) index->addToDIHash(uniqueNode);

  // Calculate how much space in the stream log this node will take up

  sizeEstimate += node->keyLength + 4;  // recordNumber + keyLength + key
  DIBucket *buckets[DEFERRED_INDEX_MAX_LEVELS];
  DIBucket *bucket = (DIBucket *)root;
  uint slot = 0;
  int level;
  // print("Adding", node);

  if (haveMinValue && (minValue == NULL || compare(node, minValue) < 0))
    minValue = node;

  if (haveMaxValue && (maxValue == NULL || compare(node, maxValue) > 0))
    maxValue = node;

  // Search down index tree for appropriate leaf

  for (level = levels; level > 0; --level) {
    buckets[level] = bucket;

    for (slot = 0;; ++slot) {
      int n = compare(node, bucket->references[slot].node);

      if (n == 0) return;  // ah ha!  a duplicate!

      if (n < 0) {
        bucket = bucket->references[(slot) ? slot - 1 : 0].bucket;

        break;
      }

      if (slot + 1 == bucket->count) {
        bucket = bucket->references[slot].bucket;
        break;
      }
    }
  }

  DILeaf *leaf = (DILeaf *)bucket;

  // If node fix in bucket, find the spot and stick it in.  Good enough.

  if (leaf->count < DEFERRED_INDEX_FANOUT) {
    // Special case sticking it at the end (insertions are sometimes ordered)

    if (leaf->count == 0 || compare(node, leaf->nodes[leaf->count - 1]) > 0) {
      leaf->nodes[leaf->count++] = node;
      ++count;

      return;
    }

    for (slot = 0; slot < leaf->count; ++slot) {
      int n = compare(node, leaf->nodes[slot]);

      if (n == 0) return;

      if (n < 0) {
        memmove(leaf->nodes + slot + 1, leaf->nodes + slot,
                (leaf->count - slot) * sizeof(leaf->nodes[0]));
        leaf->nodes[slot] = node;
        ++leaf->count;
        ++count;

        return;
      }
    }

    leaf->nodes[leaf->count++] = node;
    ++count;

    return;
  }

  // The node doesn't fit; time to split.  Note: If the nodes goes at the end,
  // just create a new, empty bucket.

  DILeaf *newLeaf;
  DINode *splitNode;

  if (compare(node, leaf->nodes[DEFERRED_INDEX_FANOUT - 1]) > 0) {
    newLeaf = (DILeaf *)alloc(sizeof(DILeaf));
    newLeaf->count = 1;
    newLeaf->nodes[0] = node;
    splitNode = node;
  } else {
    // Find the insertion point

    for (slot = 0;; ++slot) {
      int n = compare(node, leaf->nodes[slot]);

      if (n == 0) return;

      if (n < 0) break;
    }

    newLeaf = (DILeaf *)alloc(sizeof(DILeaf));
    memcpy(newLeaf->nodes, leaf->nodes + MIDPOINT,
           (MIDPOINT) * sizeof(leaf->nodes[0]));
    leaf->count = MIDPOINT;
    newLeaf->count = MIDPOINT;

    if (slot < MIDPOINT) {
      ++leaf->count;
      memmove(leaf->nodes + slot + 1, leaf->nodes + slot,
              (leaf->count - slot) * sizeof(leaf->nodes[0]));
      leaf->nodes[slot] = node;
    } else {
      slot -= MIDPOINT;
      memmove(newLeaf->nodes + slot + 1, newLeaf->nodes + slot,
              (newLeaf->count - slot) * sizeof(leaf->nodes[0]));
      newLeaf->nodes[slot] = node;
      ++newLeaf->count;
    }

    splitNode = newLeaf->nodes[0];
  }

  ++count;
  DIBucket *splitBucket = (DIBucket *)newLeaf;

  // Handle an initial split

  if (levels == 0) {
    ASSERT(bucket == (DIBucket *)leaf);
    ++levels;
    DIBucket *newBucket = (DIBucket *)alloc(sizeof(DIBucket));
    newBucket->count = 2;
    newBucket->references[0].node = NULL;      // leaf->nodes[0];
    newBucket->references[0].bucket = bucket;  //(DIBucket*) leaf;
    newBucket->references[1].node = splitNode;
    newBucket->references[1].bucket = splitBucket;
    root = newBucket;

    return;
  }

  // Propograte a split upward

  for (level = 1; level <= levels; ++level) {
    bucket = buckets[level];

    // Find insertion point

    for (slot = 0; slot < bucket->count; ++slot)
      if (compare(node, bucket->references[slot].node) < 0) break;

    // If there's room, insert node and we're done

    if (bucket->count < DEFERRED_INDEX_FANOUT) {
      memmove(bucket->references + slot + 1, bucket->references + slot,
              (bucket->count - slot) * sizeof(bucket->references[0]));
      bucket->references[slot].bucket = splitBucket;
      bucket->references[slot].node = splitNode;
      ++bucket->count;

      return;
    }

    DIBucket *newBucket = (DIBucket *)alloc(sizeof(DIBucket));
    bucket->count = MIDPOINT;
    newBucket->count = MIDPOINT;
    memcpy(newBucket->references, bucket->references + MIDPOINT,
           MIDPOINT * sizeof(bucket->references[0]));

    if (slot < MIDPOINT) {
      memmove(bucket->references + slot + 1, bucket->references + slot,
              (bucket->count - slot) * sizeof(bucket->references[0]));
      bucket->references[slot].bucket = splitBucket;
      bucket->references[slot].node = splitNode;
      ++bucket->count;
    } else {
      slot -= MIDPOINT;
      memmove(newBucket->references + slot + 1, newBucket->references + slot,
              (newBucket->count - slot) * sizeof(newBucket->references[0]));
      newBucket->references[slot].bucket = splitBucket;
      newBucket->references[slot].node = splitNode;
      ++newBucket->count;
    }

    splitBucket = newBucket;
    splitNode = newBucket->references[0].node;
  }

  ++levels;
  DIBucket *newBucket = (DIBucket *)alloc(sizeof(DIBucket));
  newBucket->count = 2;
  newBucket->references[0].node = NULL;
  newBucket->references[0].bucket = bucket;
  newBucket->references[1].node = splitNode;
  newBucket->references[1].bucket = splitBucket;
  root = newBucket;
}

bool DeferredIndex::deleteNode(IndexKey *key, int32 recordNumber) {
  bool doingDIHash = ((index->database->configuration->useDeferredIndexHash) &&
                      (INDEX_IS_UNIQUE(index->type)));

  Sync syncHash(&index->syncDIHash, "DeferredIndex::deleteNode(1)");
  if (doingDIHash) syncHash.lock(Exclusive);

  Sync sync(&syncObject, "DeferredIndex::deleteNode(2)");
  sync.lock(Exclusive);

  DIBucket *buckets[DEFERRED_INDEX_MAX_LEVELS];
  DIBucket *bucket = (DIBucket *)root;
  uint slot = 0;
  int level;

  // Search down index tree for appropriate leaf

  for (level = levels; level > 0; --level) {
    buckets[level] = bucket;

    for (slot = 0;; ++slot) {
      int n = compare(key, recordNumber, bucket->references[slot].node);

      if (n < 0) {
        bucket = bucket->references[(slot) ? slot - 1 : 0].bucket;

        break;
      }

      if (slot + 1 == bucket->count) {
        bucket = bucket->references[slot].bucket;
        break;
      }
    }
  }

  DILeaf *leaf = (DILeaf *)bucket;

  // Handle leaf

  for (slot = 0; slot < leaf->count; ++slot) {
    int n = compare(key, recordNumber, leaf->nodes[slot]);

    if (n == 0) {
      DINode *node = leaf->nodes[slot];

      if (doingDIHash) {
        DIUniqueNode *uniqueNode = UNIQUE_NODE(node);
        index->removeFromDIHash(uniqueNode);
      }

      if (node == minValue) {
        if (slot + 1 < leaf->count)
          minValue = leaf->nodes[slot + 1];
        else
          haveMinValue = false;
      }

      if (node == maxValue) {
        if (slot > 0)
          maxValue = leaf->nodes[slot - 1];
        else
          haveMaxValue = false;
      }

      --leaf->count;
      memmove(leaf->nodes + slot, leaf->nodes + slot + 1,
              (leaf->count - slot) * sizeof(leaf->nodes[0]));
      --count;
      sizeEstimate -= key->keyLength + 4;  // recordNumber + keyLength + key

      return true;
    }

    if (n < 0) return false;
  }

  return false;
}

int DeferredIndex::compare(DINode *node1, DINode *node2) {
  if (!node1) return -1;

  if (!node2) return 1;

  uint len1 = node1->keyLength;
  uint len2 = node2->keyLength;
  uint minLen = MIN(len1, len2);
  int ret = 0;

  // Check which key is greatest up to the length of
  // the shortest key

  if ((ret = memcmp(node1->key, node2->key, minLen))) return ret;

  // Still equal, check which key that is the longest

  if ((ret = len1 > len2 ? 1 : len1 < len2 ? -1 : 0)) return ret;

  // Still equal, check which has the greatest recordNumber

  int32 rno1 = node1->recordNumber;
  int32 rno2 = node2->recordNumber;
  return rno1 > rno2 ? 1 : rno1 < rno2 ? -1 : 0;
}

int DeferredIndex::compare(IndexKey *node1, DINode *node2, bool partial) {
  if (!node1) return -1;

  if (!node2) {
    if (node1->keyLength == 0) return 0;

    return 1;
  }

  uint len1 = node1->keyLength;
  uint len2 = node2->keyLength;
  uint minLen = MIN(len1, len2);
  int ret = 0;

  // Check which key is greatest up to the length of
  // the shortest key

  if ((ret = memcmp(node1->key, node2->key, minLen))) return ret;

  if (partial) return 0;

  // Still equal, check which key that is the longest

  return len1 > len2 ? 1 : len1 < len2 ? -1 : 0;
}

int DeferredIndex::compare(IndexKey *node1, int32 recordNumber, DINode *node2) {
  if (!node1) return -1;

  if (!node2) return 1;

  uint len1 = node1->keyLength;
  uint len2 = node2->keyLength;
  uint minLen = MIN(len1, len2);
  int ret = 0;

  // Check which key is greatest up to the length of
  // the shortest key

  if ((ret = memcmp(node1->key, node2->key, minLen))) return ret;

  // Still equal, check which key that is the longest

  if ((ret = len1 > len2 ? 1 : len1 < len2 ? -1 : 0)) return ret;

  // Still equal, check which has the greatest recordNumber

  int32 rno2 = node2->recordNumber;
  return recordNumber > rno2 ? 1 : recordNumber < rno2 ? -1 : 0;
}

void DeferredIndex::validate(void) {
  uint n = 0;
  DINode *prior = NULL;
  DeferredIndexWalker walker(this, NULL);

  for (DINode *node; (node = walker.next());) {
    ++n;

    if (prior && compare(prior, node) >= 0) {
      Log::log("DeferredIndex::validate: tree corrupted\n");
      print();

      return;
    }

    Bitmap bitmap;
    IndexKey indexKey(node->keyLength, node->key);
    scanIndex(&indexKey, &indexKey, false, &bitmap);

    if (!bitmap.isSet(node->recordNumber)) {
      Btn::printKey("DeferredIndex: search key: ", &indexKey, 0, false);
      print();
      scanIndex(&indexKey, &indexKey, false, &bitmap);
      return;
    }

    prior = node;
  }

  if (n != count) {
    Log::log("DeferredIndex::validate: expected %d nodes, got %d\n", count, n);
    print();

    DeferredIndexWalker walk(this, NULL);

    for (DINode *node; (node = walk.next());) print("Node:", node);
  }
}

char *DeferredIndex::format(uint indent, DINode *node, uint bufferLength,
                            char *buffer) {
  char *endBuffer = buffer + bufferLength - 5;
  char *p = buffer;

  if (indent < bufferLength) {
    memset(p, ' ', indent);
    p += indent;
  }

  if (!node) {
    snprintf(p, endBuffer - p, "%p *** null ***  ", node);

    return buffer;
  }

  snprintf(p, endBuffer - p, "%p [%d,%d]  ", node, node->recordNumber,
           node->keyLength);

  while (*p) ++p;

  bool hex = false;

  for (int n = 0; n < node->keyLength; ++n) {
    char c = printable[node->key[n]];

    if (c) {
      if (hex) *p++ = '.';

      *p++ = c;
      hex = false;
    } else {
      if (!hex) *p++ = '.';

      snprintf(p, endBuffer - p, "%x", node->key[n]);

      while (*p) ++p;

      hex = true;
    }
  }

  *p = 0;

  return buffer;
}

void DeferredIndex::print() {
  LogLock lock;
  Log::log("Deferred index for %s\n", (const char *)index->name);
  print(3, levels, (DIBucket *)root);
}

void DeferredIndex::print(DIBucket *bucket) {
  char buffer[256];
  Log::log("DIBucket %p\n", bucket);

  for (uint n = 0; n < bucket->count; ++n)
    Log::log("%s\n",
             format(3, bucket->references[n].node, sizeof(buffer), buffer));
}

void DeferredIndex::print(DILeaf *leaf) {
  char buffer[256];
  Log::log("DILeaf %p\n", leaf);

  for (uint n = 0; n < leaf->count; ++n)
    Log::log("%s\n", format(3, leaf->nodes[n], sizeof(buffer), buffer));
}

void DeferredIndex::print(int indent, int level, DIBucket *bucket) {
  char buffer[256];

  if (level == 0) {
    DILeaf *leaf = (DILeaf *)bucket;

    for (uint n = 0; n < leaf->count; ++n)
      Log::log("%s\n", format(indent, leaf->nodes[n], sizeof(buffer), buffer));

    return;
  }

  for (uint n = 0; n < bucket->count; ++n) {
    Log::log("%s\n", format(indent, bucket->references[n].node, sizeof(buffer),
                            buffer));
    print(indent + 3, level - 1, bucket->references[n].bucket);
  }
}

void DeferredIndex::print(const char *text, DINode *node) {
  char buffer[256];
  Log::log("%s %s\n", text, format(0, node, sizeof(buffer), buffer));
}

void DeferredIndex::scanIndex(IndexKey *lowKey, IndexKey *highKey,
                              int searchFlags, Bitmap *bitmap) {
  Sync sync(&syncObject, "DeferredIndex::scanIndex");
  sync.lock(Shared);
  bool isPartial = (searchFlags & Partial) == Partial;

  // If  detached from transaction all index data is available
  // in the regular index - no reason to search this

  if (!transaction) return;

  // If the starting value is above our max value, don't bother

  if (haveMaxValue && maxValue && lowKey &&
      compare(lowKey, maxValue, isPartial) > 0)
    return;

  // If the ending value is below our min value, don't bother

  if (haveMinValue && minValue && highKey &&
      compare(highKey, minValue, isPartial) < 0)
    return;

  // First, be sure it has not already been put into the stream log.

  if ((virtualOffset < virtualOffsetAtEnd) && !count) {
    sync.unlock();
    sync.lock(Exclusive);

    if ((virtualOffset < virtualOffsetAtEnd) && !count) transaction->thaw(this);

    sync.unlock();
    sync.lock(Shared);
  }

  DeferredIndexWalker walker(this, lowKey, searchFlags);

  for (DINode *node; (node = walker.next());) {
    if (highKey && compare(highKey, node, isPartial) < 0) break;

#ifdef CHECK_DEFERRED_INDEXES
    if (!bitmap->isSet(node->recordNumber)) {
      print("bad deferred index retrieval", node);
      print();
    }
#endif

    bitmap->set(node->recordNumber);
  }
}

void DeferredIndex::detachIndex(void) {
  Sync sync(&syncObject, "DeferredIndex::detachIndex");
  sync.lock(Exclusive);  // Do not change while index is in use.
  index = NULL;
}

void DeferredIndex::detachTransaction(void) {
  Sync sync(&syncObject, "DeferredIndex::detachTransaction");
  sync.lock(Exclusive);

  if (index) {
    // As soon as we unlock, index could be set to null by ~Index()
    Index *myIndex = index;
    sync.unlock();
    myIndex->detachDeferredIndex(this);
    sync.lock(Exclusive);
    index = NULL;
  }

  transaction = NULL;
}

bool DeferredIndex::chill(Dbb *dbb) {
  Sync sync(&syncObject, "DeferredIndex::chill");
  sync.lock(Exclusive);

  if (!window) window = dbb->streamLog->setWindowInterest();

  dbb->logIndexUpdates(this);

  if (virtualOffset > 0) {
    // Virtual offset will be > 0 if chill was successful.
    // Free up the space used by this DeferredIndex.

    freeHunks();
    initializeSpace();
    levels = 0;
    DILeaf *leaf = (DILeaf *)alloc(sizeof(DILeaf));
    leaf->count = 0;
    root = leaf;
    count = 0;
    minValue = NULL;
    maxValue = NULL;
    haveMinValue = true;
    haveMaxValue = true;

    Log::log(LogInfo,
             "%d: Index chill: transaction %ld, index %ld, %ld bytes, address "
             "%p, vofs %llx\n",
             dbb->database->deltaTime, transaction->transactionId,
             index->indexId, sizeEstimate, this, virtualOffset);
  } else {
    Log::log(LogInfo,
             "%d: Index chill: transaction %ld, index %ld, %ld bytes, address "
             "%p, vofs %llx - NOT CHILLED\n",
             dbb->database->deltaTime, transaction->transactionId,
             index->indexId, sizeEstimate, this, virtualOffset);
  }

  return virtualOffset > 0;
}

DINode *DeferredIndex::findMaxValue(void) { return NULL; }

DINode *DeferredIndex::findMinValue(void) { return NULL; }

void DeferredIndex::addRef() { INTERLOCKED_INCREMENT(useCount); }

void DeferredIndex::release() {
  ASSERT(useCount > 0);

  if (INTERLOCKED_DECREMENT(useCount) == 0) delete this;
}

}  // namespace Changjiang
