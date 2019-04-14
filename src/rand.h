#ifndef _RAND_H_
#define _RAND_H_

void InitRand(void);
void InitRandFixed(void);
void SeedRand(int s);
// Generates a uniformly distributed random integer in the *closed* interval [lower,upper]
int RandBetween(int lower, int upper);
// Will never be one.  But if you cast it to a float, it might become one.
double RandZeroToOne(void);

#endif
