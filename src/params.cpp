#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "io.h"
#include "params.h"

using std::string;

Params::Params(void) {
}

Params::~Params(void) {
}

void Params::AddParam(const string &name, ParamType ptype) {
  param_names_.push_back(name);
  param_types_.push_back(ptype);
  ParamValue v;
  v.s = "";
  v.i = 0;
  v.d = 0.0;
  v.set = false;
  param_values_.push_back(v);
}

int Params::GetParamIndex(const char *name) const {
  int i;
  int num_params = param_names_.size();
  for (i = 0; i < num_params; ++i) {
    if (! strcmp(param_names_[i].c_str(), name)) break;
  }
  if (i == num_params) {
    fprintf(stderr, "Unknown param name: %s\n", name);
    exit(-1);
  }
  return i;
}

void Params::ReadFromFile(const char *filename) {
  Reader reader(filename);
  string line;
  while (reader.GetLine(&line)) {
    if (line[0] == '#') continue;
    int len = line.size();
    int i;
    for (i = 0; i < len && line[i] != ' ' && line[i] != '\t'; ++i) ;
    if (i == len) {
      fprintf(stderr, "Couldn't find whitespace on line\n");
      exit(-1);
    }
    if (i == 0) {
      fprintf(stderr, "Initial whitespace on line\n");
      exit(-1);
    }
    string param_name(line, 0, i);
    int j = GetParamIndex(param_name.c_str());
    ParamType ptype = param_types_[j];
    while (i < len && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (i == len) {
      fprintf(stderr, "No value after whitespace on line\n");
      exit(-1);
    }
    ParamValue v;
    v.set = true;
    if (ptype == P_STRING) {
      v.s = string(line, i, len - i);
    } else if (ptype == P_INT) {
      if (sscanf(line.c_str() + i, "%i", &v.i) != 1) {
	fprintf(stderr, "Couldn't parse int value from line: %s\n",
		line.c_str());
	exit(-1);
      }
    } else if (ptype == P_DOUBLE) {
      if (sscanf(line.c_str() + i, "%lf", &v.d) != 1) {
	fprintf(stderr, "Couldn't parse double value from line: %s\n",
		line.c_str());
	exit(-1);
      }
    } else if (ptype == P_BOOLEAN) {
      if (! strcmp(line.c_str() + i, "true")) {
	v.i = 1;
      } else if (! strcmp(line.c_str() + i, "false")) {
	v.i = 0;
      } else {
	fprintf(stderr, "Couldn't parse boolean value from line: %s\n",
		line.c_str());
	exit(-1);
      }
    }
    param_values_[j] = v;
  }
}

string Params::GetStringValue(const char *name) const {
  int i = GetParamIndex(name);
  if (param_types_[i] != P_STRING) {
    fprintf(stderr, "Param %s not string\n", name);
    exit(-1);
  }
  return param_values_[i].s;
}

int Params::GetIntValue(const char *name) const {
  int i = GetParamIndex(name);
  if (param_types_[i] != P_INT) {
    fprintf(stderr, "Param %s not int\n", name);
    exit(-1);
  }
  return param_values_[i].i;
}

double Params::GetDoubleValue(const char *name) const {
  int i = GetParamIndex(name);
  if (param_types_[i] != P_DOUBLE) {
    fprintf(stderr, "Param %s not double\n", name);
    exit(-1);
  }
  return param_values_[i].d;
}

bool Params::GetBooleanValue(const char *name) const {
  int i = GetParamIndex(name);
  if (param_types_[i] != P_BOOLEAN) {
    fprintf(stderr, "Param %s not boolean\n", name);
    exit(-1);
  }
  return (bool)param_values_[i].i;
}

bool Params::IsSet(const char *name) const {
  int i = GetParamIndex(name);
  return param_values_[i].set;
}
