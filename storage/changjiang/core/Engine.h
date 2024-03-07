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

// copyright (c) 1999 - 2000 by James A. Starkey

#pragma once

#include <time.h>

#ifdef CHANGJIANGDB
#define MEMORY_MANAGER
#endif

#ifdef _LEAKS
#include <AFX.h>
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

#ifdef _WIN32
const char SEPARATOR = '\\';
#else
const char SEPARATOR = '/';
#endif

#ifdef MEMORY_MANAGER
#include "MemoryManager.h"
#endif

#ifdef NAMESPACE
namespace NAMESPACE {}  // namespace NAMESPACE
using namespace NAMESPACE;
#define START_NAMESPACE namespace NAMESPACE {
#define CLASS(cls)      \
  namespace NAMESPACE { \
  class cls;            \
  };
#define END_NAMESPACE }
#else
#define START_NAMESPACE
#define CLASS(cls) class cls;
#define END_NAMESPACE
#endif

namespace Changjiang {

#ifndef NULL
#define NULL 0
#endif

#define OFFSET(type, fld) (IPTR) & (((type)0)->fld)
#define MAX(a, b) ((a > b) ? a : b)
#define MIN(a, b) ((a < b) ? a : b)
#define ABS(n) (((n) >= 0) ? (n) : -(n))
#define MASK(n) (1 << (n))
#define ISLOWER(c) (c >= 'a' && c <= 'z')
#define ISUPPER(c) (c >= 'A' && c <= 'Z')
#define ISDIGIT(c) (c >= '0' && c <= '9')
#define UPPER(c) ((ISLOWER(c)) ? c - 'a' + 'A' : c)
#define ROUNDUP(n, b) ((((n) + (b)-1) / (b)) * (b))

#define ALIGN(ptr, b) ((UCHAR *)(((UIPTR)ptr + b - 1) / b * b))
#define SQLEXCEPTION SQLError

typedef int int32;
typedef unsigned int uint32;
typedef unsigned int uint;

#ifdef _WIN32

//#ifndef strcasecmp
#define strcasecmp stricmp
#define strncasecmp strnicmp
/*#define snprintf		_snprintf
#if (_MSC_VER < 1400)
#define vsnprintf		_vsnprintf
#endif*/
#define QUAD_CONSTANT(x) x##i64
#define I64FORMAT "%I64d"
//#endif

#ifdef _WIN64
typedef __int64 IPTR;
typedef unsigned __int64 UIPTR;
#define HAVE_IPTR
#endif

#define INTERLOCK_TYPE long

#else

#define __int64 long long
#define _stdcall
#define QUAD_CONSTANT(x) x##LL
#define I64FORMAT "%lld"
#endif

#ifndef HAVE_IPTR
typedef long IPTR;
typedef unsigned long UIPTR;
#endif

#ifndef UCHAR_DEFINED
#define UCHAR_DEFINED
typedef unsigned char UCHAR;
#endif

typedef unsigned long ULONG;
typedef unsigned short USHORT;

typedef short int16;
typedef unsigned short uint16;
typedef __int64 QUAD;
typedef unsigned __int64 UQUAD;
typedef __int64 int64;
typedef unsigned __int64 uint64;

// Standard Changjiang engine type definitions

typedef uint32 TransId;  // Transaction ID
typedef int64 RecordId;

//#define TXIDFORMAT			"%ld"

#ifndef INTERLOCK_TYPE
#define INTERLOCK_TYPE int
#endif

}  // namespace Changjiang

#ifdef CHANGJIANGDB
#include "Error.h"
#endif

#include "JString.h"
