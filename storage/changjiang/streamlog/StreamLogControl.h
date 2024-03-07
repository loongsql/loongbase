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

// StreamLogControl.h: interface for the StreamLogControl class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SRLSwitchLog.h"
#include "SRLData.h"
#include "SRLCommit.h"
#include "SRLRollback.h"
#include "SRLPrepare.h"
#include "SRLIndexUpdate.h"
#include "SRLWordUpdate.h"
#include "SRLPrepare.h"
#include "SRLRecordStub.h"
#include "SRLSequence.h"
#include "SRLCheckpoint.h"
#include "SRLBlobUpdate.h"
#include "SRLDelete.h"
#include "SRLDropTable.h"
#include "SRLCreateSection.h"
#include "SRLSectionPage.h"
#include "SRLFreePage.h"
#include "SRLRecordLocator.h"
#include "SRLDataPage.h"
#include "SRLIndexAdd.h"
#include "SRLIndexDelete.h"
#include "SRLIndexPage.h"
#include "SRLInversionPage.h"
#include "SRLCreateIndex.h"
#include "SRLDeleteIndex.h"
#include "SRLVersion.h"
#include "SRLUpdateRecords.h"
#include "SRLUpdateIndex.h"
#include "SRLSectionPromotion.h"
#include "SRLSequencePage.h"
#include "SRLSectionLine.h"
#include "SRLOverflowPages.h"
#include "SRLCreateTableSpace.h"
#include "SRLDropTableSpace.h"
#include "SRLBlobDelete.h"
#include "SRLUpdateBlob.h"
#include "SRLSession.h"
#include "SRLSavepointRollback.h"
#include "SRLInventoryPage.h"
#include "SRLTableSpaces.h"

namespace Changjiang {

#define LOW_BYTE_FLAG 0x80

class StreamLogControl {
 public:
  StreamLogRecord *getRecordManager(int which);
  StreamLogControl(StreamLog *streamLog);
  virtual ~StreamLogControl();

  // void		setVersion (int newVersion);
  void validate(StreamLogWindow *window, StreamLogBlock *block);
  uint64 getBlockNumber();
  int getOffset();
  StreamLogTransaction *getTransaction(TransId transactionId);
  const UCHAR *getData(int length);
  StreamLogRecord *nextRecord();
  bool atEnd();
  UCHAR getByte();
  int getInt();
  void fini(void);
  void setWindow(StreamLogWindow *window, StreamLogBlock *block, int offset);
  void printBlock(StreamLogBlock *block);
  void haveCheckpoint(int64 blockNumber);
  bool isPostFlush(void);

  uint64 lastCheckpoint;
  StreamLog *log;
  StreamLogWindow *inputWindow;
  StreamLogBlock *inputBlock;
  const UCHAR *input;
  const UCHAR *inputEnd;
  const UCHAR *recordStart;
  int version;
  bool debug;
  bool singleBlock;
  StreamLogRecord *records[srlMax];

  SRLCommit commit;
  SRLRollback rollback;
  SRLPrepare prepare;
  SRLData dataUpdate;
  SRLIndexUpdate indexUpdate;
  SRLWordUpdate wordUpdate;
  SRLSwitchLog switchLog;
  SRLRecordStub recordStub;
  SRLSequence sequence;
  SRLCheckpoint checkpoint;
  SRLBlobUpdate largeBlob;
  SRLDelete deleteData;
  SRLDropTable dropTable;
  SRLCreateSection createSection;
  SRLSectionPage sectionPage;
  SRLFreePage freePage;
  SRLRecordLocator recordLocator;
  SRLDataPage dataPage;
  SRLIndexAdd indexAdd;
  SRLIndexDelete indexDelete;
  SRLIndexPage indexPage;
  SRLInversionPage inversionPage;
  SRLCreateIndex createIndex;
  SRLDeleteIndex deleteIndex;
  SRLVersion logVersion;
  SRLUpdateRecords updateRecords;
  SRLUpdateIndex updateIndex;
  SRLSectionPromotion sectionPromotion;
  SRLSequencePage sequencePage;
  SRLSectionLine sectionLine;
  SRLOverflowPages overflowPages;
  SRLCreateTableSpace createTableSpace;
  SRLDropTableSpace dropTableSpace;
  SRLBlobDelete blobDelete;
  SRLUpdateBlob smallBlob;
  SRLSession session;
  SRLSavepointRollback savepointRollback;
  SRLInventoryPage inventoryPage;
  SRLTableSpaces tableSpaces;
};

}  // namespace Changjiang
