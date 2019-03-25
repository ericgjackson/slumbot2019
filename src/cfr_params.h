#ifndef _CFR_PARAMS_H_
#define _CFR_PARAMS_H_

#include <memory>

class Params;

std::unique_ptr<Params> CreateCFRParams(void);

#endif
