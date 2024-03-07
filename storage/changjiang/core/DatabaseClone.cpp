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
#include "DatabaseClone.h"
#include "Dbb.h"
#include "Bitmap.h"
#include "Sync.h"
#include "Hdr.h"
#include "PageInventoryPage.h"
#include "BDB.h"
#include "Cache.h"
#include "StreamLog.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

DatabaseClone::DatabaseClone(Dbb *dbb) : DatabaseCopy(dbb) { shadow = NULL; }

DatabaseClone::~DatabaseClone(void) { delete shadow; }

void DatabaseClone::close(void) {
  if (shadow) shadow->closeFile();
}

void DatabaseClone::createFile(const char *fileName) {
  shadow = new IO;
  shadow->pageSize = dbb->pageSize;
  shadow->dbb = dbb;
  shadow->createFile(fileName);
}

const char *DatabaseClone::getFileName(void) {
  return (shadow) ? (const char *)shadow->fileName : "unknown";
}

void DatabaseClone::readHeader(Hdr *header) {
  if (shadow) shadow->readHeader(header);
}

void DatabaseClone::writeHeader(Hdr *header) {
  if (shadow) shadow->writeHeader(header);
}

void DatabaseClone::writePage(Bdb *bdb) {
  if (shadow) shadow->writePage(bdb, WRITE_TYPE_CLONE);
}

void DatabaseClone::clone(void) {
  Sync sync(&syncObject, "DatabaseClone::clone(1)");
  int n = 0;

  for (;;) {
    int lastPage = PageInventoryPage::getLastPage(dbb);

    // If we've copied all pages, maybe we can finish up the process

    if (n >= lastPage) {
      // If any pages were written again after we copied them, copy them again

      if (rewrittenPages) {
        sync.lock(Exclusive);

        for (int32 pageNumber;
             (pageNumber = rewrittenPages->nextSet(0)) >= 0;) {
          rewrittenPages->clear(pageNumber);
          sync.unlock();
          Bdb *bdb = dbb->fetchPage(pageNumber, PAGE_any, Shared);
          BDB_HISTORY(bdb);
          shadow->writePage(bdb, WRITE_TYPE_CLONE);
          bdb->release(REL_HISTORY);
          sync.lock(Exclusive);
        }

        sync.unlock();
      }

      //  In theory, we're done.  Lock the cache against changes, and check
      //  again

      Sync syncCache(&dbb->cache->syncObject, "DatabaseClone::clone(2)");
      syncCache.lock(Exclusive);
      lastPage = PageInventoryPage::getLastPage(dbb);

      // Check one last time for done.  If anything snuck in, punt and try again

      if (n < lastPage || (rewrittenPages && rewrittenPages->nextSet(0) >= 0))
        continue;

      // We got all pages.  Next, update the clone header page

      Hdr header;
      shadow->readHeader(&header);
      header.logOffset = lastPage + 1;
      header.logLength = dbb->streamLog->appendLog(shadow, header.logOffset);
      shadow->writeHeader(&header);

      break;
    }

    for (; n < lastPage; ++n) {
      Bdb *bdb = dbb->fetchPage(n, PAGE_any, Shared);
      BDB_HISTORY(bdb);
      highWater = bdb->pageNumber;
      shadow->writePage(bdb, WRITE_TYPE_CLONE);
      bdb->release(REL_HISTORY);
    }

    atEnd = true;
  }
}

}  // namespace Changjiang
