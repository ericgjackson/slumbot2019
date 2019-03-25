#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "rand.h"

#ifdef CYGWIN
#define srand48 srand
#endif

void InitRandFixed(void) {
  srand48(0);
}

void InitRand(void) {
  srand48(time(0));
}

void SeedRand(int s) {
  srand48(s);
}

// Generates a random integer in the *closed* interval [lower,upper]
int RandBetween(int lower, int upper) {
  double frac;

  // frac must be less than 1.0
#ifdef CYGWIN
  frac = ((float)rand()/((float)RAND_MAX + 1.0));
#else
  frac = drand48();
#endif
  return lower + (int)(frac * ((double)(upper + 1 - lower)));
}

// Will never be one
double RandZeroToOne(void) {
#ifdef CYGWIN
  return ((float)rand()/((float)RAND_MAX + 1.0));
#else
  return drand48();
#endif
}
