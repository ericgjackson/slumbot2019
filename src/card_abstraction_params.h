#ifndef _CARD_ABSTRACTION_PARAMS_H_
#define _CARD_ABSTRACTION_PARAMS_H_

#include <memory>

class Params;

std::unique_ptr<Params> CreateCardAbstractionParams(void);

#endif
