#ifndef _RESOLVING_METHOD_H_
#define _RESOLVING_METHOD_H_

enum class ResolvingMethod { UNSAFE, CFRD, MAXMARGIN, COMBINED };
const char *ResolvingMethodName(ResolvingMethod method);

#endif
