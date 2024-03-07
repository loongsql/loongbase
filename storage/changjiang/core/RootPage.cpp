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

// RootPage.cpp: implementation of the RootPage class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "RootPage.h"
#include "Dbb.h"
#include "BDB.h"

namespace Changjiang {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

/***
RootPage::RootPage()
{

}

RootPage::~RootPage()
{

}
***/

void RootPage::createSectionRoot(Dbb *dbb, TransId transId) {
  Bdb *bdb = dbb->fakePage(SECTION_ROOT, PAGE_sections, transId);
  BDB_HISTORY(bdb);
  RootPage *sections = (RootPage *)bdb->buffer;
  sections->section = -1;
  sections->level = 0;
  sections->sequence = 0;
  bdb->release(REL_HISTORY);
}

}  // namespace Changjiang
