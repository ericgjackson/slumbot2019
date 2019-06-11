// How do I avoid concurrent updates to the same data?  Not an issue with unabstracted systems.
// This comes up in regular CFR+, no?
//
// How do I signal the workers to quit?  Could just set a boolean.  But when does the worker
// check it?  Repeatedly send requests?

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <memory>
#include <queue>

#include "betting_tree.h"
#include "board_tree.h"
#include "canonical_cards.h"
#include "hand_tree.h"
#include "vcfr.h"
#include "vcfr2.h"
#include "vcfr_state.h"

