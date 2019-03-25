#ifndef _GAME_PARAMS_H_
#define _GAME_PARAMS_H_

#include <memory>

class Params;

std::unique_ptr<Params> CreateGameParams(void);

#endif
