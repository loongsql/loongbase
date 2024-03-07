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

// Option bits from Connection.hasRole()

#define HAS_ROLE 1
#define ROLE_ACTIVE 2
#define DEFAULT_ROLE 4
#define GRANT_OPTION 8

#define PRIV_MASK(priv) (1 << priv)
#define GRANT_SHIFT 10
#define GRANT_MASK(priv) (1 << (priv + GRANT_SHIFT))

enum PrivType {
  PrivSelect = 1,
  PrivInsert,
  PrivUpdate,
  PrivDelete,
  PrivGrant,  // deprecated
  PrivAlter,
  PrivExecute,
};

enum PrivObject {
  PrivTable,
  PrivView,
  PrivProcedure,
  PrivUser,
  PrivRole,
  PrivCoterie,
};

#define ALL_PRIVILEGES -1

}  // namespace Changjiang
