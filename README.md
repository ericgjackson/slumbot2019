# slumbot2019
Implementations of CFR for solving a variety of Holdem-like poker games

Supports both CFR+ and MCCFR.  CFR+ is an algorithm that traverses the entire
game tree on each iteration.  It has the best convergence properties.  It was
used to solve Limit Holdem.

MCCFR is a family of sampling variants of CFR.  It is most useful for large
games where CFR+ is not feasible.  It employs large numbers of very cheap
iterations that each traverse only a small portion of the game tree.

The two variants of MCCFR that we support are external sampling and targeted
CFR.

The CFR+ implementation allows the independent processing of "subgames".  This
has two applications:

1) For very large games you may wish to process subgames one at a time because
the entire game may not fit in memory on a single machine.  Moreover, you may
wish to distribute the processing across multiple machines.  (We don't current
support distributed solves, but the subgame capability is a first step towards
that.)

2) We support the dynamic resolving of endgames.  The typical use of this is
that you have a base system solved with a coarse abstraction and you want to
resolve a subgame at runtime with a superior abstraction (perhaps no
abstraction).

We support the resolving of endgames using three different methods.  The
three methods are the "unsafe" method, the CFR-D method and a new combined
approach that combines the unsafe and CFR-D methods.

The MCCFR implementation supports multiplayer.

We support the construction and usage of card abstractions.  CFR+ may be used
either with or without a card abstraction.  (MCCFR requires a card abstraction,
but may use the "null" abstraction which is lossless.)

CFR can be configured through several parameter files which control the game
being solved, the card abstraction, the betting abstraction, and solving
parameters.

A large number of variants of Holdem can be solved.  You can control the
number of players, the number of cards in the deck, the number of betting
rounds, the bet sizes supported in each round, the stack size, etc.
