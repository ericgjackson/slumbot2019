#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "cards.h"
#include "cfr_values.h"
#include "game.h"
#include "hand_tree.h"
#include "reach_probs.h"

using std::shared_ptr;
using std::unique_ptr;

ReachProbs::ReachProbs(void) {
  int num_players = Game::NumPlayers();
  probs_.reset(new shared_ptr<double []>[num_players]);
}

// Allocates but does not fill in probs
void ReachProbs::Allocate(int p) {
  int max_card1 = Game::MaxCard() + 1;
  int num_enc = max_card1 * max_card1;
  probs_[p].reset(new double[num_enc]);
}

ReachProbs *ReachProbs::CreateRoot(void) {
  ReachProbs *reach_probs = new ReachProbs();
  int num_players = Game::NumPlayers();
  int max_card1 = Game::MaxCard() + 1;
  for (int p = 0; p < num_players; ++p) {
    reach_probs->Allocate(p);
    for (Card hi = 1; hi < max_card1; ++hi) {
      for (Card lo = 0; lo < hi; ++lo) {
	int enc = hi * max_card1 + lo;
	reach_probs->probs_[p][enc] = 1.0;
      }
    }
  }
  return reach_probs;
}

shared_ptr<ReachProbs []> ReachProbs::CreateSuccReachProbs(Node *node, int gbd, int lbd,
							   const CanonicalCards *hands,
							   const Buckets &buckets,
							   const CFRValues *sumprobs,
							   const ReachProbs &pred_reach_probs,
							   bool purify) {
  int num_succs = node->NumSuccs();
  if (num_succs == 0) {
    fprintf(stderr, "CreateSuccReachProbs() called with zero-succ node\n");
    exit(-1);
  }
  shared_ptr<ReachProbs []> succ_reach_probs(new ReachProbs[num_succs]);
  int num_players = Game::NumPlayers();
  int max_street = Game::MaxStreet();
  int max_card1 = Game::MaxCard() + 1;
  int st = node->Street();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  const Card *ms_board = st == max_street ? BoardTree::Board(max_street, gbd) : nullptr;
#if 0
  int lbd = BoardTree::LocalIndex(root_bd_st, root_bd, st, gbd);
  const CanonicalCards *hands = hand_tree->Hands(st, lbd);
#endif
  // Can happen when we are all-in.  Only succ is check.
  if (num_succs == 1) {
    for (int p = 0; p < num_players; ++p) {
      succ_reach_probs[0].Set(p, pred_reach_probs.Get(p));
    }
    return succ_reach_probs;
  }
  int pa = node->PlayerActing();
  for (int s = 0; s < num_succs; ++s) {
    for (int p = 0; p < num_players; ++p) {
      if (p == pa) {
	succ_reach_probs[s].Allocate(pa);
      } else {
	succ_reach_probs[s].Set(p, pred_reach_probs.Get(p));
      }
    }
  }
  int nt = node->NonterminalID();
  int dsi = node->DefaultSuccIndex();
  unique_ptr<double []> probs(new double[num_succs]);
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    const Card *cards = hands->Cards(i);
    Card hi = cards[0];
    Card lo = cards[1];
    int enc = hi * max_card1 + lo;
    int offset;
    if (buckets.None(st)) {
      offset = lbd * num_hole_card_pairs * num_succs + i * num_succs;
    } else {
      // Hands on final street were reordered by hand strength, but bucket lookup requires the
      // unordered hole card pair index
      unsigned int hcp;
      if (st == max_street) {
	// Is HCPIndex() too slow?  Should I precompute?
	hcp = HCPIndex(st, ms_board, cards);
      } else {
	hcp = i;
      }
      unsigned int h = ((unsigned int)gbd) * ((unsigned int)num_hole_card_pairs) + hcp;
      int b = buckets.Bucket(st, h);
      offset = b * num_succs;
    }
    if (purify) {
      sumprobs->PureProbs(st, pa, nt, offset, num_succs, probs.get());
    } else {
      sumprobs->RMProbs(st, pa, nt, offset, num_succs, dsi, probs.get());
    }
    for (int s = 0; s < num_succs; ++s) {
      succ_reach_probs[s].Set(pa, enc, pred_reach_probs.Get(pa, enc) * probs[s]);
    }
  }
  
  return succ_reach_probs;
}
