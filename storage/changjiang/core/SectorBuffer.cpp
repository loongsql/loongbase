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

#include <memory.h>
#include "Engine.h"
#include "SectorBuffer.h"
#include "SectorCache.h"
#include "BDB.h"
#include "Dbb.h"
#include "Page.h"

namespace Changjiang {

SectorBuffer::SectorBuffer() {
  activeLength = 0;
  sectorNumber = -1;
  syncObject.setName("SectorBuffer::syncObject");
}

SectorBuffer::~SectorBuffer(void) {}

void SectorBuffer::readPage(Bdb *bdb) {
  int pageSize = cache->pageSize;

  int offset = (bdb->pageNumber % cache->pagesPerSector) * pageSize;
  ASSERT(offset < activeLength);

  Page *page = (Page *)(buffer + offset);

  /*
  Validate page checksum.
  Do it only once and after that reset the checksum field. It is necessary
  because later the checksum in header might be incorrect (when page is read,
  modified and written back to buffer�but not yet to disk).Also, the same page
  might be read multiple times and we want to avoid the checksum calculation
  overhead.
  */
  if (page->checksum != NO_CHECKSUM_MAGIC) {
    dbb->validateChecksum(page, pageSize, ((int64)bdb->pageNumber) * pageSize);
    page->checksum = NO_CHECKSUM_MAGIC;
  }
  memcpy(bdb->buffer, page, pageSize);
}

void SectorBuffer::readSector() {
  uint64 offset = (uint64)sectorNumber * (uint64)cache->pagesPerSector *
                  (uint64)cache->pageSize;
  activeLength = dbb->pread(offset, SECTOR_BUFFER_SIZE, buffer);
}

void SectorBuffer::setSector(Dbb *db, int sector) {
  dbb = db;
  sectorNumber = sector;
}

void SectorBuffer::writePage(Bdb *bdb) {
  int offset = (bdb->pageNumber % cache->pagesPerSector) * cache->pageSize;
  memcpy(buffer + offset, bdb->buffer, cache->pageSize);
  offset += cache->pageSize;
  activeLength = MAX(activeLength, offset);
}

}  // namespace Changjiang
