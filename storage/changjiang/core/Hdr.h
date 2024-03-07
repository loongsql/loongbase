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

// Hdr.h: interface for the Hdr class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Page.h"
#include "HdrState.h"

namespace Changjiang {

enum HdrVariable {
  hdrEnd = 0,
  hdrRepositoryName,
  hdrLogPrefix,
};

class Dbb;

class Hdr : public Page {
 public:
  Hdr();
  ~Hdr();

  void putHeaderVariable(Dbb *dbb, HdrVariable variable, int size,
                         const char *buffer);
  int getHeaderVariable(Dbb *dbb, HdrVariable variable, int bufferSize,
                        char *buffer);
  static void create(Dbb *dbb, FileType fileType, TransId transId,
                     const char *logRoot);
  void backup(EncodedDataStream *stream);
  void restore(EncodedDataStream *stream);

  short odsVersion;
  int32 pageSize;
  int32 inversion;
  int32 sequence;
  HdrState state;
  int32 sequenceSectionId;
  short fileType;
  short odsMinorVersion;
  int32 volumeNumber;
  uint32 creationTime;
  UCHAR utf8;
  uint32 logOffset;
  uint32 logLength;
  UCHAR haveIndexVersionNumber;
  UCHAR defaultIndexVersionNumber;
  UCHAR sequenceSectionFixed;
  int32 tableSpaceSectionId;
  uint32 streamLogBlockSize;
};

class HdrV2 : public Page {
 public:
  short odsVersion;
  unsigned short pageSize;
  int32 inversion;
  int32 sequence;
  HdrState state;
  int32 sequenceSectionId;
  short fileType;
  short odsMinorVersion;
  int32 volumeNumber;
  uint32 creationTime;
  UCHAR utf8;
  uint32 logOffset;
  uint32 logLength;
  UCHAR haveIndexVersionNumber;
  UCHAR defaultIndexVersionNumber;
};

}  // namespace Changjiang
