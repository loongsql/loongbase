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

// EditString.h: interface for the EditString class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

namespace Changjiang {

enum FormatType {
  fmtString,
  fmtNumber,
  fmtDate,
  fmtTime,
  fmtBlob,
  fmtWrapped,
  fmtImage,
};

class Value;
class DateTime;

class EditString {
 public:
  char *formatDate(int32 date, char *output);
  char *format(const char *str, char *output);
  char *format(Value *value, char *string);
  char digit(int number, int pos, int length, bool blank);
  char *formatDate(DateTime date, char *string);
  char *formatString(Value *value, char *string);
  char *formatNumber(Value *value, char *expansion);
  char next();
  void reset();
  void parse();
  EditString(const char *string);
  virtual ~EditString();

  JString editString;
  const char *chars;
  int stringLength;
  FormatType type;
  QUAD number;
  int width, height;
  char last, quote;
  int digits, months, years, days, fractions, length, weekdays, hours, minutes,
      seconds, julians, meridians, numericMonths, currencySymbols, wrapped,
      zones;

 private:
  int pos, repeat;
  char *expansion;

 protected:
  char nextDigit();
};

}  // namespace Changjiang
