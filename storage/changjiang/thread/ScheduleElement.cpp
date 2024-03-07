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

// ScheduleElement.cpp: implementation of the ScheduleElement class.
//
//////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "ScheduleElement.h"

namespace Changjiang {

#ifdef _DEBUG
#undef THIS_FILE
static const char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ScheduleElement::ScheduleElement(const char **ptr) {
  next = NULL;
  const char *p = *ptr;
  from = 0;

  while (ISDIGIT(*p)) from = from * 10 + *p++ - '0';

  if (*p == '-') {
    to = 0;
    ++p;
    while (ISDIGIT(*p)) to = to * 10 + *p++ - '0';
  } else
    to = from;

  if (*p == ',') {
    ++p;
    next = new ScheduleElement(&p);
  }

  *ptr = p;
}

ScheduleElement::~ScheduleElement() {
  if (next) delete next;
}

int ScheduleElement::getNext(int start) {
  /***
  int best = (start > to) ? -1 : MAX (start, from);

  for (ScheduleElement *element = next; element; element = element->next)
          {
          int n = element->getNext (start);
          if (best == -1 || (n >= 0 && n < best))
                  best = n;
          }
  ***/

  int best = -1;

  for (ScheduleElement *element = this; element; element = element->next)
    if (start <= element->to) {
      int n = MAX(start, element->from);
      if (best == -1 || (n >= 0 && n < best)) best = n;
    }

  return best;
}

}  // namespace Changjiang
