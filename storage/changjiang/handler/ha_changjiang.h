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

class THD;
class my_decimal;
struct TABLE_SHARE;

namespace Changjiang {

class StorageConnection;
class StorageTable;
class StorageTableShare;
class CmdGen;
// class THD;
// class my_decimal;

#define TRUNCATE_ENABLED

static const int TRANSACTION_READ_UNCOMMITTED =
    1;  // Dirty reads, non-repeatable reads and phantom reads can occur.
static const int TRANSACTION_READ_COMMITTED =
    2;  // Dirty reads are prevented; non-repeatable reads and phantom reads can
        // occur.
static const int TRANSACTION_WRITE_COMMITTED =
    4;  // Dirty reads are prevented; non-repeatable reads happen after writes;
        // phantom reads can occur.
static const int TRANSACTION_CONSISTENT_READ =
    8;  // Dirty reads and non-repeatable reads are prevented; phantom reads can
        // occur.
static const int TRANSACTION_SERIALIZABLE =
    16;  // Dirty reads, non-repeatable reads and phantom reads are prevented.

// struct TABLE_SHARE;
class StorageIndexDesc;
struct StorageBlob;

class StorageInterface : public handler {
 public:
  StorageInterface(handlerton *, TABLE_SHARE *table_arg);
  ~StorageInterface(void);

  virtual int open(const char *name, int mode, uint test_if_locked,
                   const dd::Table *table_def) override;

  virtual const char *table_type(void) const override;
  virtual int close(void) override;
  virtual ulonglong table_flags(void) const override;
  virtual ulong index_flags(uint idx, uint part, bool all_parts) const override;

  virtual int info(uint what) override;
  virtual uint max_supported_keys(void) const override;
  virtual uint max_supported_key_length(void) const override;
  virtual uint max_supported_key_part_length(HA_CREATE_INFO *create_info
                                             [[maybe_unused]]) const override;

  virtual int rnd_init(bool scan) override;
  virtual int rnd_next(uchar *buf) override;
  virtual int rnd_pos(uchar *buf, uchar *pos) override;
  virtual void position(const uchar *record) override;

  virtual int create(const char *name, TABLE *form, HA_CREATE_INFO *info,
                     dd::Table *table_def) override;
  virtual THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                                     enum thr_lock_type lock_type) override;
  virtual int delete_table(const char *name,
                           const dd::Table *table_def) override;
  virtual int write_row(uchar *buff) override;
  virtual int update_row(const uchar *oldData, uchar *newData) override;
  virtual int delete_row(const uchar *buf) override;
  virtual void unlock_row(void) override;

  virtual int index_read(uchar *buf, const uchar *key, uint keyLen,
                         enum ha_rkey_function find_flag) override;
  virtual int index_init(uint idx, bool sorted) override;
  virtual int index_end(void) override;
  virtual int index_first(uchar *buf) override;
  virtual int index_next(uchar *buf) override;

  // Multi Range Read interface
  virtual int multi_range_read_init(RANGE_SEQ_IF *seq, void *seq_init_param,
                                    uint n_ranges, uint mode,
                                    HANDLER_BUFFER *buf) override;
  virtual int multi_range_read_next(char **range_info) override;
  virtual ha_rows multi_range_read_info_const(uint keyno, RANGE_SEQ_IF *seq,
                                              void *seq_init_param,
                                              uint n_ranges, uint *bufsz,
                                              uint *flags,
                                              Cost_estimate *cost) override;
  virtual ha_rows multi_range_read_info(uint keyno, uint n_ranges, uint keys,
                                        uint *bufsz, uint *flags,
                                        Cost_estimate *cost) override;
  // Multi Range Read interface ends

  virtual int index_next_same(uchar *buf, const uchar *key,
                              uint key_len) override;

  virtual ha_rows records_in_range(uint index, key_range *lower,
                                   key_range *upper) override;
  virtual int rename_table(const char *from, const char *to,
                           const dd::Table *from_table_def,
                           dd::Table *to_table_def) override;
  virtual double read_time(uint index, uint ranges, ha_rows rows) override;
  virtual int read_range_first(const key_range *start_key,
                               const key_range *end_key, bool eq_range_arg,
                               bool sorted) override;
  virtual double scan_time(void) override;
  virtual int extra(ha_extra_function operation) override;
  virtual int start_stmt(THD *thd, thr_lock_type lock_type) override;
  virtual int external_lock(THD *thd, int lock_type) override;
  virtual void get_auto_increment(ulonglong offset, ulonglong increment,
                                  ulonglong nb_desired_values,
                                  ulonglong *first_value,
                                  ulonglong *nb_reserved_values);
  virtual bool get_error_message(int error, String *buf) override;
  virtual bool check_if_incompatible_data(HA_CREATE_INFO *create_info,
                                          uint table_changes) override;
  virtual void update_create_info(HA_CREATE_INFO *create_info) override;
  virtual const COND *cond_push(const COND *cond) override;
  virtual int optimize(THD *thd, HA_CHECK_OPT *check_opt) override;
  virtual int check(THD *thd, HA_CHECK_OPT *check_opt) override;
  virtual int repair(THD *thd, HA_CHECK_OPT *check_opt) override;
  virtual int reset() override;
  virtual enum_alter_inplace_result check_if_supported_inplace_alter(
      TABLE *altered_table, Alter_inplace_info *ha_alter_info) override;
  virtual bool prepare_inplace_alter_table(TABLE *altered_table,
                                           Alter_inplace_info *ha_alter_info,
                                           const dd::Table *old_table_def,
                                           dd::Table *new_table_def) override;
  virtual bool inplace_alter_table(TABLE *altered_table,
                                   Alter_inplace_info *ha_alter_info,
                                   const dd::Table *old_table_def,
                                   dd::Table *new_table_def) override;
  virtual bool commit_inplace_alter_table(TABLE *altered_table,
                                          Alter_inplace_info *ha_alter_info,
                                          bool commit,
                                          const dd::Table *old_table_def,
                                          dd::Table *new_table_def) override;
#ifdef TRUNCATE_ENABLED
  virtual int truncate(dd::Table *table_def [[maybe_unused]]) override;
  virtual int delete_all_rows(void) override;
#endif
  int analyze(THD *thd, HA_CHECK_OPT *check_opt) override;

  int addColumn(TABLE *altered_table);
  int addIndex(TABLE *alteredTable);
  int dropIndex(TABLE *alteredTable);
  void getDemographics(void);
  int createIndex(const char *schemaName, const char *tableName,
                  TABLE *srvTable, int indexId);
  int dropIndex(const char *schemaName, const char *tableName, TABLE *srvTable,
                int indexId, bool online);
  void getKeyDesc(TABLE *srvTable, int indexId, StorageIndexDesc *indexInfo);
  void startTransaction(void);
  bool threadSwitch(THD *newThread);
  int threadSwitchError(void);
  int error(int storageError);
  void freeActiveBlobs(void);
  int setIndex(TABLE *srvTable, int indexId);
  int setIndexes(TABLE *srvTable);
  int remapIndexes(TABLE *srvTable);
  bool validateIndexes(TABLE *srvTable, bool exclusiveLock = false);
  int genTable(TABLE *srvTable, CmdGen *gen);
  int genType(Field *field, CmdGen *gen);
  void genKeyFields(KEY *key, CmdGen *gen);
  void encodeRecord(uchar *buf, bool updateFlag);
  void decodeRecord(uchar *buf);
  void unlockTable(void);
  void checkBinLog(void);
  int scanRange(const key_range *startKey, const key_range *endKey,
                bool eqRange);
  int fillMrrBitmap();
  void mapFields(TABLE *table);
  void unmapFields(void);

  static StorageConnection *getStorageConnection(THD *thd);

  static int changjiang_init(void *p);
  static int changjiang_deinit(void *p);
  static int commit(handlerton *, THD *thd, bool all);
  static int prepare(handlerton *hton, THD *thd, bool all);
  static int rollback(handlerton *, THD *thd, bool all);
  static int recover(handlerton *hton, XID *xids, uint length);
  static int savepointSet(handlerton *, THD *thd, void *savePoint);
  static int savepointRollback(handlerton *, THD *thd, void *savePoint);
  static int savepointRelease(handlerton *, THD *thd, void *savePoint);
  static void dropDatabase(handlerton *, char *path);
  static void shutdown(handlerton *);
  static int closeConnection(handlerton *, THD *thd);
  static void logger(int mask, const char *text, void *arg);
  static void mysqlLogger(int mask, const char *text, void *arg);
  static int panic(handlerton *hton, ha_panic_function flag);
  // static bool	show_status(handlerton* hton, THD* thd, stat_print_fn* print,
  // enum ha_stat_type stat);
  static int getMySqlError(int storageError);

#if 0
	static uint		alter_table_flags(uint flags);
#endif
  static int alter_tablespace(handlerton *hton, THD *thd,
                              st_alter_tablespace *ts_info,
                              const dd::Tablespace *, dd::Tablespace *);
  static int fill_is_table(handlerton *hton, THD *thd, TABLE_LIST *tables,
                           class Item *cond, enum enum_schema_tables);

  static int commit_by_xid(handlerton *hton, XID *xid);
  static int rollback_by_xid(handlerton *hton, XID *xid);
  static int start_consistent_snapshot(handlerton *, THD *thd);

  static void updateRecordMemoryMax(MYSQL_THD thd, SYS_VAR *variable,
                                    void *var_ptr, const void *save);
  static void updateRecordScavengeThreshold(MYSQL_THD thd, SYS_VAR *variable,
                                            void *var_ptr, const void *save);
  static void updateRecordScavengeFloor(MYSQL_THD thd, SYS_VAR *variable,
                                        void *var_ptr, const void *save);
  static void updateDebugMask(MYSQL_THD thd, SYS_VAR *variable, void *var_ptr,
                              const void *save);

  /* Turn off table cache for now */

  // uint8 table_cache_type() { return HA_CACHE_TBL_TRANSACT; }

  StorageConnection *storageConnection;
  StorageTable *storageTable;
  StorageTableShare *storageShare;
  Field **fieldMap;
  const char *errorText;
  THR_LOCK_DATA lockData;  // MySQL lock
  THD *mySqlThread;
  TABLE_SHARE *share;
  uint recordLength;
  int lastRecord;
  int nextRecord;
  int indexErrorId;
  int errorKey;
  int maxFields;
  StorageBlob *activeBlobs;
  StorageBlob *freeBlobs;
  bool haveStartKey;
  bool haveEndKey;
  bool tableLocked;
  bool tempTable;
  bool lockForUpdate;
  bool indexOrder;
  key_range startKey;
  key_range endKey;
  uint64 insertCount;
  ulonglong tableFlags;
  bool useDefaultMrrImpl;
};

class NfsPluginHandler {
 public:
  NfsPluginHandler(void);
  ~NfsPluginHandler(void);

  StorageConnection *storageConnection;
  StorageTable *storageTable;

  static int getSystemMemoryDetailInfo(THD *thd, TABLE_LIST *tables,
                                       COND *cond);
  static int initSystemMemoryDetailInfo(void *p);
  static int deinitSystemMemoryDetailInfo(void *p);

  static int getSystemMemorySummaryInfo(THD *thd, TABLE_LIST *tables,
                                        COND *cond);
  static int initSystemMemorySummaryInfo(void *p);
  static int deinitSystemMemorySummaryInfo(void *p);

  static int getRecordCacheDetailInfo(THD *thd, TABLE_LIST *tables, COND *cond);
  static int initRecordCacheDetailInfo(void *p);
  static int deinitRecordCacheDetailInfo(void *p);

  static int getRecordCacheSummaryInfo(THD *thd, TABLE_LIST *tables,
                                       COND *cond);
  static int initRecordCacheSummaryInfo(void *p);
  static int deinitRecordCacheSummaryInfo(void *p);

  static int getTableSpaceIOInfo(THD *thd, TABLE_LIST *tables, COND *cond);
  static int initTableSpaceIOInfo(void *p);
  static int deinitTableSpaceIOInfo(void *p);

  static int getTransactionInfo(THD *thd, TABLE_LIST *tables, COND *cond);
  static int initTransactionInfo(void *p);
  static int deinitTransactionInfo(void *p);

  static int getTransactionSummaryInfo(THD *thd, TABLE_LIST *tables,
                                       COND *cond);
  static int initTransactionSummaryInfo(void *p);
  static int deinitTransactionSummaryInfo(void *p);

  static int getStreamLogInfo(THD *thd, TABLE_LIST *tables, COND *cond);
  static int initStreamLogInfo(void *p);
  static int deinitStreamLogInfo(void *p);

  static int getChangjiangVersionInfo(THD *thd, TABLE_LIST *tables, COND *cond);
  static int initChangjiangVersionInfo(void *p);
  static int deinitChangjiangVersionInfo(void *p);

  static int getSyncInfo(THD *thd, TABLE_LIST *tables, COND *cond);
  static int initSyncInfo(void *p);
  static int deinitSyncInfo(void *p);
};

}  // namespace Changjiang
