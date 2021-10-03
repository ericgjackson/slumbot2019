# Slumbot2019

Implementations of Counterfactual Regret Minimization (CFR) for solving a variety of Holdem-like
poker games

## Features

* Supports both CFR+ and MCCFR.
* Supports resolving of endgames
* Supports creation of card and betting abstractions
* Supports head-to-head evaluation and real-game best-response
* Supports multiplayer

# Tutorial

## Prerequisites

You should have a version of gcc that supports C++ 17.  gcc 7.3 is known to work.

By default, the various programs will read and write from the current directory.  If
this is not desirable, edit the paths in Files::Init() in src/files.cpp.  I'll sometimes
keep "static" files in a different director that "cfr" files, but that's not necessary.

Build whatever binaries you need from the top level.  For example:

```
make bin/build_hand_value_tree
```

## CFR+

A simple example using a two street game solved with a small betting abstraction and no card
abstraction.

```
cd runs
../bin/build_hand_value_tree ms1f3_params
../bin/build_betting_tree ms1f3_params mb1b1_params 
../bin/run_cfrp ms1f3_params none_params mb1b1_params cfrps_params 8 1 200
../bin/run_rgbr ms1f3_params none_params mb1b1_params cfrps_params 8 200 avg raw
```

## MCCFR

Run external sampling on a four street game using the full 52-card deck but a crude card abstraction
and a simple betting abstraction.

```
cd runs
../bin/build_hand_value_tree holdem_params
../bin/build_betting_tree holdem_params mb1b1_params 
../bin/build_null_buckets holdem_params 0
../bin/build_null_buckets holdem_params 1
../bin/build_rollout_features holdem_params 2 hs 1.0 wmls 0.5
../bin/build_rollout_features holdem_params 3 hs 1.0 wmls 0.5
../bin/build_unique_buckets holdem_params 2 hs hs
../bin/build_unique_buckets holdem_params 3 hs hs
../bin/run_tcfr holdem_params nhs2_params mb1b1_params tcfr_params 8 0 1 100000000 1
```

The Targeted CFR implementation (in tcfr.cpp) is a little messy; there is a simpler implementation
of External CFR in ecfr.cpp.  Run it like so:

```
../bin/run_ecfr holdem_params nhs2_params mb1b1_params ecfr_params 8 0 1 100000000 1
```

## Evaluation

### Real-Game Best-Response

```
../bin/run_rgbr ms1f3_params none_params mb1b1_params cfrps_params 8 200 avg raw
```

### Head-to-head

First run MCCFR for another batch of 100 million iterations:

```
../bin/run_ecfr holdem_params nhs2_params mb1b1_params ecfr_params 8 1 2 100000000 1
```

You can compare the two checkpoints using head_to_head:

```
../bin/head_to_head holdem_params nhs2_params nhs2_params mb1b1_params mb1b1_params ecfr_params ecfr_params 0 1 10000 -1 false false
```

Alternatively you can use play:

```
../bin/play holdem_params nhs2_params nhs2_params mb1b1_params mb1b1_params ecfr_params ecfr_params 0 1 100000000
```

## Subgame Resolving

### Resolving All Subgames

Solve all the subgames and produce a new system that merges the base system with the resolved
subgames.

```
../bin/solve_all_subgames ms1f3_params none_params none_params none_params mb1b1_params mb1b1_params cfrps_params cfrps_params 1 200 200 unsafe cbrs card zerosum avg null mem 8
../bin/assemble_subgames ms1f3_params none_params none_params none_params mb1b1_params mb1b1_params cfrps_params cfrps_params cfrpsmu_params 1 200 200 unsafe
```

### Head-to-head Evaluation with Resolving

Compare a base system to a system with resolved subgames, sampling only some boards.

```
../bin/head_to_head holdem_params nhs2_params nhs2_params mb1b1_params mb1b1_params ecfr_params ecfr_params 0 1 10000 3 false true none_params mb1b1_params cfrps_params
```

### Resolving Methods

TODO

## Multiplayer

Multiplayer (i.e., more than two players) does not work with all features of this package.
For example, multiplayer is only supported by MCCFR and not by CFR+.  And you cannot compute
real-game best responses on multiplayer games.

Because the betting tree can explode with multiplayer games, it is often advisable to use
a reentrant betting tree with multiplayer.  See the "Betting Abstraction" section below.

## Disk-Based CFR+

TODO

## Asymmetric Betting Abstractions

TODO

# Discussion

CFR+ is the preferred algorithm when it is feasible, ideally with no card abstraction.  It exhibits
the best convergence to equilibrium.  However, CFR+ makes a full iteration over the game tree
on each iteration and may take too long for large games.

MCCFR is a sampling variant of CFR which makes it possible to come up with pretty good
approximations of equilibrium for large games.  We support External Sampling and Targeted CFR
which are good for large games.  Each iteration of MCCFR traverses a small fraction of the
game tree.

# Configuring CFR

## Parameter Files

CFR can be configured through several parameter files which control the game being solved, the card
abstraction, the betting abstraction, and solving parameters.

### Game Params

For an example, see runs/holdem_params or runs/ms1f3_params.

Specifies the basic rules of the game including the number of streets (betting rounds), the
size of the deck, the number of players and how many community cards are dealt on each street.

Notes:

* Although there is a parameter for the number of hole cards, the code assumes two hole cards
in many places.
* The turn and river are assumed to involve one community card being dealt.  Only the flop is
configurable, and the flop may only involve one, two or three community cards.
* Games involving "draws" like 5-card draw are not supported.

### Card Abstraction Params

Specifies the card abstraction.  For each street we specify a bucketing.  A bucketing is a way
of reducing the size of the card space by mapping raw hands into buckets which are collections
of very similar hands.

There are two reserved names for bucketing:

* "none": CFR+ can operate without a card abstraction.  To specify no abstraction, use "none"
for the bucketing.
* "null": the null abstraction only groups together groups of cards that are functionally equivalent
(sometimes called "isomorphic").  For example, the preflop hands AsKs and AhKh can safely be
grouped together in the same bucket.  MCCFR requires a card abstraction so use the "null"
abstraction if you want a "lossless" abstraction.

See the "Card Abstraction" section later for more information on card abstraction.

### Betting Abstraction Params

For an example, see runs/mb1b1_params.

Specify things like the stack size, and what bet sizes are allowed per street and how many bets.

### CFR Params

For an example, see runs/cfrps_params or runs/ecfr_params.

Specify whether you are using CFR+ or MCCFR and set other parameters that control solving.

## Card Abstraction

Use build_null_buckets to create the "null" bucketing".

You can also create your own card abstractions.

Use build_rollout_features to create a map associating hands with certain hand strength
related features.

Use build_unique_buckets or build_kmeans buckets to create a bucketing from features.

You may also wish to look into prify and combine_features.

## Betting Abstraction

Once you have create a betting abstraction parameter file, build the betting tree
with build_betting_tree:

```
../bin/build_betting tree <game params> <betting params>
```

You can also view your betting tree:

```
../bin/show_betting tree <game params> <betting params>
```

### Reentrant Betting Abstractions

For an example, see runs/mb1b1r3_params.

TODO
