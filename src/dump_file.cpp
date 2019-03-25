#include <stdio.h>

#include "io.h"

int main(int argc, char *argv[]) {
  Reader reader(argv[1]);
  for (unsigned int i = 0; i < 2550; ++i) {
    double d = reader.ReadDoubleOrDie();
    printf("%f\n", d);
  }
  fflush(stdout);
}
