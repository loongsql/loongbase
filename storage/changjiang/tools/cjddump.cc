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

#include <iostream>
#include <fstream>
#include <regex>

#include "my_getopt.h"
#include "print_version.h"
#include "welcome_copyright_notice.h"

using namespace std;

static string GetFileSuffix(const string &path) {
  string filename = path;
  const size_t last_slash_idx = filename.find_last_of(".");
  if (string::npos != last_slash_idx) {
    filename.erase(0, last_slash_idx + 1);
  }
  return filename;
}

static bool IsFileExist(const string &fileName) {
  ifstream infile(fileName);
  return infile.good();
}

static char *opt_cs_file = nullptr;
static char *opt_cs_path = nullptr;
static char *opt_delimiter = nullptr;
static int opt_pb(0), opt_pc(10), opt_ob(0), opt_oc(65536);

static struct my_option my_long_options[] = {
    {"file", 'f', "Dump changjiang specified file.", &opt_cs_file, &opt_cs_file,
     0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"delimiter", 'd', "Delimiter between objects.", &opt_delimiter,
     &opt_delimiter, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"path", 'p', "Dump changjiang table information.", &opt_cs_path,
     &opt_cs_path, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"help", '?', "Display this help message and exit.", 0, 0, 0, GET_NO_ARG,
     NO_ARG, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

static void short_usage_sub(void) {
  printf("Usage: %s -f filename [OPTIONS] \n", my_progname);
  printf("OR     %s -p tablepath\n\n", my_progname);
}

static void usage(void) {
  print_version();
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"));
  puts("Dumping structure and contents of changjiang files.");
  short_usage_sub();
  puts("The following options are given as the argument.");
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
} /* usage */

static char delimiter[16] = ",";

static bool get_one_option(int optid, const struct my_option *opt,
                           char *argument) {
  switch (optid) {
    case 'd':
      strcpy(delimiter, argument);
      opt_delimiter = delimiter;
      break;
    case '?':
      usage();
      exit(0);
      break;
  }
  return 0;
}

static int get_options(int *argc, char ***argv) {
  int ho_error;
  if ((ho_error = handle_options(argc, argv, my_long_options, get_one_option)))
    return (ho_error);
  return (0);
}

int main(int argc, char **argv) {
  int exit_code;

  MY_INIT("cjddump");

  opt_delimiter = delimiter;

  exit_code = get_options(&argc, &argv);
  if (exit_code) {
    exit(exit_code);
  }

  if (!opt_cs_file && !opt_cs_path) {
    usage();
    exit(0);
  }

  // dump file
  if (opt_cs_file) {
    string filepath = opt_cs_file;

    if (!IsFileExist(filepath)) {
      cout << "Error : The specified file does not exist!" << endl;
      exit(1);
    }

    string delimiter = opt_delimiter;

    string file_suffix = GetFileSuffix(filepath);
    if (file_suffix == "fts") {
      // Dbb::open
      // DumpTableSpaceFile
    } else if (file_suffix == "fl1" || file_suffix == "fl2") {
      // SerialLogFile::open
      // DumpSerialLogFile
    } else {
      cout << "Error : The specified file is unsupported!" << endl;
      exit(1);
    }
  } else {
    cout << "Error : The specified path is unsupported!" << endl;
    exit(1);
  }

  cout << endl;
  cout << endl;

  exit(0);
}
