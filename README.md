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

Set up a "static" directory and a "cfr" directory with enough disk space available.
Specify these paths in files.cpp.

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
../bin/run_rgbr ms1f3_params none_params mb1b1_params cfrps_params 8 200 avg
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
../bin/run_tcfr holdem_params nhs2_params mb1b1_params ecfr_params 8 0 1 100000000 1
```

## Evaluation

### Real-Game Best-Response

```
../bin/run_rgbr ms1f3_params none_params mb1b1_params cfrps_params 8 200 avg
```

### Head-to-head

First run MCCFR for another batch of 100 million iterations:

```
../bin/run_tcfr holdem_params nhs2_params mb1b1_params ecfr_params 8 1 2 100000000 1
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

TODO

## Disk-Based CFR+

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

CFR can be configured through several parameter files which control the game
being solved, the card abstraction, the betting abstraction, and solving
parameters.

A large number of variants of Holdem can be solved.  You can control the
number of players, the number of cards in the deck, the number of betting
rounds, the bet sizes supported in each round, the stack size, etc.

## Card Abstraction

## Betting Abstraction

