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

/*
 *	PROGRAM:		Virtual Data Manager
 *	MODULE:			Stack.h
 *	DESCRIPTION:	Encapsulated stack
 *
 * copyright (c) 1997 by James A. Starkey
 */

#pragma once

namespace Changjiang {

#define FOR_STACK(type, child, stack)            \
  {                                              \
    for (Stack *pos = (stack)->getTop(); pos;) { \
      type child = (type)(stack)->getPrior(&pos);
#define END_FOR \
  }             \
  }

class Stack {
 public:
  bool isMember(void *object);
  bool equal(Stack *stack);
  bool insertOrdered(void *object);
  void clear();
  bool isMark(void *mark);
  int count();
  virtual ~Stack();
  bool isEmpty();
  void *peek();
  void pop(void *mark);
  void *mark();

  Stack();

  void push(void *object);
  void *pop();
  Stack *getTop();
  void *getPrior(Stack **);

 protected:
  Stack *prior;
  void *object;
};

}  // namespace Changjiang
