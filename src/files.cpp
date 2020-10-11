#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include "files.h"

using std::string;

string Files::old_cfr_base_ = "";
string Files::new_cfr_base_ = "";
string Files::static_base_ = "";

void Files::Init(void) {
  char hostname_buf[500];
  gethostname(hostname_buf, sizeof(hostname_buf));

  if (! strcmp(hostname_buf, "koele")) {
    old_cfr_base_ = "/data/poker2019/cfr";
    new_cfr_base_ = "/data/poker2019/cfr";
    static_base_ = "/data/poker2019/static";
  } else {
    // Specify where you want files to be written to and read from.
    // Default to current directory.
    // Static files are things like the card abstraction that are reused across multiple runs
    // of CFR.
    // The CFR directory contains the files for computed strategies.
    // You can use the same directory for static files and CFR files if you want.
    // (old_cfr_base and new_cfr_base should be the same.)
    old_cfr_base_ = ".";
    new_cfr_base_ = ".";
    static_base_ = ".";
  }
}

const char *Files::OldCFRBase(void) {
  if (old_cfr_base_ == "") {
    fprintf(stderr, "You forgot to call Files::Init()\n");
    exit(-1);
  }
  return old_cfr_base_.c_str();
}

const char *Files::NewCFRBase(void) {
  if (new_cfr_base_ == "") {
    fprintf(stderr, "You forgot to call Files::Init()\n");
    exit(-1);
  }
  return new_cfr_base_.c_str();
}

const char *Files::StaticBase(void) {
  if (static_base_ == "") {
    fprintf(stderr, "You forgot to call Files::Init()\n");
    exit(-1);
  }
  return static_base_.c_str();
}
