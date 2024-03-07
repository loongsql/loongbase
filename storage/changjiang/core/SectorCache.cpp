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

#include "Engine.h"
#include "SectorCache.h"
#include "SectorBuffer.h"
#include "Dbb.h"
#include "BDB.h"
#include "Sync.h"

namespace Changjiang {

#define SECTOR_BUFFER_ALIGNMENT = 4096;

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

SectorCache::SectorCache(int numBuffers, int pgSize) {
  memset(hashTable, 0, sizeof(hashTable));
  numberBuffers = numBuffers;
  pageSize = pgSize;
  pagesPerSector = SECTOR_BUFFER_SIZE / pageSize;
  bufferSpace = new UCHAR[(numberBuffers + 1) * SECTOR_BUFFER_SIZE];
  UCHAR *p = (UCHAR *)(((UIPTR)bufferSpace + SECTOR_BUFFER_SIZE - 1) /
                       SECTOR_BUFFER_SIZE * SECTOR_BUFFER_SIZE);
  SectorBuffer *buffer = buffers = nextBuffer = new SectorBuffer[numberBuffers];
  SectorBuffer *prior = buffer + numberBuffers - 1;

  for (int n = 0; n < numberBuffers; ++n, ++buffer, p += SECTOR_BUFFER_SIZE) {
    prior->next = buffer;
    prior = buffer;
    buffer->cache = this;
    buffer->buffer = p;
  }

  syncObject.setName("SectorCache::syncObject");
}

SectorCache::~SectorCache(void) {
  delete[] bufferSpace;
  delete[] buffers;
}

void SectorCache::readPage(Bdb *bdb) {
  Sync sync(&syncObject, "SectorCache::readPage(1)");
  sync.lock(Shared);
  int sectorNumber = bdb->pageNumber / pagesPerSector;
  int slot = sectorNumber % SECTOR_HASH_SIZE;
  SectorBuffer *buffer;

  for (buffer = hashTable[slot]; buffer; buffer = buffer->collision)
    if (buffer->sectorNumber == sectorNumber && buffer->dbb == bdb->dbb) {
      Sync syncBuffer(&buffer->syncObject, "SectorCache::readPage(2)");
      syncBuffer.lock(Shared);
      sync.unlock();
      buffer->readPage(bdb);

      return;
    }

  sync.unlock();
  sync.lock(Exclusive);

  for (buffer = hashTable[slot]; buffer; buffer = buffer->collision)
    if (buffer->sectorNumber == sectorNumber && buffer->dbb == bdb->dbb) {
      Sync syncBuffer(&buffer->syncObject, "SectorCache::readPage(3)");
      syncBuffer.lock(Shared);
      sync.unlock();
      buffer->readPage(bdb);

      return;
    }

  buffer = nextBuffer;
  nextBuffer = buffer->next;
  Sync syncBuffer(&buffer->syncObject, "SectorCache::readPage(4)");
  syncBuffer.lock(Exclusive);

  if (buffer->sectorNumber >= 0)
    for (SectorBuffer **ptr =
             hashTable + (buffer->sectorNumber % SECTOR_HASH_SIZE);
         *ptr; ptr = &(*ptr)->collision)
      if (*ptr == buffer) {
        *ptr = buffer->collision;

        break;
      }

  buffer->collision = hashTable[slot];
  hashTable[slot] = buffer;
  buffer->setSector(bdb->dbb, sectorNumber);
  sync.unlock();
  buffer->readSector();
  buffer->readPage(bdb);
}

void SectorCache::writePage(Bdb *bdb) {
  Sync sync(&syncObject, "SectorCache::writePage(1)");
  sync.lock(Shared);
  int sectorNumber = bdb->pageNumber / pagesPerSector;
  int slot = sectorNumber % SECTOR_HASH_SIZE;

  for (SectorBuffer *buffer = hashTable[slot]; buffer;
       buffer = buffer->collision)
    if (buffer->sectorNumber == sectorNumber && buffer->dbb == bdb->dbb) {
      Sync syncBuffer(&buffer->syncObject, "SectorCache::writePage(2)");
      syncBuffer.lock(Shared);
      sync.unlock();
      buffer->writePage(bdb);

      return;
    }
}

}  // namespace Changjiang
