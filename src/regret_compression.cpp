#include <stdio.h>
#include <stdlib.h>

#include "rand.h"
#include "regret_compression.h"

// 0...49:        0...49
// 50...69:       50...59
// 70...99:       60...69
// 100...139:     70...79
// 140...189:     80...89
// 190...249:     90...99
// 250...319:     100...109
// 320...399:     110...119
// 400...489:     120...129
// 490...589:     130...139
// 590...739:     140...149
// 740...939:     150...159
// 940...1189:    160...169
// 1190...1489:   170...179
// 1490...1889:   180...189
// 1890...2389:   190...199
// 2390...3139:   200...209
// 3140...4139:   210...219
// 4140...5639:   220...229
// 5640...7639:   230...239
// 7640...10639:  240...249
// 10640...13139: 250...254
// >= 13140:      255
unsigned char CompressRegret(unsigned int r, double rnd,
			     unsigned int *uncompress) {
  double d;
  if (r < 50) {
    return (unsigned char)r;
  } else if (r < 70) {
    d = (r - 50) / 2.0 + 50.0;
  } else if (r < 100) {
    d = (r - 70) / 3.0 + 60.0;
  } else if (r < 140) {
    d = (r - 100) / 4.0 + 70.0;
  } else if (r < 190) {
    d = (r - 140) / 5.0 + 80.0;
  } else if (r < 250) {
    d = (r - 190) / 6.0 + 90.0;
  } else if (r < 320) {
    d = (r - 250) / 7.0 + 100.0;
  } else if (r < 400) {
    d = (r - 320) / 8.0 + 110.0;
  } else if (r < 490) {
    d = (r - 400) / 9.0 + 120.0;
  } else if (r < 590) {
    d = (r - 490) / 10.0 + 130.0;
  } else if (r < 740) {
    d = (r - 590) / 15.0 + 140.0;
  } else if (r < 940) {
    d = (r - 740) / 20.0 + 150.0;
  } else if (r < 1190) {
    d = (r - 940) / 25.0 + 160.0;
  } else if (r < 1490) {
    d = (r - 1190) / 30.0 + 170.0;
  } else if (r < 1890) {
    d = (r - 1490) / 40.0 + 180.0;
  } else if (r < 2390) {
    d = (r - 1890) / 50.0 + 190.0;
  } else if (r < 3140) {
    d = (r - 2390) / 75.0 + 200.0;
  } else if (r < 4140) {
    d = (r - 3140) / 100.0 + 210.0;
  } else if (r < 5640) {
    d = (r - 4140) / 150.0 + 220.0;
  } else if (r < 7640) {
    d = (r - 5640) / 200.0 + 230.0;
  } else if (r < 10640) {
    d = (r - 7640) / 300.0 + 240.0;
  } else if (r < 13140) {
    d = (r - 10640) / 500.0 + 250.0;
  } else {
    return (unsigned char)255;
  }
  // This is the fixed version of quantization.
  // below can't be 255, can it?
  int below = d;
  int above = below + 1;
  unsigned int below_uq = uncompress[below];
  unsigned int above_uq = uncompress[above];
  double frac = (r - below_uq) / (double)(above_uq - below_uq);
  if (rnd < frac) {
    return above;
  } else {
    return below;
  }
}

unsigned int UncompressRegret(unsigned char cr) {
  unsigned int r = cr;
  if (r < 50) {
    return r;
  } else if (r < 60) {
    return (r - 50) * 2 + 50;
  } else if (r < 70) {
    return (r - 60) * 3 + 70;
  } else if (r < 80) {
    return (r - 70) * 4 + 100;
  } else if (r < 90) {
    return (r - 80) * 5 + 140;
  } else if (r < 100) {
    return (r - 90) * 6 + 190;
  } else if (r < 110) {
    return (r - 100) * 7 + 250;
  } else if (r < 120) {
    return (r - 110) * 8 + 320;
  } else if (r < 130) {
    return (r - 120) * 9 + 400;
  } else if (r < 140) {
    return (r - 130) * 10 + 490;
  } else if (r < 150) {
    return (r - 140) * 15 + 590;
  } else if (r < 160) {
    return (r - 150) * 20 + 740;
  } else if (r < 170) {
    return (r - 160) * 25 + 940;
  } else if (r < 180) {
    return (r - 170) * 30 + 1190;
  } else if (r < 190) {
    return (r - 180) * 40 + 1490;
  } else if (r < 200) {
    return (r - 190) * 50 + 1890;
  } else if (r < 210) {
    return (r - 200) * 75 + 2390;
  } else if (r < 220) {
    return (r - 210) * 100 + 3140;
  } else if (r < 230) {
    return (r - 220) * 150 + 4140;
  } else if (r < 240) {
    return (r - 230) * 200 + 5640;
  } else if (r < 250) {
    return (r - 240) * 300 + 7640;
  } else {
    return (r - 250) * 500 + 10640;
  }
}

// 0...32767:       0...32767
// 32768...49152:   32768...40960
// 49153...81919:   40961...49153
// 81920...147455:  49154...57346
// 147456...278463: 57347...65535

unsigned short CompressRegretShort(unsigned int r, double rnd,
				   unsigned int *uncompress) {
  double d;
  if (r < 32768) {
    return (unsigned short)r;
  } else if (r < 49152) {
    d = (r - 32768) / 2.0 + 32768;
  } else if (r < 81920) {
    d = (r - 49152) / 4.0 + 40961;
  } else if (r < 147456) {
    d = (r - 81920) / 8.0 + 49154;
  } else if (r < 278464) {
    d = (r - 147456) / 16.0 + 57347;
  } else {
    return (unsigned short)65535;
  }
  int below = d;
  int above = below + 1;
  unsigned int below_uq = uncompress[below];
  unsigned int above_uq = uncompress[above];
  double frac = (r - below_uq) / (double)(above_uq - below_uq);
  if (rnd < frac) {
    return above;
  } else {
    return below;
  }
}

unsigned int UncompressRegretShort(unsigned short cr) {
  unsigned int r = cr;
  if (r < 32768) {
    return r;
  } else if (r < 40961) {
    return (r - 32768) * 2 + 32768;
  } else if (r < 49154) {
    return (r - 40961) * 4 + 49153;
  } else if (r < 57347) {
    return (r - 49154) * 8 + 81920;
  } else {
    return (r - 57347) * 16 + 147456;
  }
}
