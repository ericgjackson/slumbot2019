#ifndef _REGRET_COMPRESSION_H_
#define _REGRET_COMPRESSION_H_

unsigned char CompressRegret(unsigned int r, double rnd,
			     unsigned int *uncompress);
unsigned int UncompressRegret(unsigned char cr);
unsigned short CompressRegretShort(unsigned int r, double rnd,
				   unsigned int *uncompress);
unsigned int UncompressRegretShort(unsigned short cr);

#endif
