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

#pragma once

namespace Changjiang {

// Page Types

enum PageType {
  PAGE_any,
  PAGE_header = 1,
  PAGE_sections = 2,
  // PAGE_section		= 3,		 unused
  PAGE_record_locator = 4,  // was PAGE_section_index
  PAGE_btree = 5,
  // PAGE_btree_leaf	= 6,		unused
  PAGE_data = 7,
  PAGE_inventory = 8,
  PAGE_data_overflow = 9,
  PAGE_inversion = 10,
  PAGE_free = 11,
  PAGE_sequences = 12,
  PAGE_max
};

}  // namespace Changjiang
