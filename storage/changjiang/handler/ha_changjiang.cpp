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

/* XXX correct? */

#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include "mysql_priv.h"
#include "changjiang_probes.h"

#ifdef _WIN32
#pragma pack()
#endif

#include "ha_changjiang.h"
#include "StorageConnection.h"
#include "StorageTable.h"
#include "StorageTableShare.h"
#include "StorageHandler.h"
#include "CmdGen.h"
#include "InfoTable.h"
#include "Format.h"
#include "Error.h"
#include "Log.h"
#include "ErrorInjector.h"
#ifdef _WIN32
#include "Engine.h"
#endif

#ifdef _WIN32
#define I64FORMAT "%I64d"
#else
#define I64FORMAT "%lld"
#endif

#include "ScaledBinary.h"
#include "BigInt.h"

#include "common/CommonUtil.h"

static handlerton *changjiang_hton = nullptr;
bool changjiang_sysvar_engine_force;

static handler *changjiang_create_handler(handlerton *hton, TABLE_SHARE *table,
                                          bool flag, MEM_ROOT *mem_root) {
  return new (mem_root) Changjiang::StorageInterface(hton, table);
}

bool IsChangjiangTable(TABLE *table) {
  return table && table->s && changjiang_hton &&
         (table->s->db_type() == changjiang_hton);
}

namespace Changjiang {

/* Verify that the compiler options have enabled C++ exception support */
#if (defined(__GNUC__) && !defined(__EXCEPTIONS)) || \
    (defined(_MSC_VER) && !defined(_CPPUNWIND))
#error Changjiang needs to be compiled with support for C++ exceptions. Please check your compiler settings.
#endif

//#define NO_OPTIMIZE
#define VALIDATE
//#define DEBUG_BACKLOG

#ifndef MIN
#define MIN(a, b) ((a <= b) ? (a) : (b))
#define MAX(a, b) ((a >= b) ? (a) : (b))
#endif

#ifndef ONLINE_ALTER
#define ONLINE_ALTER
#endif

#ifdef DEBUG_BACKLOG
static const uint LOAD_AUTOCOMMIT_RECORDS = 10000000;
#else
static const uint LOAD_AUTOCOMMIT_RECORDS = 10000;
#endif

static const char changjiang_hton_name[] = "CHANGJIANG";
static const char changjiang_plugin_author[] = "LoongSQL Inc.";
static const char *changjiang_extensions[] = {".cts", ".cl1", ".cl2", NullS};

extern StorageHandler *storageHandler;

#define PARAMETER_UINT(_name, _text, _min, _default, _max, _flags, \
                       _update_function)                           \
  uint changjiang_##_name;
#define PARAMETER_BOOL(_name, _text, _default, _flags, _update_function) \
  my_bool changjiang_##_name;
#include "StorageParameters.h"
#undef PARAMETER_UINT
#undef PARAMETER_BOOL

ulonglong changjiang_record_memory_max;
ulonglong changjiang_stream_log_file_size;
uint changjiang_allocation_extent;
ulonglong changjiang_page_cache_size;
char *changjiang_stream_log_dir;
char *changjiang_checkpoint_schedule;
char *changjiang_scavenge_schedule;
char *changjiang_error_inject;
FILE *changjiang_log_file;
// bool        changjiang_sysvar_engine_force;
bool changjiang_file_per_table;

// Determine the largest memory address, assume 64-bits max

static const ulonglong MSB = ULL(1) << ((sizeof(void *) * 8 - 1) & 63);
ulonglong max_memory_address = MSB | (MSB - 1);

// These are the isolation levels we actually use.
// They corespond to enum_tx_isolation from hamdler.h
// 0 = ISO_READ_UNCOMMITTED, 1 = ISO_READ_COMMITTED,
// 2 = ISO_REPEATABLE_READ,  3 = ISO_SERIALIZABLE

int isolation_levels[4] = {
    TRANSACTION_CONSISTENT_READ,  // TRANSACTION_READ_UNCOMMITTED
    TRANSACTION_READ_COMMITTED,
    TRANSACTION_CONSISTENT_READ,   // TRANSACTION_WRITE_COMMITTED, // This is
                                   // repeatable read
    TRANSACTION_CONSISTENT_READ};  // TRANSACTION_SERIALIZABLE

static const ulonglong default_table_flags =
    (/*HA_REC_NOT_IN_SEQ
                                                   |*/
     HA_NULL_IN_KEY | HA_PARTIAL_COLUMN_READ |
     HA_CAN_GEOMETRY
     //| HA_AUTO_PART_KEY
     //| HA_ONLINE_ALTER
     | HA_BINLOG_ROW_CAPABLE
     /*| HA_CAN_READ_ORDER_IF_LIMIT*/);

static struct SHOW_VAR changjiangStatus[] = {
    //{"static",     (char*)"just a static text",     SHOW_CHAR},
    //{"called",     (char*)&number_of_calls, SHOW_LONG},
    {0, 0, SHOW_UNDEF, SHOW_SCOPE_UNDEF}};

// extern THD*		current_thd;
static int getTransactionIsolation(THD *thd);

void openChangjiangLogFile(const char *file) {
  if (changjiang_log_file) fclose(changjiang_log_file);
  changjiang_log_file = fopen(file, "a");
}

void closeChangjiangLogFile() {
  if (changjiang_log_file) {
    fclose(changjiang_log_file);
    changjiang_log_file = NULL;
  }
}

void flushChangjiangLogFile() {
  if (changjiang_log_file) fflush(changjiang_log_file);
}

bool checkExceptionSupport() {
  // Validate that the code has been compiled with support for exceptions
  // by throwing and catching an exception. If the executable does not
  // support exceptions we will reach the return false statement
  try {
    throw 1;
  } catch (int) {
    return true;
  }
  return false;
}

// Init/term routines for THR_LOCK, used within StorageTableShare.
void changjiang_lock_init(void *lock) { thr_lock_init((THR_LOCK *)lock); }

void changjiang_lock_deinit(void *lock) { thr_lock_delete((THR_LOCK *)lock); }

// dummy file to avoid sdi file such t_373.sdi
static bool dict_sdi_create(dd::Tablespace *tablespace) { return false; }
static bool dict_sdi_drop(dd::Tablespace *tablespace) { return false; }
static bool dict_sdi_get_keys(const dd::Tablespace &tablespace,
                              sdi_vector_t &vector) {
  return false;
}
static bool dict_sdi_get(const dd::Tablespace &tablespace,
                         const sdi_key_t *sdi_key, void *sdi, uint64 *sdi_len) {
  return false;
}
static bool dict_sdi_set(handlerton *hton, const dd::Tablespace &tablespace,
                         const dd::Table *table, const sdi_key_t *sdi_key,
                         const void *sdi, uint64 sdi_len) {
  return false;
}
static bool dict_sdi_delete(const dd::Tablespace &tablespace,
                            const dd::Table *table, const sdi_key_t *sdi_key) {
  return false;
}

int StorageInterface::changjiang_init(void *p) {
  DBUG_ENTER("changjiang_init");
  changjiang_hton = (handlerton *)p;

  ERROR_INJECTOR_PARSE(changjiang_error_inject);

  my_bool error = false;

  if (!checkExceptionSupport()) {
    sql_print_error(
        "Changjiang must be compiled with C++ exceptions enabled to work. "
        "Please adjust your compile flags.");
    FATAL("Changjiang exiting process.\n");
  }

  StorageHandler::setDataDirectory(mysql_real_data_home);

  storageHandler = getChangjiangStorageHandler(sizeof(THR_LOCK));

  changjiang_hton->state = SHOW_OPTION_YES;
  changjiang_hton->db_type = DB_TYPE_CHANGJIANG;
  changjiang_hton->savepoint_offset = sizeof(void *);
  changjiang_hton->close_connection = StorageInterface::closeConnection;
  changjiang_hton->savepoint_set = StorageInterface::savepointSet;
  changjiang_hton->savepoint_rollback = StorageInterface::savepointRollback;
  changjiang_hton->savepoint_release = StorageInterface::savepointRelease;
  changjiang_hton->commit = StorageInterface::commit;
  changjiang_hton->rollback = StorageInterface::rollback;
  changjiang_hton->create = changjiang_create_handler;
  changjiang_hton->drop_database = StorageInterface::dropDatabase;
  changjiang_hton->panic = StorageInterface::panic;
  changjiang_hton->file_extensions = changjiang_extensions;

  // avoid sdi files
  changjiang_hton->sdi_create = dict_sdi_create;
  changjiang_hton->sdi_drop = dict_sdi_drop;
  changjiang_hton->sdi_get_keys = dict_sdi_get_keys;
  changjiang_hton->sdi_get = dict_sdi_get;
  changjiang_hton->sdi_set = dict_sdi_set;
  changjiang_hton->sdi_delete = dict_sdi_delete;

#if 0
	changjiang_hton->alter_table_flags  = StorageInterface::alter_table_flags;
#endif

  if (changjiang_support_xa) {
#if 0  // loongsql:TODO
		changjiang_hton->prepare = StorageInterface::prepare;
		changjiang_hton->recover = StorageInterface::recover;
		changjiang_hton->commit_by_xid = StorageInterface::commit_by_xid;
		changjiang_hton->rollback_by_xid = StorageInterface::rollback_by_xid;
#endif
  } else {
    changjiang_hton->prepare = NULL;
    changjiang_hton->recover = NULL;
    changjiang_hton->commit_by_xid = NULL;
    changjiang_hton->rollback_by_xid = NULL;
  }

  changjiang_hton->start_consistent_snapshot =
      StorageInterface::start_consistent_snapshot;

  changjiang_hton->alter_tablespace = StorageInterface::alter_tablespace;
  changjiang_hton->fill_is_table = StorageInterface::fill_is_table;
  // changjiang_hton->show_status  = StorageInterface::show_status;
  changjiang_hton->flags = HTON_NO_FLAGS;
  changjiang_debug_mask &= ~(LogMysqlInfo | LogMysqlWarning | LogMysqlError);
  storageHandler->addNfsLogger(changjiang_debug_mask, StorageInterface::logger,
                               NULL);
  storageHandler->addNfsLogger(LogMysqlInfo | LogMysqlWarning | LogMysqlError,
                               StorageInterface::mysqlLogger, NULL);

  if (changjiang_debug_server) storageHandler->startNfsServer();

  try {
    storageHandler->initialize();
  } catch (SQLException &e) {
    sql_print_error("Changjiang: %s", e.getText());
    error = true;
  } catch (...) {
    sql_print_error("Changjiang: General exception in initialization");
    error = true;
  }

  if (error) {
    // Cleanup after error
    changjiang_deinit(0);
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}

int StorageInterface::changjiang_deinit(void *p) {
  if (storageHandler) {
    storageHandler->deleteNfsLogger(StorageInterface::mysqlLogger, NULL);
    storageHandler->deleteNfsLogger(StorageInterface::logger, NULL);
    storageHandler->shutdownHandler();
    freeChangjiangStorageHandler();
  }
  return 0;
}

int changjiang_strnxfrm(void *cs, const char *dst, uint dstlen, int nweights,
                        const char *src, uint srclen) {
  CHARSET_INFO *charset = (CHARSET_INFO *)cs;

  return (int)charset->coll->strnxfrm(charset, (uchar *)dst, dstlen, nweights,
                                      (uchar *)src, srclen, 0);
}

int changjiang_strnxfrm_space_pad(void *cs, const char *dst, uint dstlen,
                                  int nweights, const char *src, uint srclen) {
  CHARSET_INFO *charset = (CHARSET_INFO *)cs;
  const int flags =
      (charset->pad_attribute == NO_PAD) ? 0 : MY_STRXFRM_PAD_TO_MAXLEN;

  return (int)charset->coll->strnxfrm(charset, (uchar *)dst, dstlen, nweights,
                                      (uchar *)src, srclen, flags);
}

char changjiang_get_pad_char(void *cs) {
  return (char)((CHARSET_INFO *)cs)->pad_char;
}

int changjiang_cs_is_binary(void *cs) {
  return (0 == strcmp(((CHARSET_INFO *)cs)->csname, "binary"));
  //	return ((((CHARSET_INFO*) cs)->state & MY_CS_BINSORT) == MY_CS_BINSORT);
}

unsigned int changjiang_get_mbmaxlen(void *cs) {
  return ((CHARSET_INFO *)cs)->mbmaxlen;
}

char changjiang_get_min_sort_char(void *cs) {
  return (char)((CHARSET_INFO *)cs)->min_sort_char;
}

// Return the actual number of characters in the string
// Note, this is not the number of characters with collatable weight.

uint changjiang_strnchrlen(void *cs, const char *s, uint l) {
  CHARSET_INFO *charset = (CHARSET_INFO *)cs;

  if (charset->mbmaxlen == 1) return l;

  uint chrCount = 0;
  uchar *ch = (uchar *)s;
  uchar *end = ch + l;

  while (ch < end) {
    int len = charset->cset->mbcharlen(charset, *ch);
    if (len == 0) break;  // invalid character.

    ch += len;
    chrCount++;
  }

  return chrCount;
}

// Determine how many bytes are required to store the output of
// cs->coll->strnxfrm() cs is how the source string is formatted. srcLen is the
// number of bytes in the source string. partialKey is the max key buffer size
// if not zero. bufSize is the ultimate maximum destSize. If the string is
// multibyte, strnxfrmlen expects srcLen to be the maximum number of characters
// this can be.  Changjiang wants to send a number that represents the actual
// number of characters in the string so that the call to cs->coll->strnxfrm()
// will not pad.

uint changjiang_strnxfrmlen(void *cs, const char *s, uint srcLen,
                            int partialKey, int bufSize) {
  CHARSET_INFO *charset = (CHARSET_INFO *)cs;
  uint chrLen = changjiang_strnchrlen(cs, s, srcLen);
  int maxChrLen =
      partialKey ? std::min(chrLen, partialKey / charset->mbmaxlen) : chrLen;

  return (uint)std::min(
      (uint)charset->coll->strnxfrmlen(charset, maxChrLen * charset->mbmaxlen),
      (uint)bufSize);
}

// Return the number of bytes used in s to hold a certain number of characters.
// This partialKey is a byte length with charset->mbmaxlen figured in.
// In other words, it is the number of characters times mbmaxlen.

uint changjiang_strntrunc(void *cs, int partialKey, const char *s, uint l) {
  CHARSET_INFO *charset = (CHARSET_INFO *)cs;

  if ((charset->mbmaxlen == 1) || (partialKey == 0))
    return std::min((uint)partialKey, l);

  int charLimit = partialKey / charset->mbmaxlen;
  uchar *ch = (uchar *)s;
  uchar *end = ch + l;

  while ((ch < end) && charLimit) {
    int len = charset->cset->mbcharlen(charset, *ch);
    if (len == 0) break;  // invalid character.

    ch += len;
    charLimit--;
  }

  return (uint)(ch - (uchar *)s);
}

int changjiang_strnncoll(void *cs, const char *s1, uint l1, const char *s2,
                         uint l2, char flag) {
  CHARSET_INFO *charset = (CHARSET_INFO *)cs;

  return charset->coll->strnncoll(charset, (uchar *)s1, l1, (uchar *)s2, l2,
                                  flag);
}

int changjiang_strnncollsp(void *cs, const char *s1, uint l1, const char *s2,
                           uint l2, char flag) {
  CHARSET_INFO *charset = (CHARSET_INFO *)cs;

  if (charset->pad_attribute == NO_PAD) {
    /* MySQL specifies that CHAR fields are stripped of
  trailing spaces before being returned from the database.
  Normally this is done in Field_string::val_str(),
  but since we don't involve the Field classes for internal
  index comparisons, we need to do the same thing here
  for NO PAD collations. (If not, strnncollsp will ignore
  the spaces for us, so we don't need to do it here.) */
    l1 = charset->cset->lengthsp(charset, (const char *)s1, l1);
    l2 = charset->cset->lengthsp(charset, (const char *)s2, l2);
  }

  return charset->coll->strnncollsp(charset, (uchar *)s1, l1, (uchar *)s2, l2);
}

int (*strnncoll)(struct CHARSET_INFO *, const uchar *, uint, const uchar *,
                 uint, my_bool);

/***
#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[]=__FILE__;
#endif
***/

#ifndef DIG_PER_DEC1
typedef decimal_digit_t dec1;
typedef longlong dec2;

#define DIG_PER_DEC1 9
#define DIG_MASK 100000000
#define DIG_BASE 1000000000
#define DIG_MAX (DIG_BASE - 1)
#define DIG_BASE2 ((dec2)DIG_BASE * (dec2)DIG_BASE)
#define ROUND_UP(X) (((X) + DIG_PER_DEC1 - 1) / DIG_PER_DEC1)
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

StorageInterface::StorageInterface(handlerton *hton, TABLE_SHARE *table_arg)
    : handler(hton, table_arg) {
  ref_length = sizeof(lastRecord);
  stats.records = 1000;
  stats.data_file_length = 10000;
  stats.index_file_length = 0;
  tableLocked = false;
  lockForUpdate = false;
  storageTable = NULL;
  storageConnection = NULL;
  storageShare = NULL;
  share = table_arg;
  lastRecord = -1;
  mySqlThread = NULL;
  activeBlobs = NULL;
  freeBlobs = NULL;
  errorText = NULL;
  fieldMap = NULL;
  indexOrder = false;

  if (table_arg) {
    recordLength = table_arg->reclength;
    tempTable = false;
  }

  tableFlags = default_table_flags;
}

StorageInterface::~StorageInterface(void) {
  if (storageTable) storageTable->clearCurrentIndex();

  if (activeBlobs) freeActiveBlobs();

  for (StorageBlob *blob; (blob = freeBlobs);) {
    freeBlobs = blob->next;
    delete blob;
  }

  if (storageTable) {
    storageTable->deleteStorageTable();
    storageTable = NULL;
  }

  if (storageConnection) {
    storageConnection->release();
    storageConnection = NULL;
  }

  unmapFields();
}

int StorageInterface::rnd_init(bool scan) {
  DBUG_ENTER("StorageInterface::rnd_init");
  nextRecord = 0;
  lastRecord = -1;

  DBUG_RETURN(0);
}

int StorageInterface::open(const char *name, int mode, uint test_if_locked,
                           const dd::Table *table_def) {
  DBUG_ENTER("StorageInterface::open");

  // Temporarily comment out DTrace probes in Changjiang, see bug #36403
  // CHANGJIANG_OPEN();

  if (!mySqlThread) mySqlThread = current_thd;

  if (!storageTable) {
    storageShare = storageHandler->findTable(name);
    storageConnection = storageHandler->getStorageConnection(
        storageShare, mySqlThread, mySqlThread->thread_id(), OpenDatabase);

    if (!storageConnection) DBUG_RETURN(HA_ERR_NO_CONNECTION);

    *((StorageConnection **)thd_ha_data(mySqlThread, changjiang_hton)) =
        storageConnection;
    storageConnection->addRef();
    storageTable = storageConnection->getStorageTable(storageShare);

    if (!storageShare->initialized) {
      storageShare->lock(true);

      if (!storageShare->initialized) {
        storageShare->setTablePath(name, tempTable);
        storageShare->initialized = true;
      }

      // Register any collations used

      uint fieldCount = (table) ? table->s->fields : 0;

      for (uint n = 0; n < fieldCount; ++n) {
        Field *field = table->field[n];
        const CHARSET_INFO *charset = field->charset();

        if (charset)
          storageShare->registerCollation(charset->csname, (void *)charset);
      }

      storageShare->unlock();
    }
  }

  int ret = storageTable->open();

  if (ret == StorageErrorTableNotFound)
    sql_print_error(
        "Server is attempting to access a table %s,\n"
        "which doesn't exist in Changjiang.",
        name);

  if (ret) DBUG_RETURN(error(ret));

  thr_lock_data_init((THR_LOCK *)storageShare->impure, &lockData, NULL);

  // Map fields for Changjiang record encoding

  mapFields(table);

  // Map server indexes to Changjiang internal indexes

  setIndexes(table);

  DBUG_RETURN(error(ret));
}

StorageConnection *StorageInterface::getStorageConnection(THD *thd) {
  return *(StorageConnection **)thd_ha_data(thd, changjiang_hton);
}

int StorageInterface::close(void) {
  DBUG_ENTER("StorageInterface::close");

  if (storageTable) storageTable->clearCurrentIndex();

  unmapFields();

  // Temporarily comment out DTrace probes in Changjiang, see bug #36403
  // CHANGJIANG_CLOSE();

  DBUG_RETURN(0);
}

int StorageInterface::check(THD *thd, HA_CHECK_OPT *check_opt) {
#ifdef VALIDATE
  DBUG_ENTER("StorageInterface::check");

  if (storageConnection) storageConnection->validate(0);

  DBUG_RETURN(0);
#else
  return HA_ADMIN_NOT_IMPLEMENTED;
#endif
}

int StorageInterface::repair(THD *thd, HA_CHECK_OPT *check_opt) {
#ifdef VALIDATE
  DBUG_ENTER("StorageInterface::repair");

  if (storageConnection) storageConnection->validate(VALIDATE_REPAIR);

  DBUG_RETURN(0);
#else
  return HA_ADMIN_NOT_IMPLEMENTED;
#endif
}

int StorageInterface::rnd_next(uchar *buf) {
  DBUG_ENTER("StorageInterface::rnd_next");
  ha_statistic_increment(&System_status_var::ha_read_rnd_next_count);

  if (activeBlobs) freeActiveBlobs();

  lastRecord = storageTable->next(nextRecord, lockForUpdate);

  if (lastRecord < 0) {
    if (lastRecord == StorageErrorRecordNotFound) {
      lastRecord = -1;
      table->set_no_row();
      DBUG_RETURN(HA_ERR_END_OF_FILE);
    }

    DBUG_RETURN(error(lastRecord));
  } else
    table->set_row_status_from_handler(0);

  decodeRecord(buf);
  nextRecord = lastRecord + 1;

  DBUG_RETURN(0);
}

void StorageInterface::unlock_row(void) { storageTable->unlockRow(); }

int StorageInterface::rnd_pos(uchar *buf, uchar *pos) {
  int recordNumber;
  DBUG_ENTER("StorageInterface::rnd_pos");
  ha_statistic_increment(&System_status_var::ha_read_rnd_next_count);

  memcpy(&recordNumber, pos, sizeof(recordNumber));

  if (activeBlobs) freeActiveBlobs();

  int ret = storageTable->fetch(recordNumber, lockForUpdate);

  if (ret) {
    table->set_no_row();
    DBUG_RETURN(error(ret));
  }

  lastRecord = recordNumber;
  decodeRecord(buf);
  table->set_row_status_from_handler(0);

  DBUG_RETURN(0);
}

void StorageInterface::position(const uchar *record) {
  DBUG_ENTER("StorageInterface::position");
  memcpy(ref, &lastRecord, sizeof(lastRecord));
  DBUG_VOID_RETURN;
}

int StorageInterface::info(uint what) {
  DBUG_ENTER("StorageInterface::info");

#ifndef NO_OPTIMIZE
  if (what & HA_STATUS_VARIABLE) getDemographics();
#endif

  if (what & HA_STATUS_AUTO)
    // Return the next number to use.  Changjiang stores the last number used.
    stats.auto_increment_value = storageShare->getSequenceValue(0) + 1;

  if (what & HA_STATUS_ERRKEY) errkey = indexErrorId;

  DBUG_RETURN(0);
}

void StorageInterface::getDemographics(void) {
  DBUG_ENTER("StorageInterface::getDemographics");

  stats.records = (ha_rows)storageShare->estimateCardinality();
  // Temporary fix for Bug#28686. (HK) 2007-05-26.
  if (!stats.records) stats.records = 2;

  stats.block_size = 4096;

  storageShare->lockIndexes();

  for (uint n = 0; n < table->s->keys; ++n) {
    KEY *key = table->s->key_info + n;
    StorageIndexDesc *indexDesc = storageShare->getIndex(n);

    if (indexDesc) {
      ha_rows rows = 1 << indexDesc->numberSegments;

      for (uint segment = 0;
           segment < (uint)indexDesc->numberSegments /*key->key_parts*/;
           ++segment, rows >>= 1) {
        ha_rows recordsPerSegment =
            (ha_rows)indexDesc->segmentRecordCounts[segment];
        key->rec_per_key[segment] = (ulong)MAX(recordsPerSegment, rows);
      }
    }
  }

  storageShare->unlockIndexes();

  DBUG_VOID_RETURN;
}

int StorageInterface::optimize(THD *thd, HA_CHECK_OPT *check_opt) {
  DBUG_ENTER("StorageInterface::optimize");

  int ret = storageTable->optimize();

  if (ret) DBUG_RETURN(error(ret));

  DBUG_RETURN(0);
}

const char *StorageInterface::table_type(void) const {
  DBUG_ENTER("StorageInterface::table_type");
  DBUG_RETURN(changjiang_hton_name);
}

void StorageInterface::update_create_info(HA_CREATE_INFO *create_info) {
  DBUG_ENTER("StorageInterface::update_create_info");
  if (!(create_info->used_fields & HA_CREATE_USED_AUTO)) {
    StorageInterface::info(HA_STATUS_AUTO);
    create_info->auto_increment_value = stats.auto_increment_value;
  }
  DBUG_VOID_RETURN;
}

ulonglong StorageInterface::table_flags(void) const {
  DBUG_ENTER("StorageInterface::table_flags");
  DBUG_RETURN(tableFlags);
}

ulong StorageInterface::index_flags(uint idx, uint part, bool all_parts) const {
  DBUG_ENTER("StorageInterface::index_flags");
  ulong flags =
      HA_READ_RANGE | ((indexOrder) ? HA_READ_ORDER : HA_KEY_SCAN_NOT_ROR);

  DBUG_RETURN(flags);
  // DBUG_RETURN(HA_READ_RANGE | HA_KEY_SCAN_NOT_ROR | (indexOrder ?
  // HA_READ_ORDER : 0));
}

int StorageInterface::create(const char *mySqlName, TABLE *form,
                             HA_CREATE_INFO *info, dd::Table *table_def) {
  DBUG_ENTER("StorageInterface::create");
  tempTable = (info->options & HA_LEX_CREATE_TMP_TABLE) ? true : false;
  OpenOption openOption =
      (tempTable) ? OpenTemporaryDatabase : OpenOrCreateDatabase;

  if (storageTable) {
    storageTable->deleteStorageTable();
    storageTable = NULL;
  }

  if (!mySqlThread) mySqlThread = current_thd;

  const char *tableSpaceName = info->tablespace;
  JString strTableSpaceName;
  if (!tableSpaceName && !tempTable && changjiang_file_per_table) {
    char db_name[256] = {0}, tab_name[256] = {0};
    CommonUtil::GetNames(mySqlName, db_name, tab_name, 256);
    strTableSpaceName.append(JString::upcase(db_name));
    strTableSpaceName.append("_");
    strTableSpaceName.append(JString::upcase(tab_name));
    tableSpaceName = strTableSpaceName.getString();

    std::string data_file_name = std::string("./") + db_name +
                                 std::string("/") + tab_name +
                                 std::string(".cts");
    storageHandler->deleteTablespace(tableSpaceName);
    storageHandler->createTablespace(tableSpaceName, data_file_name.c_str(),
                                     nullptr);
    // storageHandler->commit(mySqlThread);
  }

  storageShare =
      storageHandler->createTable(mySqlName, tableSpaceName, tempTable);

  if (!storageShare) DBUG_RETURN(HA_ERR_TABLE_EXIST);

  storageConnection = storageHandler->getStorageConnection(
      storageShare, mySqlThread, mySqlThread->thread_id(), openOption);
  *((StorageConnection **)thd_ha_data(mySqlThread, changjiang_hton)) =
      storageConnection;

  if (!storageConnection) DBUG_RETURN(HA_ERR_NO_CONNECTION);

  storageTable = storageConnection->getStorageTable(storageShare);
  storageTable->setLocalTable(this);

  int ret;
  int64 incrementValue = 0;
  uint n;
  CmdGen gen;
  const char *tableName = storageTable->getName();
  const char *schemaName = storageTable->getSchemaName();
  // gen.gen("create table \"%s\".\"%s\" (\n", schemaName, tableName);
  genTable(form, &gen);

  if (form->found_next_number_field)  // && form->s->next_number_key_offset ==
                                      // 0)
  {
    incrementValue = info->auto_increment_value;

    if (incrementValue == 0) incrementValue = 1;
  }

  /***
  if (form->s->primary_key < form->s->keys)
          {
          KEY *key = form->key_info + form->s->primary_key;
          gen.gen(",\n  primary key ");
          genKeyFields(key, &gen);
          }

  gen.gen (")");
  ***/
  const char *tableSpace = NULL;

  if (tempTable) {
    if (info->tablespace)
      push_warning_printf(mySqlThread, Sql_condition::SL_WARNING,
                          ER_ILLEGAL_HA_CREATE_OPTION,
                          "TABLESPACE option is not supported for temporary "
                          "tables. Switching to '%s' tablespace.",
                          TEMPORARY_TABLESPACE);
    tableSpace = TEMPORARY_TABLESPACE;
  } else if (info->tablespace) {
    if (!strcasecmp(info->tablespace, TEMPORARY_TABLESPACE)) {
      my_printf_error(
          ER_ILLEGAL_HA_CREATE_OPTION,
          "Cannot create non-temporary table '%s' in '%s' tablespace.", MYF(0),
          tableName, TEMPORARY_TABLESPACE);
      storageTable->deleteTable();
      DBUG_RETURN(HA_WRONG_CREATE_OPTION);
    }
    tableSpace = storageTable->getTableSpaceName();
  } else if (changjiang_file_per_table) {
    tableSpace = tableSpaceName;
  } else
    tableSpace = DEFAULT_TABLESPACE;

  if (tableSpace) gen.gen(" tablespace \"%s\"", tableSpace);

  DBUG_PRINT("info",
             ("incrementValue = " I64FORMAT, (long long int)incrementValue));

  if ((ret = storageTable->create(gen.getString(), incrementValue))) {
    storageTable->deleteTable();

    DBUG_RETURN(error(ret));
  }

  for (n = 0; n < form->s->keys; ++n)
    if (n != form->s->primary_key)
      if ((ret = createIndex(schemaName, tableName, form, n))) {
        storageTable->deleteTable();

        DBUG_RETURN(error(ret));
      }

  // Map fields for Changjiang record encoding

  mapFields(form);

  // Map server indexes to Changjiang indexes

  setIndexes(table);

  DBUG_RETURN(0);
}

int StorageInterface::createIndex(const char *schemaName, const char *tableName,
                                  TABLE *srvTable, int indexId) {
  int ret = 0;
  CmdGen gen;
  StorageIndexDesc indexDesc;
  getKeyDesc(srvTable, indexId, &indexDesc);

  if (indexDesc.primaryKey) {
    int64 incrementValue = 0;
    genTable(srvTable, &gen);

    // Primary keys are a special case, so use upgrade()

    ret = storageTable->upgrade(gen.getString(), incrementValue);
  } else {
    KEY *key = srvTable->key_info + indexId;
    const char *unique = (key->flags & HA_NOSAME) ? "unique " : "";
    gen.gen("create %sindex \"%s\" on %s.\"%s\" ", unique, indexDesc.name,
            schemaName, tableName);
    genKeyFields(key, &gen);

    ret = storageTable->createIndex(&indexDesc, gen.getString());
  }

  return ret;
}

int StorageInterface::dropIndex(const char *schemaName, const char *tableName,
                                TABLE *srvTable, int indexId, bool online) {
  StorageIndexDesc indexDesc;
  getKeyDesc(srvTable, indexId, &indexDesc);

  CmdGen gen;
  gen.gen("drop index %s.\"%s\"", schemaName, indexDesc.name);
  const char *sql = gen.getString();

  return storageTable->dropIndex(&indexDesc, sql, online);
}

#if 0
uint StorageInterface::alter_table_flags(uint flags)
{
	if (flags & ALTER_DROP_PARTITION)
		return 0;

	return HA_ONLINE_ADD_INDEX | HA_ONLINE_ADD_UNIQUE_INDEX;
}
#endif

bool StorageInterface::check_if_incompatible_data(HA_CREATE_INFO *create_info,
                                                  uint table_changes) {
  if (true || create_info->auto_increment_value != 0) return COMPATIBLE_DATA_NO;

  return COMPATIBLE_DATA_YES;
}

THR_LOCK_DATA **StorageInterface::store_lock(THD *thd, THR_LOCK_DATA **to,
                                             enum thr_lock_type lock_type) {
  DBUG_ENTER("StorageInterface::store_lock");
  // lockForUpdate = (lock_type == TL_WRITE && thd->lex->sql_command ==
  // SQLCOM_SELECT);
  lockForUpdate = (lock_type == TL_WRITE);

  if (lock_type != TL_IGNORE && lockData.type == TL_UNLOCK) {
    /*
      Here is where we get into the guts of a row level lock.
      MySQL server will serialize write access to tables unless
      we tell it differently.  Changjiang can handle concurrent changes
      for most operations.  But allow the server to set its own
      lock type for certain SQL commands.
    */

    const uint sql_command = thd_sql_command(thd);
    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT && lock_type <= TL_WRITE) &&
        !(thd_in_lock_tables(thd) && sql_command == SQLCOM_LOCK_TABLES) &&
        !(thd_tablespace_op(thd)) && (sql_command != SQLCOM_ALTER_TABLE) &&
        (sql_command != SQLCOM_DROP_TABLE) &&
        (sql_command != SQLCOM_CREATE_INDEX) &&
        (sql_command != SQLCOM_DROP_INDEX) &&
        (sql_command != SQLCOM_TRUNCATE) && (sql_command != SQLCOM_OPTIMIZE) &&
        (sql_command != SQLCOM_CREATE_TABLE))
      lock_type = TL_WRITE_ALLOW_WRITE;

    /*
    In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
    MySQL would use the lock TL_READ_NO_INSERT on t2 to prevent
    concurrent inserts into this table. Since Changjiang can handle
    concurrent changes using its own mechanisms and this type of
    lock conflicts with TL_WRITE_ALLOW_WRITE we convert it to
    a normal read lock to allow concurrent changes.
    */

    if (lock_type == TL_READ_NO_INSERT &&
        !(thd_in_lock_tables(thd) && sql_command == SQLCOM_LOCK_TABLES)) {
      lock_type = TL_READ;
    }

    lockData.type = lock_type;
  }

  *to++ = &lockData;
  DBUG_RETURN(to);
}

int StorageInterface::delete_table(const char *tableName,
                                   const dd::Table *table_def) {
  DBUG_ENTER("StorageInterface::delete_table");

  if (!mySqlThread) mySqlThread = current_thd;

  if (!storageShare)
    if (!(storageShare = storageHandler->preDeleteTable(tableName)))
      DBUG_RETURN(0);

  if (!storageConnection)
    if (!(storageConnection = storageHandler->getStorageConnection(
              storageShare, mySqlThread, mySqlThread->thread_id(),
              OpenDatabase)))
      DBUG_RETURN(0);

  if (!storageTable)
    storageTable = storageConnection->getStorageTable(storageShare);

  if (storageShare) {
    // Lock out other clients before locking the table

    storageShare->lockIndexes(true);
    storageShare->lock(true);

    storageShare->initialized = false;

    storageShare->unlock();
    storageShare->unlockIndexes();
  }

  int res = storageTable->deleteTable();
  storageTable->deleteStorageTable();
  storageTable = NULL;

  if (res == StorageErrorTableNotFound)
    sql_print_error(
        "Server is attempting to drop a table %s,\n"
        "which doesn't exist in Changjiang.",
        tableName);

  // (hk) Fix for Bug#31465 Running Changjiang test suite leads
  //                        to warnings about temp tables
  // This fix could affect other DROP TABLE scenarios.
  // if (res == StorageErrorTableNotFound)

  if (res != StorageErrorUncommittedUpdates) res = 0;

  if (!res && changjiang_file_per_table) {
    JString strTableSpaceName;
    char db_name[256] = {0}, tab_name[256] = {0};
    CommonUtil::GetNames(tableName, db_name, tab_name, 256);
    strTableSpaceName.append(JString::upcase(db_name));
    strTableSpaceName.append("_");
    strTableSpaceName.append(JString::upcase(tab_name));
    const char *tableSpaceName = strTableSpaceName.getString();

    storageHandler->deleteTablespace(tableSpaceName);
  }

  DBUG_RETURN(error(res));
}

#ifdef TRUNCATE_ENABLED
int StorageInterface::truncate(dd::Table *table_def [[maybe_unused]]) {
  return delete_all_rows();
}

int StorageInterface::delete_all_rows() {
  DBUG_ENTER("StorageInterface::delete_all_rows");

  if (!mySqlThread) mySqlThread = current_thd;

  // If this isn't a truncate, punt!

  if (thd_sql_command(mySqlThread) != SQLCOM_TRUNCATE) {
    set_my_errno(HA_ERR_WRONG_COMMAND);
    DBUG_RETURN(HA_ERR_WRONG_COMMAND);
  }

  int ret = 0;
  TABLE_SHARE *tableShare = table_share;
  const char *tableName = tableShare->normalized_path.str;

  if (!storageShare)
    if (!(storageShare = storageHandler->preDeleteTable(tableName)))
      DBUG_RETURN(0);

  if (!storageConnection)
    if (!(storageConnection = storageHandler->getStorageConnection(
              storageShare, mySqlThread, mySqlThread->thread_id(),
              OpenDatabase)))
      DBUG_RETURN(0);

  if (!storageTable)
    storageTable = storageConnection->getStorageTable(storageShare);

  ret = storageTable->truncateTable();

  DBUG_RETURN(ret);
}
#endif

int StorageInterface::analyze(THD *, HA_CHECK_OPT *) {
  // TODO
  return (HA_ADMIN_OK);
}

uint StorageInterface::max_supported_keys(void) const {
  DBUG_ENTER("StorageInterface::max_supported_keys");
  DBUG_RETURN(MAX_KEY);
}

int StorageInterface::write_row(uchar *buff) {
  DBUG_ENTER("StorageInterface::write_row");
  ha_statistic_increment(&System_status_var::ha_write_count);

  /* If we have a timestamp column, update it to the current time */
#if 0  // loongsql:TODO
	if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
		table->timestamp_field->set_time();
#endif
  /*
          If we have an auto_increment column and we are writing a changed row
          or a new row, then update the auto_increment value in the record.
  */

  if (table->next_number_field && buff == table->record[0]) {
    int code = update_auto_increment();
    /*
       May fail, e.g. due to an out of range value in STRICT mode.
    */
    if (code) DBUG_RETURN(code);

    /*
       If the new value is less than the current highest value, it will be
       ignored by setSequenceValue().
    */

    code = storageShare->setSequenceValue(table->next_number_field->val_int());

    if (code) DBUG_RETURN(error(code));
  }

  encodeRecord(buff, false);
  lastRecord = storageTable->insert();

  if (lastRecord < 0) {
    int code = lastRecord >> StoreErrorIndexShift;
    indexErrorId = (lastRecord & StoreErrorIndexMask) - 1;

    DBUG_RETURN(error(code));
  }

  if ((++insertCount % LOAD_AUTOCOMMIT_RECORDS) == 0)
    switch (thd_sql_command(mySqlThread)) {
      case SQLCOM_LOAD:
      case SQLCOM_ALTER_TABLE:
      case SQLCOM_CREATE_TABLE:
      case SQLCOM_CREATE_INDEX:
        storageHandler->commit(mySqlThread);
        storageConnection->startTransaction(
            getTransactionIsolation(mySqlThread));
        storageConnection->markVerb();
        insertCount = 0;
        break;

      default:;
    }

  DBUG_RETURN(0);
}

int StorageInterface::update_row(const uchar *oldData, uchar *newData) {
  DBUG_ENTER("StorageInterface::update_row");
  DBUG_ASSERT(lastRecord >= 0);

  /* If we have a timestamp column, update it to the current time */
#if 0  // loongsql:TODO
	if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
		table->timestamp_field->set_time();
#endif
  /* If we have an auto_increment column, update the sequence value.  */

  Field *autoInc = table->found_next_number_field;

  if ((autoInc) && bitmap_is_set(table->read_set, autoInc->field_index())) {
    int code = storageShare->setSequenceValue(autoInc->val_int());

    if (code) DBUG_RETURN(error(code));
  }

  encodeRecord(newData, true);

  int ret = storageTable->updateRow(lastRecord);

  if (ret) {
    int code = ret >> StoreErrorIndexShift;
    indexErrorId = (ret & StoreErrorIndexMask) - 1;
    DBUG_RETURN(error(code));
  }

  ha_statistic_increment(&System_status_var::ha_update_count);

  DBUG_RETURN(0);
}

int StorageInterface::delete_row(const uchar *buf) {
  DBUG_ENTER("StorageInterface::delete_row");
  DBUG_ASSERT(lastRecord >= 0);
  ha_statistic_increment(&System_status_var::ha_delete_count);

  int ret = storageTable->deleteRow(lastRecord);

  if (ret < 0) DBUG_RETURN(error(ret));

  lastRecord = -1;

  DBUG_RETURN(0);
}

int StorageInterface::commit(handlerton *hton, THD *thd, bool all) {
  DBUG_ENTER("StorageInterface::commit");
  StorageConnection *storageConnection = getStorageConnection(thd);
  int ret = 0;

  if (all || !thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {
    if (storageConnection)
      ret = storageConnection->commit();
    else
      ret = storageHandler->commit(thd);
  } else {
    if (storageConnection)
      storageConnection->releaseVerb();
    else
      ret = storageHandler->releaseVerb(thd);
  }

  if (ret != 0) {
    DBUG_RETURN(getMySqlError(ret));
  }

  DBUG_RETURN(0);
}

int StorageInterface::prepare(handlerton *hton, THD *thd, bool all) {
  DBUG_ENTER("StorageInterface::prepare");

  if (all || !thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
    storageHandler->prepare(
        thd, sizeof(*thd->get_transaction()->xid_state()->get_xid()),
        (const unsigned char *)thd->get_transaction()->xid_state()->get_xid());
  else {
    StorageConnection *storageConnection = getStorageConnection(thd);

    if (storageConnection)
      storageConnection->releaseVerb();
    else
      storageHandler->releaseVerb(thd);
  }

  DBUG_RETURN(0);
}

int StorageInterface::rollback(handlerton *hton, THD *thd, bool all) {
  DBUG_ENTER("StorageInterface::rollback");
  StorageConnection *storageConnection = getStorageConnection(thd);
  int ret = 0;

  if (all || !thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {
    if (storageConnection)
      ret = storageConnection->rollback();
    else
      ret = storageHandler->rollback(thd);
  } else {
    if (storageConnection)
      ret = storageConnection->rollbackVerb();
    else
      ret = storageHandler->rollbackVerb(thd);
  }

  if (ret != 0) {
    DBUG_RETURN(getMySqlError(ret));
  }

  DBUG_RETURN(0);
}

int StorageInterface::commit_by_xid(handlerton *hton, XID *xid) {
  DBUG_ENTER("StorageInterface::commit_by_xid");
  int ret =
      storageHandler->commitByXid(sizeof(XID), (const unsigned char *)xid);
  DBUG_RETURN(ret);
}

int StorageInterface::rollback_by_xid(handlerton *hton, XID *xid) {
  DBUG_ENTER("StorageInterface::rollback_by_xid");
  int ret =
      storageHandler->rollbackByXid(sizeof(XID), (const unsigned char *)xid);
  DBUG_RETURN(ret);
}

int StorageInterface::start_consistent_snapshot(handlerton *hton, THD *thd) {
  DBUG_ENTER("StorageInterface::start_consistent_snapshot");
  int ret = storageHandler->startTransaction(thd, TRANSACTION_CONSISTENT_READ);
  if (!ret) trans_register_ha(thd, true, hton, nullptr);  // loongsql:TODO
  DBUG_RETURN(ret);
}

const COND *StorageInterface::cond_push(const COND *cond) {
#ifdef COND_PUSH_ENABLED
  DBUG_ENTER("StorageInterface::cond_push");
  char buff[256];
  String str(buff, (uint32)sizeof(buff), system_charset_info);
  str.length(0);
  Item *cond_ptr = (COND *)cond;
  cond_ptr->print(&str);
  str.append('\0');
  DBUG_PRINT("StorageInterface::cond_push", ("%s", str.ptr()));
  DBUG_RETURN(0);
#else
  return handler::cond_push(cond);
#endif
}

void StorageInterface::startTransaction(void) {
  threadSwitch(table->in_use);

  int isolation = getTransactionIsolation(mySqlThread);

  if (!storageConnection->transactionActive) {
    storageConnection->startTransaction(isolation);
    trans_register_ha(mySqlThread, true, changjiang_hton,
                      nullptr);  // loongsql:TODO
  }

  switch (thd_tx_isolation(mySqlThread)) {
    case ISO_READ_UNCOMMITTED:
      error(StorageWarningReadUncommitted);
      break;

    case ISO_SERIALIZABLE:
      error(StorageWarningSerializable);
      break;
  }
}

int StorageInterface::savepointSet(handlerton *hton, THD *thd,
                                   void *savePoint) {
  return storageHandler->savepointSet(thd, savePoint);
}

int StorageInterface::savepointRollback(handlerton *hton, THD *thd,
                                        void *savePoint) {
  return storageHandler->savepointRollback(thd, savePoint);
}

int StorageInterface::savepointRelease(handlerton *hton, THD *thd,
                                       void *savePoint) {
  return storageHandler->savepointRelease(thd, savePoint);
}

int StorageInterface::index_read(uchar *buf, const uchar *keyBytes,
                                 uint key_len,
                                 enum ha_rkey_function find_flag) {
  DBUG_ENTER("StorageInterface::index_read");
  int which = 0;
  ha_statistic_increment(&System_status_var::ha_read_key_count);

  int ret = storageTable->checkCurrentIndex();

  if (ret) DBUG_RETURN(error(ret));

  // XXX This needs to be revisited
  switch (find_flag) {
    case HA_READ_KEY_EXACT:
      which = UpperBound | LowerBound;
      break;

    case HA_READ_KEY_OR_NEXT:
      storageTable->setPartialKey();
      which = LowerBound;
      break;

    case HA_READ_AFTER_KEY:  // ???
      if (!storageTable->isKeyNull((const unsigned char *)keyBytes, key_len))
        which = LowerBound;

      break;

    case HA_READ_BEFORE_KEY:           // ???
    case HA_READ_PREFIX_LAST_OR_PREV:  // ???
    case HA_READ_PREFIX_LAST:          // ???
    case HA_READ_PREFIX:
    case HA_READ_KEY_OR_PREV:
    default:
      DBUG_RETURN(HA_ERR_UNSUPPORTED);
  }

  const unsigned char *key = (const unsigned char *)keyBytes;

  if (which)
    if ((ret = storageTable->setIndexBound(key, key_len, which)))
      DBUG_RETURN(error(ret));

  storageTable->clearBitmap();
  if ((ret = storageTable->indexScan(indexOrder))) DBUG_RETURN(error(ret));

  nextRecord = 0;

  for (;;) {
    int ret = index_next(buf);

    if (ret) DBUG_RETURN(ret);

    int comparison = storageTable->compareKey(key, key_len);

    if ((which & LowerBound) && comparison < 0) continue;

    if ((which & UpperBound) && comparison > 0) continue;

    DBUG_RETURN(0);
  }
}

int StorageInterface::index_init(uint idx, bool sorted) {
  DBUG_ENTER("StorageInterface::index_init");
  active_index = idx;
  nextRecord = 0;
  haveStartKey = false;
  haveEndKey = false;

  // Get and hold a shared lock on StorageTableShare::indexes, then set
  // the corresponding Changjiang index for use on the current thread

  int ret = storageTable->setCurrentIndex(idx);

  // If the index is not found, remap the index and try again

  if (ret) {
    storageShare->lockIndexes(true);
    remapIndexes(table);
    storageShare->unlockIndexes();

    ret = storageTable->setCurrentIndex(idx);

    // Online ALTER allows access to partially deleted indexes, so
    // fail silently for now to avoid fatal assert in server.
    //
    // TODO: Restore error when server imposes DDL lock across the
    //       three phases of online ALTER.

    if (ret)
      // DBUG_RETURN(error(ret));
      DBUG_RETURN(error(0));
  }

  DBUG_RETURN(error(ret));
}

int StorageInterface::index_end(void) {
  DBUG_ENTER("StorageInterface::index_end");

  storageTable->indexEnd();

  DBUG_RETURN(0);
}

ha_rows StorageInterface::records_in_range(uint indexId, key_range *lower,
                                           key_range *upper) {
  DBUG_ENTER("StorageInterface::records_in_range");

#ifdef NO_OPTIMIZE
  DBUG_RETURN(handler::records_in_range(indexId, lower, upper));
#endif

  ha_rows cardinality = (ha_rows)storageShare->estimateCardinality();

  if (!lower) DBUG_RETURN(MAX(cardinality, 2));

  StorageIndexDesc indexDesc;

  if (!storageShare->getIndex(indexId, &indexDesc))
    DBUG_RETURN(MAX(cardinality, 2));

  int numberSegments = 0;

  for (int map = lower->keypart_map; map; map >>= 1) ++numberSegments;

  if (indexDesc.unique && numberSegments == indexDesc.numberSegments)
    DBUG_RETURN(1);

  ha_rows segmentRecords =
      (ha_rows)indexDesc.segmentRecordCounts[numberSegments - 1];
  ha_rows guestimate = cardinality;

  if (lower->flag == HA_READ_KEY_EXACT) {
    if (segmentRecords)
      guestimate = segmentRecords;
    else
      for (int n = 0; n < numberSegments; ++n) guestimate /= 10;
  } else
    guestimate /= 3;

  DBUG_RETURN(MAX(guestimate, 2));
}

template <uint dec>
static int64 mysql_packed_to_cj_timef(const char *keyBytes) {
  return my_time_packed_from_binary((const uchar *)keyBytes, dec);
}
template <uint dec>
static int64 mysql_packed_to_cj_datetimef(const char *keyBytes) {
  return my_datetime_packed_from_binary((const uchar *)keyBytes, dec);
}
// Field_timestampf::get_date_internal_at
template <uint dec>
static int64 mysql_packed_to_cj_timestampf(const char *keyBytes) {
  my_timeval tm;
  my_timestamp_from_binary(&tm, (const uchar *)keyBytes, dec);
  // '0000-00-00 00:00:00.000000'
  if (tm.m_tv_sec == 0 && tm.m_tv_usec == 0) return 0;
  MYSQL_TIME time;
  // THD *thd = current_thd;
  // my_tz_UTC?
  // Time_zone *tz = thd->time_zone();
  Time_zone *tz = my_tz_OFFSET0;
  tz->gmt_sec_to_TIME(&time, tm);
  int64 value = TIME_to_longlong_packed(time);

  return value;
}

void StorageInterface::getKeyDesc(TABLE *srvTable, int indexId,
                                  StorageIndexDesc *indexDesc) {
  KEY *keyInfo = srvTable->key_info + indexId;
  int numberKeys = keyInfo->user_defined_key_parts;

  indexDesc->id = indexId;
  indexDesc->numberSegments = numberKeys;
  indexDesc->unique = (keyInfo->flags & HA_NOSAME);
  indexDesc->primaryKey = (srvTable->s->primary_key == (uint)indexId);

  // Clean up the index name for internal use

  strncpy(indexDesc->rawName, (const char *)keyInfo->name,
          MIN(indexNameSize, (int)strlen(keyInfo->name) + 1));
  indexDesc->rawName[indexNameSize - 1] = '\0';
  storageShare->createIndexName(indexDesc->rawName, indexDesc->name,
                                indexDesc->primaryKey);

  for (int n = 0; n < numberKeys; ++n) {
    StorageSegment *segment = indexDesc->segments + n;
    KEY_PART_INFO *part = keyInfo->key_part + n;

    segment->offset = part->offset;
    segment->length = part->length;
    segment->type = part->field->key_type();
    segment->nullBit = part->null_bit;
    segment->mysql_charset = NULL;

    // Separate correctly between types that may map to
    // the same key type, but that should be treated differently.
    // This way StorageInterface::getSegmentValue only have
    // to switch on the keyFormat, and the logic needed at runtime
    // is minimal.
    // Also set the correct charset where appropriate.
    switch (segment->type) {
      case HA_KEYTYPE_LONG_INT:
        segment->keyFormat = KEY_FORMAT_LONG_INT;
        break;

      case HA_KEYTYPE_SHORT_INT:
        segment->keyFormat = KEY_FORMAT_SHORT_INT;
        break;

      case HA_KEYTYPE_ULONGLONG:
        segment->keyFormat = KEY_FORMAT_ULONGLONG;
        break;

      case HA_KEYTYPE_LONGLONG:
        segment->keyFormat = KEY_FORMAT_LONGLONG;
        break;

      case HA_KEYTYPE_FLOAT:
        segment->keyFormat = KEY_FORMAT_FLOAT;
        break;

      case HA_KEYTYPE_DOUBLE:
        segment->keyFormat = KEY_FORMAT_DOUBLE;
        break;

      case HA_KEYTYPE_VARBINARY1:
      case HA_KEYTYPE_VARBINARY2:
        segment->keyFormat = KEY_FORMAT_VARBINARY;
        segment->mysql_charset = (void *)part->field->charset();
        break;

      case HA_KEYTYPE_VARTEXT1:
      case HA_KEYTYPE_VARTEXT2:
        segment->keyFormat = KEY_FORMAT_VARTEXT;
        segment->mysql_charset = (void *)part->field->charset();
        break;

      case HA_KEYTYPE_BINARY:
        switch (part->field->real_type()) {
          case MYSQL_TYPE_TINY:
          case MYSQL_TYPE_BIT:
          case MYSQL_TYPE_YEAR:
          case MYSQL_TYPE_SET:
          case MYSQL_TYPE_ENUM:
          case MYSQL_TYPE_DATETIME:
            segment->keyFormat = KEY_FORMAT_BINARY_INTEGER;
            break;

          case MYSQL_TYPE_NEWDECIMAL:
            segment->keyFormat = KEY_FORMAT_BINARY_NEWDECIMAL;
            break;

          case MYSQL_TYPE_TIME2:
            if (((Field_timef *)part->field)->decimals() == 0)
              segment->type_converter = &mysql_packed_to_cj_timef<0>;
            else if (((Field_timef *)part->field)->decimals() == 1)
              segment->type_converter = &mysql_packed_to_cj_timef<1>;
            else if (((Field_timef *)part->field)->decimals() == 2)
              segment->type_converter = &mysql_packed_to_cj_timef<2>;
            else if (((Field_timef *)part->field)->decimals() == 3)
              segment->type_converter = &mysql_packed_to_cj_timef<3>;
            else if (((Field_timef *)part->field)->decimals() == 4)
              segment->type_converter = &mysql_packed_to_cj_timef<4>;
            else if (((Field_timef *)part->field)->decimals() == 5)
              segment->type_converter = &mysql_packed_to_cj_timef<5>;
            else if (((Field_timef *)part->field)->decimals() == 6)
              segment->type_converter = &mysql_packed_to_cj_timef<6>;
            else
              ASSERT(false);
            segment->keyFormat = KEY_FORMAT_LONGLONG;
            break;
          case MYSQL_TYPE_DATETIME2:
            if (((Field_datetimef *)part->field)->decimals() == 0)
              segment->type_converter = &mysql_packed_to_cj_datetimef<0>;
            else if (((Field_datetimef *)part->field)->decimals() == 1)
              segment->type_converter = &mysql_packed_to_cj_datetimef<1>;
            else if (((Field_datetimef *)part->field)->decimals() == 2)
              segment->type_converter = &mysql_packed_to_cj_datetimef<2>;
            else if (((Field_datetimef *)part->field)->decimals() == 3)
              segment->type_converter = &mysql_packed_to_cj_datetimef<3>;
            else if (((Field_datetimef *)part->field)->decimals() == 4)
              segment->type_converter = &mysql_packed_to_cj_datetimef<4>;
            else if (((Field_datetimef *)part->field)->decimals() == 5)
              segment->type_converter = &mysql_packed_to_cj_datetimef<5>;
            else if (((Field_datetimef *)part->field)->decimals() == 6)
              segment->type_converter = &mysql_packed_to_cj_datetimef<6>;
            else
              ASSERT(false);
            segment->keyFormat = KEY_FORMAT_LONGLONG;
            break;
          case MYSQL_TYPE_TIMESTAMP2:
            if (((Field_timestampf *)part->field)->decimals() == 0)
              segment->type_converter = &mysql_packed_to_cj_timestampf<0>;
            else if (((Field_timestampf *)part->field)->decimals() == 1)
              segment->type_converter = &mysql_packed_to_cj_timestampf<1>;
            else if (((Field_timestampf *)part->field)->decimals() == 2)
              segment->type_converter = &mysql_packed_to_cj_timestampf<2>;
            else if (((Field_timestampf *)part->field)->decimals() == 3)
              segment->type_converter = &mysql_packed_to_cj_timestampf<3>;
            else if (((Field_timestampf *)part->field)->decimals() == 4)
              segment->type_converter = &mysql_packed_to_cj_timestampf<4>;
            else if (((Field_timestampf *)part->field)->decimals() == 5)
              segment->type_converter = &mysql_packed_to_cj_timestampf<5>;
            else if (((Field_timestampf *)part->field)->decimals() == 6)
              segment->type_converter = &mysql_packed_to_cj_timestampf<6>;
            else
              ASSERT(false);
            segment->keyFormat = KEY_FORMAT_LONGLONG;
            break;
          default:
            segment->keyFormat = KEY_FORMAT_BINARY_STRING;
            break;
        }
        break;

      case HA_KEYTYPE_TEXT:
        segment->keyFormat = KEY_FORMAT_TEXT;
        segment->mysql_charset = (void *)part->field->charset();
        break;

      case HA_KEYTYPE_ULONG_INT:
        assert(part->field->real_type() != MYSQL_TYPE_TIMESTAMP2);
        if (part->field->real_type() == MYSQL_TYPE_TIMESTAMP)
          segment->keyFormat = KEY_FORMAT_TIMESTAMP;
        else
          segment->keyFormat = KEY_FORMAT_ULONG_INT;
        break;

      case HA_KEYTYPE_INT8:
        segment->keyFormat = KEY_FORMAT_INT8;
        break;

      case HA_KEYTYPE_USHORT_INT:
        segment->keyFormat = KEY_FORMAT_USHORT_INT;
        break;

      case HA_KEYTYPE_UINT24:
        segment->keyFormat = KEY_FORMAT_UINT24;
        break;

      case HA_KEYTYPE_INT24:
        segment->keyFormat = KEY_FORMAT_INT24;
        break;

      default:
        segment->keyFormat = KEY_FORMAT_OTHER;
    }
  }
}

int StorageInterface::rename_table(const char *from, const char *to,
                                   const dd::Table *from_table_def,
                                   dd::Table *to_table_def) {
  DBUG_ENTER("StorageInterface::rename_table");
  // tempTable = storageHandler->isTempTable(from) > 0;
  table = 0;  // XXX hack?
  int ret = open(from, 0, 0, nullptr);

  if (ret) DBUG_RETURN(error(ret));

  storageTable->clearCurrentIndex();
  storageShare->lockIndexes(true);
  storageShare->lock(true);

  ret = storageShare->renameTable(storageConnection, to);

  remapIndexes(table);

  storageShare->unlock();
  storageShare->unlockIndexes();

  DBUG_RETURN(error(ret));
}

double StorageInterface::read_time(uint index, uint ranges, ha_rows rows) {
  DBUG_ENTER("StorageInterface::read_time");
  DBUG_RETURN(rows2double(rows / 3));
}

int StorageInterface::read_range_first(const key_range *start_key,
                                       const key_range *end_key,
                                       bool eq_range_arg, bool sorted) {
  int res;
  DBUG_ENTER("StorageInterface::read_range_first");

  storageTable->clearIndexBounds();
  if ((res = scanRange(start_key, end_key, eq_range_arg))) DBUG_RETURN(res);

  int result = index_next(table->record[0]);

  if (result) {
    if (result == HA_ERR_KEY_NOT_FOUND) result = HA_ERR_END_OF_FILE;

    table->set_no_row();
    DBUG_RETURN(result);
  }

  DBUG_RETURN(0);
}

int StorageInterface::scanRange(const key_range *start_key,
                                const key_range *end_key, bool eqRange) {
  DBUG_ENTER("StorageInterface::read_range_first");
  haveStartKey = false;
  haveEndKey = false;
  storageTable->upperBound = storageTable->lowerBound = NULL;

  int ret = storageTable->checkCurrentIndex();

  if (ret) DBUG_RETURN(error(ret));

  if (start_key &&
      !storageTable->isKeyNull((const unsigned char *)start_key->key,
                               start_key->length)) {
    haveStartKey = true;
    startKey = *start_key;

    if (start_key->flag == HA_READ_KEY_OR_NEXT)
      storageTable->setPartialKey();
    else if (start_key->flag == HA_READ_AFTER_KEY)
      storageTable->setReadAfterKey();

    ret = storageTable->setIndexBound((const unsigned char *)start_key->key,
                                      start_key->length, LowerBound);
    if (ret) DBUG_RETURN(error(ret));
  }

  if (end_key) {
    if (end_key->flag == HA_READ_BEFORE_KEY) storageTable->setReadBeforeKey();

    ret = storageTable->setIndexBound((const unsigned char *)end_key->key,
                                      end_key->length, UpperBound);
    if (ret) DBUG_RETURN(error(ret));
  }

  storageTable->indexScan(indexOrder);
  nextRecord = 0;
  lastRecord = -1;
  eq_range = eqRange;
  end_range = 0;

  if (end_key) {
    haveEndKey = true;
    endKey = *end_key;
    end_range = &save_end_range;
    save_end_range = *end_key;
    key_compare_result_on_equal = ((end_key->flag == HA_READ_BEFORE_KEY)  ? 1
                                   : (end_key->flag == HA_READ_AFTER_KEY) ? -1
                                                                          : 0);
  }

  range_key_part = table->key_info[active_index].key_part;
  DBUG_RETURN(0);
}

int StorageInterface::index_first(uchar *buf) {
  storageTable->indexScan(indexOrder);

  return index_next(buf);
}

int StorageInterface::index_next(uchar *buf) {
  DBUG_ENTER("StorageInterface::index_next");
  ha_statistic_increment(&System_status_var::ha_read_next_count);

  if (activeBlobs) freeActiveBlobs();

  int ret = storageTable->checkCurrentIndex();

  if (ret) DBUG_RETURN(error(ret));

  for (;;) {
    lastRecord = storageTable->nextIndexed(nextRecord, lockForUpdate);

    if (lastRecord < 0) {
      if (lastRecord == StorageErrorRecordNotFound) {
        lastRecord = -1;
        table->set_no_row();
        DBUG_RETURN(HA_ERR_END_OF_FILE);
      }

      DBUG_RETURN(error(lastRecord));
    }

    nextRecord = lastRecord + 1;

    if (haveStartKey) {
      int n = storageTable->compareKey((const unsigned char *)startKey.key,
                                       startKey.length);

      if (n < 0 || (n == 0 && startKey.flag == HA_READ_AFTER_KEY)) {
        storageTable->unlockRow();
        continue;
      }
    }

    if (haveEndKey) {
      int n = storageTable->compareKey((const unsigned char *)endKey.key,
                                       endKey.length);

      if (n > 0 || (n == 0 && endKey.flag == HA_READ_BEFORE_KEY)) {
        storageTable->unlockRow();
        continue;
      }
    }

    decodeRecord(buf);
    table->set_row_status_from_handler(0);

    DBUG_RETURN(0);
  }
}

int StorageInterface::index_next_same(uchar *buf, const uchar *key,
                                      uint key_len) {
  DBUG_ENTER("StorageInterface::index_next_same");
  ha_statistic_increment(&System_status_var::ha_read_next_count);

  for (;;) {
    int ret = index_next(buf);

    if (ret) DBUG_RETURN(ret);

    int comparison =
        storageTable->compareKey((const unsigned char *)key, key_len);

    if (comparison == 0) DBUG_RETURN(0);
  }
}

//*****************************************************************************
// Changjiang MRR implementation: One-sweep DS-MRR
//
// Overview
//  - MRR scan is always done in one pass, which consists of two steps:
//      1. Scan the index and populate Changjiang's internal record number
//      bitmap
//      2. Scan the bitmap, retrieve and return records
//    (without MRR changjiang does steps 1 and 2 for each range)
//  - The record number bitmap is allocated using Changjiang's internal
//    allocation routines, so we're not using the SQL layer's join buffer space.
//  - multi_range_read_next() may return "garbage" records - records that are
//    outside of any of the scanned ranges. Filtering out these records is
//    the responsibility of whoever is making MRR calls.
//
//*****************************************************************************

int StorageInterface::multi_range_read_init(RANGE_SEQ_IF *seq,
                                            void *seq_init_param, uint n_ranges,
                                            uint mode, HANDLER_BUFFER *buf) {
  DBUG_ENTER("StorageInterface::multi_range_read_init");
  if (mode & HA_MRR_USE_DEFAULT_IMPL) {
    useDefaultMrrImpl = true;
    int res = handler::multi_range_read_init(seq, seq_init_param, n_ranges,
                                             mode, buf);
    DBUG_RETURN(res);
  }
  useDefaultMrrImpl = false;
  multi_range_buffer = buf;
  mrr_funcs = *seq;

  mrr_iter = mrr_funcs.init(seq_init_param, n_ranges, mode);
  fillMrrBitmap();

  // multi_range_read_next() will be calling index_next(). The following is
  // to make index_next() not to check whether the retrieved record is in
  // range
  haveStartKey = haveEndKey = 0;
  DBUG_RETURN(0);
}

int StorageInterface::fillMrrBitmap() {
  int res;
  key_range *startKey;
  key_range *endKey;
  DBUG_ENTER("StorageInterface::fillMrrBitmap");

  storageTable->clearBitmap();
  while (!(res = mrr_funcs.next(mrr_iter, &mrr_cur_range))) {
    startKey =
        mrr_cur_range.start_key.keypart_map ? &mrr_cur_range.start_key : NULL;
    endKey = mrr_cur_range.end_key.keypart_map ? &mrr_cur_range.end_key : NULL;
    if ((res = scanRange(startKey, endKey,
                         test(mrr_cur_range.range_flag & EQ_RANGE))))
      DBUG_RETURN(res);
  }
  DBUG_RETURN(0);
}

int StorageInterface::multi_range_read_next(char **rangeInfo) {
  if (useDefaultMrrImpl) return handler::multi_range_read_next(rangeInfo);
  return index_next(table->record[0]);
}

ha_rows StorageInterface::multi_range_read_info_const(
    uint keyno, RANGE_SEQ_IF *seq, void *seq_init_param, uint n_ranges,
    uint *bufsz, uint *flags, Cost_estimate *cost) {
  // Reference : DsMrr_impl::choose_mrr_impl
  THD *thd = current_thd;
  TABLE_LIST *tl = table->pos_in_table_list;
  const bool mrr_on =
      hint_key_state(thd, tl, keyno, MRR_HINT_ENUM, OPTIMIZER_SWITCH_MRR);
  const bool force_dsmrr_by_hints =
      hint_key_state(thd, tl, keyno, MRR_HINT_ENUM, 0) ||
      hint_table_state(thd, tl, BKA_HINT_ENUM, 0);

  ha_rows res;
  // TODO: SergeyP: move the optimizer_use_mrr check from here out to the
  // SQL layer, do same for all other MRR implementations
  bool native_requested = test(*flags & HA_MRR_USE_DEFAULT_IMPL ||
                               !(mrr_on || force_dsmrr_by_hints));
  res = handler::multi_range_read_info_const(keyno, seq, seq_init_param,
                                             n_ranges, bufsz, flags, cost);
  if ((res != HA_POS_ERROR) && !native_requested) {
    *flags &= ~(HA_MRR_USE_DEFAULT_IMPL | HA_MRR_SORTED);
    /* We'll be returning records without telling which range they are contained
     * in */
    *flags |= HA_MRR_NO_ASSOCIATION;
    /* We'll use our own internal buffer so we won't need any buffer space from
     * the SQL layer */
    *bufsz = 0;
  }
  return res;
}

ha_rows StorageInterface::multi_range_read_info(uint keyno, uint n_ranges,
                                                uint keys, uint *bufsz,
                                                uint *flags,
                                                Cost_estimate *cost) {
  THD *thd = current_thd;
  TABLE_LIST *tl = table->pos_in_table_list;
  const bool mrr_on =
      hint_key_state(thd, tl, keyno, MRR_HINT_ENUM, OPTIMIZER_SWITCH_MRR);
  const bool force_dsmrr_by_hints =
      hint_key_state(thd, tl, keyno, MRR_HINT_ENUM, 0) ||
      hint_table_state(thd, tl, BKA_HINT_ENUM, 0);

  ha_rows res;
  bool native_requested = test(*flags & HA_MRR_USE_DEFAULT_IMPL ||
                               !(mrr_on || force_dsmrr_by_hints));
  res =
      handler::multi_range_read_info(keyno, n_ranges, keys, bufsz, flags, cost);
  if ((res != HA_POS_ERROR) && !native_requested) {
    *flags &= ~(HA_MRR_USE_DEFAULT_IMPL | HA_MRR_SORTED);
    /* See _info_const() function for explanation of these: */
    *flags |= HA_MRR_NO_ASSOCIATION;
    *bufsz = 0;
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////////

double StorageInterface::scan_time(void) {
  DBUG_ENTER("StorageInterface::scan_time");
  DBUG_RETURN((double)stats.records * 1000);
}

bool StorageInterface::threadSwitch(THD *newThread) {
  if (newThread == mySqlThread) return false;

  if (storageConnection) {
    if (!storageConnection->mySqlThread && !mySqlThread) {
      storageConnection->setMySqlThread(newThread);
      mySqlThread = newThread;

      return false;
    }

    storageConnection->release();
  }

  storageConnection = storageHandler->getStorageConnection(
      storageShare, newThread, newThread->thread_id(), OpenDatabase);

  if (storageTable)
    storageTable->setConnection(storageConnection);
  else
    storageTable = storageConnection->getStorageTable(storageShare);

  mySqlThread = newThread;

  return true;
}

int StorageInterface::threadSwitchError(void) { return 1; }

int StorageInterface::error(int storageError) {
  DBUG_ENTER("StorageInterface::error");

  if (storageError == 0) {
    DBUG_PRINT("info", ("returning 0"));
    DBUG_RETURN(0);
  }

  int mySqlError = getMySqlError(storageError);

  switch (storageError) {
    case StorageErrorNoSequence:
      if (storageConnection)
        storageConnection->setErrorText(
            "no sequenced defined for autoincrement operation");

      break;

    case StorageWarningSerializable:
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          ER_CANT_CHANGE_TX_CHARACTERISTICS,
                          "Changjiang does not support SERIALIZABLE ISOLATION, "
                          "using REPEATABLE READ instead.");
      break;

    case StorageWarningReadUncommitted:
      push_warning_printf(current_thd, Sql_condition::SL_WARNING,
                          ER_CANT_CHANGE_TX_CHARACTERISTICS,
                          "Changjiang does not support READ UNCOMMITTED "
                          "ISOLATION, using REPEATABLE READ instead.");
      break;

    case StorageErrorIndexOverflow:
      my_error(ER_TOO_LONG_KEY, MYF(0), max_key_length());
      break;

    default:;
  }

  DBUG_RETURN(mySqlError);
}

int StorageInterface::getMySqlError(int storageError) {
  switch (storageError) {
    case 0:
      return 0;

    case StorageErrorDupKey:
      DBUG_PRINT("info", ("StorageErrorDupKey"));
      return (HA_ERR_FOUND_DUPP_KEY);

    case StorageErrorDeadlock:
      DBUG_PRINT("info", ("StorageErrorDeadlock"));
      return (HA_ERR_LOCK_DEADLOCK);

    case StorageErrorLockTimeout:
      DBUG_PRINT("info", ("StorageErrorLockTimeout"));
      return (HA_ERR_LOCK_WAIT_TIMEOUT);

    case StorageErrorRecordNotFound:
      DBUG_PRINT("info", ("StorageErrorRecordNotFound"));
      return (HA_ERR_KEY_NOT_FOUND);

    case StorageErrorTableNotFound:
      DBUG_PRINT("info", ("StorageErrorTableNotFound"));
      return (HA_ERR_NO_SUCH_TABLE);

    case StorageErrorNoIndex:
      DBUG_PRINT("info", ("StorageErrorNoIndex"));
      return (HA_ERR_WRONG_INDEX);

    case StorageErrorBadKey:
      DBUG_PRINT("info", ("StorageErrorBadKey"));
      return (HA_ERR_WRONG_INDEX);

    case StorageErrorTableExits:
      DBUG_PRINT("info", ("StorageErrorTableExits"));
      return (HA_ERR_TABLE_EXIST);

    case StorageErrorUpdateConflict:
      DBUG_PRINT("info", ("StorageErrorUpdateConflict"));
      return (HA_ERR_RECORD_CHANGED);

    case StorageErrorUncommittedUpdates:
      DBUG_PRINT("info", ("StorageErrorUncommittedUpdates"));
      return (ER_LOCK_OR_ACTIVE_TRANSACTION);

    case StorageErrorUncommittedRecords:
      DBUG_PRINT("info", ("StorageErrorUncommittedRecords"));
      return (200 - storageError);

    case StorageErrorNoSequence:
      DBUG_PRINT("info", ("StorageErrorNoSequence"));
      return (200 - storageError);

    case StorageErrorTruncation:
      DBUG_PRINT("info", ("StorageErrorTruncation"));
      return (HA_ERR_TOO_BIG_ROW);

    case StorageErrorTableSpaceNotExist:
      DBUG_PRINT("info", ("StorageErrorTableSpaceNotExist"));
      return (HA_ERR_NO_SUCH_TABLE);

    case StorageErrorTableSpaceExist:
      DBUG_PRINT("info", ("StorageErrorTableSpaceExist"));
      return (HA_ERR_TABLESPACE_EXISTS);

    case StorageErrorDeviceFull:
      DBUG_PRINT("info", ("StorageErrorDeviceFull"));
      return (HA_ERR_RECORD_FILE_FULL);

    case StorageWarningSerializable:
      return (0);

    case StorageWarningReadUncommitted:
      return (0);

    case StorageErrorOutOfMemory:
      DBUG_PRINT("info", ("StorageErrorOutOfMemory"));
      return (HA_ERR_OUT_OF_MEM);

    case StorageErrorOutOfRecordMemory:
      DBUG_PRINT("info", ("StorageErrorOutOfRecordMemory"));
      return (200 - storageError);

    case StorageErrorTableNotEmpty:
      DBUG_PRINT("info", ("StorageErrorTableNotEmpty"));
      return HA_ERR_TABLESPACE_IS_NOT_EMPTY;

    case StorageErrorTableSpaceDataFileExist:
      DBUG_PRINT("info", ("StorageErrorTableSpaceDataFileExist"));
      return (HA_ERR_TABLESPACE_EXISTS);

    case StorageErrorIOErrorStreamLog:
      DBUG_PRINT("info", ("StorageErrorIOErrorStreamLog"));
      return (HA_ERR_LOGGING_IMPOSSIBLE);

    default:
      DBUG_PRINT("info", ("Unknown Changjiang Error"));
      return (200 - storageError);
  }
}

int StorageInterface::start_stmt(THD *thd, thr_lock_type lock_type) {
  DBUG_ENTER("StorageInterface::start_stmt");
  threadSwitch(thd);

  if (storageConnection->markVerb())
    trans_register_ha(thd, false, changjiang_hton, nullptr);

  DBUG_RETURN(0);
}

int StorageInterface::reset() {
  DBUG_ENTER("StorageInterface::start_stmt");
  indexOrder = false;

  DBUG_RETURN(0);
}

int StorageInterface::external_lock(THD *thd, int lock_type) {
  DBUG_ENTER("StorageInterface::external_lock");
  threadSwitch(thd);

  if (lock_type == F_UNLCK) {
    int ret = 0;

    storageConnection->setCurrentStatement(NULL);

    if (!thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))
      ret = storageConnection->endImplicitTransaction();
#if 0  // Commit or rollback will do it afterwards.
		else
			storageConnection->releaseVerb();
#endif
    if (storageTable) {
      storageTable->clearStatement();
      storageTable->clearCurrentIndex();
    }

    if (ret) DBUG_RETURN(error(ret));
  } else {
    if (storageConnection && thd->query().str)
      storageConnection->setCurrentStatement(thd->query().str);

    insertCount = 0;

    switch (thd_sql_command(thd)) {
      case SQLCOM_TRUNCATE:
      case SQLCOM_DROP_TABLE:
      case SQLCOM_ALTER_TABLE:
      case SQLCOM_DROP_INDEX:
      case SQLCOM_CREATE_INDEX: {
        int ret = storageTable->alterCheck();

        if (ret) {
          if (storageTable) storageTable->clearCurrentIndex();

          DBUG_RETURN(error(ret));
        }
      }
        storageTable->waitForWriteComplete();
        break;
      default:
        break;
    }

    int isolation = getTransactionIsolation(thd);

    if (thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {
      checkBinLog();

      if (storageConnection->startTransaction(isolation))
        trans_register_ha(thd, true, changjiang_hton, nullptr);

      if (storageConnection->markVerb())
        trans_register_ha(thd, false, changjiang_hton, nullptr);
    } else {
      checkBinLog();

      if (storageConnection->startImplicitTransaction(isolation))
        trans_register_ha(thd, false, changjiang_hton, nullptr);
    }

    switch (thd_tx_isolation(mySqlThread)) {
      case ISO_READ_UNCOMMITTED:
        error(StorageWarningReadUncommitted);
        break;

      case ISO_SERIALIZABLE:
        error(StorageWarningSerializable);
        break;
    }
  }

  DBUG_RETURN(0);
}

void StorageInterface::get_auto_increment(ulonglong offset, ulonglong increment,
                                          ulonglong nb_desired_values,
                                          ulonglong *first_value,
                                          ulonglong *nb_reserved_values) {
  DBUG_ENTER("StorageInterface::get_auto_increment");
  *first_value = storageShare->getSequenceValue(1);
  *nb_reserved_values = 1;

  DBUG_VOID_RETURN;
}

void StorageInterface::dropDatabase(handlerton *hton, char *path) {
  DBUG_ENTER("StorageInterface::dropDatabase");
  storageHandler->dropDatabase(path);

  DBUG_VOID_RETURN;
}

void StorageInterface::freeActiveBlobs(void) {
  for (StorageBlob *blob; (blob = activeBlobs);) {
    activeBlobs = blob->next;
    storageTable->freeBlob(blob);
    blob->next = freeBlobs;
    freeBlobs = blob;
  }
}

void StorageInterface::shutdown(handlerton *htons) { changjiang_deinit(0); }

int StorageInterface::panic(handlerton *hton, ha_panic_function flag) {
  return changjiang_deinit(0);
}

int StorageInterface::closeConnection(handlerton *hton, THD *thd) {
  DBUG_ENTER("NfsStorageEngine::closeConnection");
  storageHandler->closeConnections(thd);
  *thd_ha_data(thd, hton) = NULL;

  DBUG_RETURN(0);
}

int StorageInterface::alter_tablespace(handlerton *hton, THD *thd,
                                       st_alter_tablespace *ts_info,
                                       const dd::Tablespace *,
                                       dd::Tablespace *) {
  DBUG_ENTER("NfsStorageEngine::alter_tablespace");
  int ret = 0;
  const char *data_file_name = ts_info->data_file_name;
  char buff[FN_REFLEN];
  /*
  CREATE TABLESPACE tablespace
          ADD DATAFILE 'file'
          USE LOGFILE GROUP logfile_group         // NDB only
          [EXTENT_SIZE [=] extent_size]           // Not supported
          [INITIAL_SIZE [=] initial_size]         // Not supported
          [AUTOEXTEND_SIZE [=] autoextend_size]   // Not supported
          [MAX_SIZE [=] max_size]                 // Not supported
          [NODEGROUP [=] nodegroup_id]            // NDB only
          [WAIT]                                  // NDB only
          [COMMENT [=] comment_text]
          ENGINE [=] engine


  Parameters EXTENT_SIZE, INITIAL,SIZE, AUTOEXTEND_SIZE and MAX_SIZE are
  currently not supported by Changjiang. LOGFILE GROUP, NODEGROUP and WAIT are
  for NDB only.
  */

  if (data_file_name) {
    size_t length = strlen(data_file_name);
    if (length <= 4 || strcmp(data_file_name + length - 4, ".cts")) {
      if (!length || length > FN_REFLEN - 5) {
        my_printf_error(
            ER_WRONG_FILE_NAME, "%s", MYF(0),
            "The ADD DATAFILE filepath does not have a proper filename.");
      } else if (strcmp(data_file_name + length - 4, ".cts")) {
        my_printf_error(ER_WRONG_FILE_NAME, "%s", MYF(0),
                        "The ADD DATAFILE filepath must end with '.cts'.");
      } else {
        my_printf_error(
            ER_WRONG_FILE_NAME, "%s", MYF(0),
            "The ADD DATAFILE filepath does not have a proper filename.");
      }
      DBUG_RETURN(1);

      /*if (!length || length > FN_REFLEN - 5)
              {
              my_error(ER_WRONG_FILE_NAME, MYF(0), data_file_name);
              DBUG_RETURN(1);
              }
      memcpy(buff, data_file_name, length);
      buff[length]= '.';
      buff[length + 1]= 'c';
      buff[length + 2]= 't';
      buff[length + 3]= 's';
      buff[length + 4]= '\0';
      data_file_name= buff;*/
    }
  }

  switch (ts_info->ts_cmd_type) {
    case CREATE_TABLESPACE:
      ret = storageHandler->createTablespace(
          ts_info->tablespace_name, data_file_name, ts_info->ts_comment);
      break;

    case DROP_TABLESPACE:
      ret = storageHandler->deleteTablespace(ts_info->tablespace_name);
      break;

    default:
      DBUG_RETURN(HA_ADMIN_NOT_IMPLEMENTED);
  }

  DBUG_RETURN(getMySqlError(ret));
}

int StorageInterface::fill_is_table(handlerton *hton, THD *thd,
                                    TABLE_LIST *tables, class Item *cond,
                                    enum enum_schema_tables schema_table_idx) {
  InfoTableImpl infoTable(thd, tables, system_charset_info);

  if (!storageHandler) return 0;

  switch (schema_table_idx) {
    case SCH_TABLESPACES:
      storageHandler->getTableSpaceInfo(&infoTable);
      break;
    /*loongsql:TODOcase SCH_FILES:
            storageHandler->getTableSpaceFilesInfo(&infoTable);
            break;*/
    default:
      assert(false);
      return 0;
  }

  return infoTable.error;
}

// int StorageInterface::check_if_supported_alter(TABLE *altered_table,
// HA_CREATE_INFO *create_info, HA_ALTER_FLAGS *alter_flags, uint table_changes)
enum_alter_inplace_result StorageInterface::check_if_supported_inplace_alter(
    TABLE *altered_table, Alter_inplace_info *ha_alter_info) {
  HA_CREATE_INFO *create_info = ha_alter_info->create_info;
  Alter_inplace_info::HA_ALTER_FLAGS alter_flags = ha_alter_info->handler_flags;

  DBUG_ENTER("StorageInterface::check_if_supported_alter");
  tempTable = (create_info->options & HA_LEX_CREATE_TMP_TABLE) ? true : false;
  Alter_inplace_info::HA_ALTER_FLAGS supported = 0;
  supported =
      supported | Alter_inplace_info::ADD_INDEX |
      Alter_inplace_info::DROP_INDEX | Alter_inplace_info::ADD_UNIQUE_INDEX |
      Alter_inplace_info::DROP_UNIQUE_INDEX | Alter_inplace_info::ADD_PK_INDEX |
      Alter_inplace_info::DROP_PK_INDEX | Alter_inplace_info::ADD_COLUMN;
  Alter_inplace_info::HA_ALTER_FLAGS notSupported = ~(supported);

#ifndef ONLINE_ALTER
  DBUG_RETURN(HA_ALTER_NOT_SUPPORTED);
#endif

  if (tempTable || alter_flags & notSupported)
    DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);

  if (alter_flags & Alter_inplace_info::ADD_COLUMN) {
    Field *field = NULL;

    for (uint i = 0; i < altered_table->s->fields; i++) {
      field = altered_table->s->field[i];
      bool found = false;

      for (uint n = 0; n < table->s->fields; n++)
        if (found = (strcmp(table->s->field[n]->field_name,
                            field->field_name) == 0))
          break;

      if (field && !found)
        if (!field->is_nullable()) {
          DBUG_PRINT("info", ("Online add column must be nullable"));
          DBUG_RETURN(HA_ALTER_INPLACE_NOT_SUPPORTED);
        }
    }
  }

  DBUG_RETURN(HA_ALTER_INPLACE_NO_LOCK);
}

// Prepare for online ALTER

bool StorageInterface::prepare_inplace_alter_table(
    TABLE *altered_table, Alter_inplace_info *ha_alter_info,
    const dd::Table *old_table_def, dd::Table *new_table_def) {
  DBUG_ENTER("StorageInterface::alter_table_phase1");

  DBUG_RETURN(0);
}

// Perform the online ALTER

// int StorageInterface::alter_table_phase2(THD* thd, TABLE* altered_table,
// HA_CREATE_INFO* create_info, HA_ALTER_INFO* alter_info, HA_ALTER_FLAGS*
// alter_flags)
bool StorageInterface::inplace_alter_table(TABLE *altered_table,
                                           Alter_inplace_info *ha_alter_info,
                                           const dd::Table *old_table_def,
                                           dd::Table *new_table_def) {
  Alter_inplace_info::HA_ALTER_FLAGS alter_flags = ha_alter_info->handler_flags;
  DBUG_ENTER("StorageInterface::alter_table_phase2");

  int ret = 0;

  if (alter_flags & Alter_inplace_info::ADD_COLUMN)
    ret = addColumn(altered_table);

  if ((alter_flags & Alter_inplace_info::ADD_INDEX ||
       alter_flags & Alter_inplace_info::ADD_UNIQUE_INDEX ||
       alter_flags & Alter_inplace_info::ADD_PK_INDEX) &&
      !ret)
    ret = addIndex(altered_table);

  if ((alter_flags & Alter_inplace_info::DROP_INDEX ||
       alter_flags & Alter_inplace_info::DROP_UNIQUE_INDEX ||
       alter_flags & Alter_inplace_info::DROP_PK_INDEX) &&
      !ret)
    ret = dropIndex(altered_table);

  DBUG_RETURN(ret);
}

// Notification that changes are written and table re-opened

// int StorageInterface::alter_table_phase3(THD* thd, TABLE* altered_table)
bool StorageInterface::commit_inplace_alter_table(
    TABLE *altered_table, Alter_inplace_info *ha_alter_info, bool commit,
    const dd::Table *old_table_def, dd::Table *new_table_def) {
  DBUG_ENTER("StorageInterface::alter_table_phase3");

  DBUG_RETURN(0);
}

int StorageInterface::addColumn(TABLE *alteredTable) {
  int ret;
  int64 incrementValue = 0;
  /***
  const char *tableName = storageTable->getName();
  const char *schemaName = storageTable->getSchemaName();
  ***/
  CmdGen gen;
  genTable(alteredTable, &gen);

  /***
  if (alteredTable->found_next_number_field)
          {
          incrementValue = alterInfo->auto_increment_value;

          if (incrementValue == 0)
                  incrementValue = 1;
          }
  ***/

  /***
  if (alteredTable->s->primary_key < alteredTable->s->keys)
          {
          KEY *key = alteredTable->key_info + alteredTable->s->primary_key;
          gen.gen(",\n  primary key ");
          genKeyFields(key, &gen);
          }

  gen.gen (")");
  ***/

  if ((ret = storageTable->upgrade(gen.getString(), incrementValue)))
    return (error(ret));

  return 0;
}

int StorageInterface::addIndex(TABLE *alteredTable) {
  int ret = 0;
  const char *tableName = storageTable->getName();
  const char *schemaName = storageTable->getSchemaName();

  // Lock out other clients before locking the table

  storageShare->lockIndexes(true);
  storageShare->lock(true);

  // Find indexes to be added by comparing table and alteredTable

  for (unsigned int n = 0; n < alteredTable->s->keys; n++) {
    KEY *key = alteredTable->key_info + n;
    KEY *tableEnd = table->key_info + table->s->keys;
    KEY *tableKey;

    for (tableKey = table->key_info; tableKey < tableEnd; tableKey++)
      if (!strcmp(tableKey->name, key->name)) break;

    if (tableKey >= tableEnd)
      if ((ret = createIndex(schemaName, tableName, alteredTable, n))) break;
  }

  // The server indexes may have been reordered, so remap to the Changjiang
  // indexes

  remapIndexes(alteredTable);

  storageShare->unlock();
  storageShare->unlockIndexes();

  return error(ret);
}

int StorageInterface::dropIndex(TABLE *alteredTable) {
  int ret = 0;
  const char *tableName = storageTable->getName();
  const char *schemaName = storageTable->getSchemaName();

  // Lock out other clients before locking the table

  storageShare->lockIndexes(true);
  storageShare->lock(true);

  // Find indexes to be dropped by comparing table and alteredTable

  for (unsigned int n = 0; n < table->s->keys; n++) {
    KEY *key = table->key_info + n;
    KEY *alterEnd = alteredTable->key_info + alteredTable->s->keys;
    KEY *alterKey;

    for (alterKey = alteredTable->key_info; alterKey < alterEnd; alterKey++)
      if (!strcmp(alterKey->name, key->name)) break;

    if (alterKey >= alterEnd)
      if ((ret = dropIndex(schemaName, tableName, table, n, true))) break;
  }

  // The server indexes have been reordered, so remap to the Changjiang indexes

  remapIndexes(alteredTable);

  storageShare->unlock();
  storageShare->unlockIndexes();

  return error(ret);
}

uint StorageInterface::max_supported_key_length(void) const {
  // Assume 4K page unless proven otherwise.
  if (storageConnection) return storageConnection->getMaxKeyLength();

  return MAX_INDEX_KEY_LENGTH_4K;  // Default value.
}

uint StorageInterface::max_supported_key_part_length(HA_CREATE_INFO *create_info
                                                     [[maybe_unused]]) const {
  // Assume 4K page unless proven otherwise.
  if (storageConnection) return storageConnection->getMaxKeyLength();

  return MAX_INDEX_KEY_LENGTH_4K;  // Default for future sizes.
}

void StorageInterface::logger(int mask, const char *text, void *arg) {
  if (mask & changjiang_debug_mask) {
    printf("%s", text);
    fflush(stdout);

    if (changjiang_log_file) {
      fprintf(changjiang_log_file, "%s", text);
      fflush(changjiang_log_file);
    }
  }
}

void StorageInterface::mysqlLogger(int mask, const char *text, void *arg) {
  if (mask & LogMysqlError)
    sql_print_error("%s", text);
  else if (mask & LogMysqlWarning)
    sql_print_warning("%s", text);
  else if (mask & LogMysqlInfo)
    sql_print_information("%s", text);
}

int StorageInterface::setIndex(TABLE *srvTable, int indexId) {
  StorageIndexDesc indexDesc;
  getKeyDesc(srvTable, indexId, &indexDesc);

  return storageTable->setIndex(&indexDesc);
}

int StorageInterface::setIndexes(TABLE *srvTable) {
  int ret = 0;

  if (!srvTable || storageShare->haveIndexes(srvTable->s->keys)) return ret;

  storageShare->lockIndexes(true);
  storageShare->lock(true);

  ret = remapIndexes(srvTable);

  storageShare->unlock();
  storageShare->unlockIndexes();

  return ret;
}

// Create an index entry in StorageTableShare for each TABLE index
// Assume exclusive lock on StorageTableShare::syncIndexMap

int StorageInterface::remapIndexes(TABLE *srvTable) {
  int ret = 0;

  storageShare->deleteIndexes();

  if (!srvTable) return ret;

  // Ok to ignore errors in this context

  for (uint n = 0; n < srvTable->s->keys; ++n) setIndex(srvTable, n);

  // validateIndexes(srvTable, true);

  return ret;
}

bool StorageInterface::validateIndexes(TABLE *srvTable, bool exclusiveLock) {
  bool ret = true;

  if (!srvTable) return false;

  storageShare->lockIndexes(exclusiveLock);

  for (uint n = 0; (n < srvTable->s->keys) && ret; ++n) {
    StorageIndexDesc indexDesc;
    getKeyDesc(srvTable, n, &indexDesc);

    if (!storageShare->validateIndex(n, &indexDesc)) ret = false;
  }

  if (ret && (srvTable->s->keys != (uint)storageShare->numberIndexes()))
    ret = false;

  storageShare->unlockIndexes();

  return ret;
}

int StorageInterface::genTable(TABLE *srvTable, CmdGen *gen) {
  const char *tableName = storageTable->getName();
  const char *schemaName = storageTable->getSchemaName();
  gen->gen("upgrade table \"%s\".\"%s\" (\n", schemaName, tableName);
  const char *sep = "";
  char nameBuffer[256];

  for (uint n = 0; n < srvTable->s->fields; ++n) {
    Field *field = srvTable->field[n];
    CHARSET_INFO *charset = (CHARSET_INFO *)field->charset();

    if (charset) storageShare->registerCollation(charset->csname, charset);

    storageShare->cleanupFieldName(field->field_name, nameBuffer,
                                   sizeof(nameBuffer), true);
    gen->gen("%s  \"%s\" ", sep, nameBuffer);
    int ret = genType(field, gen);

    if (ret) return (ret);

    if (!field->is_nullable()) gen->gen(" not null");

    sep = ",\n";
  }

  if (srvTable->s->primary_key < srvTable->s->keys) {
    KEY *key = srvTable->key_info + srvTable->s->primary_key;
    gen->gen(",\n  primary key ");
    genKeyFields(key, gen);
  }

#if 0		
	// Disable until UPGRADE TABLE supports index syntax
	
	for (unsigned int n = 0; n < srvTable->s->keys; n++)
		{
		if (n != srvTable->s->primary_key)
			{
			KEY *key = srvTable->key_info + n;
			const char *unique = (key->flags & HA_NOSAME) ? "unique " : "";
			gen->gen(",\n  %s key ", unique);
			genKeyFields(key, gen);
			}
		}
#endif

  gen->gen(")");

  return 0;
}

int StorageInterface::genType(Field *field, CmdGen *gen) {
  const char *type;
  const char *arg = NULL;
  int length = 0;

  switch (field->real_type()) {
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_BIT:
      type = "smallint";
      break;

    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_YEAR:
      type = "int";
      break;

    case MYSQL_TYPE_FLOAT:
      type = "float";
      break;

    case MYSQL_TYPE_DOUBLE:
      type = "double";
      break;

    case MYSQL_TYPE_TIMESTAMP:
      type = "timestamp";
      break;

    case MYSQL_TYPE_TIMESTAMP2:
    case MYSQL_TYPE_DATETIME2:
    case MYSQL_TYPE_TIME2:
      type = "bigint";
      break;

    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_LONGLONG:
      type = "bigint";
      break;

      /*
              Changjiang's date and time types don't handle invalid dates like
         MySQL's do, so we just use an int for storage
      */

    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_NEWDATE:
      type = "int";
      break;

    case MYSQL_TYPE_DATETIME:
      type = "bigint";
      break;

    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING: {
      CHARSET_INFO *charset = (CHARSET_INFO *)field->charset();

      if (charset) {
        arg = charset->csname;
        type = "varchar (%d) collation %s";
      } else
        type = "varchar (%d)";

      length = field->field_length;
    } break;

    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_GEOMETRY:
      if (field->field_length < 256)
        type = "varchar (256)";
      else
        type = "blob";
      break;

    case MYSQL_TYPE_NEWDECIMAL: {
      Field_new_decimal *newDecimal = (Field_new_decimal *)field;

      /***
      if (newDecimal->precision > 18 && newDecimal->dec > 9)
              {
              errorText = "columns with greater than 18 digits precision and
      greater than 9 digits of fraction are not supported";

              return HA_ERR_UNSUPPORTED;
              }
      ***/

      gen->gen("numeric (%d,%d)", newDecimal->precision, newDecimal->dec);

      return 0;
    }

    default:
      errorText = "unsupported Changjiang data type";

      return HA_ERR_UNSUPPORTED;
  }

  gen->gen(type, length, arg);

  return 0;
}

void StorageInterface::genKeyFields(KEY *key, CmdGen *gen) {
  const char *sep = "(";
  char nameBuffer[256];

  for (uint n = 0; n < key->user_defined_key_parts; ++n) {
    KEY_PART_INFO *part = key->key_part + n;
    Field *field = part->field;
    storageShare->cleanupFieldName(field->field_name, nameBuffer,
                                   sizeof(nameBuffer), true);

    if (part->key_part_flag & HA_PART_KEY_SEG)
      gen->gen("%s\"%s\"(%d)", sep, nameBuffer, part->length);
    else
      gen->gen("%s\"%s\"", sep, nameBuffer);

    sep = ", ";
  }

  gen->gen(")");
}

void StorageInterface::encodeRecord(uchar *buf, bool updateFlag) {
  storageTable->preInsert();
  ptrdiff_t ptrDiff = buf - table->record[0];
  my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->read_set);
  EncodedDataStream *dataStream = &storageTable->dataStream;
  FieldFormat *fieldFormat = storageShare->format->format;
  int maxId = storageShare->format->maxId;

  for (int n = 0; n < maxId; ++n, ++fieldFormat) {
    if (fieldFormat->fieldId < 0 || fieldFormat->offset == 0) continue;

    Field *field = fieldMap[fieldFormat->fieldId];
    ASSERT(field);

    if (ptrDiff) field->move_field_offset(ptrDiff);

    if (updateFlag && !bitmap_is_set(table->write_set, field->field_index())) {
      const unsigned char *p = storageTable->getEncoding(n);
      dataStream->encodeEncoding(p);
    } else if (field->is_null())
      dataStream->encodeNull();
    else
      switch (field->real_type()) {
        case MYSQL_TYPE_TINY:
        case MYSQL_TYPE_SHORT:
        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_LONG:
        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_ENUM:
        case MYSQL_TYPE_SET:
        case MYSQL_TYPE_BIT:
          dataStream->encodeInt64(field->val_int());
          break;

        case MYSQL_TYPE_LONGLONG: {
          int64 temp = field->val_int();

          // If the field is unsigned and the MSB is set,
          // encode it as a BigInt to support unsigned values
          // with the MSB set in the index

          if (((Field_num *)field)->is_unsigned() &&
              (temp & LL(0x8000000000000000))) {
            BigInt bigInt;
            bigInt.set2((uint64)temp, 0);
            dataStream->encodeBigInt(&bigInt);
          } else {
            dataStream->encodeInt64(temp);
          }

        } break;

        case MYSQL_TYPE_YEAR:
          // Have to use the ptr directly to get the same number for
          // both two and four digit YEAR
          dataStream->encodeInt64((int)field->field_ptr()[0]);
          break;

        case MYSQL_TYPE_NEWDECIMAL: {
          int precision = ((Field_new_decimal *)field)->precision;
          int scale = ((Field_new_decimal *)field)->dec;

          if (precision < 19) {
            int64 value = ScaledBinary::getInt64FromBinaryDecimal(
                (const char *)field->field_ptr(), precision, scale);
            dataStream->encodeInt64(value, scale);
          } else {
            BigInt bigInt;
            ScaledBinary::getBigIntFromBinaryDecimal(
                (const char *)field->field_ptr(), precision, scale, &bigInt);

            // Handle value as int64 if possible. Even if the number fits
            // an int64, it can only be scaled within 18 digits or less.

            if (bigInt.fitsInInt64() && scale < 19) {
              int64 value = bigInt.getInt();
              dataStream->encodeInt64(value, scale);
            } else
              dataStream->encodeBigInt(&bigInt);
          }
        } break;

        case MYSQL_TYPE_DOUBLE:
        case MYSQL_TYPE_FLOAT:
          dataStream->encodeDouble(field->val_real());
          break;

        case MYSQL_TYPE_TIMESTAMP: {
          my_timeval tm;
          int warnings;
          ((Field_timestamp *)field)->get_timestamp(&tm, &warnings);
          int64 value = tm.m_tv_sec;
          dataStream->encodeDate(value * 1000);
        } break;
        case MYSQL_TYPE_TIMESTAMP2: {
          MYSQL_TIME time;
          /*((Field_timestampf*) field)->get_date(&time, 0);*/
          my_timeval tm;
          ((Field_timestampf *)field)->get_timestamp(&tm, 0);
          int64 value = 0;
          if (tm.m_tv_sec == 0 && tm.m_tv_usec == 0) {
            // '0000-00-00 00:00:00.000000'
          } else {
            my_tz_OFFSET0->gmt_sec_to_TIME(&time, tm);
            value = TIME_to_longlong_packed(time);
          }
          dataStream->encodeInt64(value);
        } break;
        case MYSQL_TYPE_DATETIME2: {
          int64 value = ((Field_datetimef *)field)->val_date_temporal();
          dataStream->encodeInt64(value);
        } break;
        case MYSQL_TYPE_TIME2: {
          int64 value = ((Field_timef *)field)->val_time_temporal();
          dataStream->encodeInt64(value);
        } break;

        case MYSQL_TYPE_DATE:
          dataStream->encodeInt64(field->val_int());
          break;

        case MYSQL_TYPE_NEWDATE:
          // dataStream->encodeInt64(field->val_int());
          dataStream->encodeInt64(uint3korr(field->field_ptr()));
          break;

        case MYSQL_TYPE_TIME:
          dataStream->encodeInt64(field->val_int());
          break;

        case MYSQL_TYPE_DATETIME:
          dataStream->encodeInt64(field->val_int());
          break;

        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_STRING: {
          String string;
          String buffer;
          field->val_str(&buffer, &string);
          dataStream->encodeOpaque(string.length(), string.ptr());
        } break;

        case MYSQL_TYPE_TINY_BLOB: {
          Field_blob *blob = (Field_blob *)field;
          uint length = blob->get_length();
          const uchar *ptr;
          ptr = blob->data_ptr();
          dataStream->encodeOpaque(length, (const char *)ptr);
        } break;

        case MYSQL_TYPE_LONG_BLOB:
        case MYSQL_TYPE_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_GEOMETRY: {
          Field_blob *blob = (Field_blob *)field;
          uint length = blob->get_length();
          const uchar *ptr;
          ptr = blob->data_ptr();
          StorageBlob *storageBlob;
          uint32 blobId;

          for (storageBlob = activeBlobs; storageBlob;
               storageBlob = storageBlob->next)
            if (storageBlob->data == (uchar *)ptr) {
              blobId = storageBlob->blobId;
              break;
            }

          if (!storageBlob) {
            StorageBlob storageBlob;
            storageBlob.length = length;
            storageBlob.data = (uchar *)ptr;
            blobId = storageTable->storeBlob(&storageBlob);
            blob->set_ptr(storageBlob.length, storageBlob.data);
          }

          dataStream->encodeBinaryBlob(blobId);
        } break;

        default:
          dataStream->encodeOpaque(field->field_length,
                                   (const char *)field->field_ptr());
      }

    if (ptrDiff) field->move_field_offset(-ptrDiff);
  }

  dbug_tmp_restore_column_map(table->read_set, old_map);
}

void StorageInterface::decodeRecord(uchar *buf) {
  EncodedDataStream *dataStream = &storageTable->dataStream;
  ptrdiff_t ptrDiff = buf - table->record[0];
  my_bitmap_map *old_map = dbug_tmp_use_all_columns(table, table->write_set);
  DBUG_ENTER("StorageInterface::decodeRecord");

  // Format of this record

  FieldFormat *fieldFormat = storageTable->format->format;
  int maxId = storageTable->format->maxId;

  // Current format for the table, possibly newer than the record format

  int tableMaxId = storageTable->share->format->maxId;
  FieldFormat *tableFieldFormat = storageTable->share->format->format;

  for (int n = 0; n < tableMaxId; ++n, ++fieldFormat, ++tableFieldFormat) {
    // Online ALTER ADD COLUMN creates a new record format for the table
    // that will have more fields than the older formats associated with
    // existing rows.
    //
    // Currently, online ALTER ADD COLUMN only supports nullable columns and
    // no default value. If the format of this record has fewer fields
    // than the default format of the table, then there are no fields to
    // decode beyond maxId, so set them to NULL.

    if (n >= maxId) {
      Field *newField = fieldMap[tableFieldFormat->fieldId];
      newField->set_null();
      newField->reset();
      continue;
    }

    // If the format doesn't have an offset, the field doesn't exist in the
    // record

    if (fieldFormat->fieldId < 0 || fieldFormat->offset == 0) continue;

    dataStream->decode();
    Field *field = fieldMap[fieldFormat->fieldId];

    // If we don't have a field for the physical field, just skip over it and
    // don't worry

    if (field == NULL) continue;

    if (ptrDiff) field->move_field_offset(ptrDiff);

    if (dataStream->type == edsTypeNull ||
        !bitmap_is_set(table->read_set, field->field_index())) {
      field->set_null();
      field->reset();
    } else {
      field->set_notnull();

      switch (field->real_type()) {
        case MYSQL_TYPE_TINY:
        case MYSQL_TYPE_SHORT:
        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_LONG:
        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_ENUM:
        case MYSQL_TYPE_SET:
        case MYSQL_TYPE_BIT:
          field->store(dataStream->getInt64(),
                       ((Field_num *)field)->is_unsigned());
          break;

        case MYSQL_TYPE_LONGLONG: {
          // If the type is edsTypeBigInt, the value is
          // unsigned and has the MSB set. This case has
          // been handled specially in encodeRecord() to
          // support unsigned values with the MSB set in
          // the index

          if (dataStream->type == edsTypeBigInt) {
            int64 value = dataStream->bigInt.getInt();
            field->store(value, ((Field_num *)field)->is_unsigned());
          } else {
            field->store(dataStream->getInt64(),
                         ((Field_num *)field)->is_unsigned());
          }
        } break;

        case MYSQL_TYPE_YEAR:
          // Must add 1900 to give Field_year::store the value it
          // expects. See also case 'MYSQL_TYPE_YEAR' in encodeRecord()
          field->store(dataStream->getInt64() + 1900,
                       ((Field_num *)field)->is_unsigned());
          break;

        case MYSQL_TYPE_NEWDECIMAL: {
          int precision = ((Field_new_decimal *)field)->precision;
          int scale = ((Field_new_decimal *)field)->dec;

          if (dataStream->type == edsTypeBigInt)
            ScaledBinary::putBigInt(&dataStream->bigInt,
                                    (char *)field->field_ptr(), precision,
                                    scale);
          else {
            int64 value = dataStream->getInt64(scale);
            ScaledBinary::putBinaryDecimal(value, (char *)field->field_ptr(),
                                           precision, scale);
          }
        } break;

        case MYSQL_TYPE_DOUBLE:
        case MYSQL_TYPE_FLOAT:
          field->store(dataStream->value.dbl);
          break;

        case MYSQL_TYPE_TIMESTAMP: {
          int64 value = (int64)(dataStream->value.integer64 / 1000);
          ((Field_timestamp *)field)->store_packed(value);
        } break;
        case MYSQL_TYPE_TIMESTAMP2: {
          int64 value = (int64)(dataStream->getInt64());

          if (value != 0) {
            // Set use current time zone
            MYSQL_TIME ltime;
            TIME_from_longlong_datetime_packed(&ltime, value);
            Time_zone *tz = mySqlThread->time_zone();
            ltime.time_type = MYSQL_TIMESTAMP_DATETIME_TZ;
            convert_time_zone_displacement(tz, &ltime);
            value = TIME_to_longlong_packed(ltime);
          }

          ((Field_timestampf *)field)->store_packed(value);
        } break;
        case MYSQL_TYPE_DATETIME2: {
          int64 value = (int64)(dataStream->getInt64());
          ((Field_datetimef *)field)->store_packed(value);
        } break;
        case MYSQL_TYPE_TIME2: {
          int64 value = (int64)(dataStream->getInt64());
          ((Field_timef *)field)->store_packed(value);
        } break;

        case MYSQL_TYPE_DATE:
          field->store(dataStream->getInt64(), false);
          break;

        case MYSQL_TYPE_NEWDATE:
          int3store(field->field_ptr(), dataStream->getInt32());
          break;

        case MYSQL_TYPE_TIME:
          field->store(dataStream->getInt64(), false);
          break;

        case MYSQL_TYPE_DATETIME:
          int8store(field->field_ptr(), dataStream->getInt64());
          break;

        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_STRING:
          field->store((const char *)dataStream->value.string.data,
                       dataStream->value.string.length, field->charset());
          break;

        case MYSQL_TYPE_TINY_BLOB: {
          Field_blob *blob = (Field_blob *)field;
          blob->set_ptr(dataStream->value.string.length,
                        (uchar *)dataStream->value.string.data);
        } break;

        case MYSQL_TYPE_LONG_BLOB:
        case MYSQL_TYPE_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_GEOMETRY: {
          Field_blob *blob = (Field_blob *)field;
          StorageBlob *storageBlob = freeBlobs;

          if (storageBlob)
            freeBlobs = storageBlob->next;
          else
            storageBlob = new StorageBlob;

          storageBlob->next = activeBlobs;
          activeBlobs = storageBlob;
          storageBlob->blobId = dataStream->value.blobId;
          storageTable->getBlob(storageBlob->blobId, storageBlob);
          blob->set_ptr(storageBlob->length, (uchar *)storageBlob->data);
        } break;

        default: {
          uint l = dataStream->value.string.length;

          if (field->field_length < l) l = field->field_length;

          memcpy(field->field_ptr(), dataStream->value.string.data, l);
        }
      }
    }

    if (ptrDiff) field->move_field_offset(-ptrDiff);
  }
  dbug_tmp_restore_column_map(table->write_set, old_map);

  DBUG_VOID_RETURN;
}

int StorageInterface::extra(ha_extra_function operation) {
  DBUG_ENTER("StorageInterface::extra");
#if 0  // loongsql:TODO
	if (operation == HA_EXTRA_ORDERBY_LIMIT)
		{
		// SQL Layer informs us that it is considering an ORDER BY .. LIMIT
		// query. It's time we could
		//  1. start returning HA_READ_ORDER flag from index_flags() calls,
		//     which will make the SQL layer consider using indexes to
		//     satisfy ORDER BY ... LIMIT.
		//  2. If doing #1, every index/range scan must return records in
		//     index order.

		indexOrder = true;
		}

	if (operation == HA_EXTRA_NO_ORDERBY_LIMIT)
		{
		// SQL Layer figured it won't be able to use index to resolve the 
		// ORDER BY ... LIMIT. This could happen for a number of reasons,
		// but the result is that we don't have to return records in index
		// order.
		
		indexOrder = false;
		}
#endif
  DBUG_RETURN(0);
}

bool StorageInterface::get_error_message(int error, String *buf) {
  if (storageConnection) {
    const char *text = storageConnection->getLastErrorString();
    buf->set(text, (uint32)strlen(text), system_charset_info);
  } else if (errorText)
    buf->set(errorText, (uint32)strlen(errorText), system_charset_info);

  return false;
}

void StorageInterface::unlockTable(void) {
  if (tableLocked) {
    storageShare->unlock();
    tableLocked = false;
  }
}

void StorageInterface::checkBinLog(void) {
  // If binary logging is enabled, ensure that records are fully populated for
  // replication

  if (mysql_bin_log.is_open())
    tableFlags |= HA_PRIMARY_KEY_REQUIRED_FOR_DELETE;
  else
    tableFlags &= ~HA_PRIMARY_KEY_REQUIRED_FOR_DELETE;
}

//*****************************************************************************
//
// NfsPluginHandler
//
//*****************************************************************************
NfsPluginHandler::NfsPluginHandler() {
  storageConnection = NULL;
  storageTable = NULL;
}

NfsPluginHandler::~NfsPluginHandler() {}

//*****************************************************************************
//
// CHANGJIANG_SYSTEM_MEMORY_DETAIL
//
//*****************************************************************************
int NfsPluginHandler::getSystemMemoryDetailInfo(THD *thd, TABLE_LIST *tables,
                                                COND *cond) {
  InfoTableImpl infoTable(thd, tables, system_charset_info);

  if (storageHandler) storageHandler->getMemoryDetailInfo(&infoTable);

  return infoTable.error;
}

ST_FIELD_INFO memoryDetailFieldInfo[] = {
    {"FILE", 120, MYSQL_TYPE_STRING, 0, 0, "File", 0},
    {"LINE", 4, MYSQL_TYPE_LONG, 0, 0, "Line", 0},
    {"OBJECTS_IN_USE", 4, MYSQL_TYPE_LONG, 0, 0, "Objects in Use", 0},
    {"SPACE_IN_USE", 4, MYSQL_TYPE_LONG, 0, 0, "Space in Use", 0},
    {"OBJECTS_DELETED", 4, MYSQL_TYPE_LONG, 0, 0, "Objects Deleted", 0},
    {"SPACE_DELETED", 4, MYSQL_TYPE_LONG, 0, 0, "Space Deleted", 0},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}};

int NfsPluginHandler::initSystemMemoryDetailInfo(void *p) {
  DBUG_ENTER("initSystemMemoryDetailInfo");
  ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
  schema->fields_info = memoryDetailFieldInfo;
  schema->fill_table = NfsPluginHandler::getSystemMemoryDetailInfo;
  DBUG_RETURN(0);
}

int NfsPluginHandler::deinitSystemMemoryDetailInfo(void *p) {
  DBUG_ENTER("deinitSystemMemoryDetailInfo");
  DBUG_RETURN(0);
}

//*****************************************************************************
//
// CHANGJIANG_SYSTEM_MEMORY_SUMMARY
//
//*****************************************************************************

int NfsPluginHandler::getSystemMemorySummaryInfo(THD *thd, TABLE_LIST *tables,
                                                 COND *cond) {
  // return(pluginHandler->fillSystemMemorySummaryTable(thd, tables, cond));
  InfoTableImpl infoTable(thd, tables, system_charset_info);

  if (storageHandler) storageHandler->getMemorySummaryInfo(&infoTable);

  return infoTable.error;
}

ST_FIELD_INFO memorySummaryFieldInfo[] = {
    {"TOTAL_SPACE", 4, MYSQL_TYPE_LONGLONG, 0, 0, "Total Space", 0},
    {"FREE_SPACE", 4, MYSQL_TYPE_LONGLONG, 0, 0, "Free Space", 0},
    {"FREE_SEGMENTS", 4, MYSQL_TYPE_LONG, 0, 0, "Free Segments", 0},
    {"BIG_HUNKS", 4, MYSQL_TYPE_LONG, 0, 0, "Big Hunks", 0},
    {"SMALL_HUNKS", 4, MYSQL_TYPE_LONG, 0, 0, "Small Hunks", 0},
    {"UNIQUE_SIZES", 4, MYSQL_TYPE_LONG, 0, 0, "Unique Sizes", 0},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}};

int NfsPluginHandler::initSystemMemorySummaryInfo(void *p) {
  DBUG_ENTER("initSystemMemorySummaryInfo");
  ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
  schema->fields_info = memorySummaryFieldInfo;
  schema->fill_table = NfsPluginHandler::getSystemMemorySummaryInfo;

  DBUG_RETURN(0);
}

int NfsPluginHandler::deinitSystemMemorySummaryInfo(void *p) {
  DBUG_ENTER("deinitSystemMemorySummaryInfo");
  DBUG_RETURN(0);
}

//*****************************************************************************
//
// CHANGJIANG_RECORD_CACHE_DETAIL
//
//*****************************************************************************

int NfsPluginHandler::getRecordCacheDetailInfo(THD *thd, TABLE_LIST *tables,
                                               COND *cond) {
  InfoTableImpl infoTable(thd, tables, system_charset_info);

  if (storageHandler) storageHandler->getRecordCacheDetailInfo(&infoTable);

  return infoTable.error;
}

ST_FIELD_INFO recordDetailFieldInfo[] = {
    {"FILE", 120, MYSQL_TYPE_STRING, 0, 0, "File", 0},
    {"LINE", 4, MYSQL_TYPE_LONG, 0, 0, "Line", 0},
    {"OBJECTS_IN_USE", 4, MYSQL_TYPE_LONG, 0, 0, "Objects in Use", 0},
    {"SPACE_IN_USE", 4, MYSQL_TYPE_LONG, 0, 0, "Space in Use", 0},
    {"OBJECTS_DELETED", 4, MYSQL_TYPE_LONG, 0, 0, "Objects Deleted", 0},
    {"SPACE_DELETED", 4, MYSQL_TYPE_LONG, 0, 0, "Space Deleted", 0},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}};

int NfsPluginHandler::initRecordCacheDetailInfo(void *p) {
  DBUG_ENTER("initRecordCacheDetailInfo");
  ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
  schema->fields_info = recordDetailFieldInfo;
  schema->fill_table = NfsPluginHandler::getRecordCacheDetailInfo;

  DBUG_RETURN(0);
}

int NfsPluginHandler::deinitRecordCacheDetailInfo(void *p) {
  DBUG_ENTER("deinitRecordCacheDetailInfo");
  DBUG_RETURN(0);
}

//*****************************************************************************
//
// CHANGJIANG_RECORD_CACHE_SUMMARY
//
//*****************************************************************************

int NfsPluginHandler::getRecordCacheSummaryInfo(THD *thd, TABLE_LIST *tables,
                                                COND *cond) {
  InfoTableImpl infoTable(thd, tables, system_charset_info);

  if (storageHandler) storageHandler->getRecordCacheSummaryInfo(&infoTable);

  return infoTable.error;
}

ST_FIELD_INFO recordSummaryFieldInfo[] = {
    {"TOTAL_SPACE", 4, MYSQL_TYPE_LONGLONG, 0, 0, "Total Space", 0},
    {"FREE_SPACE", 4, MYSQL_TYPE_LONGLONG, 0, 0, "Free Space", 0},
    {"FREE_SEGMENTS", 4, MYSQL_TYPE_LONG, 0, 0, "Free Segments", 0},
    {"BIG_HUNKS", 4, MYSQL_TYPE_LONG, 0, 0, "Big Hunks", 0},
    {"SMALL_HUNKS", 4, MYSQL_TYPE_LONG, 0, 0, "Small Hunks", 0},
    {"UNIQUE_SIZES", 4, MYSQL_TYPE_LONG, 0, 0, "Unique Sizes", 0},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}};

int NfsPluginHandler::initRecordCacheSummaryInfo(void *p) {
  DBUG_ENTER("initRecordCacheSummaryInfo");
  ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
  schema->fields_info = recordSummaryFieldInfo;
  schema->fill_table = NfsPluginHandler::getRecordCacheSummaryInfo;

  DBUG_RETURN(0);
}

int NfsPluginHandler::deinitRecordCacheSummaryInfo(void *p) {
  DBUG_ENTER("deinitRecordCacheSummaryInfo");
  DBUG_RETURN(0);
}

//*****************************************************************************
//
// CHANGJIANG_TABLESPACE_IO
//
//*****************************************************************************

int NfsPluginHandler::getTableSpaceIOInfo(THD *thd, TABLE_LIST *tables,
                                          COND *cond) {
  InfoTableImpl infoTable(thd, tables, system_charset_info);

  if (storageHandler) storageHandler->getIOInfo(&infoTable);

  return infoTable.error;
}

ST_FIELD_INFO tableSpaceIOFieldInfo[] = {
    {"TABLESPACE", 120, MYSQL_TYPE_STRING, 0, 0, "Tablespace", 0},
    {"PAGE_SIZE", 4, MYSQL_TYPE_LONG, 0, 0, "Page Size", 0},
    {"BUFFERS", 4, MYSQL_TYPE_LONG, 0, 0, "Buffers", 0},
    {"PHYSICAL_READS", 4, MYSQL_TYPE_LONG, 0, 0, "Physical Reads", 0},
    {"WRITES", 4, MYSQL_TYPE_LONG, 0, 0, "Writes", 0},
    {"LOGICAL_READS", 4, MYSQL_TYPE_LONG, 0, 0, "Logical Reads", 0},
    {"FAKES", 4, MYSQL_TYPE_LONG, 0, 0, "Fakes", 0},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, 0}};

int NfsPluginHandler::initTableSpaceIOInfo(void *p) {
  DBUG_ENTER("initTableSpaceIOInfo");
  ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
  schema->fields_info = tableSpaceIOFieldInfo;
  schema->fill_table = NfsPluginHandler::getTableSpaceIOInfo;

  DBUG_RETURN(0);
}

int NfsPluginHandler::deinitTableSpaceIOInfo(void *p) {
  DBUG_ENTER("deinitTableSpaceIOInfo");
  DBUG_RETURN(0);
}

//*****************************************************************************
//
// CHANGJIANG_TRANSACTIONS
//
//*****************************************************************************

int NfsPluginHandler::getTransactionInfo(THD *thd, TABLE_LIST *tables,
                                         COND *cond) {
  InfoTableImpl infoTable(thd, tables, system_charset_info);

  if (storageHandler) storageHandler->getTransactionInfo(&infoTable);

  return infoTable.error;
}

ST_FIELD_INFO transactionInfoFieldInfo[] = {
    {"STATE", 120, MYSQL_TYPE_STRING, 0, 0, "State", SKIP_OPEN_TABLE},
    {"THREAD_ID", 4, MYSQL_TYPE_LONG, 0, 0, "Thread Id", SKIP_OPEN_TABLE},
    {"ID", 4, MYSQL_TYPE_LONG, 0, 0, "Id", SKIP_OPEN_TABLE},
    {"UPDATES", 4, MYSQL_TYPE_LONG, 0, 0, "Has Updates", SKIP_OPEN_TABLE},
    {"PENDING", 4, MYSQL_TYPE_LONG, 0, 0, "Write Pending", SKIP_OPEN_TABLE},
    {"DEP", 4, MYSQL_TYPE_LONG, 0, 0, "Dependencies", SKIP_OPEN_TABLE},
    {"OLDEST", 4, MYSQL_TYPE_LONG, 0, 0, "Oldest Active", SKIP_OPEN_TABLE},
    {"RECORDS", 4, MYSQL_TYPE_LONG, 0, 0, "Has Records", SKIP_OPEN_TABLE},
    {"WAITING_FOR", 4, MYSQL_TYPE_LONG, 0, 0, "Waiting For", SKIP_OPEN_TABLE},
    {"STATEMENT", 120, MYSQL_TYPE_STRING, 0, 0, "Statement", SKIP_OPEN_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

int NfsPluginHandler::initTransactionInfo(void *p) {
  DBUG_ENTER("initTransactionInfo");
  ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
  schema->fields_info = transactionInfoFieldInfo;
  schema->fill_table = NfsPluginHandler::getTransactionInfo;

  DBUG_RETURN(0);
}

int NfsPluginHandler::deinitTransactionInfo(void *p) {
  DBUG_ENTER("deinitTransactionInfo");
  DBUG_RETURN(0);
}

//*****************************************************************************
//
// CHANGJIANG_TRANSACTION_SUMMARY
//
//*****************************************************************************

int NfsPluginHandler::getTransactionSummaryInfo(THD *thd, TABLE_LIST *tables,
                                                COND *cond) {
  InfoTableImpl infoTable(thd, tables, system_charset_info);

  if (storageHandler) storageHandler->getTransactionSummaryInfo(&infoTable);

  return infoTable.error;
}

ST_FIELD_INFO transactionInfoFieldSummaryInfo[] = {
    //	{"DATABASE",		120, MYSQL_TYPE_STRING,		0, 0, "Database",
    //SKIP_OPEN_TABLE},
    {"COMMITTED", 4, MYSQL_TYPE_LONG, 0, 0, "Committed Transaction.",
     SKIP_OPEN_TABLE},
    {"ROLLED_BACK", 4, MYSQL_TYPE_LONG, 0, 0, "Transactions Rolled Back.",
     SKIP_OPEN_TABLE},
    {"ACTIVE", 4, MYSQL_TYPE_LONG, 0, 0, "Active Transactions",
     SKIP_OPEN_TABLE},
    {"PENDING_COMMIT", 4, MYSQL_TYPE_LONG, 0, 0, "Transaction Pending Commit",
     SKIP_OPEN_TABLE},
    {"PENDING_COMPLETION", 4, MYSQL_TYPE_LONG, 0, 0,
     "Transaction Pending Completion", SKIP_OPEN_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

int NfsPluginHandler::initTransactionSummaryInfo(void *p) {
  DBUG_ENTER("initTransactionSummaryInfo");
  ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
  schema->fields_info = transactionInfoFieldSummaryInfo;
  schema->fill_table = NfsPluginHandler::getTransactionSummaryInfo;

  DBUG_RETURN(0);
}

int NfsPluginHandler::deinitTransactionSummaryInfo(void *p) {
  DBUG_ENTER("deinitTransactionInfo");
  DBUG_RETURN(0);
}

//*****************************************************************************
//
// CHANGJIANG_STREAM_LOG_INFO
//
//*****************************************************************************

int NfsPluginHandler::getStreamLogInfo(THD *thd, TABLE_LIST *tables,
                                       COND *cond) {
  InfoTableImpl infoTable(thd, tables, system_charset_info);

  if (storageHandler) storageHandler->getStreamLogInfo(&infoTable);

  return infoTable.error;
}

ST_FIELD_INFO serialStreamLogFieldInfo[] = {
    //	{"DATABASE",		120, MYSQL_TYPE_STRING,		0, 0, "Database",
    //SKIP_OPEN_TABLE},
    {"TRANSACTIONS", 4, MYSQL_TYPE_LONG, 0, 0, "Transactions", SKIP_OPEN_TABLE},
    {"BLOCKS", 8, MYSQL_TYPE_LONGLONG, 0, 0, "Blocks", SKIP_OPEN_TABLE},
    {"WINDOWS", 4, MYSQL_TYPE_LONG, 0, 0, "Windows", SKIP_OPEN_TABLE},
    {"BUFFERS", 4, MYSQL_TYPE_LONG, 0, 0, "Buffers", SKIP_OPEN_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

int NfsPluginHandler::initStreamLogInfo(void *p) {
  DBUG_ENTER("initStreamLogInfoInfo");
  ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
  schema->fields_info = serialStreamLogFieldInfo;
  schema->fill_table = NfsPluginHandler::getStreamLogInfo;

  DBUG_RETURN(0);
}

int NfsPluginHandler::deinitStreamLogInfo(void *p) {
  DBUG_ENTER("deinitStreamLogInfo");
  DBUG_RETURN(0);
}

//*****************************************************************************
//
// CHANGJIANG_VERSION
//
//*****************************************************************************

int NfsPluginHandler::getChangjiangVersionInfo(THD *thd, TABLE_LIST *tables,
                                               COND *cond) {
  InfoTableImpl infoTable(thd, tables, system_charset_info);

  if (storageHandler) storageHandler->getChangjiangVersionInfo(&infoTable);

  return infoTable.error;
}

ST_FIELD_INFO changjiangVersionFieldInfo[] = {
    {"Version", 32, MYSQL_TYPE_STRING, 0, 0, "Version", SKIP_OPEN_TABLE},
    {"Date", 32, MYSQL_TYPE_STRING, 0, 0, "Date", SKIP_OPEN_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

int NfsPluginHandler::initChangjiangVersionInfo(void *p) {
  DBUG_ENTER("initChangjiangVersionInfo");
  ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
  schema->fields_info = changjiangVersionFieldInfo;
  schema->fill_table = NfsPluginHandler::getChangjiangVersionInfo;

  DBUG_RETURN(0);
}

int NfsPluginHandler::deinitChangjiangVersionInfo(void *p) {
  DBUG_ENTER("deinitChangjiangVersionInfo");
  DBUG_RETURN(0);
}

//*****************************************************************************
//
// CHANGJIANG_SYNCOBJECTS
//
//*****************************************************************************

int NfsPluginHandler::getSyncInfo(THD *thd, TABLE_LIST *tables, COND *cond) {
  InfoTableImpl infoTable(thd, tables, system_charset_info);

  if (storageHandler) storageHandler->getSyncInfo(&infoTable);

  return infoTable.error;
}

ST_FIELD_INFO syncInfoFieldInfo[] = {
    {"CALLER", 120, MYSQL_TYPE_STRING, 0, 0, "Caller", SKIP_OPEN_TABLE},
    {"SHARED", 4, MYSQL_TYPE_LONG, 0, 0, "Shared", SKIP_OPEN_TABLE},
    {"EXCLUSIVE", 4, MYSQL_TYPE_LONG, 0, 0, "Exclusive", SKIP_OPEN_TABLE},
    {"WAITS", 4, MYSQL_TYPE_LONG, 0, 0, "Waits", SKIP_OPEN_TABLE},
    {"QUEUE_LENGTH", 4, MYSQL_TYPE_LONG, 0, 0, "Queue Length", SKIP_OPEN_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

int NfsPluginHandler::initSyncInfo(void *p) {
  DBUG_ENTER("initSyncInfo");
  ST_SCHEMA_TABLE *schema = (ST_SCHEMA_TABLE *)p;
  schema->fields_info = syncInfoFieldInfo;
  schema->fill_table = NfsPluginHandler::getSyncInfo;

  DBUG_RETURN(0);
}

int NfsPluginHandler::deinitSyncInfo(void *p) {
  DBUG_ENTER("deinitSyncInfo");
  DBUG_RETURN(0);
}

static void updateIndexChillThreshold(MYSQL_THD thd, struct SYS_VAR *var,
                                      void *var_ptr, const void *save) {
  changjiang_index_chill_threshold = *(uint *)save;
  if (storageHandler)
    storageHandler->setIndexChillThreshold(changjiang_index_chill_threshold);
}

static void updateRecordChillThreshold(MYSQL_THD thd, struct SYS_VAR *var,
                                       void *var_ptr, const void *save) {
  changjiang_record_chill_threshold = *(uint *)save;
  if (storageHandler)
    storageHandler->setRecordChillThreshold(changjiang_record_chill_threshold);
}

static void updateErrorInject(MYSQL_THD thd, struct SYS_VAR *var, void *var_ptr,
                              const void *save) {
  ERROR_INJECTOR_PARSE(*(const char **)save);
}
void StorageInterface::updateRecordMemoryMax(MYSQL_THD thd, SYS_VAR *variable,
                                             void *var_ptr, const void *save) {
  changjiang_record_memory_max = *(ulonglong *)save;
  storageHandler->setRecordMemoryMax(changjiang_record_memory_max);
}

void StorageInterface::updateRecordScavengeThreshold(MYSQL_THD thd,
                                                     SYS_VAR *variable,
                                                     void *var_ptr,
                                                     const void *save) {
  changjiang_record_scavenge_threshold = *(uint *)save;
  storageHandler->setRecordScavengeThreshold(
      changjiang_record_scavenge_threshold);
}

void StorageInterface::updateRecordScavengeFloor(MYSQL_THD thd,
                                                 SYS_VAR *variable,
                                                 void *var_ptr,
                                                 const void *save) {
  changjiang_record_scavenge_floor = *(uint *)save;
  storageHandler->setRecordScavengeFloor(changjiang_record_scavenge_floor);
}

void StorageInterface::updateDebugMask(MYSQL_THD thd, SYS_VAR *variable,
                                       void *var_ptr, const void *save) {
  changjiang_debug_mask = *(uint *)save;
  changjiang_debug_mask &= ~(LogMysqlInfo | LogMysqlWarning | LogMysqlError);
  storageHandler->deleteNfsLogger(StorageInterface::logger, NULL);
  storageHandler->addNfsLogger(changjiang_debug_mask, StorageInterface::logger,
                               NULL);
}

int StorageInterface::recover(handlerton *hton, XID *xids, uint length) {
  DBUG_ENTER("StorageInterface::recover");

  uint count = 0;
  unsigned char xid[sizeof(XID)];

  memset(xid, 0, sizeof(XID));

  while (storageHandler->recoverGetNextLimbo(sizeof(XID), xid)) {
    count++;
    memcpy(xids++, xid, sizeof(XID));

    if (count >= length) break;
  }

  DBUG_RETURN(count);
}

// Build a record field map for use by encode/decodeRecord()

void StorageInterface::mapFields(TABLE *srvTable) {
  if (!srvTable) return;

  maxFields = storageShare->format->maxId;
  unmapFields();
  fieldMap = new Field *[maxFields];
  memset(fieldMap, 0, sizeof(fieldMap[0]) * maxFields);
  char nameBuffer[256];

  for (uint n = 0; n < srvTable->s->fields; ++n) {
    Field *field = srvTable->field[n];
    storageShare->cleanupFieldName(field->field_name, nameBuffer,
                                   sizeof(nameBuffer), false);
    int id = storageShare->getFieldId(nameBuffer);

    if (id >= 0) fieldMap[id] = field;
  }
}

void StorageInterface::unmapFields(void) {
  if (fieldMap) {
    delete[] fieldMap;
    fieldMap = NULL;
  }
}

static MYSQL_SYSVAR_STR(stream_log_dir, changjiang_stream_log_dir,
                        PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY |
                            PLUGIN_VAR_MEMALLOC,
                        "Changjiang stream log file directory.", NULL, NULL,
                        mysql_real_data_home);

static MYSQL_SYSVAR_STR(checkpoint_schedule, changjiang_checkpoint_schedule,
                        PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY |
                            PLUGIN_VAR_MEMALLOC,
                        "Changjiang checkpoint schedule.", NULL, NULL,
                        "7 * * * * *");

static MYSQL_SYSVAR_STR(error_inject, changjiang_error_inject,
                        PLUGIN_VAR_MEMALLOC,
                        "Used for testing purposes (error injection)", NULL,
                        &updateErrorInject, "");

static MYSQL_SYSVAR_STR(scavenge_schedule, changjiang_scavenge_schedule,
                        PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY |
                            PLUGIN_VAR_MEMALLOC,
                        "Changjiang record scavenge schedule.", NULL, NULL,
                        "15,45 * * * * *");

// #define MYSQL_SYSVAR_UINT(name, varname, opt, comment, check, update, def,
// min, max, blk)

#define PARAMETER_UINT(_name, _text, _min, _default, _max, _flags,    \
                       _update_function)                              \
  static MYSQL_SYSVAR_UINT(_name, changjiang_##_name,                 \
                           PLUGIN_VAR_RQCMDARG | _flags, _text, NULL, \
                           _update_function, _default, _min, _max, 0);

#define PARAMETER_BOOL(_name, _text, _default, _flags, _update_function) \
  static MYSQL_SYSVAR_BOOL(_name, changjiang_##_name,                    \
                           PLUGIN_VAR_RQCMDARG | _flags, _text, NULL,    \
                           _update_function, _default);

#include "StorageParameters.h"
#undef PARAMETER_UINT
#undef PARAMETER_BOOL

static MYSQL_SYSVAR_ULONGLONG(record_memory_max, changjiang_record_memory_max,
                              PLUGIN_VAR_RQCMDARG,  // | PLUGIN_VAR_READONLY,
                              "The maximum size of the record memory cache.",
                              NULL, StorageInterface::updateRecordMemoryMax,
                              LL(250) << 20, 0, (ulonglong)max_memory_address,
                              LL(1) << 20);

static MYSQL_SYSVAR_ULONGLONG(stream_log_file_size,
                              changjiang_stream_log_file_size,
                              PLUGIN_VAR_RQCMDARG,
                              "If stream log file grows larger than this "
                              "value, it will be truncated when it is reused",
                              NULL, NULL, LL(10) << 20, LL(1) << 20,
                              LL(0x7fffffffffffffff), LL(1) << 20);

static MYSQL_SYSVAR_BOOL(engine_force, changjiang_sysvar_engine_force,
                         PLUGIN_VAR_READONLY,
                         "Always set table engine to changjiang", NULL, NULL,
                         false);

static MYSQL_SYSVAR_BOOL(
    file_per_table, changjiang_file_per_table, PLUGIN_VAR_NOCMDARG,
    "Stores each CHANGJIANG table to an .cjd file in the database dir.",
    nullptr, nullptr, false);

/***
static MYSQL_SYSVAR_UINT(allocation_extent, changjiang_allocation_extent,
  PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_READONLY,
  "The percentage of the current size of changjiang_user.fts to use as the size
of the next extension to the file.", NULL, NULL, 10, 0, 100, 1);
***/

static MYSQL_SYSVAR_ULONGLONG(
    page_cache_size, changjiang_page_cache_size,
    PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
    "The amount of memory to be used for the database page cache.", NULL, NULL,
    LL(4) << 20, LL(2) << 20, (ulonglong)max_memory_address, LL(1) << 20);

static MYSQL_THDVAR_BOOL(consistent_read, PLUGIN_VAR_OPCMDARG,
                         "Enable Consistent Read Mode for Repeatable Reads",
                         NULL, NULL, 1);

static int getTransactionIsolation(THD *thd) {
  int level = isolation_levels[thd_tx_isolation(thd)];

  // TRANSACTION_CONSISTENT_READ  mapped to TRANSACTION_WRITE_COMMITTED,
  // if changjiang_consistent_read is not set
  if (level == TRANSACTION_CONSISTENT_READ && !THDVAR(thd, consistent_read))
    return TRANSACTION_WRITE_COMMITTED;

  return level;
}

static struct SYS_VAR *changjiangVariables[] = {

#define PARAMETER_UINT(_name, _text, _min, _default, _max, _flags, \
                       _update_function)                           \
  MYSQL_SYSVAR(_name),
#define PARAMETER_BOOL(_name, _text, _default, _flags, _update_function) \
  MYSQL_SYSVAR(_name),

#include "StorageParameters.h"
#undef PARAMETER_UINT
#undef PARAMETER_BOOL

    MYSQL_SYSVAR(stream_log_dir), MYSQL_SYSVAR(checkpoint_schedule),
    MYSQL_SYSVAR(scavenge_schedule),
    // MYSQL_SYSVAR(debug_mask),
    MYSQL_SYSVAR(record_memory_max),
    // MYSQL_SYSVAR(allocation_extent),
    MYSQL_SYSVAR(page_cache_size), MYSQL_SYSVAR(consistent_read),
    MYSQL_SYSVAR(stream_log_file_size), MYSQL_SYSVAR(engine_force),
    MYSQL_SYSVAR(file_per_table), MYSQL_SYSVAR(error_inject), NULL};

static st_mysql_storage_engine changjiang_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION};
static st_mysql_information_schema changjiang_system_memory_detail = {
    MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema changjiang_system_memory_summary = {
    MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema changjiang_record_cache_detail = {
    MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema changjiang_record_cache_summary = {
    MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema changjiang_tablespace_io = {
    MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema changjiang_transactions = {
    MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema changjiang_transaction_summary = {
    MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema changjiang_syncobjects = {
    MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema changjiang_stream_log_info = {
    MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};
static st_mysql_information_schema changjiang_version = {
    MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};

}  // namespace Changjiang

mysql_declare_plugin(changjiang){
    MYSQL_STORAGE_ENGINE_PLUGIN,
    &Changjiang::changjiang_storage_engine,
    Changjiang::changjiang_hton_name,
    Changjiang::changjiang_plugin_author,
    "LoongBase-OLTP storage engine (MVCC, row locking)",  //"Changjiang storage
                                                          //engine",
    PLUGIN_LICENSE_GPL,
    Changjiang::StorageInterface::changjiang_init, /* plugin init */
    nullptr,
    Changjiang::StorageInterface::changjiang_deinit, /* plugin deinit */
    0x0100,                                          /* 1.0 */
    Changjiang::changjiangStatus,                    /* status variables */
    Changjiang::changjiangVariables,                 /* system variables */
    NULL,                                            /* config options */
    0},

    {MYSQL_INFORMATION_SCHEMA_PLUGIN,
     &Changjiang::changjiang_system_memory_detail,
     "CHANGJIANG_SYSTEM_MEMORY_DETAIL",
     Changjiang::changjiang_plugin_author,
     "Changjiang System Memory Detail.",
     PLUGIN_LICENSE_GPL,
     Changjiang::NfsPluginHandler::initSystemMemoryDetailInfo, /* plugin init */
     nullptr,
     Changjiang::NfsPluginHandler::deinitSystemMemoryDetailInfo, /* plugin
                                                                    deinit */
     0x0005,
     NULL, /* status variables */
     NULL, /* system variables */
     NULL, /* config options */
     0},

    {MYSQL_INFORMATION_SCHEMA_PLUGIN,
     &Changjiang::changjiang_system_memory_summary,
     "CHANGJIANG_SYSTEM_MEMORY_SUMMARY",
     Changjiang::changjiang_plugin_author,
     "Changjiang System Memory Summary.",
     PLUGIN_LICENSE_GPL,
     Changjiang::NfsPluginHandler::initSystemMemorySummaryInfo, /* plugin init
                                                                 */
     nullptr,
     Changjiang::NfsPluginHandler::deinitSystemMemorySummaryInfo, /* plugin
                                                                     deinit */
     0x0005,
     NULL, /* status variables */
     NULL, /* system variables */
     NULL, /* config options */
     0},

    {MYSQL_INFORMATION_SCHEMA_PLUGIN,
     &Changjiang::changjiang_record_cache_detail,
     "CHANGJIANG_RECORD_CACHE_DETAIL",
     Changjiang::changjiang_plugin_author,
     "Changjiang Record Cache Detail.",
     PLUGIN_LICENSE_GPL,
     Changjiang::NfsPluginHandler::initRecordCacheDetailInfo, /* plugin init */
     nullptr,
     Changjiang::NfsPluginHandler::deinitRecordCacheDetailInfo, /* plugin deinit
                                                                 */
     0x0005,
     NULL, /* status variables */
     NULL, /* system variables */
     NULL, /* config options */
     0},

    {MYSQL_INFORMATION_SCHEMA_PLUGIN,
     &Changjiang::changjiang_record_cache_summary,
     "CHANGJIANG_RECORD_CACHE_SUMMARY",
     Changjiang::changjiang_plugin_author,
     "Changjiang Record Cache Summary.",
     PLUGIN_LICENSE_GPL,
     Changjiang::NfsPluginHandler::initRecordCacheSummaryInfo, /* plugin init */
     nullptr,
     Changjiang::NfsPluginHandler::deinitRecordCacheSummaryInfo, /* plugin
                                                                    deinit */
     0x0005,
     NULL, /* status variables */
     NULL, /* system variables */
     NULL, /* config options   */
     0},

    {MYSQL_INFORMATION_SCHEMA_PLUGIN,
     &Changjiang::changjiang_transactions,
     "CHANGJIANG_TRANSACTIONS",
     Changjiang::changjiang_plugin_author,
     "Changjiang Transactions.",
     PLUGIN_LICENSE_GPL,
     Changjiang::NfsPluginHandler::initTransactionInfo, /* plugin init */
     nullptr,
     Changjiang::NfsPluginHandler::deinitTransactionInfo, /* plugin deinit */
     0x0005,
     NULL, /* status variables */
     NULL, /* system variables */
     NULL, /* config options   */
     0},

    {MYSQL_INFORMATION_SCHEMA_PLUGIN,
     &Changjiang::changjiang_transaction_summary,
     "CHANGJIANG_TRANSACTION_SUMMARY",
     Changjiang::changjiang_plugin_author,
     "Changjiang Transaction Summary.",
     PLUGIN_LICENSE_GPL,
     Changjiang::NfsPluginHandler::initTransactionSummaryInfo, /* plugin init */
     nullptr,
     Changjiang::NfsPluginHandler::deinitTransactionSummaryInfo, /* plugin
                                                                    deinit */
     0x0005,
     NULL, /* status variables */
     NULL, /* system variables */
     NULL, /* config options   */
     0},

    {MYSQL_INFORMATION_SCHEMA_PLUGIN,
     &Changjiang::changjiang_syncobjects,
     "CHANGJIANG_SYNCOBJECTS",
     Changjiang::changjiang_plugin_author,
     "Changjiang SyncObjects.",
     PLUGIN_LICENSE_GPL,
     Changjiang::NfsPluginHandler::initSyncInfo, /* plugin init */
     nullptr,
     Changjiang::NfsPluginHandler::deinitSyncInfo, /* plugin deinit */
     0x0005,
     NULL, /* status variables */
     NULL, /* system variables */
     NULL, /* config options   */
     0},

    {MYSQL_INFORMATION_SCHEMA_PLUGIN,
     &Changjiang::changjiang_stream_log_info,
     "CHANGJIANG_STREAM_LOG_INFO",
     Changjiang::changjiang_plugin_author,
     "Changjiang Stream Log Information.",
     PLUGIN_LICENSE_GPL,
     Changjiang::NfsPluginHandler::initStreamLogInfo, /* plugin init */
     nullptr,
     Changjiang::NfsPluginHandler::deinitStreamLogInfo, /* plugin deinit */
     0x0005,
     NULL, /* status variables */
     NULL, /* system variables */
     NULL, /* config options   */
     0},

    {MYSQL_INFORMATION_SCHEMA_PLUGIN,
     &Changjiang::changjiang_tablespace_io,
     "CHANGJIANG_TABLESPACE_IO",
     Changjiang::changjiang_plugin_author,
     "Changjiang Tablespace IO.",
     PLUGIN_LICENSE_GPL,
     Changjiang::NfsPluginHandler::initTableSpaceIOInfo, /* plugin init */
     nullptr,
     Changjiang::NfsPluginHandler::deinitTableSpaceIOInfo, /* plugin deinit */
     0x0005,
     NULL, /* status variables */
     NULL, /* system variables */
     NULL, /* config options   */
     0},

    {MYSQL_INFORMATION_SCHEMA_PLUGIN,
     &Changjiang::changjiang_version,
     "CHANGJIANG_VERSION",
     Changjiang::changjiang_plugin_author,
     "Changjiang Database Version Number.",
     PLUGIN_LICENSE_GPL,
     Changjiang::NfsPluginHandler::initChangjiangVersionInfo, /* plugin init */
     nullptr,
     Changjiang::NfsPluginHandler::deinitChangjiangVersionInfo, /* plugin deinit
                                                                 */
     0x0005,
     NULL, /* status variables */
     NULL, /* system variables */
     NULL, /* config options   */
     0}

mysql_declare_plugin_end;
