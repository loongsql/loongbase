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

// Table.h: interface for the Table class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "PrivilegeObject.h"
#include "LinkedList.h"
#include "SyncObject.h"
#include "Types.h"
#include "Index.h"
#include "SparseArray.h"

namespace Changjiang {

static const int PreInsert = 1;
static const int PostInsert = 2;
static const int PreUpdate = 4;
static const int PostUpdate = 8;
static const int PreDelete = 16;
static const int PostDelete = 32;
static const int PreCommit = 64;
static const int PostCommit = 128;

static const int BL_SIZE = 128;
static const int FORMAT_HASH_SIZE = 20;
static const int SYNC_VERSIONS_SIZE = 32;  // 16;
static const int SYNC_THAW_SIZE = 32;      // 16;

#define FOR_FIELDS(field, table) \
  {                              \
    for (Field *field = table->fields; field; field = field->next) {
// For all indexes, regardless of state

#define FOR_ALL_INDEXES(index, table) \
  {                                   \
    for (Index *index = table->indexes; index; index = index->next) {
// For all indexes except incomplete or invalid

#define FOR_INDEXES(index, table)                                     \
  {                                                                   \
    for (Index *index = table->indexes; index; index = index->next) { \
      if (index->indexId == -1) continue;

class Database;
class Dbb;
class Index;
class Transaction;
class TransactionState;
class Value;
CLASS(Field);
class Format;
class Record;
class RecordSection;
class RecordVersion;
class ForeignKey;
class TableAttachment;
class View;
class ChjTrigger;
class InversionFilter;
class Bitmap;
class Collation;
class Repository;
class BlobReference;
class AsciiBlob;
class BinaryBlob;
class Section;
class TableSpace;
class RecordScavenge;

class Table : public PrivilegeObject {
 public:
  Table(Database *db, const char *schema, const char *tableName, int id,
        int version, uint64 numberRecords, TableSpace *tblSpace);
  Table(Database *db, int tableId, const char *schema, const char *name,
        TableSpace *tableSpace);
  virtual ~Table();

  void expunge(Transaction *transaction);
  JString getPrimaryKeyName(void);
  void getBlob(int recordNumber, Stream *stream);
  int storeBlob(Transaction *transaction, uint32 length, const UCHAR *data);
  void rename(const char *newSchema, const char *newName);
  void getIndirectBlob(int recordId, BlobReference *blob);
  BinaryBlob *getBinaryBlob(int recordId);
  AsciiBlob *getAsciiBlob(int recordId);
  int32 getIndirectId(BlobReference *reference, TransactionState *transaction);
  void refreshFields();
  void insertView(Transaction *transaction, int count, Field **fieldVector,
                  Value **values);
  void bind(Table *table);
  void clearIndexesRebuild();
  void rebuildIndexes(Transaction *transaction, bool force = false);
  void collationChanged(Field *field);
  void validateBlobs(int optionMask);
  void rebuildIndex(Index *index, Transaction *transaction);
  void pruneRecords(RecordScavenge *recordScavenge);
  void retireRecords(RecordScavenge *recordScavenge);
  int countActiveRecords();
  int chartActiveRecords(int *chart);
  bool foreignKeyMember(ForeignKey *key);
  void makeNotSearchable(Field *field, Transaction *transaction);
  bool dropForeignKey(int fieldCount, Field **fields, Table *references);
  void checkUniqueIndexes(Transaction *transaction, RecordVersion *record);
  bool checkUniqueIndex(Index *index, Transaction *transaction,
                        RecordVersion *record, Sync *sync);
  bool checkUniqueRecordVersion(int32 recordNumber, Index *index,
                                Transaction *transaction, RecordVersion *record,
                                Sync *sync);
  bool isDuplicate(Index *index, Record *record1, Record *record2);
  void checkDrop();
  Field *findField(const WCString *fieldName);
  void setType(const char *typeName);
  InversionFilter *getFilters(Field *field, Record *records, Record *limit);
  void garbageCollectInversion(Field *field, Record *leaving, Record *staying,
                               Transaction *transaction);
  void postCommit(Transaction *transaction, RecordVersion *record);
  void buildFieldVector();
  int nextPrimaryKeyColumn(int previous);
  int nextColumnId(int previous);
  void loadStuff();
  void clearAlter(void);
  bool setAlter(void);

  void addTrigger(ChjTrigger *trigger);
  void dropTrigger(ChjTrigger *trigger);
  ChjTrigger *findTrigger(const char *name);
  void fireTriggers(Transaction *transaction, int operation, Record *before,
                    RecordVersion *after);

#ifndef STORAGE_ENGINE
  void zapLinkages();
#endif

  void addIndex(Index *index);
  void dropIndex(Index *index);
  void reIndex(Transaction *transaction);
  void loadIndexes();

  void garbageCollect(Record *leaving, Record *staying,
                      Transaction *transaction, bool quiet);
  void expungeBlob(Value *blob);
  bool duplicateBlob(Value *blob, int fieldId, Record *recordChain);
  void expungeRecord(int32 recordNumber);
  void setView(View *view);
  Index *findIndex(const char *indexName);
  virtual PrivObject getPrivilegeType();
  void populateIndex(Index *index, Transaction *transaction);
  const char *getSchema();
  ForeignKey *dropForeignKey(ForeignKey *key);
  void dropField(Field *field);
  void addAttachment(TableAttachment *attachment);
  void addField(Field *field);
  void checkNullable(Record *record);
  virtual void drop(Transaction *transaction);
  virtual void truncate(Transaction *transaction);
  void updateInversion(Record *record, Transaction *transaction);
  int getFieldId(const char *name);
  ForeignKey *findForeignKey(ForeignKey *key);
  bool indexExists(ForeignKey *foreignKey);
  ForeignKey *findForeignKey(Field *field, bool foreign);
  Field *findField(int id);
  void addForeignKey(ForeignKey *key);
  Index *getPrimaryKey();
  bool isCreated();
  void reIndexInversion(Transaction *transaction);
  void makeSearchable(Field *field, Transaction *transaction);
  int32 getBlobId(Value *value, int32 oldId, bool cloneFlag,
                  TransactionState *transaction);
  void addFormat(Format *format);
  void rollbackRecord(RecordVersion *recordVersion, Transaction *transaction);
  Record *fetch(int32 recordNumber);
  void init(int id, const char *schema, const char *tableName,
            TableSpace *tblSpace);
  void loadFields();
  void setBlobSection(int32 section);
  void setDataSection(int32 section);
  void deleteIndex(Index *index, Transaction *transaction);
  Record *databaseFetch(int32 recordNumber);
  Record *fetchNext(int32 recordNumber);
  int numberFields();
  void updateRecord(RecordVersion *record);
  void reformat();
  Format *getFormat(int version);
  void save();
  void create(const char *tableType, Transaction *transaction);
  const char *getName();
  Index *addIndex(const char *name, int numberFields, int type);
  void dropIndex(const char *name, Transaction *transaction);
  void renameIndexes(const char *newTableName);
  Field *addField(const char *name, Type type, int length, int precision,
                  int scale, int flags);
  Field *findField(const char *name);
  int getFormatVersion();
  void validateAndInsert(Transaction *transaction, RecordVersion *record);
  bool hasUncommittedRecords(Transaction *transaction);
  void waitForWriteComplete();
  void checkAncestor(Record *current, Record *oldRecord);
  int64 estimateCardinality(void);
  void optimize(Connection *connection);
  void findSections(void);
  bool validateUpdate(int32 recordNumber, TransId transactionId);
  Record *treeFetch(int32 recordNumber);

  bool backlogRecord(RecordVersion *record, Bitmap *backlogBitmap);
  Record *backlogFetch(int32 recordNumber);
  void deleteRecordBacklog(int32 recordNumber);

  RecordVersion *allocRecordVersion(Format *format, Transaction *transaction,
                                    Record *priorVersion);
  Record *allocRecord(int recordNumber, Stream *stream);
  Format *getCurrentFormat(void);
  Record *fetchForUpdate(Transaction *transaction, Record *record,
                         bool usingIndex);
  void unlockRecord(int recordNumber, int verbMark);
  void unlockRecord(RecordVersion *record, int verbMark);
  void queueForDelete(Record *record);
  void queueForDelete(Value **record);
  void queueForDelete(char *record);

  void insert(Transaction *transaction, int count, Field **fields,
              Value **values);
  uint insert(Transaction *transaction, Stream *stream);
  bool insertIntoTree(Record *record, Record *prior, int recordNumber);
  void insertIndexes(Transaction *transaction, RecordVersion *record);

  void update(Transaction *transaction, Record *record, int numberFields,
              Field **fields, Value **values);
  void update(Transaction *transaction, Record *oldRecord, Stream *stream);
  void updateIndexes(Transaction *transaction, RecordVersion *record,
                     Record *oldRecord);

  void deleteRecord(Transaction *transaction, Record *record);
  void deleteRecord(int recordNumber);
  void deleteRecord(RecordVersion *record, Transaction *transaction);

  SyncObject *getSyncThaw(Record *record);
  SyncObject *getSyncThaw(int recordNumber);

  Dbb *dbb;
  SyncObject syncObject;
  SyncObject syncTriggers;
  SyncObject syncAlter;  // prevent concurrent Alter statements.
  SyncObject syncThaw[SYNC_THAW_SIZE];
  Table *collision;    // Hash collision in database
  Table *idCollision;  // mod(id) collision in database
  Table *next;         // next in database linked list
  Field *fields;
  Field **fieldVector;
  Index *indexes;
  ForeignKey *foreignKeys;
  LinkedList attachments;
  Format **formats;
  Format *format;
  RecordSection *records;
  Index *primaryKey;
  View *view;
  ChjTrigger *triggers;
  Bitmap *recordBitmap;
  Bitmap *emptySections;
  SparseArray<int32, BL_SIZE> *backloggedRecords;
  Section *dataSection;
  Section *blobSection;
  TableSpace *tableSpace;
  uint64 cardinality;
  uint64 priorCardinality;
  int tableId;
  int dataSectionId;
  int blobSectionId;
  int nextFieldId;
  int formatVersion;
  int fieldCount;
  int maxFieldId;
  bool changed;
  bool eof;
  bool markedForDelete;
  bool alterIsActive;
  bool deleting;  // dropping or truncating.
  int32 recordBitmapHighWater;
  int32 ageGroup;
  uint32 debugThawedRecords;
  uint64 debugThawedBytes;

 protected:
  const char *type;
};

}  // namespace Changjiang
