#include <stdio.h>
#include <stdlib.h>

#include "resolving_method.h"

const char *ResolvingMethodName(ResolvingMethod method) {
  if (method == ResolvingMethod::UNSAFE) {
    return "unsafe";
  } else if (method == ResolvingMethod::CFRD) {
    return "cfrd";
  } else if (method == ResolvingMethod::MAXMARGIN) {
    return "maxmargin";
  } else if (method == ResolvingMethod::COMBINED) {
    return "combined";
  } else {
    fprintf(stderr, "Unknown resolving method\n");
    exit(-1);
  }
}

