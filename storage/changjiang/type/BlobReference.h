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

// BlobReference.h: interface for the BlobReference class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "JString.h"

namespace Changjiang {

#define ZERO_REPOSITORY_PLACE ((int32)0x80000000)

#ifndef _WIN32
#define __int64 long long
#endif

typedef __int64 QUAD;
typedef unsigned char UCHAR;

class Repository;

class BlobReference {
 public:
  int getReference(int size, UCHAR *buffer);
  void setReference(int length, UCHAR *buffer);
  int getReferenceLength();
  void copy(BlobReference *source);
#ifdef CHANGJIANGDB
  virtual Stream *getStream();
  void getReference(Stream *stream);
  void setReference(int length, Stream *stream);
  void setRepository(Repository *repo);
#endif
  virtual bool isBlobReference();
  virtual void unsetBlobReference();
  virtual void setBlobReference(JString repo, int volume, QUAD id);
  virtual void unsetData();
  BlobReference();
  virtual ~BlobReference();

  JString repositoryName;
  int repositoryVolume;
  QUAD blobId;
  Repository *repository;
  bool dataUnset;
};

}  // namespace Changjiang
