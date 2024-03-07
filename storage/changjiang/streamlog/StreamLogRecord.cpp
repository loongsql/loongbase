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

// StreamLogRecord.cpp: implementation of the StreamLogRecord class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdarg.h>
#include "Engine.h"
#include "StreamLogRecord.h"
#include "StreamLog.h"
#include "StreamLogControl.h"
#include "Stream.h"
#include "StreamLogTransaction.h"

namespace Changjiang {

#define GET_BYTE                                            \
  ((control->input < control->inputEnd) ? *control->input++ \
                                        : control->getByte())

static const int FIXED_INT_LENGTH = 5;

#define BYTES_POS(n)     \
  ((n < (1 << 6))    ? 1 \
   : (n < (1 << 13)) ? 2 \
   : (n < (1 << 20)) ? 3 \
   : (n < (1 << 27)) ? 4 \
                     : 5)

#define BYTES_NEG(n)       \
  ((n >= -(1 << 6))    ? 1 \
   : (n >= -(1 << 13)) ? 2 \
   : (n >= -(1 << 20)) ? 3 \
   : (n >= -(1 << 27)) ? 4 \
                       : 5)

#define BYTE_COUNT(n) ((n >= 0) ? BYTES_POS(n) : BYTES_NEG(n))

#define BYTES_POS64(n)          \
  ((n < (1 << 6))           ? 1 \
   : (n < (1 << 13))        ? 2 \
   : (n < (1 << 20))        ? 3 \
   : (n < (1 << 27))        ? 4 \
   : (n < ((int64)1 << 34)) ? 5 \
   : (n < ((int64)1 << 41)) ? 6 \
   : (n < ((int64)1 << 48)) ? 7 \
   : (n < ((int64)1 << 55)) ? 8 \
                            : 9)

#define BYTES_NEG64(n)            \
  ((n >= -(1 << 6))           ? 1 \
   : (n >= -(1 << 13))        ? 2 \
   : (n >= -(1 << 20))        ? 3 \
   : (n >= -(1 << 27))        ? 4 \
   : (n >= -((int64)1 << 34)) ? 5 \
   : (n >= -((int64)1 << 41)) ? 6 \
   : (n >= -((int64)1 << 48)) ? 7 \
   : (n >= -((int64)1 << 55)) ? 8 \
                              : 9)

#define BYTE_COUNT64(n) ((n >= 0) ? BYTES_POS64(n) : BYTES_NEG64(n))

static const UCHAR lengthShifts[] = {0, 7, 14, 21, 28, 35, 42, 49, 56};
static const UCHAR tag[2] = {0, 0};

static int init();
static char printable[256];
static int initialized = init();

int init() {
  int n;
  memset(printable, '.', sizeof(printable));

  for (n = 'a'; n <= 'z'; ++n) printable[n] = n;

  for (n = 'A'; n <= 'Z'; ++n) printable[n] = n;

  for (n = '0'; n <= '9'; ++n) printable[n] = n;

  for (const char *p = "#!-_?+="; *p; ++p) printable[(UCHAR)*p] = *p;

  return 1;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

StreamLogRecord::StreamLogRecord() {
  transactionId = 0;
  tableSpaceId = 0;
  type = 0;
}

StreamLogRecord::~StreamLogRecord() {}

void StreamLogRecord::putInt(int32 value) {
  int count = BYTE_COUNT(value);
  UCHAR data[6], *p = data;

  while (--count > 0) *p++ = (value >> (lengthShifts[count])) & 0x7f;

  *p++ = value | LOW_BYTE_FLAG;
  putData((int)(p - data), data);
}

void StreamLogRecord::putInt64(int64 value) {
  int64 count = BYTE_COUNT64(value);
  UCHAR data[10], *p = data;

  while (--count > 0) *p++ = (UCHAR)(value >> (lengthShifts[count])) & 0x7f;

  *p++ = ((UCHAR)value) | LOW_BYTE_FLAG;
  putData((int)(p - data), data);
}

UCHAR *StreamLogRecord::putFixedInt(int value) {
  int count = FIXED_INT_LENGTH;
  UCHAR data[6], *p = data;

  while (--count > 0) *p++ = (value >> (lengthShifts[count])) & 0x7f;

  *p++ = value | LOW_BYTE_FLAG;
  putData((int)(p - data), data);

  return log->writePtr - FIXED_INT_LENGTH;
}

void StreamLogRecord::putFixedInt(int value, UCHAR *ptr) {
  ASSERT(log->writeBlock->data < ptr && ptr < log->writePtr);
  int count = FIXED_INT_LENGTH;
  UCHAR *p = ptr;

  while (--count > 0) *p++ = (value >> (lengthShifts[count])) & 0x7f;

  *p++ = value | LOW_BYTE_FLAG;
}

int StreamLogRecord::getInt() {
  UCHAR c = GET_BYTE;
  int number = (c & 0x40) ? -1 : 0;

  for (;;) {
    number = (number << 7) | (c & 0x7f);

    if (c & LOW_BYTE_FLAG) break;

    c = GET_BYTE;
  }

  return number;
}

int64 StreamLogRecord::getInt64() {
  UCHAR c = GET_BYTE;
  int64 number = (c & 0x40) ? -1 : 0;

  for (;;) {
    number = (number << 7) | (c & 0x7f);

    if (c & LOW_BYTE_FLAG) break;

    c = GET_BYTE;
  }

  return number;
}

int StreamLogRecord::getInt(const UCHAR **ptr) {
  const UCHAR *p = *ptr;
  UCHAR c = *p++;
  int number = (c & 0x40) ? -1 : 0;

  for (;;) {
    number = (number << 7) | (c & 0x7f);

    if (c & LOW_BYTE_FLAG) break;

    c = *p++;
  }

  *ptr = p;

  return number;
}

void StreamLogRecord::putData(uint32 length, const UCHAR *data) {
  log->putData(length, data);
}

void StreamLogRecord::startRecord() { log->startRecord(); }

void StreamLogRecord::putStream(Stream *stream) {
  putInt(stream->totalLength);

  for (Segment *segment = stream->segments; segment; segment = segment->next)
    log->putData(segment->length, (const UCHAR *)segment->address);
}

const UCHAR *StreamLogRecord::getData(int32 length) {
  return control->getData(length);
}

void StreamLogRecord::print() { printf("Log record type ?\n"); }

void StreamLogRecord::redo() {}

StreamLogTransaction *StreamLogRecord::getTransaction(TransId transactionId) {
  if (transactionId == 0) return NULL;

  StreamLogTransaction *transaction = log->findTransaction(transactionId);

  if (transaction) return transaction;

  transaction = log->getTransaction(transactionId);
  transaction->setStart(log->recordStart, log->writeBlock, log->writeWindow);

  return transaction;
}

void StreamLogRecord::pass1() {}

void StreamLogRecord::commit() {}

void StreamLogRecord::rollback() {}

void StreamLogRecord::pass2() {}

void StreamLogRecord::recoverLimbo(void) {}

int StreamLogRecord::byteCount(int value) { return BYTE_COUNT(value); }

char *StreamLogRecord::format(int length, const UCHAR *data, int tempLength,
                              char *temp) {
  int len = MIN(length, tempLength - 1);

  for (int n = 0; n < len; ++n) temp[n] = printable[data[n]];

  temp[len] = 0;

  return temp;
}

void StreamLogRecord::logPrint(const char *text, ...) {
  va_list args;
  va_start(args, text);
  char temp[1024];

  if (vsnprintf(temp, sizeof(temp) - 1, text, args) < 0)
    temp[sizeof(temp) - 1] = 0;

  if (log->recoveryPhase)
    printf("Recovery phase %d %s", log->recoveryPhase, temp);
  else
    printf("Log %s", temp);
}

}  // namespace Changjiang
