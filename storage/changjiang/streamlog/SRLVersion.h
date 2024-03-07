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

// SRLVersion.h: interface for the SRLVersion class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "StreamLogRecord.h"

namespace Changjiang {

static const int srlVersion0 = 0;
static const int srlVersion1 = 1;  // Added length and data to SRLIndexPage
static const int srlVersion2 = 2;  // Added transactionId to SRLCreateIndex
static const int srlVersion3 = 3;  // Added transactionId to SRLDropTable
static const int srlVersion4 = 4;  // Added transactionId to SRLDeleteIndex
static const int srlVersion5 = 5;  // Added locatorPageNumber to SRLDataPage
static const int srlVersion6 =
    6;  // Added index version number of SRLIndexUpdate, SRLUpdateIndex, and
        // SRLDeleteIndex
static const int srlVersion7 = 7;  // Added xid to SRLPrepare	1/20/07
static const int srlVersion8 =
    8;  // Adding tableSpaceId to many classes	June 5, 2007 (Ann's birthday!)
static const int srlVersion9 =
    9;  // Added block number for checkpoint operation	July 9, 2007
static const int srlVersion10 =
    10;  // Added transaction id for drop table space	July 9, 2007
static const int srlVersion11 =
    11;  // Added table space type (repository support)	December 4, 2007
static const int srlVersion12 =
    12;  // Added index version number to SRLIndexPage	February 13, 2008
static const int srlVersion13 = 13;  // Added savepoint id to SRLUpdateRecords
                                     // February 14, 2008
static const int srlVersion14 =
    14;  // Added supernodes logging	March 7, 2008
static const int srlVersion15 =
    15;  // Added tablespace parameters to SRLCreateTableSpace	March 27, 2008
static const int srlVersion16 = 16;  // Added SRLInventoryPage January 26, 2009
static const int srlVersion17 = 17;  // Log root page number in SRLCreateIndex
static const int srlVersion18 = 18;  // Log tablespace list in SRLTableSpaces
static const int srlVersion19 =
    19;  // Remove parent and prior pointers from SRLIndexPage
static const int srlVersion20 = 20;  // Add earlyWrite to SRLOverflowPages
static const int srlCurrentVersion = srlVersion20;

class SRLVersion : public StreamLogRecord {
 public:
  void read();
  SRLVersion();
  virtual ~SRLVersion();

  int version;
};

}  // namespace Changjiang
