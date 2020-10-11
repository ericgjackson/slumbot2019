#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>

#include "agent.h"
#include "betting_abstraction.h"
#include "betting_tree.h"
#include "betting_trees.h"
#include "board_tree.h"
#include "buckets.h"
#include "canonical.h"
#include "card_abstraction.h"
#include "cards.h"
#include "cfr_street_values.h"
#include "cfr_values.h"
#include "constants.h"
#include "disk_probs.h"
#include "dynamic_cbr.h"
#include "eg_cfr.h"
#include "game.h"
#include "hand_tree.h"
#include "subgame_utils.h"
#include "unsafe_eg_cfr.h"

using std::string;
using std::unique_ptr;

bool g_debug = false;

void Agent::Initialize(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
		       int it, int big_blind, int seed) {
  subgame_ba_ = nullptr;
  srand48_r(seed, &rand_buf_);
  big_blind_ = big_blind;
  small_blind_ = big_blind / 2;
  // The stack size in the actual game
  stack_size_ = big_blind_ * ba.StackSize();
  translation_method_ = 0;
  resolve_st_ = -1;
  int max_street = Game::MaxStreet();
  boards_.reset(new int[max_street + 1]);
  buckets_.reset(new Buckets(ca, false));
  betting_trees_.reset(new BettingTrees(ba));
  disk_probs_.reset(new DiskProbs(ca, ba, cc, *buckets_, betting_trees_->GetBettingTree(), it));
}

Agent::Agent(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
	     int it, int big_blind, int seed) {
  Initialize(ca, ba, cc, it, big_blind, seed);
}

Agent::Agent(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
	     const CardAbstraction *subgame_ca, const BettingAbstraction *subgame_ba,
	     const CFRConfig *subgame_cc, int it, int big_blind, int resolve_st, int seed) {
  Initialize(ca, ba, cc, it, big_blind, seed);
  subgame_ba_ = subgame_ba;
  resolve_st_ = resolve_st;
  if (resolve_st_ >= 0) {
    dynamic_cbr_.reset(new DynamicCBR(ca, cc, *buckets_, 1));
    subgame_buckets.reset(new Buckets(*subgame_ca, false));
    // 1 thread
    eg_cfr.reset(new UnsafeEGCFR(*subgame_ca, ca, ba, *subgame_cc, cc, *subgame_buckets, 1));
  }
}

void Agent::SetBuckets(int st, const Card *raw_board, const Card *raw_hole_cards,
		       int *buckets) {
  int num_hole_cards = Game::NumCardsForStreet(0);
  int num_board_cards = Game::NumBoardCards(st);
  Card canon_board[5];
  Card canon_hole_cards[2];
  CanonicalizeCards(raw_board, raw_hole_cards, st, canon_board, canon_hole_cards);
  for (int st1 = 0; st1 <= st; ++st1) {
    int bd = BoardTree::LookupBoard(canon_board, st1);
    boards_[st1] = bd;
    Card canon_cards[7];
    for (int i = 0; i < num_board_cards; ++i) {
      canon_cards[num_hole_cards + i] = canon_board[i];
    }
    for (int i = 0; i < num_hole_cards; ++i) {
      canon_cards[i] = canon_hole_cards[i];
    }
    int raw_hcp = HCPIndex(st1, canon_cards);
    int num_hole_card_pairs = Game::NumHoleCardPairs(st1);
    // What about final street?
    unsigned int h = ((unsigned int)bd) * ((unsigned int)num_hole_card_pairs) + raw_hcp;
    buckets[st1] = buckets_->Bucket(st1, h);
  }
}

Node *Agent::ChooseOurActionWithRaiseNode(Node *node, Node *raise_node, const int *buckets,
					  bool mapped_bet_to_closing_call) {
  fprintf(stderr, "ChooseOurActionWithRaiseNode\n");
  int raise_nt = raise_node->NonterminalID();
  int pa = raise_node->PlayerActing();
  double r;
  drand48_r(&rand_buf_, &r);
  int num_raise_node_succs = raise_node->NumSuccs();
  if (num_raise_node_succs > 0) {
    // Choose a response from the raise node's
    int st = raise_node->Street();
    int b = buckets[st];
    unique_ptr<double []> raise_node_probs(new double[num_raise_node_succs]);
    disk_probs_->Probs(pa, st, raise_nt, b, num_raise_node_succs, raise_node_probs.get());
    double cum = 0;
    int s;
    for (s = 0; s < num_raise_node_succs - 1; ++s) {
      cum += raise_node_probs[s];
      if (r < cum) break;
    }
    if (s != raise_node->CallSuccIndex() && s != raise_node->FoldSuccIndex()) {
      return raise_node->IthSucc(s);
    }
  }
  if (mapped_bet_to_closing_call) {
    // This is the case where we mapped an opponent's bet to a call (and we decided not to
    // raise).
    return node;
  }
  // If we're not raising, then we need to call or fold.  If we're not facing a bet, then
  // obviously we just call.
  int csi = node->CallSuccIndex();
  int fsi = node->FoldSuccIndex();
  if (fsi == -1) {
    return node->IthSucc(csi);
  }
  int st = node->Street();
  int b = buckets[st];
  int nt = node->NonterminalID();
  int num_succs = node->NumSuccs();
  unique_ptr<double []> probs(new double[num_succs]);
  disk_probs_->Probs(pa, st, nt, b, num_succs, probs.get());
  double call_prob = probs[csi];
  double fold_prob = probs[fsi];
  // How much to scale up the call and fold probs so that the scaled-up versions sum to 1.0.
  double scaling = 1.0 / (call_prob + fold_prob);
  // Do I need to draw a new random number?
  drand48_r(&rand_buf_, &r);
  if (r < call_prob * scaling) {
    return node->IthSucc(csi);
  } else {
    return node->IthSucc(fsi);
  }
}

Node *Agent::ChooseOurAction(Node *node, const int *buckets, bool mapped_bet_to_closing_call) {
  fprintf(stderr, "ChooseOurAction simple\n");
  if (mapped_bet_to_closing_call) {
    fprintf(stderr, "ChooseOurAction simple mbtcc\n");
  } else {
    fprintf(stderr, "ChooseOurAction simple not mbtcc\n");
  }
  if (mapped_bet_to_closing_call) {
    // This is the case where we mapped an opponent's bet to a call.
    return node;
  }
  int st = node->Street();
  int b = buckets[st];
  int nt = node->NonterminalID();
  int pa = node->PlayerActing();
  int num_succs = node->NumSuccs();
  unique_ptr<double []> probs(new double[num_succs]);
  disk_probs_->Probs(pa, st, nt, b, num_succs, probs.get());
  double r;
  drand48_r(&rand_buf_, &r);
  double cum = 0;
  int s = 0;
  for (s = 0; s < num_succs - 1; ++s) {
    cum += probs[s];
    if (r < cum) break;
  }
  fprintf(stderr, "ChooseOurAction: chose succ %i r %f cum %f\n", s, r, cum);
  Node *ret = node->IthSucc(s);
  fprintf(stderr, "  ret st %i pa %i nt %i\n", ret->Street(), ret->PlayerActing(),
	  ret->NonterminalID());
  return ret;
}

// Assume SetBuckets() has been called.
Node *Agent::ChooseOurAction(Node *node, Node *raise_node, const int *buckets,
			     bool mapped_bet_to_closing_call) {
  if (raise_node) {
    return ChooseOurActionWithRaiseNode(node, raise_node, buckets, mapped_bet_to_closing_call);
  } else {
    return ChooseOurAction(node, buckets, mapped_bet_to_closing_call);
  }
}

// In order to do translation, find the two succs that most closely match
// the current action.
void Agent::GetTwoClosestSuccs(Node *node, int actual_bet_to, int *below_succ, int *below_bet_to,
			       int *above_succ, int *above_bet_to) {
  int num_succs = node->NumSuccs();
  // Want to find closest bet below and closest bet above
  int csi = node->CallSuccIndex();
  int fsi = node->FoldSuccIndex();
  if (g_debug) {
    fprintf(stderr, "s %i pa %i nt %i ns %i fsi %i csi %i\n", node->Street(),
	    node->PlayerActing(), node->NonterminalID(), node->NumSuccs(),
	    fsi, csi);
  }
  *below_succ = -1;
  *below_bet_to = -1;
  *above_succ = -1;
  *above_bet_to = -1;
  int best_below_diff = kMaxInt;
  int best_above_diff = kMaxInt;
  for (int s = 0; s < num_succs; ++s) {
    if (s == fsi) continue;
    int this_bet_to = node->IthSucc(s)->LastBetTo() * small_blind_;
    int diff = this_bet_to - actual_bet_to;
    if (g_debug) {
      fprintf(stderr, "s %i this_bet_to %i actual_bet_to %i diff %i\n", s,
	      this_bet_to, actual_bet_to, diff);
    }
    if (diff <= 0) {
      if (-diff < best_below_diff) {
	best_below_diff = -diff;
	*below_succ = s;
	*below_bet_to = this_bet_to;
      }
    } else {
      if (diff < best_above_diff) {
	best_above_diff = diff;
	*above_succ = s;
	*above_bet_to = this_bet_to;
      }
    }
  }
  if (g_debug) {
    fprintf(stderr, "Best below %i diff %i\n", *below_succ, best_below_diff);
    fprintf(stderr, "Best above %i diff %i\n", *above_succ, best_above_diff);
  }
}

double Agent::BelowProb(int actual_bet_to, int below_bet_to, int above_bet_to,
			int actual_pot_size) {
  double below_prob;
  if (translation_method_ == 0 || translation_method_ == 1) {
    // Express bet sizes as fraction of pot
    int last_bet_to = actual_pot_size / 2;
    int actual_bet = actual_bet_to - last_bet_to;
    double d_actual_pot_size = actual_pot_size;
    double actual_frac = actual_bet / d_actual_pot_size;
    // below_bet could be negative, I think, so I make all these bet_to
    // quantities signed integers.
    int below_bet = below_bet_to - last_bet_to;
    double below_frac = below_bet / d_actual_pot_size;
    if (below_frac < -1.0) {
      fprintf(stderr, "below_frac %f\n", below_frac);
      exit(-1);
    }
    if (g_debug) fprintf(stderr, "actual_frac %f\n", actual_frac);
    if (g_debug) fprintf(stderr, "below_frac %f\n", below_frac);
    int above_bet = above_bet_to - last_bet_to;
    double above_frac = above_bet / d_actual_pot_size;
    if (g_debug) fprintf(stderr, "above_frac %f\n", above_frac);
    below_prob =
      ((above_frac - actual_frac) *
       (1.0 + below_frac)) /
      ((above_frac - below_frac) *
       (1.0 + actual_frac));
    if (translation_method_ == 1) {
      if (g_debug) fprintf(stderr, "Raw below prob: %f\n", below_prob);
      // Translate to nearest
      if (below_prob < 0.5) below_prob = 0;
      else                  below_prob = 1.0;
    }
    if (g_debug) fprintf(stderr, "Below prob: %f\n", below_prob);
  } else {
    fprintf(stderr, "Unknown translation method %i\n", translation_method_);
    exit(-1);
  }
  return below_prob;
}

// At least one of below_succ and above_succ is not -1.
int Agent::ChooseBetweenBetAndCall(Node *node, int below_succ, int above_succ, int actual_bet_to,
				   int actual_pot_size, Node **raise_node) {
  int selected_succ = -1;
  *raise_node = nullptr;

  int call_succ = node->CallSuccIndex();
  int smallest_bet_succ = -1;
  if (below_succ == call_succ) {
    if (above_succ != -1) {
      smallest_bet_succ = above_succ;
    }
  } else {
    // Some corner cases here.
    int num_succs = node->NumSuccs();
    for (int s = 0; s < num_succs; ++s) {
      if (s == call_succ) continue;
      if (s == node->FoldSuccIndex()) continue;
      smallest_bet_succ = s;
      break;
    }
  }
  Node *smallest_bet_node = nullptr;
  if (smallest_bet_succ != -1) {
    smallest_bet_node = node->IthSucc(smallest_bet_succ);
  }

  // Sometimes we have only one valid succ.
  if (above_succ == -1) {
    selected_succ = below_succ;
  } else if (below_succ == -1) {
    selected_succ = above_succ;
  } else {
    // We have two valid succs
    int below_bet_to = node->IthSucc(below_succ)->LastBetTo() * small_blind_;
    int above_bet_to = node->IthSucc(above_succ)->LastBetTo() * small_blind_;
    double below_prob = BelowProb(actual_bet_to, below_bet_to, above_bet_to, actual_pot_size);
    double r;
    // drand48_r(&rand_bufs_[node->PlayerActing()], &r);
    drand48_r(&rand_buf_, &r);
    if (r < below_prob) {
      selected_succ = below_succ;
    } else {
      selected_succ = above_succ;
    }
    if (g_debug) fprintf(stderr, "ChooseBetweenBetAndCall s %i r %f\n", selected_succ, r);
  }
  if (selected_succ == call_succ) {
    // Don't use above_succ; it might be kMaxUInt
    *raise_node = smallest_bet_node;
  }
  return selected_succ;
}

int Agent::ChooseBetweenTwoBets(Node *node, int below_succ, int above_succ, int actual_bet_to,
				int actual_pot_size) {
  if (above_succ == -1) {
    // Can happen if we do not have all-ins in our betting abstraction
    if (g_debug) {
      fprintf(stderr, "above_succ -1; selected below bet; succ %i\n", below_succ);
    }
    return below_succ;
  } else if (below_succ == -1) {
    // There should always be a below succ.  All abstractions always
    // allow check and call.
    fprintf(stderr, "No below succ?!?\n");
    exit(-1);
  } else {
    int below_bet_to = node->IthSucc(below_succ)->LastBetTo() * small_blind_;
    int above_bet_to = node->IthSucc(above_succ)->LastBetTo() * small_blind_;
    // Opponent's bet size is between two bets in our abstraction.
    double below_prob = BelowProb(actual_bet_to, below_bet_to, above_bet_to, actual_pot_size);
    double r;
    // Do I need a separate rand_buf_ for each player?
    // drand48_r(&rand_bufs_[node->PlayerActing()], &r);
    drand48_r(&rand_buf_, &r);
    if (g_debug) fprintf(stderr, "below_prob %f r %f\n", below_prob, r);
    if (r < below_prob) {
      if (g_debug) fprintf(stderr, "Selected below bet; succ %i\n", below_succ);
      return below_succ;
    } else {
      if (g_debug) fprintf(stderr, "Selected above bet; succ %i\n", above_succ);
      return above_succ;
    }
  }
}

// Map the opponent's actual bet to an action in our abstraction.  We may map a bet to a
// check/call.
int Agent::ChooseOppAction(Node *node, int below_succ, int above_succ, int actual_bet_to,
			   int actual_pot_size, Node **raise_node) {
  int call_succ = node->CallSuccIndex();
  if (below_succ == call_succ || above_succ == call_succ) {
    if (g_debug) fprintf(stderr, "Choosing between bet and call\n");
    return ChooseBetweenBetAndCall(node, below_succ, above_succ, actual_bet_to, actual_pot_size,
				   raise_node);
  } else {
    if (g_debug) fprintf(stderr, "Choosing between two bets\n");
    *raise_node = nullptr;
    return ChooseBetweenTwoBets(node, below_succ, above_succ, actual_bet_to, actual_pot_size);
  }
}

Node *Agent::ProcessAction(const string &action, int we_p, const int *buckets, Node **raise_node,
			   bool *mapped_bet_to_closing_call) {
  Node *node = betting_trees_->GetBettingTree()->Root();
  int len = action.size();
  int actual_bet_to = big_blind_;
  int st = 0;
  int pa = 1;
  bool call_ends_street = false;
  *raise_node = nullptr;
  *mapped_bet_to_closing_call = false;
  int i = 0;
  fprintf(stderr, "ProcessAction %s\n", action.c_str());
  while (i < len) {
    fprintf(stderr, "aaa1 %s %i %s\n", action.c_str(), i, action.c_str() + i);
    fprintf(stderr, "nst %i pa %i nt %i\n", node->Street(), node->PlayerActing(),
	    node->NonterminalID());
#if 0
    // This can happen when we map an opponent's bet to a street-ending call.
    if (st != node->Street()) {
      fprintf(stderr, "st %i node st %i: exiting\n", st, node->Street());
      exit(-1);
    }
#endif
    char c = action[i];
    if (c == 'c') {
      // In the case where we mapped an opponent's bet to a street-closing call, then we have
      // a "c" here that does not correspond to any transition in the betting tree.  So
      // consume the "c" and move on.
      if (! *mapped_bet_to_closing_call) {
	int csi = node->CallSuccIndex();
	node = node->IthSucc(csi);
      }
      ++i;
      if (call_ends_street) {
	st += 1;
	fprintf(stderr, "i %i st advancing\n", i);
	pa = 0;
	call_ends_street = false;
	if (st == 4) break;
      } else {
	call_ends_street = true;
	pa = pa^1;
      }
      *raise_node = nullptr;
      *mapped_bet_to_closing_call = false;
    } else if (c == 'f') {
      int fsi = node->FoldSuccIndex();
      node = node->IthSucc(fsi);
      ++i;
      *raise_node = nullptr;
      *mapped_bet_to_closing_call = false;
      break;
    } else if (c == 'r') {
      ++i;
      int j = i;
      while (i < len && action[i] >= '0' && action[i] <= '9') ++i;
      string str(action, j, i-j);
      int new_actual_bet_to;
      if (sscanf(str.c_str(), "%i", &new_actual_bet_to) != 1) {
	fprintf(stderr, "Couldn't parse: %s\n", action.c_str());
	return nullptr;
      }
      fprintf(stderr, "aaa2 %s %i\n", action.c_str(), new_actual_bet_to);

      if (pa == we_p) {
	fprintf(stderr, "Our action\n");
	node = ChooseOurAction(node, *raise_node, buckets, *mapped_bet_to_closing_call);
	*raise_node = nullptr;
	*mapped_bet_to_closing_call = false;
      } else {
	fprintf(stderr, "Opp action\n");
	// Translation code
	// Do we need to pass current actual bet to (or pot size) down?  It's not going to
	// match the betting tree.
	int below_bet_to, above_bet_to;
	int below_succ, above_succ;
	// Find the two closest "bets" in our abstraction.  One will have a
	// bet size <= the actual bet size; one will have a bet size >= the
	// actual bet size.  We treat a check/call as a bet of size zero.
	// In some cases, there are not two possible succs in which case either
	// below_succ or above_succ will be -1.  In some cases there will not
	// be any possible bets in which case the below succ will be the call
	// succ and the above succ will be -1.
	GetTwoClosestSuccs(node, new_actual_bet_to, &below_succ, &below_bet_to,
			   &above_succ, &above_bet_to);
	if (below_succ == -1 && above_succ == -1) {
	  fprintf(stderr, "below_succ and above_succ -1\n");
	  if (node->Terminal()) {
	    fprintf(stderr, "Terminal node\n");
	  }
	  exit(-1);
	}
	if (g_debug) {
	  fprintf(stderr, "Two closest: %i %i\n", below_succ, above_succ);
	}
	
	int actual_pot_size = 2 * actual_bet_to;
	int succ = ChooseOppAction(node, below_succ, above_succ, new_actual_bet_to, actual_pot_size,
				   raise_node);
	if (call_ends_street && succ == node->CallSuccIndex()) {
	  // If 1) we mapped an opponent's bet to a call, and 2) that call ends the current
	  // street, then set *mapped_bet_to_closing_call to true
	  *mapped_bet_to_closing_call = true;
	} else {
	  *mapped_bet_to_closing_call = false;
	}
	fprintf(stderr, "Translated to opp succ %i\n", succ);
	node = node->IthSucc(succ);
#if 0
	Node *prior_node = node;
	// Why did I used to have this?
	int actual_bet_size = (node->LastBetTo() - prior_node->LastBetTo()) * small_blind_;
	actual_bet_to += actual_bet_size;
#endif
      }
      actual_bet_to = new_actual_bet_to;
      fprintf(stderr, "aaa3 actual_bet_to %i\n", actual_bet_to);
      call_ends_street = true;
      pa = pa^1;
    } else if (c == '/') {
      ++i;
    } else {
      fprintf(stderr, "Couldn't parse: %s\n", action.c_str());
      exit(-1);
    }
  }
  if (i != len) {
    fprintf(stderr, "i %i len %i action %s\n", i, len, action.c_str());
    exit(-1);
  }
  return node;
}

// Pass in root_bd.  Turn root_bd for turn and river, or switch to river root_bd for river?
// Probably need to pass around global board and map to local board.
void Agent::ReadSumprobsFromDisk(Node *node, int p, const Card *board, CFRValues *values) {
  if (node->Terminal()) return;
  int num_succs = node->NumSuccs();
  if (num_succs > 1 && node->PlayerActing() == p) {
    int nt = node->NonterminalID();
    int st = node->Street();
    int num_buckets = buckets_->NumBuckets(st);
    int num = num_buckets * num_succs;
    unique_ptr<double []> bucket_probs(new double[num]);
    for (int b = 0; b < num_buckets; ++b) {
      disk_probs_->Probs(p, st, nt, b, num_succs, bucket_probs.get());
    }
    CFRStreetValues<double> *street_values =
      dynamic_cast<CFRStreetValues<double> *>(values->StreetValues(st));
    double *probs = street_values->AllValues(p, nt);
    int root_gbd = boards_[st];
    int start_gbd, end_gbd;
    if (st == resolve_st_) {
      start_gbd = root_gbd;
      end_gbd = root_gbd + 1;
    } else {
      start_gbd = BoardTree::SuccBoardBegin(resolve_st_, root_gbd, st);
      end_gbd = BoardTree::SuccBoardEnd(resolve_st_, root_gbd, st);
    }
    int max_card = Game::MaxCard();
    int num_hole_card_pairs = Game::NumHoleCardPairs(st);
    for (int gbd = start_gbd; gbd < end_gbd; ++gbd) {
      int lbd = BoardTree::LocalIndex(resolve_st_, root_gbd, st, gbd);
      Card hole_cards[2];
      for (Card hi = 1; hi <= max_card; ++hi) {
	// Check for board conflicts
	hole_cards[0] = hi;
	for (Card lo = 0; lo < hi; ++lo) {
	  // Check for board conflicts
	  hole_cards[1] = lo;
	  // Get board
	  int raw_hcp = HCPIndex(st, board, hole_cards);
	  unsigned int h = ((unsigned int)gbd) * ((unsigned int)num_hole_card_pairs) + raw_hcp;
	  int b = buckets_->Bucket(st, h);
	  // Is this correct on the river?  Probably not.
	  int card_offset = lbd * num_hole_card_pairs + raw_hcp;
	  int bucket_offset = b * num_succs;
	  for (int s = 0; s < num_succs; ++s) {
	    probs[card_offset + s] = bucket_probs[bucket_offset + s];
	  }
	}
      }
    }
  }
  for (int s = 0; s < num_succs; ++s) {
    ReadSumprobsFromDisk(node->IthSucc(s), p, board, values);
  }
}

// What to do if node is on a later street than resolve_st_?
// We don't back up and solve the whole turn game, do we?
CFRValues *Agent::ReadSumprobs(Node *node, int p, const Card *board) {
  int num_players = Game::NumPlayers();
  unique_ptr<bool []> players(new bool[num_players]);
  for (int p1 = 0; p1 < num_players; ++p1) players[p1] = (p1 == p);
  int st = node->Street();
  int max_street = Game::MaxStreet();
  unique_ptr<bool []> streets(new bool[max_street + 1]);
  for (int st1 = 0; st1 <= max_street; ++st1) streets[st1] = (st1 >= st);
  int root_bd = boards_[st];
  CFRValues *values = new CFRValues(players.get(), streets.get(), root_bd, st,
				    *buckets_, betting_trees_->GetBettingTree());
  // Need to create CFRStreetValues
  ReadSumprobsFromDisk(node, p, board, values);
  return values;
}

// dynamic_cbr needs:
// a) our probs for the resolve streets
// b) opp reach probs
// Can't load the entire flop into memory
CFRValues *Agent::Resolve(const Card *board, int we_p, Node *node) {
  unique_ptr<BettingTrees> betting_trees;
  int pa = node->PlayerActing();
  betting_trees.reset(CreateSubtrees(resolve_st_, pa, node->LastBetTo(), we_p,
				     *subgame_ba_));
  return nullptr;
}

// Return true if we take an action.
bool Agent::ProcessMatchState(const MatchState &match_state, CFRValues **resolved_strategy,
			      bool *call, bool *fold, int *bet_size) {
  // In our reconstruction of the action (inside ProcessAction()) we choose our actions
  // randomly.  We need to be able to reconstruct the action of the hand and make the same
  // choices each time.  For this to work, I need to seed the RNG here consistently.
  int hand_no = match_state.HandNo();
  srand48_r(hand_no, &rand_buf_);
  bool we_p1 = match_state.P1();
  int we_p = we_p1 ? 1 : 0;
  const string &action = match_state.Action();
  Card hole_cards[2];
  hole_cards[0] = match_state.OurHi();
  hole_cards[1] = match_state.OurLo();
  const Card *board = match_state.Board();
  int st = match_state.Street();
  unique_ptr<int []> buckets(new int[st + 1]);
  SetBuckets(st, board, hole_cards, buckets.get());
  Node *raise_node;
  bool mapped_bet_to_closing_call;
  Node *node = ProcessAction(action, we_p, buckets.get(), &raise_node, &mapped_bet_to_closing_call);
  fprintf(stderr, "Back from ProcessAction\n");
  *call = false;
  *fold = false;
  *bet_size = 0;
#if 0
  // This can happen if we translate a bet to a call, and the call leads to showdown
  if (node->Terminal()) {
    fprintf(stderr, "At terminal node; exiting\n");
    exit(-1);
  }
#endif
#if 0
  // This can happen if we translate a bet to a call, and the call ends the street.
  if (node->PlayerActing() != we_p) {
    fprintf(stderr, "Not our action; returning false\n");
    return false;
  }
#endif
#if 0
  int num_succs = node->NumSuccs();
  if (num_succs == 0) {
    fprintf(stderr, "Zero succ node that is not terminal?!?\n");
    return false;
  }
  if (num_succs == 1) {
    *call = true;
    return true;
  }
#endif
#if 0
  // This is possible if we map an opponent's bet to a street-ending call.
  int nst = node->Street();
  if (nst != st) {
    fprintf(stderr, "nst %i != st %i?!?\n", nst, st);
    exit(-1);
  }
#endif
  Node *next_node = ChooseOurAction(node, raise_node, buckets.get(), mapped_bet_to_closing_call);
  int csi = node->CallSuccIndex();
  int fsi = node->FoldSuccIndex();
  if (next_node == node) {
    // This is the case where we mapped an opponent's bet to a street-ending call (and we decided
    // not to raise).
    *call = true;
    return true;
  } else if (csi >= 0 && next_node == node->IthSucc(csi)) {
    *call = true;
    return true;
  } else if (fsi >= 0 && next_node == node->IthSucc(fsi)) {
    *fold = true;
    return true;
  }
  // Should we bet the amount dictated by our abstraction or the pot fraction?
  // Maybe the latter.
  fprintf(stderr, "Betting %i small blinds\n", next_node->LastBetTo() - node->LastBetTo());
  *bet_size = (next_node->LastBetTo() - node->LastBetTo()) * small_blind_;
  return true;
}
