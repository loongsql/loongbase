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

// Timestamp.cpp: implementation of the Timestamp class.
//
//////////////////////////////////////////////////////////////////////

// copyright (c) 1999 - 2000 by James A. Starkey

#include <stdio.h>
#include <time.h>
#include <string.h>
#include "Engine.h"
#include "TimeStamp.h"
#include "SQLError.h"

namespace Changjiang {

#ifdef _WIN32
#define snprintf _snprintf
#endif

#define MS_PER_DAY (24 * 60 * 60 * 1000)

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

int TimeStamp::getString(int length, char *buffer) {
  // return DateTime::getString ("%Y-%m-%d %H:%M", length, buffer);
  tm time;
  getLocalTime(&time);
  snprintf(buffer, length, "%d-%.2d-%.2d %.2d:%.2d:%.2d", time.tm_year + 1900,
           time.tm_mon + 1, time.tm_mday, time.tm_hour, time.tm_min,
           time.tm_sec);

  return (int)strlen(buffer);
}

int TimeStamp::getNanos() { return nanos; }

void TimeStamp::setNanos(int nanoseconds) { nanos = nanoseconds; }

int TimeStamp::compare(TimeStamp when) {
  if (date > when.date)
    return 1;
  else if (date < when.date)
    return -1;

  return nanos - when.nanos;
}

/***
DateTime TimeStamp::getDate()
{
        tm time;
        getLocalTime (&time);
        time.tm_sec = 0;
        time.tm_min = 0;
        time.tm_hour = 0;
        time.tm_isdst = -1;
        DateTime date;
        date.setSeconds (getSeconds (&time));

        return date;
}
***/

void TimeStamp::setDate(DateTime value) {
  date = value.getMilliseconds();
  nanos = 0;
}

}  // namespace Changjiang
