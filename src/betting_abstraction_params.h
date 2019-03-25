#ifndef _BETTING_ABSTRACTION_PARAMS_H_
#define _BETTING_ABSTRACTION_PARAMS_H_

#include <memory>

class Params;

std::unique_ptr<Params> CreateBettingAbstractionParams(void);

#endif
