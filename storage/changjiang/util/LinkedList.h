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
 *	MODULE:			LinkedList.h
 *	DESCRIPTION:	Generic Linked List
 *
 * copyright (c) 1997 - 2000 by James A. Starkey
 */

#pragma once

namespace Changjiang {

#define FOR_OBJECTS(type, child, list)                              \
  {                                                                 \
    for (LinkedList *pos = (list)->getHead(); (list)->more(pos);) { \
      type child = (type)(list)->getNext(&pos);
#define FOR_OBJECTS_BACKWARD(type, child, list)                              \
  {                                                                          \
    for (LinkedList *pos = (list)->getTail(); (list)->moreBackwards(pos);) { \
      type child = (type)(list)->getPrior(&pos);
#define END_FOR \
  }             \
  }

#ifndef NULL
#define NULL 0
#endif

class LinkedNode;

class LinkedList {
 public:
  void addressCheck(void *address);
  bool insertBefore(void *insertItem, void *item);

  LinkedList();
  ~LinkedList();

  void append(void *object);
  bool appendUnique(void *object);
  void clear();
  int count();
  bool deleteItem(void *object);

  LinkedList *getHead();
  bool more(LinkedList *node);
  void *getNext(LinkedList **node);

  LinkedList *getTail();
  void *getElement(int position);
  void *getPrior(LinkedList **);
  bool isEmpty();
  bool isMember(void *object);

  bool moreBackwards(LinkedList *);

 protected:
  LinkedNode *next, *prior;
  // void			*object;
};

class LinkedNode : public LinkedList {
 public:
  LinkedNode(void *object);
  ~LinkedNode();

  void *object;
};

inline LinkedList *LinkedList::getHead() { return (LinkedList *)next; }

inline bool LinkedList::more(LinkedList *node) { return (node != NULL); }

inline void *LinkedList::getNext(LinkedList **node) {
  void *object = ((LinkedNode *)(*node))->object;
  *node = (*node)->next;

  return object;
}

}  // namespace Changjiang
