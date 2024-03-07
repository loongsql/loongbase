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

// Hdr.cpp: implementation of the Hdr class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <string.h>
#include "Engine.h"
#include "Hdr.h"
#include "BDB.h"
#include "Dbb.h"
#include "InversionPage.h"
#include "Database.h"
#include "DateTime.h"
#include "Index.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Hdr::Hdr() {}

Hdr::~Hdr() {}

void Hdr::create(Dbb *dbb, FileType type, TransId transId,
                 const char *logRoot) {
  Bdb *bdb = dbb->fakePage(HEADER_PAGE, PAGE_header, transId);
  BDB_HISTORY(bdb);
  Hdr *header = (Hdr *)bdb->buffer;
  header->pageSize = dbb->pageSize;
  header->odsVersion = dbb->odsVersion;
  header->odsMinorVersion = dbb->odsMinorVersion;
  header->inversion = -1;
  header->sequence = dbb->sequence + 1;
  header->state = HdrOpen;
  header->fileType = type;
  header->creationTime =
      (int32)(dbb->database->creationTime = DateTime::getNow());
  header->defaultIndexVersionNumber = INDEX_CURRENT_VERSION;
  header->haveIndexVersionNumber = true;

  if (logRoot)
    header->putHeaderVariable(dbb, hdrLogPrefix, (int)strlen(logRoot), logRoot);

  bdb->release(REL_HISTORY);
}

int Hdr::getHeaderVariable(Dbb *dbb, HdrVariable variable, int bufferSize,
                           char *buffer) {
  int length;

  for (UCHAR *p = (UCHAR *)this + dbb->pageSize;; p -= length) {
    HdrVariable var = (HdrVariable) * --p;
    length = *--p;
    length |= (*--p) << 8;

    if (var == hdrEnd) return -1;

    if (var == variable) {
      if (buffer) {
        int l = MIN(length, bufferSize);
        if (l) {
          memcpy(buffer, p - length, l);
          if (l < bufferSize - 1) buffer[l] = 0;
        }

        return length;
      }
    }
  }
}

void Hdr::putHeaderVariable(Dbb *dbb, HdrVariable variable, int size,
                            const char *buffer) {
  int length;
  UCHAR *q = NULL;

  for (UCHAR *p = (UCHAR *)this + dbb->pageSize;; p -= length) {
    HdrVariable var = (HdrVariable) * --p;
    length = *--p;
    length |= (*--p) << 8;

    if (var == hdrEnd) {
      if (!q) q = p + 3;
      *--q = variable;
      *--q = (UCHAR)size;
      *--q = (UCHAR)(size >> 8);
      p = (UCHAR *)buffer + size;

      for (int n = 0; n < size; ++n) *--q = *--p;

      *--q = hdrEnd;

      return;
    }

    if (var == variable)
      q = p + 3;
    else if (q) {
      *--q = var;
      *--q = (UCHAR)length;
      *--q = (UCHAR)(length >> 8);

      for (int n = 0; n < length; ++n) *--q = p[-n];
    }
  }
}

void Hdr::backup(EncodedDataStream *stream) {}

void Hdr::restore(EncodedDataStream *stream) {}

}  // namespace Changjiang
