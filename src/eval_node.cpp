// Evaluate the P1 EV of a node through either exact evaluation or through sampling boards.
// Derived from play_resolved.

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_tree.h"
#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "canonical.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "cfr_config.h"
#include "cfr_params.h"
#include "cfr_values.h"
#include "constants.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "hand_tree.h"
#include "hand_value_tree.h"
#include "io.h"
#include "params.h"
#include "rand.h"
#include "sorting.h"
#include "unsafe_eg_cfr.h"

using std::pair;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

class Player {
public:
  Player(const BettingAbstraction &a_ba, const BettingAbstraction &b_ba,
	 const CardAbstraction &a_ca, const CardAbstraction &b_ca, const CFRConfig &a_cc,
	 const CFRConfig &b_cc, int a_it, int b_it, bool resolve_a, bool resolve_b,
	 const string &target_action_sequence);
  ~Player(void) {}
  void Go(int num_sampled_max_street_boards);
private:
  double SumJointProbs(shared_ptr<double []> *reach_probs);
  void Showdown(Node *a_node, Node *b_node, shared_ptr<double []> *reach_probs,
		const string &action_sequence);
  void Fold(Node *a_node, Node *b_node, shared_ptr<double []> *reach_probs,
	    const string &action_sequence);
  shared_ptr<double []> **GetSuccReachProbs(Node *node, int gbd, const Buckets &buckets,
					    const CFRValues *sumprobs,
					    shared_ptr<double []> *reach_probs);
  void Nonterminal(Node *a_node, Node *b_node, const string &action_sequence,
		   shared_ptr<double []> *reach_probs);
  void Walk(Node *a_node, Node *b_node, const string &action_sequence,
	    shared_ptr<double []> *reach_probs, int last_st);
  void ProcessMaxStreetBoard(int msbd);

  unique_ptr<BettingTree> a_betting_tree_;
  unique_ptr<BettingTree> b_betting_tree_;
  shared_ptr<Buckets> a_buckets_;
  shared_ptr<Buckets> b_buckets_;
  unique_ptr<CFRValues> a_probs_;
  unique_ptr<CFRValues> b_probs_;
  bool resolve_a_;
  bool resolve_b_;
  string target_action_sequence_;
  unique_ptr<int []> boards_;
  // The number of times we sampled this board.
  int num_samples_;
  int msbd_;
  int b_pos_;
  // unique_ptr<int []> multipliers_;
  // unique_ptr<HandTree> trunk_hand_tree_;
  unique_ptr<HandTree> hand_tree_;
  const CanonicalCards *hands_;
  double sum_p1_outcomes_;
  double sum_weights_;
  double sum_target_weights_;
  unique_ptr<EGCFR> a_eg_cfr_;
  unique_ptr<EGCFR> b_eg_cfr_;
  int num_subgame_its_;
  unique_ptr< unique_ptr<int []> []> ms_hcp_to_pms_hcp_;
  // true if we are only sampling some of the max street boards
  bool sampling_;
  unique_ptr< unique_ptr<double []> []> sampled_board_weights_;
};

Player::Player(const BettingAbstraction &a_ba, const BettingAbstraction &b_ba,
	       const CardAbstraction &a_ca, const CardAbstraction &b_ca, const CFRConfig &a_cc,
	       const CFRConfig &b_cc, int a_it, int b_it, bool resolve_a, bool resolve_b,
	       const string &target_action_sequence) {
  int max_street = Game::MaxStreet();
  boards_.reset(new int[max_street + 1]);
  boards_[0] = 0;
  sum_p1_outcomes_ = 0;
  sum_weights_ = 0;
  sum_target_weights_ = 0;
  resolve_a_ = resolve_a;
  resolve_b_ = resolve_b;
  target_action_sequence_ = target_action_sequence;

  a_buckets_.reset(new Buckets(a_ca, false));
  if (strcmp(a_ca.CardAbstractionName().c_str(), b_ca.CardAbstractionName().c_str())) {
    b_buckets_.reset(new Buckets(b_ca, false));
  } else {
    b_buckets_ = a_buckets_;
  }
  BoardTree::Create();
  BoardTree::CreateLookup();
  BoardTree::BuildBoardCounts();
  BoardTree::BuildPredBoards();

  a_betting_tree_.reset(new BettingTree(a_ba));
  b_betting_tree_.reset(new BettingTree(b_ba));

  // Note assumption that we can use the betting tree for position 0
  a_probs_.reset(new CFRValues(nullptr, nullptr, 0, 0, *a_buckets_, a_betting_tree_.get()));
  b_probs_.reset(new CFRValues(nullptr, nullptr, 0, 0, *b_buckets_, b_betting_tree_.get()));

  char dir[500];
  
  sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::OldCFRBase(),
	  Game::GameName().c_str(), Game::NumPlayers(),
	  a_ca.CardAbstractionName().c_str(),
	  Game::NumRanks(), Game::NumSuits(), Game::MaxStreet(),
	  a_ba.BettingAbstractionName().c_str(),
	  a_cc.CFRConfigName().c_str());
  // Note assumption that we can use the betting tree for position 0
  a_probs_->Read(dir, a_it, a_betting_tree_.get(), "x", -1, true, false);

  sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::OldCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), b_ca.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(), b_ba.BettingAbstractionName().c_str(),
	  b_cc.CFRConfigName().c_str());
  // Note assumption that we can use the betting tree for position 0
  b_probs_->Read(dir, b_it, b_betting_tree_.get(), "x", -1, true, false);

  // trunk_hand_tree_.reset(new HandTree(0, 0, max_street - 1));
#if 0
  multipliers_.reset(new int[max_street + 1]);
  multipliers_[max_street] = 1;
  for (int st = max_street - 1; st >= 0; --st) {
    multipliers_[st] = multipliers_[st+1] * Game::StreetPermutations(st+1);
  }
#endif

#if 0
  if (resolve_a_) {
    a_eg_cfr_.reset(new UnsafeEGCFR(subgame_card_abstraction_, a_ca, subgame_betting_abstraction_,
				    a_ba, subgame_cfr_config_, a_cc, subgame_buckets_, 1));
  }
  if (resolve_b_) {
    b_eg_cfr_.reset(new UnsafeEGCFR(subgame_card_abstraction_, b_ca, subgame_betting_abstraction_,
				    b_ba, subgame_cfr_config_, b_cc, subgame_buckets_, 1));
  }
#endif
  num_subgame_its_ = 200;
  // We index hole card pairs differently on the final street than on prior streets.  Need to be
  // abel to map from final street hcp indices to prior street hcp indices.
  int num_ms_hole_card_pairs = Game::NumHoleCardPairs(max_street);
  ms_hcp_to_pms_hcp_.reset(new unique_ptr<int []>[max_street]);
  for (int st = 0; st < max_street; ++st) {
    ms_hcp_to_pms_hcp_[st].reset(new int[num_ms_hole_card_pairs]);
  }

  sampling_ = false;
  sampled_board_weights_.reset(new unique_ptr<double []>[max_street]);
  for (int st = 0; st < max_street; ++st) {
    int num_boards = BoardTree::NumBoards(st);
    sampled_board_weights_[st].reset(new double[num_boards]);
    for (int bd = 0; bd < num_boards; ++bd) {
      sampled_board_weights_[st][bd] = 0;
    }
  }
}

double Player::SumJointProbs(shared_ptr<double []> *reach_probs) {
  int max_street = Game::MaxStreet();
  int num_hole_card_pairs = Game::NumHoleCardPairs(max_street);
  Card max_card1 = Game::MaxCard() + 1;
  double *a_probs = b_pos_ == 0 ? reach_probs[1].get() : reach_probs[0].get();
  double *b_probs = b_pos_ == 0 ? reach_probs[0].get() : reach_probs[1].get();
  
  double sum_a_probs = 0;
  unique_ptr<double []> total_a_card_probs(new double[52]);
  for (Card c = 0; c < max_card1; ++c) {
    total_a_card_probs[c] = 0;
  }
  for (int hcp = 0; hcp < num_hole_card_pairs; ++hcp) {
    const Card *cards = hands_->Cards(hcp);
    Card hi = cards[0];
    Card lo = cards[1];
    int enc = hi * max_card1 + lo;
    double a_prob = a_probs[enc];
    total_a_card_probs[hi] += a_prob;
    total_a_card_probs[lo] += a_prob;
    sum_a_probs += a_prob;
    if (a_prob > 1.0) {
      fprintf(stderr, "SumJointProbs: opp_prob (a) %f hcp %u\n", a_prob, hcp);
      exit(-1);
    }
  }

  double sum_joint_probs = 0;
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    const Card *cards = hands_->Cards(i);
    Card hi = cards[0];
    Card lo = cards[1];
    int enc = hi * max_card1 + lo;
    double b_prob = b_probs[enc];
    // This is the sum of all A reach probabilities consistent with B holding <hi, lo>.
    double sum_consistent_a_probs = sum_a_probs + a_probs[enc] -
      total_a_card_probs[hi] - total_a_card_probs[lo];
    sum_joint_probs += b_prob * sum_consistent_a_probs;
  }
  double wtd_sum_joint_probs;
  if (sampling_) {
    wtd_sum_joint_probs = sum_joint_probs * (double)num_samples_;
  } else {
    wtd_sum_joint_probs = sum_joint_probs * (double)BoardTree::BoardCount(max_street, msbd_);
  }
  return wtd_sum_joint_probs;
}

// Compute outcome from B's perspective
void Player::Showdown(Node *a_node, Node *b_node, shared_ptr<double []> *reach_probs,
		      const string &action_sequence) {
  // fprintf(stderr, "Showdown %i\n", a_node->TerminalID());
  Card max_card1 = Game::MaxCard() + 1;

  double *a_probs = b_pos_ == 0 ? reach_probs[1].get() : reach_probs[0].get();
  double *b_probs = b_pos_ == 0 ? reach_probs[0].get() : reach_probs[1].get();
  unique_ptr<double []> cum_opp_card_probs(new double[52]);
  unique_ptr<double []> total_opp_card_probs(new double[52]);
  for (Card c = 0; c < max_card1; ++c) {
    cum_opp_card_probs[c] = 0;
    total_opp_card_probs[c] = 0;
  }
  int max_street = Game::MaxStreet();
  int num_hole_card_pairs = Game::NumHoleCardPairs(max_street);
  double sum_opp_probs = 0;
  for (int hcp = 0; hcp < num_hole_card_pairs; ++hcp) {
    const Card *cards = hands_->Cards(hcp);
    Card hi = cards[0];
    Card lo = cards[1];
    int enc = hi * max_card1 + lo;
    double opp_prob = a_probs[enc];
    total_opp_card_probs[hi] += opp_prob;
    total_opp_card_probs[lo] += opp_prob;
    sum_opp_probs += opp_prob;
    if (opp_prob > 1.0) {
      fprintf(stderr, "Showdown: opp_prob (a) %f hcp %u\n", opp_prob, hcp);
      exit(-1);
    }
  }

#if 0
  if (action_sequence == target_action_sequence_ && msbd_ == 11 && b_pos_ == 1) {
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      const Card *cards = hands_->Cards(i);
      Card hi = cards[0];
      Card lo = cards[1];
      int enc = hi * max_card1 + lo;
      OutputTwoCards(hi, lo);
      printf(" %f (%i)\n", reach_probs[1][enc], i);
      fflush(stdout);
    }
  }
#endif

  double opp_cum_prob = 0;
  unique_ptr<double []> win_probs(new double[num_hole_card_pairs]);
  double half_pot = a_node->LastBetTo();
  double sum_our_vals = 0, sum_joint_probs = 0;

  int j = 0;
  while (j < num_hole_card_pairs) {
    int last_hand_val = hands_->HandValue(j);
    int begin_range = j;
    // Make three passes through the range of equally strong hands
    // First pass computes win counts for each hand and finds end of range
    // Second pass updates cumulative counters
    // Third pass computes lose counts for each hand
    while (j < num_hole_card_pairs) {
      const Card *cards = hands_->Cards(j);
      Card hi = cards[0];
      Card lo = cards[1];
      int hand_val = hands_->HandValue(j);
      if (hand_val != last_hand_val) break;
      win_probs[j] = opp_cum_prob - cum_opp_card_probs[hi] - cum_opp_card_probs[lo];
      ++j;
    }
    // Positions begin_range...j-1 (inclusive) all have the same hand value
    for (int k = begin_range; k < j; ++k) {
      const Card *cards = hands_->Cards(k);
      Card hi = cards[0];
      Card lo = cards[1];
      int enc = hi * max_card1 + lo;
      double opp_prob = a_probs[enc];
      if (opp_prob <= 0) continue;
      cum_opp_card_probs[hi] += opp_prob;
      cum_opp_card_probs[lo] += opp_prob;
      opp_cum_prob += opp_prob;
    }
    for (int k = begin_range; k < j; ++k) {
      const Card *cards = hands_->Cards(k);
      Card hi = cards[0];
      Card lo = cards[1];
      int enc = hi * max_card1 + lo;
      double our_prob = b_probs[enc];
      double better_hi_prob = total_opp_card_probs[hi] - cum_opp_card_probs[hi];
      double better_lo_prob = total_opp_card_probs[lo] - cum_opp_card_probs[lo];
      double lose_prob = (sum_opp_probs - opp_cum_prob) - better_hi_prob - better_lo_prob;
      sum_our_vals += our_prob * (win_probs[k] - lose_prob) * half_pot;
      // This is the sum of all A reach probabilities consistent with B holding <hi, lo>.
      double sum_consistent_opp_probs = sum_opp_probs + a_probs[enc] -
	total_opp_card_probs[hi] - total_opp_card_probs[lo];
      sum_joint_probs += our_prob * sum_consistent_opp_probs;
    }
  }

  // Scale by board count
  double wtd_sum_our_vals;
  double wtd_sum_joint_probs;
  if (sampling_) {
    wtd_sum_our_vals = sum_our_vals * (double)num_samples_;
    wtd_sum_joint_probs = sum_joint_probs * (double)num_samples_;
  } else {
    wtd_sum_our_vals = sum_our_vals * (double)BoardTree::BoardCount(max_street, msbd_);
    wtd_sum_joint_probs = sum_joint_probs * (double)BoardTree::BoardCount(max_street, msbd_);
  }

  if (action_sequence == target_action_sequence_) {
    if (b_pos_ == 0) sum_p1_outcomes_ -= wtd_sum_our_vals;
    else             sum_p1_outcomes_ += wtd_sum_our_vals;
    sum_target_weights_ += wtd_sum_joint_probs;
  }
  sum_weights_ += wtd_sum_joint_probs;
}

// Compute outcome from B's perspective
void Player::Fold(Node *a_node, Node *b_node, shared_ptr<double []> *reach_probs,
		  const string &action_sequence) {
  // fprintf(stderr, "Fold %i\n", a_node->TerminalID());
  Card max_card1 = Game::MaxCard() + 1;

  double half_pot = a_node->LastBetTo();
  // Player acting encodes player remaining at fold nodes
  int rem_p = a_node->PlayerActing();
  // Outcomes are from B's perspective
  if (b_pos_ != rem_p) {
    // B has folded
    half_pot = -half_pot;
  }
  double sum_our_vals = 0, sum_joint_probs = 0;

  double *a_probs = b_pos_ == 0 ? reach_probs[1].get() : reach_probs[0].get();
  double *b_probs = b_pos_ == 0 ? reach_probs[0].get() : reach_probs[1].get();
  unique_ptr<double []> cum_opp_card_probs(new double[52]);
  unique_ptr<double []> total_opp_card_probs(new double[52]);
  for (Card c = 0; c < max_card1; ++c) {
    cum_opp_card_probs[c] = 0;
    total_opp_card_probs[c] = 0;
  }

  double sum_opp_probs = 0;
  int max_street = Game::MaxStreet();
  // Always iterate through hole card pairs consistent with the sampled *max street* board, even if
  // this is a pre-max-street node.
  int num_hole_card_pairs = Game::NumHoleCardPairs(max_street);
  for (int hcp = 0; hcp < num_hole_card_pairs; ++hcp) {
    const Card *cards = hands_->Cards(hcp);
    Card hi = cards[0];
    Card lo = cards[1];
    int enc = hi * max_card1 + lo;
    double opp_prob = a_probs[enc];
    total_opp_card_probs[hi] += opp_prob;
    total_opp_card_probs[lo] += opp_prob;
    sum_opp_probs += opp_prob;
  }

  for (int i = 0; i < num_hole_card_pairs; ++i) {
    const Card *cards = hands_->Cards(i);
    Card hi = cards[0];
    Card lo = cards[1];
    int enc = hi * max_card1 + lo;
    double our_prob = b_probs[enc];
    // This is the sum of all the A reach probabilities consistent with B holding <hi, lo>.
    double sum_consistent_opp_probs = sum_opp_probs + a_probs[enc] -
      total_opp_card_probs[hi] - total_opp_card_probs[lo];
    sum_our_vals += our_prob * half_pot * sum_consistent_opp_probs;
    sum_joint_probs += our_prob * sum_consistent_opp_probs;    
  }

  double wtd_sum_our_vals = sum_our_vals;
  double wtd_sum_joint_probs = sum_joint_probs;
  if (sampling_) {
    int st = a_node->Street();
    double wt;
    if (st == max_street) {
      wt = num_samples_;
    } else {
      int pbd = BoardTree::PredBoard(msbd_, st);
      wt = sampled_board_weights_[st][pbd];
    }
    wtd_sum_our_vals = sum_our_vals * wt;
    wtd_sum_joint_probs = sum_joint_probs * wt;
  } else {
    wtd_sum_our_vals = sum_our_vals * (double)BoardTree::BoardCount(max_street, msbd_);
    wtd_sum_joint_probs = sum_joint_probs * (double)BoardTree::BoardCount(max_street, msbd_);
  }
  if (action_sequence == target_action_sequence_) {
    if (b_pos_ == 0) sum_p1_outcomes_ -= wtd_sum_our_vals;
    else             sum_p1_outcomes_ += wtd_sum_our_vals;
    sum_target_weights_ += wtd_sum_joint_probs;
  }
  sum_weights_ += wtd_sum_joint_probs;
}

// Hard-coded for heads-up
shared_ptr<double []> **Player::GetSuccReachProbs(Node *node, int gbd, const Buckets &buckets,
						  const CFRValues *sumprobs,
						  shared_ptr<double []> *reach_probs) {
  int st = node->Street();
  int num_succs = node->NumSuccs();
  shared_ptr<double []> **succ_reach_probs = new shared_ptr<double []> *[num_succs];
  int max_card1 = Game::MaxCard() + 1;
  int num_enc = max_card1 * max_card1;
  int max_street = Game::MaxStreet();
  // For some purposes below, we care about the number of hole card pairs on the final street.
  // (We are maintaining probabilities for every max street hand for the sampled max street
  // board.)  For other purposes, we care about the number of hole card pairs on the current
  // street (for looking up the probability of the current actions).
  int num_ms_hole_card_pairs = Game::NumHoleCardPairs(max_street);
  int num_st_hole_card_pairs = Game::NumHoleCardPairs(st);
  for (int s = 0; s < num_succs; ++s) {
    succ_reach_probs[s] = new shared_ptr<double []>[2];
    for (int p = 0; p < 2; ++p) {
      succ_reach_probs[s][p].reset(new double[num_enc]);
    }
  }
  // Can happen when we are all-in.  Only succ is check.
  if (num_succs == 1) {
    for (int i = 0; i < num_ms_hole_card_pairs; ++i) {
      const Card *cards = hands_->Cards(i);
      Card hi = cards[0];
      Card lo = cards[1];
      int enc = hi * max_card1 + lo;
      for (int p = 0; p <= 1; ++p) {
	succ_reach_probs[0][p][enc] = reach_probs[p][enc];
      }
    }
    return succ_reach_probs;
  }
  int pa = node->PlayerActing();
  int nt = node->NonterminalID();
  int dsi = node->DefaultSuccIndex();
  unique_ptr<double []> probs(new double[num_succs]);
  for (int i = 0; i < num_ms_hole_card_pairs; ++i) {
    const Card *cards = hands_->Cards(i);
    Card hi = cards[0];
    Card lo = cards[1];
    int enc = hi * max_card1 + lo;
    int offset;
    int hcp;
    if (st == max_street) {
      hcp = i;
    } else {
      hcp = ms_hcp_to_pms_hcp_[st][i];
    }
    if (buckets.None(st)) {
      offset = gbd * num_st_hole_card_pairs * num_succs + hcp * num_succs;
    } else {
      unsigned int h = ((unsigned int)gbd) * ((unsigned int)num_st_hole_card_pairs) + hcp;
      int b = buckets.Bucket(st, h);
      offset = b * num_succs;
    }
    sumprobs->RMProbs(st, pa, nt, offset, num_succs, dsi, probs.get());
    for (int s = 0; s < num_succs; ++s) {
      for (int p = 0; p <= 1; ++p) {
	if (p == pa) {
	  double prob = reach_probs[p][enc] * probs[s];
	  if (prob > 1.0 || prob < 0) {
	    fprintf(stderr, "OOB prob %f (%f * %f) enc %i st %i\n", prob, reach_probs[p][enc],
		    probs[s], enc, st);
	    OutputCard(hi);
	    printf(" ");
	    OutputCard(lo);
	    printf("\n");
	    const Card *board = BoardTree::Board(max_street, msbd_);
	    OutputFiveCards(board);
	    printf("\n");
	    exit(-1);
	  }
	  succ_reach_probs[s][p][enc] = prob;
#if 0
	  if (b_pos_ == 1 && st == 1 && node->NonterminalID() == 4 && node->PlayerActing() == 1 &&
	      hi == MakeCard(1, 3) && lo == MakeCard(1, 2) && msbd_ == 11) {
	    printf("BC/C i %i ", i);
	    OutputTwoCards(hi, lo);
	    printf(" s %i %f * %f = %f\n", s, reach_probs[p][enc], probs[s], prob);
	    fflush(stdout);
	  }
#endif
	} else {
	  double prob = reach_probs[p][enc];
	  if (prob > 1.0 || prob < 0) {
	    fprintf(stderr, "OOB prob %f enc %i\n", prob, enc);
	    exit(-1);
	  }
	  succ_reach_probs[s][p][enc] = prob;
	}
      }
    }
  }
  
  return succ_reach_probs;
}

void Player::Nonterminal(Node *a_node, Node *b_node, const string &action_sequence,
			 shared_ptr<double []> *reach_probs) {
  if (action_sequence == target_action_sequence_) {
    double sjp = SumJointProbs(reach_probs);
    sum_target_weights_ += sjp;
  }
  
  int st = a_node->Street();
  int pa = a_node->PlayerActing();
  shared_ptr<double []> **succ_reach_probs;
  if (pa == b_pos_) {
    // This doesn't support multiplayer yet
    succ_reach_probs = GetSuccReachProbs(a_node, boards_[st], *b_buckets_, b_probs_.get(),
					 reach_probs);
  } else {
    succ_reach_probs = GetSuccReachProbs(a_node, boards_[st], *a_buckets_, a_probs_.get(),
					 reach_probs);
  }
  int num_succs = a_node->NumSuccs();
  for (int s = 0; s < num_succs; ++s) {
    string action = a_node->ActionName(s);
    Walk(a_node->IthSucc(s), b_node->IthSucc(s), action_sequence + action, succ_reach_probs[s], st);
  }
  for (int s = 0; s < num_succs; ++s) {
    delete [] succ_reach_probs[s];
  }
  delete [] succ_reach_probs;
}
 
void Player::Walk(Node *a_node, Node *b_node, const string &action_sequence,
		  shared_ptr<double []> *reach_probs, int last_st) {
  int st = a_node->Street();
  if (st > last_st && st == Game::MaxStreet()) {
#if 0
    if (resolve_a_) {
    }
    if (resolve_b_) {
      b_eg_cfr_->SolveSubgame(subgame_subtree, msbd_, reach_probs, action_sequence,
			      subgame_hand_tree_.get(), nullptr, -1, true, num_subgame_its_);
    }
#endif
  }
  if (a_node->Terminal()) {
    if (! b_node->Terminal()) {
      fprintf(stderr, "A terminal B nonterminal?!?\n");
      exit(-1);
    }
    if (a_node->Showdown()) {
      Showdown(a_node, b_node, reach_probs, action_sequence);
    } else {
      Fold(a_node, b_node, reach_probs, action_sequence);
    }
  } else {
    if (b_node->Terminal()) {
      fprintf(stderr, "A nonterminal B terminal?!?\n");
      exit(-1);
    }
    Nonterminal(a_node, b_node, action_sequence, reach_probs);
  }
}

void Player::ProcessMaxStreetBoard(int msbd) {
  int max_street = Game::MaxStreet();
  msbd_ = msbd;
  boards_[max_street] = msbd_;
  for (int st = 1; st < max_street; ++st) {
    boards_[st] = BoardTree::PredBoard(msbd_, st);
  }
  hand_tree_.reset(new HandTree(max_street, msbd_, max_street));
  hands_ = hand_tree_->Hands(max_street, 0);
  int num_ms_hole_card_pairs = Game::NumHoleCardPairs(max_street);
  int max_card1 = Game::MaxCard() + 1;
  int num_enc = max_card1 * max_card1;
  unique_ptr<int []> enc_to_pshcp(new int[num_enc]);
  Card max_card = Game::MaxCard();
  for (int st = 0; st < max_street; ++st) {
    const Card *board = BoardTree::Board(st, boards_[st]);
    int num_board_cards = Game::NumBoardCards(st);
    int pshcp = 0;
    for (Card hi = 1; hi <= max_card; ++hi) {
      if (InCards(hi, board, num_board_cards)) continue;
      for (Card lo = 0; lo < hi; ++lo) {
	if (InCards(lo, board, num_board_cards)) continue;
	int enc = hi * (max_card + 1) + lo;
	enc_to_pshcp[enc] = pshcp;
	++pshcp;
      }
    }
    for (int i = 0; i < num_ms_hole_card_pairs; ++i) {
      const Card *cards = hands_->Cards(i);
      Card hi = cards[0];
      Card lo = cards[1];
      int enc = hi * (max_card + 1) + lo;
      int pshcp = enc_to_pshcp[enc];
      ms_hcp_to_pms_hcp_[st][i] = pshcp;
    }
  }

  // Maintaining reach probs for hole card pairs consistent with *max-street* board.
  int num_players = Game::NumPlayers();
  unique_ptr<shared_ptr<double []> []> reach_probs(new shared_ptr<double []>[num_players]);
  for (int p = 0; p < num_players; ++p) {
    reach_probs[p].reset(new double[num_enc]);
    for (int i = 0; i < num_ms_hole_card_pairs; ++i) {
      const Card *cards = hands_->Cards(i);
      Card hi = cards[0];
      Card lo = cards[1];
      int enc = hi * max_card1 + lo;
      reach_probs[p][enc] = 1.0;
    }
  }
  for (b_pos_ = 0; b_pos_ < num_players; ++b_pos_) {
    Walk(a_betting_tree_->Root(), b_betting_tree_->Root(), "x", reach_probs.get(), 0);
  }
}

void Player::Go(int num_sampled_max_street_boards) {
  fprintf(stderr, "Go\n");
  int max_street = Game::MaxStreet();
  int num_max_street_boards = BoardTree::NumBoards(max_street);
  if (num_sampled_max_street_boards == 0 ||
      num_sampled_max_street_boards > num_max_street_boards) {
    num_sampled_max_street_boards = num_max_street_boards;
  }

  unique_ptr<int []> max_street_board_samples(new int[num_max_street_boards]);
  for (int bd = 0; bd < num_max_street_boards; ++bd) max_street_board_samples[bd] = 0;

  if (num_sampled_max_street_boards == num_max_street_boards) {
    fprintf(stderr, "Processing all max street boards\n");
    for (int bd = 0; bd < num_max_street_boards; ++bd) {
      max_street_board_samples[bd] = BoardTree::BoardCount(max_street, bd);
    }
    sampling_ = false;
  } else {
    struct drand48_data rand_buf;
    srand48_r(time(0), &rand_buf);
    vector< pair<double, int> > v;
    for (int bd = 0; bd < num_max_street_boards; ++bd) {
      int board_count = BoardTree::BoardCount(max_street, bd);
      for (int i = 0; i < board_count; ++i) {
	double r;
	drand48_r(&rand_buf, &r);
	v.push_back(std::make_pair(r, bd));
      }
    }
    std::sort(v.begin(), v.end());
    for (int i = 0; i < num_sampled_max_street_boards; ++i) {
      int bd = v[i].second;
      ++max_street_board_samples[bd];
    }

    sampling_ = true;
#if 0
    int num_max_street_permutations = 0;
    int num_sampled_max_street_permutations = 0;
    for (int bd = 0; bd < num_max_street_boards; ++bd) {
      num_max_street_permutations += BoardTree::BoardCount(max_street, bd);
      // Should I multiply by board count?
      num_sampled_max_street_permutations += max_street_board_samples[bd];
    }
    max_street_frac_sampled_ =
      num_sampled_max_street_permutations / (double)num_max_street_permutations;
    fprintf(stderr, "num_max_street_permutations %i\n", num_max_street_permutations);
    fprintf(stderr, "num_sampled_max_street_permutations %i\n", num_sampled_max_street_permutations);
    fprintf(stderr, "max_street_frac_sampled_ %f\n", max_street_frac_sampled_);
#endif

    unique_ptr< unique_ptr<int []> []> sampled_board_counts;
    sampled_board_counts.reset(new unique_ptr<int []> [max_street]);
    for (int st = 0; st < max_street; ++st) {
      int num_boards = BoardTree::NumBoards(st);
      sampled_board_counts[st].reset(new int[num_boards]);
      for (int bd = 0; bd < num_boards; ++bd) {
	sampled_board_counts[st][bd] = 0;
      }
    }
    for (int msbd = 0; msbd < num_max_street_boards; ++msbd) {
      int num_samples = max_street_board_samples[msbd];
      int ms_board_count = BoardTree::BoardCount(max_street, msbd);
      for (int st = 0; st < max_street; ++st) {
	int pbd = BoardTree::PredBoard(msbd, st);
	sampled_board_counts[st][pbd] += num_samples * ms_board_count;
      }
    }
    for (int st = 0; st < max_street; ++st) {
      int num_board_completions = 1;
      for (int st1 = st + 1; st1 <= max_street; ++st1) {
	num_board_completions *= Game::StreetPermutations3(st1);
      }
      fprintf(stderr, "num_board_completions %i\n", num_board_completions);
      int num_boards = BoardTree::NumBoards(st);
      for (int bd = 0; bd < num_boards; ++bd) {
	sampled_board_weights_[st][bd] =
	  num_board_completions / (double)sampled_board_counts[st][bd];
	// TEMPORARY
	sampled_board_weights_[st][bd] = 1.0;
	if (sampled_board_counts[st][bd] > 0) {
	  fprintf(stderr, "%i %i sbw %f\n", st, bd, sampled_board_weights_[st][bd]);
	}
      }
    }
  }

  for (int bd = 0; bd < num_max_street_boards; ++bd) {
    num_samples_ = max_street_board_samples[bd];
    if (num_samples_ == 0) continue;
    ProcessMaxStreetBoard(bd);
  }

  double avg_p1_target_outcome = sum_p1_outcomes_ / sum_target_weights_;
  double p1_target_mbb_g = (avg_p1_target_outcome / 2.0) * 1000.0;
  fprintf(stderr, "Avg P1 target outcome: %f (%.1f mbb/g)\n", avg_p1_target_outcome,
	  p1_target_mbb_g);
  double target_joint_prob = sum_target_weights_ / sum_weights_;
  fprintf(stderr, "Target joint prob: %f (%f)\n", target_joint_prob, sum_weights_);
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <A card params> <B card params> "
	  "<A betting abstraction params> <B betting abstraction params> <A CFR params> "
	  "<B CFR params> <A it> <B it> <num sampled max street boards> <action sequence>\n",
	  prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 12) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unique_ptr<Params> a_card_params = CreateCardAbstractionParams();
  a_card_params->ReadFromFile(argv[2]);
  unique_ptr<CardAbstraction>
    a_card_abstraction(new CardAbstraction(*a_card_params));
  unique_ptr<Params> b_card_params = CreateCardAbstractionParams();
  b_card_params->ReadFromFile(argv[3]);
  unique_ptr<CardAbstraction>
    b_card_abstraction(new CardAbstraction(*b_card_params));
  unique_ptr<Params> a_betting_params = CreateBettingAbstractionParams();
  a_betting_params->ReadFromFile(argv[4]);
  unique_ptr<BettingAbstraction>
    a_betting_abstraction(new BettingAbstraction(*a_betting_params));
  unique_ptr<Params> b_betting_params = CreateBettingAbstractionParams();
  b_betting_params->ReadFromFile(argv[5]);
  unique_ptr<BettingAbstraction>
    b_betting_abstraction(new BettingAbstraction(*b_betting_params));
  unique_ptr<Params> a_cfr_params = CreateCFRParams();
  a_cfr_params->ReadFromFile(argv[6]);
  unique_ptr<CFRConfig>
    a_cfr_config(new CFRConfig(*a_cfr_params));
  unique_ptr<Params> b_cfr_params = CreateCFRParams();
  b_cfr_params->ReadFromFile(argv[7]);
  unique_ptr<CFRConfig>
    b_cfr_config(new CFRConfig(*b_cfr_params));

  int a_it, b_it;
  if (sscanf(argv[8], "%i", &a_it) != 1) Usage(argv[0]);
  if (sscanf(argv[9], "%i", &b_it) != 1) Usage(argv[0]);
  int num_sampled_max_street_boards;
  if (sscanf(argv[10], "%i", &num_sampled_max_street_boards) != 1) Usage(argv[0]);
  string action_sequence = argv[11];

  bool resolve_a = false;
  bool resolve_b = false;
  string target_action_sequence = "x";
  target_action_sequence += action_sequence;
  Player player(*a_betting_abstraction, *b_betting_abstraction, *a_card_abstraction,
		*b_card_abstraction, *a_cfr_config, *b_cfr_config, a_it, b_it, resolve_a,
		resolve_b, target_action_sequence);
  player.Go(num_sampled_max_street_boards);
}
