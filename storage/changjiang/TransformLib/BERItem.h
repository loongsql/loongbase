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

// BERItem.h: interface for the BERItem class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

namespace Changjiang {

class BERDecode;
class BEREncode;

class BERItem {
 public:
  static BERItem *parseItem(BERDecode *decode);
  BERItem(BERDecode *decode);
  void encodeComponent(BEREncode *encode, int component);
  static int getComponentLength(int component);
  void encodeInteger(BEREncode *encode, int number);
  BERItem(UCHAR itemClass, int number, int length, const UCHAR *stuff);
  virtual void encode(BEREncode *encode);
  virtual void encodeContent(BEREncode *encode, int length);
  int encodeLength(BEREncode *encode);
  void encodeIdentifier(BEREncode *encode);
  static int getIntegerLength(int integer);
  virtual int getEncodingLength();
  virtual int getContentLength();
  virtual int getLengthLength();
  static int getLengthLength(int len);
  virtual int getIdentifierLength();
  static int getIdentifierLength(int tagNumber);
  virtual void printContents();
  BERItem *findChild(int position);
  virtual void prettyPrint(int column);
  void addChild(BERItem *child);
  // BERItem(BERDecode *decode, UCHAR itemClass, int itemNumber);
  virtual ~BERItem();

  BERItem *children;
  BERItem *sibling;
  BERItem *lastChild;
  UCHAR tagClass;
  int tagNumber;
  int contentLength;
  int parseLength;
  const UCHAR *content;
};

}  // namespace Changjiang
