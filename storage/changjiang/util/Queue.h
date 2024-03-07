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

// Queue.h: interface for the Queue class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "SyncObject.h"

namespace Changjiang {

template <class T>
class Queue {
 public:
  Queue() {
    first = NULL;
    last = NULL;
    count = 0;
  };

  ~Queue(){
      /***
      for (T *object; object = first;)
              {
              first = object->next;
              delete object;
              }
      ***/
  };

  void prepend(T *object) {
    if ((object->next = first))
      first->prior = object;
    else
      last = object;

    object->prior = NULL;
    first = object;
    ++count;
  };

  void append(T *object) {
    if ((object->prior = last))
      last->next = object;
    else
      first = object;

    object->next = NULL;
    last = object;
    ++count;
  };

  void appendAfter(T *object, T *item) {
    if ((object->next = item->next))
      object->next->prior = object;
    else
      last = object;

    item->next = object;
    object->prior = item;
    ++count;
  }

  void remove(T *object) {
    if (object->next)
      object->next->prior = object->prior;
    else
      last = object->prior;

    if (object->prior)
      object->prior->next = object->next;
    else
      first = object->next;

    --count;
  };

  T *first;
  T *last;
  SyncObject syncObject;
  int count;
};

}  // namespace Changjiang
