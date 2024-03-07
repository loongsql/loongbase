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
#include "SRLInventoryPage.h"
#include "PageInventoryPage.h"
#include "StreamLogControl.h"
#include "Dbb.h"
#include "BDB.h"
#include "Page.h"
#include "Log.h"

namespace Changjiang {

// Recreate inventory page
void SRLInventoryPage::pass2() {
  if (control->isPostFlush()) {
    Bdb *bdb = PageInventoryPage::createInventoryPage(
        log->getDbb(tableSpaceId), pageNumber, NO_TRANSACTION);
    bdb->mark(NO_TRANSACTION);
    bdb->release(REL_HISTORY);
  }
}

void SRLInventoryPage::print() {
  logPrint("InventoryPage tableSpaceId %d, page %d \n", tableSpaceId,
           pageNumber);
}

void SRLInventoryPage::read() {
  tableSpaceId = getInt();
  pageNumber = getInt();
}

void SRLInventoryPage::append(Dbb *dbb, int32 pageNumber) {
  START_RECORD(srlInventoryPage, "SRLInventoryPage::append");
  putInt(dbb->tableSpaceId);
  putInt(pageNumber);
  sync.unlock();
}

SRLInventoryPage::SRLInventoryPage() {}

SRLInventoryPage::~SRLInventoryPage() {}

}  // namespace Changjiang
