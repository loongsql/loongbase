/* Copyright (c) 2013, 2022, Oracle and/or its affiliates.

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

/* See http://code.google.com/p/googletest/wiki/Primer */

#include <gtest/gtest.h>

#include "common/mysql_priv.h"
#include "include/thr_lock.h"
#include "util/JString.h"
#include "core/StorageHandler.h"
#include "core/SQLException.h"
#include "core/Log.h"
#include "handler/ha_falcon.h"

#include "unittest/gunit/temptable/table_helper.h"
#include "unittest/gunit/test_utils.h"

#include "cj_mock_field_long.h"
#include "cj_fake_table.h"
// We choose non-zero to avoid it working by coincidence.
int CJ_Fake_TABLE::highest_table_id = 5;

#include <thread>

//StorageParameters.h
bool changjiang_checksums = true;
uint changjiang_debug_mask = 0;
bool changjiang_debug_server = false;
uint changjiang_debug_trace = 0;
uint changjiang_direct_io = 1;
uint changjiang_gopher_threads = 5;
uint changjiang_index_chill_threshold = 4*1024*1024;
uint changjiang_io_threads = 2;
uint changjiang_large_blob_threshold = 160000;
uint changjiang_lock_wait_timeout = 50;
uint changjiang_page_size = 2048;
uint changjiang_record_chill_threshold = 5*1024*1024;
uint changjiang_record_scavenge_floor = 80;
uint changjiang_record_scavenge_threshold = 90;
uint changjiang_serial_log_block_size = 0;
uint changjiang_serial_log_buffers = 20;
uint changjiang_serial_log_priority = 1;
bool changjiang_use_deferred_index_hash = 0;
bool changjiang_support_xa = 0;
bool changjiang_use_supernodes = 1;
bool changjiang_use_sectorcache = 0;


#include <chrono>
class StopWatch {
  typedef std::chrono::high_resolution_clock cs_clock_t;
public:
  StopWatch() {}
  virtual ~StopWatch() {}

  void Start() {
	  m_start_time = cs_clock_t::now();
	  is_start = true;
  }
  void Stop() {
	  if (is_start) {
	    m_time_elapsed += cs_clock_t::now() - m_start_time;
	  }
	  is_start = false;
  }
  //milisecond
  uint64_t GetTime() {
	  auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(m_time_elapsed);
	  return milliseconds.count();
  }

  void Reset() {
	  m_time_elapsed = cs_clock_t::duration { 0 };
  }
private:

  bool is_start = false;
  cs_clock_t::time_point m_start_time;
  cs_clock_t::duration m_time_elapsed = cs_clock_t::duration { 0 };
};

/**
 * make -j4 storage_handler-t
 */

namespace changjiang_example_unittest {

using my_testing::Server_initializer;
using namespace temptable_test;

class StorageHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    init_handlerton();
    initializer.SetUp();
  }
  void TearDown() override {
    StorageInterface::changjiang_deinit(nullptr);
    initializer.TearDown();
    delete remove_hton2plugin(m_changjiang_handlerton.slot);
  }
  Server_initializer initializer;
 public:
  THD *thd() { return initializer.thd(); }

  handlerton *hton() { return &m_changjiang_handlerton; }

 private:
  handlerton m_changjiang_handlerton;

  void init_handlerton() {
    strcpy(mysql_real_data_home, "/data/ldy/soft/loongbase/data/30007/");
    //Changing the current directory
    chdir(mysql_real_data_home);
    m_changjiang_handlerton = handlerton();
    // comment : //ERROR_INJECTOR_PARSE(changjiang_error_inject);
    StorageInterface::changjiang_init(&m_changjiang_handlerton);

    insert_hton2plugin(m_changjiang_handlerton.slot, new st_plugin_int());
  }
};

#if 0
TEST(foo, willsucceed) {
  EXPECT_EQ(5, 5);
  EXPECT_TRUE(true);
}

TEST(foo, willfail) {
  EXPECT_EQ(5, 6);
  EXPECT_TRUE(123 == 456);
}

TEST(foo, StorageHandler) {
  const char *directory = "/data/ldy2/soft/loongbase/data/30007";
  StorageHandler::setDataDirectory(directory);

  StorageHandler* storageHandler = getFalconStorageHandler(sizeof(THR_LOCK));
  EXPECT_EQ(true, storageHandler != nullptr);

  my_bool error = false;
  uint changjiang_debug_mask = 0;
  changjiang_debug_mask&= ~(LogMysqlInfo|LogMysqlWarning|LogMysqlError);
  storageHandler->addNfsLogger(changjiang_debug_mask, StorageInterface::logger, NULL);

  storageHandler->startNfsServer();
  try {
    storageHandler->initialize();
  } catch(SQLException &e) {
    //sql_print_error("Falcon: %s", e.getText());
    error = true;
  } catch(...) {
    //sql_print_error("Falcon: General exception in initialization");
    error = true;
  }

  freeFalconStorageHandler();
}

TEST_F(StorageHandlerTest, StorageInterface) {


  const char *table_name = "t1";
  Table_helper table_helper(table_name, thd());
  table_helper.add_field_long("c1", false);
  table_helper.finalize();

  const char *fakepath = "ldy5";
  table_helper.table_share()->path.str = const_cast<char *>(fakepath);
  table_helper.table_share()->path.length = std::strlen(fakepath);

  StorageInterface handler(hton(), table_helper.table_share());
  table_helper.set_handler(&handler);

  /*EXPECT_EQ(handler.create(table_name, table_helper.table(), nullptr, nullptr),
            0);*/
  EXPECT_EQ(handler.open("./ldy5/t1", 0, 0, nullptr), 0);


  //set values
  table_helper.field<Field_long>(0)->store(99, false);
  EXPECT_EQ(handler.write_row(table_helper.table()->record[0]), 0);
  EXPECT_EQ(StorageInterface::commit(hton(), thd(), true), 0);

  EXPECT_EQ(handler.close(), 0);
  EXPECT_EQ(handler.delete_table("./ldy5/t1", nullptr), 0);

}

TEST_F(StorageHandlerTest, WriteRow) {
  CJ_Mock_field_long field_long("c1");
  CJ_Fake_TABLE *table = reinterpret_cast<CJ_Fake_TABLE *>(field_long.table);
  StorageInterface cj_handler(hton(), table->get_share());
  table->set_handler(&cj_handler);
  cj_handler.change_table_ptr(table, table->s);

  EXPECT_EQ(cj_handler.open("./ldy5/t1", 0, 0, nullptr), 0);

  //set values
  field_long.store(299, false);
  field_long.set_notnull(0);
  EXPECT_EQ(field_long.val_int(), 299);
  EXPECT_EQ(cj_handler.write_row(table->record[0]), 0);
  field_long.store(399, false);
  field_long.set_notnull(0);
  EXPECT_EQ(cj_handler.write_row(table->record[0]), 0);
  EXPECT_EQ(StorageInterface::commit(hton(), thd(), true), 0);

  EXPECT_EQ(cj_handler.close(), 0);

}
#endif

static void WriteManyRows(StorageHandlerTest* testor, const char* db_name, const char* tb_name) {
  // Init thread
  const bool error = my_thread_init();
  ASSERT_FALSE(error);
  // Init THD
  Server_initializer initializer;
  initializer.SetUp();

  CJ_Mock_field_long field_long("c1");
  CJ_Fake_TABLE *table = reinterpret_cast<CJ_Fake_TABLE *>(field_long.table);
  table->SetName(db_name, tb_name);
  StorageInterface cj_handler(testor->hton(), table->get_share());
  table->set_handler(&cj_handler);
  cj_handler.change_table_ptr(table, table->s);

  std::string tb_path = std::string("./") + db_name + std::string("/") + tb_name;

  EXPECT_EQ(cj_handler.open(tb_path.c_str(), 0, 0, nullptr), 0);

  //set values
  for (longlong nr = 0; nr < 100000; nr++) {
    field_long.store(nr, false);
    field_long.set_notnull(0);
    EXPECT_EQ(cj_handler.external_lock(initializer.thd(), F_WRLCK), 0);
    EXPECT_EQ(cj_handler.write_row(table->record[0]), 0);
    EXPECT_EQ(StorageInterface::commit(testor->hton(), initializer.thd(), true), 0);
    EXPECT_EQ(cj_handler.external_lock(initializer.thd(), F_UNLCK), 0);
  }
  EXPECT_EQ(cj_handler.close(), 0);

  initializer.TearDown();
  my_thread_end();
}

/**
create database ldy5;
use ldy5;
create table t1(c1 int)engine=changjiang;
create table t2(c1 int)engine=changjiang;
create table t3(c1 int)engine=changjiang;
 */
/** Need optimization
ldy@ldy-virtual-machine:~/project/mobiusdb/mysql-8.0.30/bld-release$ ./runtime_output_directory/storage_handler-t
[==========] Running 2 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 2 tests from StorageHandlerTest
[ RUN      ] StorageHandlerTest.WriteManyRows
[       OK ] StorageHandlerTest.WriteManyRows (1250 ms)
[ RUN      ] StorageHandlerTest.ParallelWriteManyRows
[       OK ] StorageHandlerTest.ParallelWriteManyRows (25612 ms)
[----------] 2 tests from StorageHandlerTest (26863 ms total)
 */
TEST_F(StorageHandlerTest, WriteManyRows) {
  StopWatch mywatch;
  mywatch.Start();
  //WriteManyRows(this, "ldy5", "t1");
  std::thread t(WriteManyRows, this, "ldy5", "t1");
  t.join();
  mywatch.Stop();
  std::cout << "    WriteManyRows : " << mywatch.GetTime() << std::endl;
}

TEST_F(StorageHandlerTest, ParallelWriteManyRows) {
  StopWatch mywatch;
  mywatch.Start();
  std::thread t(WriteManyRows, this, "ldy5", "t2");
  std::thread t2(WriteManyRows, this, "ldy5", "t3");
  t.join();
  t2.join();
  mywatch.Stop();
  std::cout << "    ParallelWriteManyRows : " << mywatch.GetTime() << std::endl;
}


}  // namespace innodb_example_unittest
