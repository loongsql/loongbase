/* Copyright (C) 2006-2008 MySQL AB, 2008-2009 Sun Microsystems, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

// Database.cpp: implementation of the Database class.
//
//////////////////////////////////////////////////////////////////////

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <memory.h>
#include <errno.h>
#include <limits.h>
#include "Engine.h"
#include "Database.h"
#include "Dbb.h"
#include "PStatement.h"
#include "CompiledStatement.h"
#include "Table.h"
#include "UnTable.h"
#include "Index.h"
#include "IndexKey.h"
#include "Connection.h"
#include "Transaction.h"
#include "Stream.h"
#include "SQLError.h"
#include "RSet.h"
#include "Search.h"
#include "ImageManager.h"
#include "SequenceManager.h"
#include "Inversion.h"
#include "Sync.h"
#include "Threads.h"
#include "Thread.h"
#include "Scheduler.h"
#include "Scavenger.h"
#include "RoleModel.h"
#include "User.h"
#include "Registry.h"
#include "SymbolManager.h"
#include "FilterSetManager.h"
#include "Log.h"
#include "DateTime.h"
#include "Interlock.h"
#include "Cache.h"
#include "NetfraVersion.h"
#include "OpSys.h"
#include "SearchWords.h"
#include "Repository.h"
#include "RepositoryManager.h"
#include "Schema.h"
#include "Configuration.h"
#include "StreamLog.h"
#include "StreamLogControl.h"
#include "PageWriter.h"
#include "Trigger.h"
#include "TransactionManager.h"
#include "TableSpaceManager.h"
#include "TableSpace.h"
#include "InfoTable.h"
#include "MemoryManager.h"
#include "MemMgr.h"
#include "MemControl.h"
#include "RecordScavenge.h"
#include "RecordSection.h"
#include "LogStream.h"
#include "SyncTest.h"
#include "SyncHandler.h"
#include "PriorityScheduler.h"
#include "Sequence.h"
#include "BackLog.h"
#include "Bitmap.h"
#include "CycleManager.h"
#include "CycleLock.h"

namespace Changjiang {

#ifdef _WIN32
#define PATH_MAX 1024
#endif

#ifndef STORAGE_ENGINE
#include "Applications.h"
#include "Application.h"
#include "JavaVM.h"
#include "Java.h"
#include "Template.h"
#include "Templates.h"
#include "TemplateContext.h"
#include "TemplateManager.h"
#include "Session.h"
#include "SessionManager.h"
#include "DataResourceLocator.h"
#else
extern unsigned int changjiang_page_size;
#endif

#ifdef LICENSE
#include "LicenseManager.h"
#include "LicenseProduct.h"
#endif

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

#define HASH(address, size) (int)(((UIPTR)address >> 2) % size)
#define EXPIRATION_DAYS 180

#define STATEMENT_RETIREMENT_AGE 60
#define RECORD_RETIREMENT_AGE 60
#define MAX_LOW_MEMORY 10

extern uint changjiang_debug_trace;

static const char *createTables =
    "create table Tables (\
			tableName varchar (128) not null,\
			schema varchar (128) not null,\
			type varchar (16),\
			dataSection int,\
			blobSection int,\
			tableId int,\
			currentVersion int,\
			remarks text,\
			viewDefinition clob,\
			primary key (tableName, schema));";

static const char *createOds3aTables =
    "upgrade table Tables (\
			tableName varchar (128) not null,\
			schema varchar (128) not null,\
			type varchar (16),\
			dataSection int,\
			blobSection int,\
			tableId int,\
			currentVersion int,\
			remarks text,\
			viewDefinition clob,\
			cardinality bigint,\
			primary key (tableName, schema));";

static const char *createOds3bTables =
    "upgrade table Tables (\
			tableName varchar (128) not null,\
			schema varchar (128) not null,\
			type varchar (16),\
			dataSection int,\
			blobSection int,\
			tableId int,\
			currentVersion int,\
			remarks text,\
			viewDefinition clob,\
			cardinality bigint,\
			tablespace varchar(128),\
			primary key (tableName, schema));";

static const char *createOds2Fields =
    "create table Fields (\
			field varchar (128) not null,\
			tableName varchar (128) not null,\
			schema varchar (128) not null,"
    //"domainName varchar (128),"
    "collationsequence varchar (128),"
    "fieldId int,\
			dataType int,\
			length int,\
			scale int,\
			flags int,\
			remarks text,\
			primary key (schema, tableName, field));";

static const char *createOds3Fields =
    "upgrade table Fields ("
    "field varchar (128) not null,"
    "tableName varchar (128) not null,"
    "schema varchar (128) not null,"
    "domainName varchar (128),"
    "repositoryName varchar (128),"
    "collationsequence varchar (128),"
    "fieldId int,"
    "dataType int,"
    "length int,"
    "scale int,"
    "precision int,"
    "flags int,"
    "remarks text,"
    "primary key (schema, tableName, field));";

static const char *createFieldDomainName =
    "upgrade index FieldDomainName on Fields (domainName);";

static const char *createFieldCollationSequenceName =
    "create index FieldCollationSequenceName on Fields (collationsequence);";

static const char *createFormats =
    "create table Formats (\
			tableId int not null,\
			fieldId int not null,\
			version int not null,\
			dataType int,\
			offset int,\
			length int,\
			scale int,\
			maxId int,\
			primary key (tableId, version, fieldId));";

static const char *createIndexes =
    "create table Indexes (\
			indexName varchar (128) not null,\
			tableName varchar (128) not null,\
			schema varchar (128) not null,\
			indexType int,\
			fieldCount int,\
			indexId int,\
			primary key (schema, indexName));";

static const char *createOd3IndexIndex =
    "create index index_table on indexes (schema, tableName)";

static const char *createIndexFields =
    "create table IndexFields (\
			indexName varchar (128) not null,\
			schema varchar (128) not null,\
			tableName varchar (128) not null,\
			field varchar (128) not null,\
			position int,\
			primary key (schema, indexName, field));";

static const char *createOd3IndexFieldsIndex =
    "create index indexfields_table on IndexFields (schema, tableName)";

static const char *createOds3IndexFields =
    "upgrade table IndexFields (\
			indexName varchar (128) not null,\
			schema varchar (128) not null,\
			tableName varchar (128) not null,\
			field varchar (128) not null,\
			position int,\
			partial int,\
			primary key (schema, indexName, field));";

static const char *createOds3aIndexFields =
    "upgrade table IndexFields (\
			indexName varchar (128) not null,\
			schema varchar (128) not null,\
			tableName varchar (128) not null,\
			field varchar (128) not null,\
			position int,\
			partial int,\
			records_per_value int,\
			primary key (schema, indexName, field));";

static const char *createForeignKeys =
    "create table ForeignKeys (\
			primaryTableId int not null,\
			primaryFieldId int not null,\
			foreignTableId int not null,\
			foreignFieldId int not null,\
			numberKeys int,\
			position int,\
			updateRule smallint,\
			deleteRule smallint,\
			deferrability smallint,\
			primary key (foreignTableId, primaryTableId, foreignFieldId));";

static const char *createForeignKeysIndex =
    "create index ForeignKeysIndex on ForeignKeys\
			(primaryTableId);";

static const char *createDomains =
    "create table Domains ("
    "domainName varchar (128) not null,"
    "schema varchar (128) not null,"
    "dataType int,"
    "length int,"
    "scale int,"
    "remarks text,"
    "primary key (domainName, schema));";

// static const char *createOds3Domains =
//	"create table Domains ("
//			"domainName varchar (128) not null,"
//			"schema varchar (128) not null,"
//			"dataType int,"
//			"length int,"
//			"scale int,"
//			"remarks text,"
//			"primary key (domainName, schema));";

static const char *createView_tables =
    "upgrade table view_tables ("
    "viewName varchar (128) not null,"
    "viewSchema varchar (128) not null,"
    "sequence int not null,"
    "tableName varchar (128) not null,"
    "schema varchar (128) not null,"
    "primary key (viewSchema,viewName,sequence))";

static const char *createRepositories =
    "upgrade table repositories ("
    "repositoryName varchar (128) not null,"
    "schema varchar (128) not null,"
    "sequenceName varchar (128),"
    "filename varchar (128),"
    "rollovers varchar (128),"
    "currentVolume int,"
    "primary key (repositoryName, schema))";

static const char *createSchemas =
    "upgrade table schemas ("
    "schema varchar (128) not null primary key,"
    "sequence_interval int,"
    "system_id int)";

static const char *createTableSpaces =
    "upgrade table tablespaces ("
    "tablespace varchar(128) not null primary key,"
    "tablespace_id int not null,"
    "filename varchar(512) not null,"
    "type int,"
    "comment text)";

static const char *createTableSpaceSequence = "upgrade sequence tablespace_ids";

static const char *ods2Statements[] = {createTables,
                                       createOds2Fields,
                                       createFieldCollationSequenceName,
                                       createFormats,
                                       createIndexes,
                                       createIndexFields,
                                       createForeignKeys,
                                       createForeignKeysIndex,
                                       createDomains,

                                       "grant select on tables to public",
                                       "grant select on fields to public",
                                       "grant select on formats to public",
                                       "grant select on indexes to public",
                                       "grant select on indexFields to public",
                                       "grant select on foreignKeys to public",
                                       "grant select on Domains to public",

                                       NULL};

static const char *ods2UpgradeStatements[] = {
    createView_tables,
    createRepositories,
    createSchemas,
    "grant select on repositories to public",
    "grant select on schemas to public",

    NULL};

static const char *ods3Upgrade[] = {
    createOds3Fields,    createFieldDomainName,     createOds3IndexFields,
    createOd3IndexIndex, createOd3IndexFieldsIndex, NULL};

static const char *ods3aUpgrade[] = {createOds3aTables, createOds3aIndexFields,
                                     NULL};

static const char *ods3bUpgrade[] = {
    createOds3bTables, createTableSpaces, createTableSpaceSequence,
    "grant select on tablespaces to public", NULL};

static const char *changedTables[] = {"TABLES", "FIELDS", "INDEXFIELDS",
                                      //"INDEXES",
                                      //"FORMATS",
                                      //"FOREIGNKEYS",
                                      //"DOMAINS",
                                      //"REPOSITORIES",
                                      NULL};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Database::Database(const char *dbName, Configuration *config, Threads *parent)
    : syncCardinality("Database::syncCardinality"),
      syncMemory("Database::syncMemory") {
  panicShutdown = false;
  name = dbName;
  configuration = config;
  useCount = 1;
  nextTableId = 0;
  compiledStatements = NULL;
  memset(tables, 0, sizeof(tables));
  memset(tablesModId, 0, sizeof(tablesModId));
  memset(unTables, 0, sizeof(unTables));
  memset(schemas, 0, sizeof(schemas));
  currentGeneration = 1;
  tableList = NULL;
  recordMemoryMax = configuration->recordMemoryMax;
  recordScavengeFloor = configuration->recordScavengeFloor;
  recordScavengeThreshold = configuration->recordScavengeThreshold;
  recordScavengeMaxGroupSize = recordMemoryMax / AGE_GROUPS_IN_CACHE;
  recordPoolAllocCount = 0;
  lastGenerationMemory = 0;
  lastActiveMemoryChecked = 0;
  utf8 = false;
  stepNumber = 0;
  shuttingDown = false;
  lowMemory = false;
  pendingIOErrors = 0;
  noSchedule = 0;
  // noSchedule = 1;

  fieldExtensions = false;
  cache = NULL;
  dbb = new Dbb(this);
  numberQueries = 0;
  numberRecords = 0;
  numberTemplateEvals = 0;
  numberTemplateExpands = 0;
  threads = new Threads(parent);
  startTime = time(NULL);

  symbolManager = NULL;
  templateManager = NULL;
  imageManager = NULL;
  roleModel = NULL;
  licenseManager = NULL;
  transactionManager = NULL;
  applications = NULL;
  systemConnection = NULL;
  searchWords = NULL;
  java = NULL;
  scheduler = NULL;
  internalScheduler = NULL;
  scavenger = NULL;
#ifndef STORAGE_ENGINE
  garbageCollector = NULL;
#endif
  sequenceManager = NULL;
  repositoryManager = NULL;
  sessionManager = NULL;
  filterSetManager = NULL;
  tableSpaceManager = NULL;
  cycleManager = NULL;
  timestamp = time(NULL);
  tickerThread = NULL;
  cardinalityThread = NULL;
  cardinalityThreadSleeping = 0;
  cardinalityThreadSignaled = 0;
  scavengerThread = NULL;
  scavengerThreadSleeping = 0;
  scavengerThreadSignaled = 0;
  scavengeForced = 0;
  scavengeCount = 0;
  streamLog = NULL;
  pageWriter = NULL;
  zombieTables = NULL;
  updateCardinality = NULL;
  backLog = NULL;
  syncHandler = getChangjiangSyncHandler();
  ioScheduler = new PriorityScheduler;
  lastScavenge = 0;
  scavengeCycle = 0;
  streamLogBlockSize = configuration->streamLogBlockSize;
  longSync = false;
  recordMemoryControl = MemMgrGetControl(MemMgrControlRecord);
  recordPool = MemMgrGetFixedPool(MemMgrRecord);
  recordVersionPool = MemMgrGetFixedPool(MemMgrRecordVersion);
  recordDataPool = MemMgrGetFixedPool(MemMgrRecordData);
  syncObject.setName("Database::syncObject");
  syncTables.setName("Database::syncTables");
  syncStatements.setName("Database::syncStatements");
  syncAddStatement.setName("Database::syncAddStatement");
  syncResultSets.setName("Database::syncResultSets");
  syncConnectionStatements.setName("Database::syncConnectionStatements");
  syncScavenge.setName("Database::syncScavenge");
  syncSysDDL.setName("Database::syncSysDDL");
  IO::deleteFile(BACKLOG_FILE);
}

void Database::start() {
  cycleManager = new CycleManager(this);
  symbolManager = new SymbolManager;

#ifdef LICENSE
  licenseManager = new LicenseManager(this);
#endif

#ifndef STORAGE_ENGINE
  templateManager = new TemplateManager(this);
  java = new Java(this, applications, configuration->classpath);
  sessionManager = new SessionManager(this);
  applications = new Applications(this);
#endif

  imageManager = new ImageManager(this);
  roleModel = new RoleModel(this);
  systemConnection = new Connection(this);
  dbb->streamLog = streamLog =
      new StreamLog(this, configuration->checkpointSchedule,
                    configuration->maxTransactionBacklog);
  pageWriter = new PageWriter(this);
  searchWords = new SearchWords(this);
  systemConnection->setLicenseNotRequired(true);
  systemConnection->pushSchemaName("SYSTEM");
  addSystemTables();
  scheduler = new Scheduler(this);
  internalScheduler = new Scheduler(this);
  scavenger = new Scavenger(this, scvRecords, configuration->scavengeSchedule);
#ifndef STORAGE_ENGINE
  garbageCollector = new Scavenger(this, scvJava, configuration->gcSchedule);
#endif
  sequenceManager = new SequenceManager(this);
  repositoryManager = new RepositoryManager(this);
  transactionManager = new TransactionManager(this);
  internalScheduler->addEvent(repositoryManager);
  filterSetManager = new FilterSetManager(this);
  timestamp = time(NULL);
  tickerThread = threads->start("Database::Database", &Database::ticker, this);
  cardinalityThread = threads->start("Database::cardinalityThreadMain",
                                     &Database::cardinalityThreadMain, this);
  scavengerThread = threads->start("Database::scavengerThreadMain",
                                   &Database::scavengerThreadMain, this);
  internalScheduler->addEvent(scavenger);
#ifndef STORAGE_ENGINE
  internalScheduler->addEvent(garbageCollector);
#endif
  internalScheduler->addEvent(streamLog);
  pageWriter->start();
  cache->setPageWriter(pageWriter);
  cycleManager->start();
}

Database::~Database() {
  if (systemConnection) {
    systemConnection->rollback();
    systemConnection->close();
  }

  for (Table *table; (table = tableList);) {
    tableList = table->next;
    delete table;
  }

  int n;

  for (n = 0; n < TABLE_HASH_SIZE; ++n)
    for (UnTable *untable; (untable = unTables[n]);) {
      unTables[n] = untable->collision;
      delete untable;
    }

  for (n = 0; n < TABLE_HASH_SIZE; ++n)
    for (Schema *schema; (schema = schemas[n]);) {
      schemas[n] = schema->collision;
      delete schema;
    }

  for (CompiledStatement *statement = compiledStatements; statement;) {
    CompiledStatement *object = statement;
    statement = statement->next;
    if (object->useCount > 1)
      Log::debug("~Database: '%s' in use\n", (const char *)object->sqlString);
    object->release();
  }

  delete dbb;

  if (scavenger) scavenger->release();

#ifndef STORAGE_ENGINE
  if (garbageCollector) garbageCollector->release();
#endif
  if (threads) {
    threads->shutdownAll();
    threads->waitForAll();
    threads->release();
  }

#ifndef STORAGE_ENGINE
  delete java;
  delete templateManager;
  delete sessionManager;
  delete applications;
#endif

#ifdef LICENSE
  delete licenseManager;
#endif

  if (scheduler) scheduler->release();

  if (internalScheduler) internalScheduler->release();

  delete imageManager;
  delete sequenceManager;
  delete searchWords;
  delete streamLog;
  delete pageWriter;
  delete tableSpaceManager;
  delete cycleManager;
  delete cache;
  delete roleModel;
  delete symbolManager;
  delete filterSetManager;
  delete repositoryManager;
  delete transactionManager;
  delete ioScheduler;
  delete backLog;
  if (syncHandler) delete syncHandler;
}

void Database::createDatabase(const char *filename) {
  // If valid, use the user-defined stream log path, otherwise use the default.

  JString logRoot = setLogRoot(filename, true);
  tableSpaceManager = new TableSpaceManager(this);
  // TBD: Return error to server.

#ifdef STORAGE_ENGINE
  int page_size = changjiang_page_size;
#else
  int page_size = PAGE_SIZE;
#endif

  cache = dbb->create(filename, page_size, configuration->pageCacheSize,
                      HdrDatabaseFile, 0, "");

  try {
    start();
    streamLog->open(logRoot, true);
    streamLog->start();
    sequence = dbb->sequence;
    odsVersion = dbb->odsVersion;
    dbb->createInversion(0);
    Table *table;

    Sync syncDDL(&syncSysDDL, "Database::createDatabase");
    syncDDL.lock(Exclusive);

    Transaction *sysTrans = getSystemTransaction();

    for (table = tableList; table; table = table->next)
      table->create("SYSTEM TABLE", sysTrans);

    for (table = tableList; table; table = table->next) table->save();

    roleModel->createTables();
    sequenceManager->initialize();
    ChjTrigger::initialize(this);
    systemConnection->commit();

#ifndef STORAGE_ENGINE
    java->initialize();
    checkManifest();
    templateManager->getTemplates("base");

#ifdef LICENSE
    licenseManager->initialize();
    licenseCheck();
#endif

    startSessionManager();
#endif

    imageManager->getImages("base", NULL);
    filterSetManager->initialize();
    upgradeSystemTables();
    scheduler->start();
    internalScheduler->start();
    streamLogBlockSize = streamLog->getBlockSize();
    dbb->updateStreamLogBlockSize();
    commitSystemTransaction();
    streamLog->checkpoint(true);
  } catch (...) {
    deleteFilesOnExit = true;
    throw;
  }
}

void Database::openDatabase(const char *filename) {
  try {
    cache = dbb->open(filename, configuration->pageCacheSize, 0);
  } catch (SQLException &e) {
    // Master cannot be opened - throw OPEN_MASTER error to initiate
    // create database. Don't do it if file exists, but there is a problem
    // with permissions and/or locking.
    if (e.getSqlcode() != FILE_ACCESS_ERROR)
      throw SQLError(OPEN_MASTER_ERROR, e.getText());
    else
      throw;
  }
  start();

  if (dbb->logRoot.IsEmpty() || (!configuration->streamLogDir.IsEmpty() &&
                                 dbb->logRoot != configuration->streamLogDir)) {
    // If valid, use the user-defined stream log path, otherwise use the
    // default.

    dbb->logRoot = setLogRoot(filename, true);
  }

  if (streamLog) {
    ASSERT(COMBINED_VERSION(dbb->odsVersion, dbb->odsMinorVersion) >=
           VERSION_STREAM_LOG);

    if (dbb->logLength)
      streamLog->copyClone(dbb->logRoot, dbb->logOffset, dbb->logLength);

    streamLog->open(dbb->logRoot, false);

    try {
      streamLog->recover();
    } catch (SQLError &e) {
      throw SQLError(RECOVERY_ERROR, "Recovery failed: %s", e.getText());
    }

    tableSpaceManager->postRecovery();
    streamLog->start();
  }

  sequence = dbb->sequence;
  odsVersion = dbb->odsVersion;
  utf8 = dbb->utf8;
  int indexId = 0;
  int sectionId = 0;

  for (Table *table = tableList; table; table = table->next) {
    table->setDataSection(sectionId++);
    table->setBlobSection(sectionId++);

    // Iterate all indexes, set indexIDs

    FOR_ALL_INDEXES(index, table)
    index->setIndex(indexId++);
    END_FOR;
  }

  Sync syncDDL(&syncSysDDL, "Database::openDatabase");
  syncDDL.lock(Shared);
  checkODSVersion23();

  PreparedStatement *statement = prepareStatement("select tableid from tables");
  ResultSet *resultSet = statement->executeQuery();

  while (resultSet->next()) {
    int n = resultSet->getInt(1);

    if (n >= nextTableId) nextTableId = n + 1;
  }

  resultSet->close();
  statement->close();
  syncDDL.unlock();

  upgradeSystemTables();
  ChjTrigger::initialize(this);
  streamLog->checkpoint(true);
  // validate(validateOrBreak);

#ifndef STORAGE_ENGINE

#ifdef LICENSE
  licenseManager->initialize();
  licenseCheck();
#endif

  java->initialize();
  checkManifest();
  startSessionManager();
#endif

  sequenceManager->initialize();
  filterSetManager->initialize();
  searchWords->initialize();
  roleModel->initialize();

  if (streamLog) streamLog->recoverLimboTransactions();

#ifndef STORAGE_ENGINE
  getApplication("base");
#endif

  tableSpaceManager->initialize();
  internalScheduler->start();

  if (configuration->schedulerEnabled) scheduler->start();
}

// Check if ODS version 2.3 is really 2.3 and not 2.4
//
// Background:
// There was a subtle change from 2.3 to 2.4 in the way multisegment indexes are
// built. And there are databases that 2.3 in header page, but with 2.4 indexes.
// This function will fix minor version the header page and Dbb in such case
//
// For the check, we use a query that is known to return empty  resultSet if
// and only if ODS is <= 2.3 and engine > 2.3.
void Database::checkODSVersion23() {
  if (dbb->odsVersion == ODS_VERSION2 &&
      dbb->odsMinorVersion == ODS_MINOR_VERSION3) {
    // For the next query, force index code to use 2.4 algorithm for
    // multisegments
    dbb->odsMinorVersion = ODS_MINOR_VERSION4;

    const char checkVersion24Query[] =
        "select privilegeMask from system.privileges where holderType=3 and "
        "holderSchema='' and holderName='MYSQL' and  objectType=0 and "
        "objectSchema='CHANGJIANG' and objectName='TABLES'";
    PreparedStatement *statement = prepareStatement(checkVersion24Query);
    ResultSet *resultSet = statement->executeQuery();

    if (resultSet->next())
      // Got non-empty result -> have ODS 2.4
      dbb->setODSMinorVersion(ODS_MINOR_VERSION4);
    else
      // Got empty result -> have ODS 2.3
      dbb->odsMinorVersion = ODS_MINOR_VERSION3;

    resultSet->close();
    statement->close();
  }
}

#ifndef STORAGE_ENGINE
void Database::startSessionManager() {
  try {
    java->run(systemConnection);
    sessionManager->start(systemConnection);
  } catch (SQLException &exception) {
    Log::debug("Exception during Java initialization: %s\n",
               (const char *)exception.getText());
    const char *stackTrace = exception.getTrace();
    if (stackTrace && stackTrace[0])
      Log::debug("Stack trace:\n%s\n", stackTrace);
  }
}

void Database::genHTML(ResultSet *resultSet, const char *series,
                       const char *type, TemplateContext *context,
                       Stream *stream, JavaCallback *callback) {
  Templates *tmpls = templateManager->getTemplates(series);
  tmpls->genHTML(type, resultSet, context, 0, stream, callback);
}

void Database::genHTML(ResultSet *resultSet, const WCString *series,
                       const WCString *type, TemplateContext *context,
                       Stream *stream, JavaCallback *callback) {
  Templates *tmpls = templateManager->getTemplates(series);
  tmpls->genHTML(symbolManager->getSymbol(type), resultSet, context, 0, stream,
                 callback);
}

JString Database::expandHTML(ResultSet *resultSet,
                             const WCString *applicationName,
                             const char *source, TemplateContext *context,
                             JavaCallback *callback) {
  Templates *tmpls = templateManager->getTemplates(applicationName);

  return tmpls->expandHTML(source, resultSet, context, callback);
}

const char *Database::fetchTemplate(JString applicationName,
                                    JString templateName,
                                    TemplateContext *context) {
  Templates *templates = templateManager->getTemplates(applicationName);
  Template *pTemplate = templates->findTemplate(context, templateName);

  if (!pTemplate) return NULL;

  return pTemplate->body;
}

void Database::zapLinkages() {
  for (Table *table = tableList; table; table = table->next)
    table->zapLinkages();

  sessionManager->zapLinkages();
}

void Database::checkManifest() {
  if (!java->findManifest("netfrastructure/model/Application"))
    throw SQLError(LICENSE_ERROR, "missing manifest for base application");
}

int Database::attachDebugger() { return java->attachDebugger(); }

JString Database::debugRequest(const char *request) {
  return java->debugRequest(request);
}

void Database::detachDebugger() { java->detachDebugger(); }

Application *Database::getApplication(const char *applicationName) {
  return applications->getApplication(applicationName);
}

int Connection::initiateService(const char *applicationName,
                                const char *service) {
  Application *application = database->getApplication(applicationName);

  if (!application)
    throw SQLEXCEPTION(RUNTIME_ERROR, "application '%s' is unknown",
                       (const char *)applicationName);

  application->pushNameSpace(this);
  Session *session = NULL;
  int port;

  try {
    session = database->sessionManager->createSession(application);
    port = database->sessionManager->initiateService(this, session, service);
  } catch (...) {
    if (session) session->release();
    throw;
  }

  session->release();

  return port;
}

PreparedStatement *Connection::prepareDrl(const char *drl) {
  DataResourceLocator locator;

  return locator.prepareStatement(this, drl);
}

#endif  // STORAGE_ENGINE

void Database::serverOperation(int op, Parameters *parameters) {
  switch (op) {
#ifndef STORAGE_ENGINE
    case opTraceAll:
      java->traceAll();
      break;
#endif

#ifdef LICENSE
    case opInstallLicense: {
      const char *license = parameters->findValue("license", NULL);
      if (!license)
        throw SQLEXCEPTION(RUNTIME_ERROR, "installLicense requires license");
      licenseManager->installLicense(license);
      licenseCheck();
      break;
    }
#endif

    case opShutdown:
      break;

    case opClone:
    case opCreateShadow: {
      const char *fileName = parameters->findValue("fileName", NULL);

      if (!fileName)
        throw SQLEXCEPTION(RUNTIME_ERROR,
                           "Filename required to shadow database");

      dbb->cloneFile(this, fileName, op == opCreateShadow);
    } break;

    default:
      throw SQLEXCEPTION(RUNTIME_ERROR,
                         "Server operation %d is not currently supported", op);
  }
}

Statement *Database::createStatement() {
  return systemConnection->createStatement();
}

Table *Database::findTable(const char *schema, const char *name) {
  if (!schema) return NULL;

  schema = symbolManager->getSymbol(schema);
  name = symbolManager->getSymbol(name);

  Sync syncTbl(&syncTables, "Database::findTable(1)");
  syncTbl.lock(Shared);

  int slot = HASH(name, TABLE_HASH_SIZE);
  Table *table;

  for (table = tables[slot]; table; table = table->collision)
    if (table->name == name && table->schemaName == schema) return table;

  syncTbl.unlock();

  for (UnTable *untable = unTables[slot]; untable; untable = untable->collision)
    if (untable->name == name && untable->schemaName == schema) return NULL;

  Sync syncDDL(&syncSysDDL, "Database::findTable(2)");
  syncDDL.lock(Shared);

  PStatement statement = prepareStatement(
      (fieldExtensions)
          ? "select tableName,tableId,dataSection,blobSection,currentVersion,schema,viewDefinition,cardinality,tablespace \
				from Tables where tableName=? and schema=?"
          : "select tableName,tableId,dataSection,blobSection,currentVersion,schema,viewDefinition,0,'' \
				from Tables where tableName=? and schema=?");

  statement->setString(1, name);
  statement->setString(2, schema);
  RSet resultSet = statement->executeQuery();
  table = loadTable(resultSet);
  resultSet.close();
  statement.close();

  if (!table) {
    UnTable *untable = new UnTable(schema, name);
    untable->collision = unTables[slot];
    unTables[slot] = untable;
  }

  return table;
}

Table *Database::addTable(User *owner, const char *name, const char *schema,
                          TableSpace *tableSpace) {
  if (!schema)
    throw SQLEXCEPTION(DDL_ERROR, "no schema defined for table %s\n", name);

  if (!formatting && findTable(schema, name))
    throw SQLEXCEPTION(DDL_ERROR, "table %s is already defined", name);

  Table *table = new Table(this, nextTableId++, schema, name, tableSpace);
  addTable(table);

  return table;
}

/***
int32 Database::createSection(Transaction *transaction)
{
        return dbb->createSection(TRANSACTION_ID(transaction));
}
***/

void Database::addSystemTables() {
  formatting = true;
  Statement *statement = createStatement();

  for (const char **sql = ods2Statements; *sql; ++sql) statement->execute(*sql);

  statement->close();
  formatting = false;
}

PreparedStatement *Database::prepareStatement(const char *sqlStr) {
  return systemConnection->prepareStatement(sqlStr);
}

void Database::addRef() { ++useCount; }

void Database::release() {
  int n = --useCount;

  if (n == 1 && systemConnection) {
    Connection *temp = systemConnection;
    temp->commit();
    systemConnection = NULL;
    temp->close();
  } else if (n == 0) {
    shutdown();
    delete this;
  }
}

Statement *Database::createStatement(Connection *connection) {
  return new Statement(connection, this);
}

PreparedStatement *Database::prepareStatement(Connection *connection,
                                              const char *sqlStr) {
  PreparedStatement *statement = new PreparedStatement(connection, this);

  try {
    statement->setSqlString(sqlStr);
  } catch (...) {
    statement->close();
    throw;
  }

  return statement;
}

PreparedStatement *Database::prepareStatement(Connection *connection,
                                              const WCString *sqlStr) {
  PreparedStatement *statement = new PreparedStatement(connection, this);

  try {
    statement->setSqlString(sqlStr);
  } catch (...) {
    statement->close();
    throw;
  }

  return statement;
}

CompiledStatement *Database::getCompiledStatement(Connection *connection,
                                                  const char *sqlString) {
  Sync syncDDL(&syncSysDDL, "Database::getCompiledStatement(1)");
  syncDDL.lock(Shared);

  Sync syncStmt(&syncStatements, "Database::getCompiledStatement(2)");
  syncStmt.lock(Shared);

  // printf("%s\n", (const char*) sqlString);

  for (CompiledStatement *statement = compiledStatements; statement;
       statement = statement->next)
    if (statement->sqlString == sqlString && statement->validate(connection)) {
      statement->addRef();
      syncStmt.unlock();
      try {
        statement->checkAccess(connection);
      } catch (...) {
        statement->release();
        throw;
      }
      return statement;
    }

  syncStmt.unlock();

  return compileStatement(connection, sqlString);
}

CompiledStatement *Database::getCompiledStatement(Connection *connection,
                                                  const WCString *sqlString) {
  Sync syncDDL(&syncSysDDL, "Database::getCompiledStatement(3)");
  syncDDL.lock(Shared);

  Sync syncStmt(&syncStatements, "Database::getCompiledStatement(4)");
  syncStmt.lock(Shared);

  // JString str(sqlString);
  // printf("%s\n", (const char*) str);

  for (CompiledStatement *statement = compiledStatements; statement;
       statement = statement->next)
    if (statement->sqlString == sqlString && statement->validate(connection)) {
      statement->addRef();
      syncStmt.unlock();
      try {
        statement->checkAccess(connection);
      } catch (...) {
        statement->release();
        throw;
      }
      return statement;
    }

  syncStmt.unlock();
  JString sql(sqlString);

  return compileStatement(connection, sql);
}

CompiledStatement *Database::compileStatement(Connection *connection,
                                              JString sqlString) {
  Sync syncDDL(&syncSysDDL, "Database::compileStatement(1)");
  syncDDL.lock(Shared);

  CompiledStatement *statement = new CompiledStatement(connection);

  try {
    statement->compile(sqlString);
    statement->checkAccess(connection);
  } catch (...) {
    delete statement;
    throw;
  }

  if (statement->useable &&
      (statement->numberParameters > 0 || !statement->filters.isEmpty())) {
    Sync syncStmt(&syncStatements, "Database::compileStatement(2)");
    syncStmt.lock(Shared);
    Sync syncAddStmt(&syncAddStatement, "Database::compileStatement(3)");
    syncAddStmt.lock(Exclusive);
    statement->addRef();
    statement->next = compiledStatements;
    compiledStatements = statement;
  }

  return statement;
}

Transaction *Database::startTransaction(Connection *connection) {
  return transactionManager->startTransaction(connection);
}

bool Database::flush(int64 arg) {
  if (cache->flushing) return false;

  streamLog->preFlush();
  cache->flush(arg);

  return true;
}

void Database::commitSystemTransaction() {
  Sync sync(&syncSysDDL, "Database::commitSystemTransaction");
  sync.lock(Exclusive);
  systemConnection->commit();
}

void Database::rollbackSystemTransaction(void) {
  Sync sync(&syncSysDDL, "Database::rollbackSystemTransaction");
  sync.lock(Exclusive);
  systemConnection->rollback();
}

void Database::setDebug() { dbb->setDebug(); }

void Database::clearDebug() { dbb->setDebug(); }

int32 Database::addInversion(InversionFilter *filter,
                             Transaction *transaction) {
  return dbb->inversion->addInversion(filter, TRANSACTION_ID(transaction),
                                      true);
}

void Database::removeFromInversion(InversionFilter *filter,
                                   Transaction *transaction) {
  dbb->inversion->addInversion(filter, TRANSACTION_ID(transaction), false);
}

void Database::search(ResultList *resultList, const char *string) {
  Search search(string, searchWords);
  search.search(resultList);
}

void Database::reindex(Transaction *transaction) {
  // rebuildIndexes();

  dbb->inversion->deleteInversion(TRANSACTION_ID(transaction));
  dbb->inversion->createInversion(TRANSACTION_ID(transaction));

  for (Table *table = tableList; table; table = table->next)
    table->reIndexInversion(transaction);
}

Table *Database::getTable(int tableId) {
  Table *table;

  Sync syncDDL(&syncSysDDL, "Database::getTable");
  syncDDL.lock(Shared);

  for (table = tablesModId[tableId % TABLE_HASH_SIZE]; table;
       table = table->idCollision)
    if (table->tableId == tableId) return table;

  PStatement statement = prepareStatement(
      "select tableName,tableId,dataSection,blobSection,currentVersion,schema,viewDefinition,cardinality,tablespace \
		 from system.Tables where tableid=?");
  statement->setInt(1, tableId);
  RSet resultSet = statement->executeQuery();
  table = loadTable(resultSet);

  return table;
}

Table *Database::loadTable(ResultSet *resultSet) {
  Sync syncDDL(&syncSysDDL, "Database::loadTable(1)");
  syncDDL.lock(Shared);

  Sync syncObj(&syncTables, "Database::loadTable(2)");

  if (!resultSet->next()) return NULL;

  const char *name = getString(resultSet->getString(1));
  int version = resultSet->getInt(5);
  const char *schemaName = getString(resultSet->getString(6));
  const char *tableSpaceName = getString(resultSet->getString(9));
  TableSpace *tableSpace = NULL;

  if (tableSpaceName[0])
    tableSpace = tableSpaceManager->findTableSpace(tableSpaceName);

  Table *table = new Table(this, schemaName, name, resultSet->getInt(2),
                           version, resultSet->getLong(8), tableSpace);

  int dataSection = resultSet->getInt(3);
  int blobSection = resultSet->getInt(4);

  if (dataSection || blobSection) {
    table->setDataSection(dataSection);
    table->setBlobSection(blobSection);
  } else {
    const char *viewDef = resultSet->getString(7);

    if (viewDef[0]) {
      CompiledStatement statement(systemConnection);
      JString string;

      // Do a little backward compatibility

      if (strncmp(viewDef, "create view ", strlen("create view ")) == 0)
        string = viewDef;
      else
        string.Format("create view %s.%s %s", schemaName, name, viewDef);

      table->setView(statement.getView(string));
    }
  }

  table->loadStuff();

  syncObj.lock(Exclusive);
  addTable(table);

  return table;
}

bool Database::matches(const char *fileName) {
  return strcasecmp(dbb->fileName, fileName) == 0;
}

void Database::flushInversion(Transaction *transaction) {
  dbb->inversion->flush(TRANSACTION_ID(transaction));
}

void Database::dropTable(Table *table, Transaction *transaction) {
  Sync syncDDL(&syncSysDDL, "Database::dropTable(1)");
  syncDDL.lock(Exclusive);

  table->checkDrop();

  // Check for records in active transactions.  If so, barf

  if (hasUncommittedRecords(table, transaction))
    throw SQLError(UNCOMMITTED_UPDATES,
                   "table %s.%s has uncommitted updates and can't be dropped",
                   table->schemaName, table->name);

  // OK, now make sure any records are purged out of committed transactions as
  // well

  transactionManager->dropTable(table, transaction);

  Sync syncTbl(&syncTables, "Database::dropTable(2)");
  syncTbl.lock(Exclusive);

  // Remove table from linear table list

  Table **ptr;

  for (ptr = &tableList; *ptr; ptr = &((*ptr)->next))
    if (*ptr == table) {
      *ptr = table->next;
      break;
    }

  // Remove table from name hash table

  for (ptr = tables + HASH(table->name, TABLE_HASH_SIZE); *ptr;
       ptr = &((*ptr)->collision))
    if (*ptr == table) {
      *ptr = table->collision;
      break;
    }

  // Remove table from id hash table

  for (ptr = tablesModId + table->tableId % TABLE_HASH_SIZE; *ptr;
       ptr = &((*ptr)->idCollision))
    if (*ptr == table) {
      *ptr = table->idCollision;
      break;
    }

  syncTbl.unlock();

  invalidateCompiledStatements(table);
  table->drop(transaction);

  // Lock sections (factored out of SRLDropTable to avoid a deadlock)

  Sync syncSections(&streamLog->syncSections, "Database::dropTable(3)");
  syncSections.lock(Exclusive);
  table->expunge(getSystemTransaction());
  delete table;
}

void Database::truncateTable(Table *table, Sequence *sequence,
                             Transaction *transaction) {
  // Check for records in active transactions

  if (hasUncommittedRecords(table, transaction))
    throw SQLError(
        UNCOMMITTED_UPDATES,
        "table %s.%s has uncommitted updates and cannot be truncated",
        table->schemaName, table->name);

  // Lock SystemDDL first.  This lock can happen multiple times in many call
  // stacks, both before and after the following locks.  So it is important that
  // we get an exclusive lock first.

  Sync syncDDLLock(&syncSysDDL, "Database::truncateTable(SysDDL)");
  syncDDLLock.lock(Exclusive);

  // Lock syncScavenge before locking syncTables, or table->syncObject.
  // The scavenger locks syncScavenge, then syncTables, then table->syncObject

  Sync syncScavengeLock(&syncScavenge, "Database::truncateTable(scavenge)");
  syncScavengeLock.lock(Exclusive);

  table->checkDrop();

  // Block table drop/add, table list scans ok

  Sync syncTablesLock(&syncTables, "Database::truncateTable(tables)");
  syncTablesLock.lock(Shared);

  // Lock sections (factored out of SRLDropTable to avoid a deadlock)
  // The lock order (streamLog->syncSections before table->syncObject) is
  // important

  Sync syncSectionsLock(&streamLog->syncSections,
                        "Database::truncateTable(sections)");
  syncSectionsLock.lock(Exclusive);

  // No table access until truncate completes

  Sync syncTableLock(&table->syncObject, "Database::truncateTable(table)");
  syncTableLock.lock(Exclusive);

  table->deleting = true;

  // Purge records out of committed transactions

  transactionManager->truncateTable(table, transaction);

  Transaction *sysTransaction = getSystemTransaction();

  // Recreate data/blob sections and indexes

  table->truncate(sysTransaction);

  commitSystemTransaction();

  // Delete and recreate the sequence

  if (sequence) sequence = sequence->recreate();
}

void Database::addTable(Table *table) {
  Sync sync(&syncTables, "Database::addTable");
  sync.lock(Exclusive);

  if (formatting) {
    Table **ptr;

    for (ptr = &tableList; *ptr; ptr = &((*ptr)->next))
      ;

    table->next = *ptr;
    *ptr = table;
  } else {
    table->next = tableList;
    tableList = table;
  }

  int slot = HASH(table->name, TABLE_HASH_SIZE);
  table->collision = tables[slot];
  tables[slot] = table;

  slot = table->tableId % TABLE_HASH_SIZE;
  table->idCollision = tablesModId[slot];
  tablesModId[slot] = table;

  // Notify folks who track stuff that there's a new table

#ifndef STORAGE_ENGINE
  templateManager->tableAdded(table);
  applications->tableAdded(table);
#endif

#ifdef LICENSE
  licenseManager->tableAdded(table);
#endif

  imageManager->tableAdded(table);
  searchWords->tableAdded(table);
}

void Database::execute(const char *sql) {
  Statement *statement = createStatement();
  statement->execute(sql);
  statement->close();
}

void Database::shutdown() {
  Log::log("%d: Changjiang shutdown\n", deltaTime);

  if (shuttingDown) return;

  // Wait for all porpoises to finish.
  waitForWriteComplete(NULL);

  if (updateCardinality) {
    updateCardinality->close();
    updateCardinality = NULL;
  }

  shuttingDown = true;

  if (systemConnection && systemConnection->transaction &&
      systemConnection->transaction->getState() == Active)
    systemConnection->commit();

  if (repositoryManager) repositoryManager->close();

  // flush(0);

  if (scheduler) scheduler->shutdown(false);

  if (internalScheduler) internalScheduler->shutdown(false);

  if (pageWriter) pageWriter->shutdown(false);

#ifndef STORAGE_ENGINE
  if (java) java->shutdown(false);
#endif

  streamLog->shutdown();
  cache->shutdown();

  if (cycleManager) cycleManager->shutdown();

  if (threads) {
    threads->shutdownAll();
    threads->waitForAll();
  }

  tableSpaceManager->shutdown(0);
  dbb->shutdown(0);

  if (streamLog) streamLog->close();
}

/***
void Database::deleteSection(int32 sectionId, Transaction *transaction)
{
        dbb->deleteSection (sectionId, TRANSACTION_ID(transaction));

        if (transaction)
                transaction->hasUpdates = true;

}
***/

void Database::invalidateCompiledStatements(Table *table) {
  Sync sync(&syncStatements, "Database::invalidateCompiledStatements");
  sync.lock(Exclusive);

  for (CompiledStatement *statement, **ptr = &compiledStatements;
       (statement = *ptr);)
    if (statement->references(table)) {
      statement->invalidate();
      *ptr = statement->next;
      statement->release();
    } else
      ptr = &(*ptr)->next;
}

User *Database::createUser(const char *account, const char *password,
                           bool encrypted, Coterie *coterie) {
  Sync syncDDL(&syncSysDDL, "Database::createUser");
  syncDDL.lock(Exclusive);

  return roleModel->createUser(getSymbol(account), password, encrypted,
                               coterie);
}

User *Database::findUser(const char *account) {
  Sync syncDDL(&syncSysDDL, "Database::findUser");
  syncDDL.lock(Shared);

  return roleModel->findUser(symbolManager->getSymbol(account));
}

Role *Database::findRole(const char *schemaName, const char *roleName) {
  const char *schema = symbolManager->getSymbol(schemaName);
  const char *role = symbolManager->getSymbol(roleName);
  return roleModel->findRole(schema, role);
}

Role *Database::findRole(const WCString *schemaName, const WCString *roleName) {
  return roleModel->findRole(symbolManager->getSymbol(schemaName),
                             symbolManager->getSymbol(roleName));
}

void Database::validate(int optionMask) {
  Sync syncDDL(&syncSysDDL, "Database::validate(1)");
  syncDDL.lock(Shared);

  Sync syncObj(&syncObject, "Database::validate(2)");
  syncObj.lock(Exclusive);

  Log::debug("Validation:\n");
  dbb->validate(optionMask);
  tableSpaceManager->validate(optionMask);

  if (optionMask & validateBlobs) {
    PreparedStatement *statement =
        prepareStatement("select tableId,viewDefinition from system.tables");
    ResultSet *resultSet = statement->executeQuery();

    while (resultSet->next())
      if (!resultSet->getString(2)[0]) {
        Table *table = getTable(resultSet->getInt(1));
        table->validateBlobs(optionMask);
      }

    resultSet->close();
    statement->close();
  }

  Log::debug("Database::validate: validation complete\n");
}

void Database::scavenge(bool forced) {
  // Signal the cardinality task unless a forced scavenge is pending

  if (!forced) {
    // Don't disturb recovery
    if (streamLog && streamLog->recovering) return;

    if (++scavengeCount % CARDINALITY_FREQUENCY == 0) signalCardinality();
  }

  scavengeForced = 0;

  // Start by scavenging compiled statements.

  scavengeCompiledStatements();

  // purgeTransactions will release records that are  attached to old
  // transactions, thus freeing up old invisible records to be pruned
  // and actually released.  It is not likely that the scavenger will
  // retire these freshly released base records, but disconnecting
  // pruneable records from their transactions is definitely needed.

  transactionManager->purgeTransactions();

  // Scavenge the record cache
  scavengeRecords(forced);

  // Scavenge expired licenses

  DateTime now;
  now.setNow();

#ifdef LICENSE
  licenseManager->scavenge(&now);
#endif

#ifndef STORAGE_ENGINE
  sessionManager->scavenge(&now);
#endif

  transactionManager->reportStatistics();

  if (streamLog) {
    streamLog->reportStatistics();

    if (tableSpaceManager && !streamLog->recovering)
      tableSpaceManager->reportStatistics();
  }

  dbb->reportStatistics();
  repositoryManager->reportStatistics();

  if (backLog) backLog->reportStatistics();
}

void Database::scavengeCompiledStatements(void) {
  Sync syncStmt(&syncStatements, "Database::scavenge");
  syncStmt.lock(Exclusive);

  time_t threshold = timestamp - STATEMENT_RETIREMENT_AGE;
  lastScavenge = timestamp;

  for (CompiledStatement *statement, **ptr = &compiledStatements;
       (statement = *ptr);)
    if (statement->useCount > 1 || statement->lastUse > threshold)
      ptr = &statement->next;
    else {
      *ptr = statement->next;
      statement->release();
    }
}

void Database::scavengeRecords(bool forced) {
  Sync syncScavenger(&syncScavenge, "Database::scavengeRecords(Scavenge)");
  syncScavenger.lock(Exclusive);

  // Create an object to track this record scavenge cycle. Scavenge up to and
  // including the current generation.

  RecordScavenge recordScavenge(this, currentGeneration, forced);

  // Take inventory of the record cache and prune invisible record versions

  pruneRecords(&recordScavenge);
  recordScavenge.prunedActiveMemory =
      recordMemoryControl->getCurrentMemory(MemMgrRecordData);
  recordScavenge.pruneStop = deltaTime;
  syncScavenger.unlock();  // take a breath!

  // Retire visible records with no dependencies in the oldest age groups

  syncScavenger.lock(Exclusive);
  retireRecords(&recordScavenge);
  recordScavenge.retiredActiveMemory =
      recordMemoryControl->getCurrentMemory(MemMgrRecordData);
  recordScavenge.retireStop = deltaTime;

  // Backlogging disabled: Bug#43504 "Changjiang DBT2 crash in
  // Table::rollbackRecord()"

#if 0 
	// Enable backlogging if memory is low

	if (recordScavenge.retiredActiveMemory > recordScavengeFloor)
		if (!lowMemory)
			setLowMemory(recordScavenge.retiredActiveMemory - recordScavengeFloor);
	else
		{
		if (lowMemoryCount)
			if (--lowMemoryCount == 0)
				clearLowMemory();
		}
#endif

  recordScavenge.print();
  // Log::log(analyze(analyzeRecordLeafs));

  // syncmemory is used to protect lastActiveMemoryChecked
  // and lastGenerationMemory.  In addition, it is used to
  // serialize signaling of the scavenger thread by allowing
  // only one thread at a time to check and change
  // scavengerThreadSignaled.  It is a Mutex instead of a
  // SyncObject because it is only locked exclusively, no
  // shared locks.  It does not need any of the other
  // features of SyncObject.  And a Mutex is faster.

  Sync syncMem(&syncMemory, "Database::checkRecordScavenge");
  syncMem.lock(Exclusive);

  lastActiveMemoryChecked = lastGenerationMemory =
      recordMemoryControl->getCurrentMemory(MemMgrRecordData);
}

// Take inventory of the record cache and prune invisible record versions

void Database::pruneRecords(RecordScavenge *recordScavenge) {
  // Log::log(analyze(analyzeRecordLeafs));
  // LogStream stream;
  // recordDataPool->analyze(0, &stream, NULL, NULL);

  Sync syncTbl(&syncTables, "Database::pruneRecords(tables)");
  syncTbl.lock(Shared);

  for (Table *table = tableList; table; table = table->next) {
    try {
      table->pruneRecords(recordScavenge);
    } catch (SQLException &exception) {
      Log::debug("Exception during pruning of table %s.%s: %s\n",
                 table->schemaName, table->name, exception.getText());
    }
  }
}

void Database::retireRecords(RecordScavenge *recordScavenge) {
  // Scavenge if we passed the upper limit or if a forced scavenge
  // was requested.

  if (recordMemoryControl->getCurrentMemory(MemMgrRecordData) <
          recordScavengeThreshold &&
      !recordScavenge->forced)
    return;

  // LogStream stream;
  // recordDataPool->analyze(0, &stream, NULL, NULL);

  Sync syncTbl(&syncTables, "Database::retireRecords(2)");
  syncTbl.lock(Shared);

  uint64 spaceToRetire =
      recordMemoryControl->getCurrentMemory(MemMgrRecordData) -
      recordScavengeFloor;
  recordScavenge->computeThreshold(spaceToRetire);

  for (Table *table = tableList; table; table = table->next) {
    try {
      table->retireRecords(recordScavenge);
    } catch (SQLException &exception) {
      Log::debug("Exception during scavenge of table %s.%s: %s\n",
                 table->schemaName, table->name, exception.getText());
    }
  }
}

void Database::ticker(void *database) {
#ifdef _PTHREADS
  prctl(PR_SET_NAME, "cj_ticker");
#endif
  ((Database *)database)->ticker();
}

void Database::ticker() {
  Thread *thread = Thread::getThread("Database::ticker");

  while (!thread->shutdownInProgress) {
    timestamp = time(NULL);
    deltaTime = (int)(timestamp - startTime);
    thread->sleep(1000);

#ifdef STORAGE_ENGINE
    if (changjiang_debug_trace) debugTrace();
#endif
  }
}

void Database::scavengerThreadMain(void *database) {
#ifdef _PTHREADS
  prctl(PR_SET_NAME, "cj_scavenger");
#endif
  ((Database *)database)->scavengerThreadMain();
}

void Database::scavengerThreadMain(void) {
  Thread *thread = Thread::getThread("Database::scavengerThreadMain");

  thread->sleep(1000);

  while (!thread->shutdownInProgress) {
    scavenge((scavengeForced > 0));

    if (recordMemoryControl->getCurrentMemory(MemMgrRecordData) <
        recordScavengeThreshold) {
      INTERLOCKED_INCREMENT(scavengerThreadSleeping);
      thread->sleep();
      scavengerThreadSignaled = 0;
      INTERLOCKED_DECREMENT(scavengerThreadSleeping);
    }
  }
}

void Database::scavengerThreadWakeup(void) {
  if (scavengerThread) scavengerThread->wake();
}

int Database::createSequence(int64 initialValue) {
  Transaction *transaction = getSystemTransaction();

  return dbb->createSequence(initialValue, TRANSACTION_ID(transaction));
}

int64 Database::updateSequence(int sequenceId, int64 delta,
                               Transaction *transaction) {
  return dbb->updateSequence(sequenceId, delta, TRANSACTION_ID(transaction));
}

Transaction *Database::getSystemTransaction() {
  return systemConnection->getTransaction();
}

void Database::rebuildIndexes() {
  Transaction *transaction = getSystemTransaction();

  for (Table *table = tableList; table; table = table->next)
    table->reIndex(transaction);

  commitSystemTransaction();
}

const char *Database::getSymbol(const char *string) {
  return symbolManager->getSymbol(string);
}

bool Database::isSymbol(const char *string) {
  return symbolManager->isSymbol(string);
}

const char *Database::getSymbol(const WCString *string) {
  return symbolManager->getSymbol(string);
}

const char *Database::getString(const char *string) {
  return symbolManager->getString(string);
}

void Database::upgradeSystemTables() {
  Sync syncDDL(&syncSysDDL, "Database::upgradeSystemTables");
  syncDDL.lock(Exclusive);

  for (const char **tableName = changedTables; *tableName; ++tableName) {
    Table *table = findTable("SYSTEM", *tableName);
    table->refreshFields();
  }

  Table *table = findTable("SYSTEM", "SCHEMAS");

  if (!table) {
    Statement *statement = createStatement();

    for (const char **sql = ods2UpgradeStatements; *sql; ++sql)
      statement->execute(*sql);

    statement->close();
  }

  table = findTable("SYSTEM", "FIELDS");
  table->loadIndexes();

  if (!table->findField("PRECISION")) {
    Statement *statement = createStatement();

    for (const char **sql = ods3Upgrade; *sql; ++sql) statement->execute(*sql);

    statement->close();
  }

  Table *tables = findTable("SYSTEM", "TABLES");

  if (!tables->findField("CARDINALITY")) {
    Statement *statement = createStatement();

    for (const char **sql = ods3aUpgrade; *sql; ++sql) statement->execute(*sql);

    statement->close();
  }

  if (!tables->findField("TABLESPACE")) {
    Statement *statement = createStatement();

    for (const char **sql = ods3bUpgrade; *sql; ++sql) statement->execute(*sql);

    statement->close();
    tables->refreshFields();
  }

  table = findTable("SYSTEM", "TABLESPACES");

  if (table && table->dataSectionId != dbb->tableSpaceSectionId)
    dbb->updateTableSpaceSection(table->dataSectionId);

  fieldExtensions = true;
}

JString Database::analyze(int mask) {
  Stream stream;
  stream.setMalloc(true);
  Sync syncDDL(&syncSysDDL, "Database::analyze(1)");

  if (mask & analyzeMemory) MemMgrAnalyze(mask, &stream);

#ifndef STORAGE_ENGINE
  if (mask & analyzeMemory) java->analyzeMemory(mask, &stream);

  if (mask & analyzeClasses) {
    stream.putSegment("\nClasses\n");
    java->analyze(mask, &stream);
    stream.putCharacter('\n');
  }

  if (mask & analyzeObjects) {
    stream.putSegment("\nObject allocations\n");
    java->analyzeObjects(mask, &stream);
    stream.putCharacter('\n');
  }
#endif

  if (mask & analyzeRecords) {
    stream.putSegment("\nRecords\n");

    for (Table *table = tableList; table; table = table->next) {
      int count = table->countActiveRecords();

      if (count)
        stream.format("%s.%s\t%d\n", table->schemaName, table->name, count);
    }

    stream.putCharacter('\n');
  }

  if (mask & analyzeRecordLeafs) {
    int *chart = new int[RECORD_SLOTS + 1];
    stream.putSegment("\nRecordLeafs\n");

    for (Table *table = tableList; table; table = table->next) {
      memset(chart, 0, sizeof(int) * (RECORD_SLOTS + 1));
      int count = table->chartActiveRecords(chart);

      if (count) {
        stream.format("%s.%s\t%d\t", table->schemaName, table->name, count);
        for (int a = 0; a < RECORD_SLOTS + 1; a++)
          if (chart[a]) stream.format("[%d]%d ", a, chart[a]);

        stream.format("\n");
      }
    }

    stream.putCharacter('\n');
  }

  if (mask & analyzeStatements) {
    stream.putSegment("\nStatements\n");
    Sync syncStmt(&syncStatements, "Database::analyze(2)");
    syncStmt.lock(Shared);

    for (CompiledStatement *statement = compiledStatements; statement;
         statement = statement->next) {
      stream.putSegment(statement->sqlString);
      stream.format("\t(%d)\n", statement->countInstances());
    }
    stream.putCharacter('\n');
    syncStmt.unlock();
  }

  if (mask & analyzeTables) {
    syncDDL.lock(Shared);
    PreparedStatement *statement = prepareStatement(
        "select schema,tableName,dataSection,blobSection,tablespace from "
        "tables order by schema, tableName");
    PreparedStatement *indexQuery = prepareStatement(
        "select indexName, indexId from system.indexes where schema = ? and "
        "tableName = ?");
    ResultSet *resultSet = statement->executeQuery();

    while (resultSet->next()) {
      int n = 1;
      const char *schema = resultSet->getString(n++);
      const char *tableName = resultSet->getString(n++);
      int dataSection = resultSet->getInt(n++);
      int blobSection = resultSet->getInt(n++);
      const char *tableSpaceName = resultSet->getString(n++);
      TableSpace *tableSpace = NULL;

      if (tableSpaceName[0])
        tableSpace = tableSpaceManager->findTableSpace(tableSpaceName);

      Dbb *tableDbb = (tableSpace) ? tableSpace->dbb : dbb;
      stream.format("Table %s.%s\n", schema, tableName);
      tableDbb->analyzeSection(dataSection, "Data section", 3, &stream);
      tableDbb->analyzeSection(blobSection, "Blob section", 3, &stream);

      indexQuery->setString(1, schema);
      indexQuery->setString(2, tableName);
      ResultSet *indexes = indexQuery->executeQuery();

      while (indexes->next()) {
        const char *indexName = indexes->getString(1);
        int combinedId = indexes->getInt(2);
        int indexId = INDEX_ID(combinedId);
        int indexVersion = INDEX_VERSION(combinedId);
        tableDbb->analyseIndex(indexId, indexVersion, indexName, 3, &stream);
      }

      indexes->close();
    }

    statement->close();
    indexQuery->close();
  }

  if (mask & analyzeSpace) dbb->analyzeSpace(0, &stream);

  if (mask & analyzeCache) dbb->cache->analyze(&stream);

  if (mask & analyzeSync) SyncObject::analyze(&stream);

  return stream.getJString();
}

int Database::getMemorySize(const char *string) {
  int n = 0;

  for (const char *p = string; *p;) {
    char c = *p++;
    if (c >= '0' && c <= '9')
      n = n * 10 + c - '0';
    else if (c == 'm' || c == 'M')
      n *= 1000000;
    else if (c == 'k' || c == 'K')
      n *= 1000;
  }

  return n;
}

void Database::licenseCheck() {
#ifdef LICENSE
  licensed = false;
  LicenseProduct *product = licenseManager->getProduct(SERVER_PRODUCT);

  if (!(licensed = product->isLicensed())) {
    DateTime expiration = DateTime::convert(BUILD_DATE);
    expiration.add(EXPIRATION_DAYS * 24 * 60 * 60);
    DateTime now;
    now.setNow();
    if (now.after(expiration))
      throw SQLError(LICENSE_ERROR,
                     "Unlicensed server usage period has expired");
  }
#endif
}

Repository *Database::findRepository(const char *schema, const char *name) {
  return repositoryManager->findRepository(schema, name);
}

Repository *Database::getRepository(const char *schema, const char *name) {
  return repositoryManager->getRepository(schema, name);
}

Repository *Database::createRepository(const char *name, const char *schema,
                                       Sequence *sequence, const char *fileName,
                                       int volume, const char *rolloverString) {
  return repositoryManager->createRepository(name, schema, sequence, fileName,
                                             volume, rolloverString);
}

Schema *Database::getSchema(const char *name) {
  int slot = HASH(name, TABLE_HASH_SIZE);
  Schema *schema;

  Sync syncDDL(&syncSysDDL, "Database::getSchema");
  syncDDL.lock(Shared);

  for (schema = schemas[slot]; schema; schema = schema->collision)
    if (schema->name == name) return schema;

  schema = new Schema(this, name);
  schema->collision = schemas[slot];
  schemas[slot] = schema;

  return schema;
}

void Database::deleteRepository(Repository *repository) {
  repositoryManager->deleteRepository(repository);
}

void Database::deleteRepositoryBlob(const char *schema,
                                    const char *repositoryName, int volume,
                                    int64 blobId, Transaction *transaction) {
  Repository *repository =
      getRepository(getSymbol(schema), getSymbol(repositoryName));
  repository->deleteBlob(volume, blobId, transaction);
}

void Database::renameTable(Table *table, const char *newSchema,
                           const char *newName) {
  newSchema = getSymbol(newSchema);
  newName = getSymbol(newName);
  roleModel->renameTable(table, newSchema, newName);

  // Remove table from name hash table

  for (Table **ptr = tables + HASH(table->name, TABLE_HASH_SIZE); *ptr;
       ptr = &((*ptr)->collision))
    if (*ptr == table) {
      *ptr = table->collision;
      break;
    }

  // Add table back to name hash table

  table->name = newName;
  table->schemaName = newSchema;
  int slot = HASH(table->name, TABLE_HASH_SIZE);
  table->collision = tables[slot];
  tables[slot] = table;

  invalidateCompiledStatements(table);
}

void Database::dropDatabase() {
  shutdown();

  if (streamLog) streamLog->dropDatabase();

  tableSpaceManager->dropDatabase();
  dbb->dropDatabase();
}

void Database::shutdownNow() {
  panicShutdown = true;

  if (cache) cache->shutdownNow();

  if (streamLog) streamLog->shutdownNow();
}

void Database::validateCache(void) { dbb->validateCache(); }

bool Database::hasUncommittedRecords(Table *table, Transaction *transaction) {
  return transactionManager->hasUncommittedRecords(table, transaction);
}

void Database::waitForWriteComplete(Table *table) {
  transactionManager->waitForWriteComplete(table);
}

void Database::commitByXid(int xidLength, const UCHAR *xid) {
  streamLog->commitByXid(xidLength, xid);
  transactionManager->commitByXid(xidLength, xid);
}

void Database::rollbackByXid(int xidLength, const UCHAR *xid) {
  streamLog->rollbackByXid(xidLength, xid);
  transactionManager->rollbackByXid(xidLength, xid);
}

int Database::getMaxKeyLength(void) {
  switch (dbb->pageSize) {
    case 1024:
      return MAX_INDEX_KEY_LENGTH_1K;
    case 2048:
      return MAX_INDEX_KEY_LENGTH_2K;
    case 4096:
      return MAX_INDEX_KEY_LENGTH_4K;
    case 8192:
      return MAX_INDEX_KEY_LENGTH_8K;
    case 16384:
      return MAX_INDEX_KEY_LENGTH_16K;
    case 32768:
      return MAX_INDEX_KEY_LENGTH_32K;
  }

  // Any other page size is programatically unlikely (it would be a bug).

  return MAX_INDEX_KEY_LENGTH_4K;  // Default value.
}

void Database::getIOInfo(InfoTable *infoTable) {
  int n = 0;
  infoTable->putString(n++, name);
  infoTable->putInt(n++, dbb->pageSize);
  infoTable->putInt(n++, dbb->cache->numberBuffers);
  infoTable->putInt(n++, dbb->reads);
  infoTable->putInt(n++, dbb->writes);
  infoTable->putInt(n++, dbb->fetches);
  infoTable->putInt(n++, dbb->fakes);
  infoTable->putRecord();
}

void Database::getTransactionInfo(InfoTable *infoTable) {
  transactionManager->getTransactionInfo(infoTable);
}

void Database::getStreamLogInfo(InfoTable *infoTable) {
  streamLog->getStreamLogInfo(infoTable);
}

void Database::getTransactionSummaryInfo(InfoTable *infoTable) {
  transactionManager->getSummaryInfo(infoTable);
}

void Database::getTableSpaceInfo(InfoTable *infoTable) {
  tableSpaceManager->getTableSpaceInfo(infoTable);
}

void Database::getTableSpaceFilesInfo(InfoTable *infoTable) {
  tableSpaceManager->getTableSpaceFilesInfo(infoTable);
}

void Database::cardinalityThreadMain(void *database) {
#ifdef _PTHREADS
  prctl(PR_SET_NAME, "cj_cardinality");
#endif
  ((Database *)database)->cardinalityThreadMain();
}

void Database::cardinalityThreadMain(void) {
  Thread *thread = Thread::getThread("Database::cardinalityThreadMain");

  thread->sleep(1000);

  // Wait for recovery to finish.

  while ((!thread->shutdownInProgress) &&
         ((!streamLog) || (streamLog->recovering)))
    thread->sleep(1000);

  while (!thread->shutdownInProgress) {
    updateCardinalities();
    INTERLOCKED_INCREMENT(cardinalityThreadSleeping);
    thread->sleep();
    cardinalityThreadSignaled = 0;
    INTERLOCKED_DECREMENT(cardinalityThreadSleeping);
  }
}

void Database::signalCardinality(void) {
  Sync syncCard(&syncCardinality, "Database::signalCardinality");
  syncCard.lock(Exclusive);

  if (cardinalityThreadSleeping && !cardinalityThreadSignaled) {
    INTERLOCKED_INCREMENT(cardinalityThreadSignaled);
    cardinalityThreadWakeup();
  }
}

void Database::cardinalityThreadWakeup(void) {
  if (cardinalityThread) cardinalityThread->wake();
}

void Database::updateCardinalities(void) {
  Sync syncDDL(&syncSysDDL, "Database::updateCardinalities(1)");
  syncDDL.lock(Shared);

  Sync syncTbl(&syncTables, "Database::updateCardinalities(2)");
  syncTbl.lock(Shared);

  Log::log("Update cardinalities\n");
  bool hit = false;

  try {
    // Establish the record cardinality for each table. Abandon the effort
    // if a forced scavenge operation is pending.

    for (Table *table = tableList; (table && scavengeForced == 0);
         table = table->next) {
      uint64 cardinality = table->cardinality;

      if (cardinality != table->priorCardinality) {
        if (!hit) {
          if (!updateCardinality)
            updateCardinality = prepareStatement(
                "update system.tables set cardinality=? where schema=? and "
                "tablename=?");

          hit = true;
        }

        updateCardinality->setLong(1, cardinality);
        updateCardinality->setString(2, table->schemaName);
        updateCardinality->setString(3, table->name);
        updateCardinality->executeUpdate();
        table->priorCardinality = cardinality;
      }
    }
  } catch (...) {
  }

  syncTbl.unlock();
  syncDDL.unlock();

  try {
    commitSystemTransaction();
  } catch (...) {
    // Ignores any errors from committing the updates of the cardinalities
    // Situations where this might happen can be due to problems with
    // writing to the stream log
  }
}

void Database::sync() {
  cache->syncFile(dbb, "sync");
  tableSpaceManager->sync();
}

void Database::setIOError(SQLException *exception) {
  Sync sync(&syncObject, "Database::setIOError");
  sync.lock(Exclusive);
  ++pendingIOErrors;
  ioError = exception->getText();
  pendingIOErrorCode = exception->getSqlcode();
}

void Database::clearIOError(void) {
  Sync sync(&syncObject, "Database::clearIOError");
  sync.lock(Exclusive);
  --pendingIOErrors;
}

void Database::preUpdate() {
  if (pendingIOErrors)
    throw SQLError(pendingIOErrorCode, "Pending I/O error: %s",
                   (const char *)ioError);

  streamLog->preUpdate();
}

void Database::setRecordMemoryMax(uint64 value) {
  if (configuration) {
    configuration->setRecordMemoryMax(value);
    recordMemoryMax = configuration->recordMemoryMax;
    recordScavengeThreshold = configuration->recordScavengeThreshold;
    recordScavengeFloor = configuration->recordScavengeFloor;
  }
}

void Database::setRecordScavengeThreshold(int value) {
  if (configuration) {
    configuration->setRecordScavengeThreshold(value);
    recordScavengeThreshold = configuration->recordScavengeThreshold;
    recordScavengeFloor = configuration->recordScavengeFloor;
  }
}

void Database::setRecordScavengeFloor(int value) {
  if (configuration) {
    configuration->setRecordScavengeFloor(value);
    recordScavengeFloor = configuration->recordScavengeFloor;
  }
}

void Database::checkRecordScavenge(void) {
  // Signal a load-based scavenge if we are over the threshold

  if (scavengerThreadSleeping && !scavengerThreadSignaled) {
    Sync syncMem(&syncMemory, "Database::checkRecordScavenge");
    syncMem.lock(Exclusive);

    if (!scavengerThreadSignaled &&
        (recordMemoryControl->getCurrentMemory(MemMgrRecordData) >
         lastActiveMemoryChecked)) {
      // Start a new age generation regularly.  Note that since activeMemory
      // can go down due to a recent scavenge, it is possible for
      // lastGenerationMemory to be > recordMemoryControl->getCurrentMemory()

      if ((int64)(recordMemoryControl->getCurrentMemory(MemMgrRecordData) -
                  lastGenerationMemory) > (int64)recordScavengeMaxGroupSize) {
        // Let the scavenger run to prune records.
        // It will also retire records if recordScavengeThreshold has been
        // reached.

        INTERLOCKED_INCREMENT(currentGeneration);
        lastGenerationMemory =
            recordMemoryControl->getCurrentMemory(MemMgrRecordData);

        INTERLOCKED_INCREMENT(scavengerThreadSignaled);
        scavengerThreadWakeup();
      }

      else if (recordMemoryControl->getCurrentMemory(MemMgrRecordData) >=
               recordScavengeThreshold) {
        INTERLOCKED_INCREMENT(scavengerThreadSignaled);
        scavengerThreadWakeup();
      }

      lastActiveMemoryChecked =
          recordMemoryControl->getCurrentMemory(MemMgrRecordData);
    }
  }
}

// Signal the scavenger thread

void Database::signalScavenger(bool force) {
  Sync syncMem(&syncMemory, "Database::signalScavenger");
  syncMem.lock(Exclusive);

  if (scavengerThreadSleeping && !scavengerThreadSignaled) {
    INTERLOCKED_INCREMENT(scavengerThreadSignaled);

    if (force) INTERLOCKED_INCREMENT(scavengeForced);

    scavengerThreadWakeup();
  }
}

void Database::debugTrace(void) {
#ifdef STORAGE_ENGINE
  if (changjiang_debug_trace & FALC0N_TRACE_TRANSACTIONS)
    transactionManager->printBlockage();

  if (changjiang_debug_trace & FALC0N_SYNC_TEST) {
    SyncTest syncTest;
    syncTest.test();
  }

  if (changjiang_debug_trace & FALC0N_SYNC_OBJECTS) SyncObject::dump();

  if (changjiang_debug_trace & FALC0N_SYNC_HANDLER)
    if (syncHandler) syncHandler->dump();

  if (changjiang_debug_trace & FALC0N_REPORT_WRITES)
    tableSpaceManager->reportWrites();

  if (changjiang_debug_trace & FALC0N_FREEZE) Synchronize::freezeSystem();

  if (changjiang_debug_trace & FALC0N_TEST_BITMAP) Bitmap::unitTest();

  changjiang_debug_trace = 0;
#endif
}

void Database::pageCacheFlushed(int64 flushArg) {
  streamLog->pageCacheFlushed(flushArg);
}

JString Database::setLogRoot(const char *defaultPath, bool create) {
  bool error = false;
  char fullDefaultPath[PATH_MAX];
  const char *baseName;
  JString userRoot;

  // Construct a fully qualified path for the default stream log location.

  const char *p = strrchr(defaultPath, '.');
  JString defaultRoot =
      (p) ? JString(defaultPath, (int)(p - defaultPath)) : name;

  dbb->expandFileName((const char *)defaultRoot, sizeof(fullDefaultPath),
                      fullDefaultPath, &baseName);

  // If defined, streamLogDir is a valid, fully qualified path. Verify that
  // it is also a valid location for the stream log. Otherwise, use the default.

  if (!configuration->streamLogDir.IsEmpty()) {
    int errnum;

    userRoot = configuration->streamLogDir + baseName;

    if (fileStat_cj(JString(userRoot + ".cl1"), NULL, &errnum) != 0) {
      switch (errnum) {
        case 0:  // file exists, don't care if create == true
          break;

        case ENOENT:  // no file, but !create means file expected
          error = !create;
          break;

        default:  // invalid path or other error
          error = true;
          break;
      }
    }
  }

  if (!userRoot.IsEmpty() && !error)
    return userRoot.getString();
  else
    return defaultRoot.getString();
}

int Database::recoverGetNextLimbo(int xidSize, unsigned char *xid) {
  if (streamLog) return (streamLog->recoverGetNextLimbo(xidSize, xid));

  return 0;
}

void Database::flushWait(void) { cache->flushWait(); }

void Database::setLowMemory(uint64 spaceNeeded) {
  if (!backLog) {
    Sync lock(&syncScavenge, "Database::setLowMemory");
    lock.lock(Exclusive);

    if (!backLog) backLog = new BackLog(this, BACKLOG_FILE);
  }

  lowMemory = true;
  lowMemoryCount = MAX_LOW_MEMORY;
  //	this->transactionManager->setLowMemory(spaceNeeded);
}

void Database::clearLowMemory(void) {
  //	this->transactionManager->clearLowMemory();
  lowMemory = false;
}

}  // namespace Changjiang
