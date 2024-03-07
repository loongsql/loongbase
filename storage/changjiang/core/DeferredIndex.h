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

#pragma once

#include "SyncObject.h"

namespace Changjiang {

static const uint DEFERRED_INDEX_FANOUT = 32;  // 4;
static const uint DEFERRED_INDEX_HUNK_SIZE = 32768;
static const int DEFERRED_INDEX_MAX_LEVELS = 16;

struct DIHunk {
  DIHunk *next;
  UCHAR space[DEFERRED_INDEX_HUNK_SIZE];
};

struct DINode {
  int32 recordNumber;
  uint16 keyLength;
  UCHAR key[1];
};

struct DIUniqueNode {
  DIUniqueNode *collision;
  DINode node;
};

#define UNIQUE_NODE(node) \
  ((DIUniqueNode *)(((char *)node) - OFFSET(DIUniqueNode *, node)))

struct DILeaf {
  uint count;
  DINode *nodes[DEFERRED_INDEX_FANOUT];
};

struct DIBucket;

struct DIRef {
  DINode *node;
  DIBucket *bucket;
};

struct DIBucket {
  uint count;
  DIRef references[DEFERRED_INDEX_FANOUT];
};

struct DIState {
  DIBucket *bucket;
  uint slot;
};

class Transaction;
class Index;
class IndexKey;
class Bitmap;
class Dbb;
class StreamLogWindow;

class DeferredIndex {
 public:
  DeferredIndex(Index *index, Transaction *trans);
  ~DeferredIndex(void);

  void freeHunks(void);
  void *alloc(uint length);
  void addNode(IndexKey *indexKey, int32 recordNumber);
  bool deleteNode(IndexKey *key, int32 recordNumber);
  int compare(IndexKey *node1, DINode *node2, bool partial);
  int compare(DINode *node1, DINode *node2);
  int compare(IndexKey *node1, int32 recordNumber, DINode *node2);
  void detachIndex(void);
  void detachTransaction(void);
  void scanIndex(IndexKey *lowKey, IndexKey *highKey, int searchFlags,
                 Bitmap *bitmap);
  bool chill(Dbb *dbb);

  void validate(void);
  static char *format(uint indent, DINode *node, uint bufferLength,
                      char *buffer);
  void print();
  void print(DIBucket *bucket);
  void print(DILeaf *leaf);
  void print(int indent, int level, DIBucket *bucket);
  void print(const char *text, DINode *node);
  void initializeSpace(void);
  DINode *findMaxValue(void);
  DINode *findMinValue(void);
  void addRef();
  void release();

  SyncObject syncObject;
  volatile INTERLOCK_TYPE useCount;
  DeferredIndex *next;
  DeferredIndex *prior;
  DeferredIndex *nextInTransaction;
  Index *index;
  Transaction *transaction;
  TransId transactionId;
  DIHunk *hunks;
  DINode *minValue;
  DINode *maxValue;
  UCHAR initialSpace[512];
  UCHAR *base;
  void *root;
  uint currentHunkOffset;
  uint count;
  int levels;
  bool haveMinValue;
  bool haveMaxValue;
  uint sizeEstimate;
  uint64 virtualOffset;  // virtual offset into the stream log where this DI was
                         // flushed.
  uint64 virtualOffsetAtEnd;
  StreamLogWindow *window;
};

}  // namespace Changjiang
