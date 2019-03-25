// There are two players (computed strategies) given to the program, named
// A and B.  P0 and P1 in contrast refer to the two positions (big blind and
// button respectively).  A and B alternate during play between being the
// button (P1) and the big blind (P0).

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>

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

using std::string;
using std::unique_ptr;

class Player {
public:
  Player(const BettingAbstraction &ba, const CardAbstraction &a_ca, const CardAbstraction &b_ca,
	 const CFRConfig &a_cc, const CFRConfig &b_cc, int a_it, int b_it);
  ~Player(void);
  void Go(unsigned long long int num_duplicate_hands, bool deterministic);
private:
  void DealNCards(Card *cards, int n);
  void SetHCPsAndBoards(Card **raw_hole_cards, const Card *raw_board);
  void Play(Node **nodes, int b_pos, int *contributions, int last_bet_to, bool *folded,
	    int num_remaining, int last_player_acting, int last_st, double *outcomes);
  void PlayDuplicateHand(unsigned long long int h, const Card *cards,
			 bool deterministic, double *a_sum, double *b_sum);

  int num_players_;
  bool asymmetric_;
  BettingTree **betting_trees_;
  const Buckets *a_buckets_;
  const Buckets *b_buckets_;
  CFRValues **a_probs_;
  CFRValues **b_probs_;
  int *boards_;
  int **raw_hcps_;
  unique_ptr<int []> hvs_;
  unique_ptr<bool []> winners_;
  unsigned short **sorted_hcps_;
  unique_ptr<double []> sum_pos_outcomes_;
  struct drand48_data *rand_bufs_;
};

void Player::Play(Node **nodes, int b_pos, int *contributions, int last_bet_to, bool *folded,
		  int num_remaining, int last_player_acting, int last_st, double *outcomes) {
  Node *p0_node = nodes[0];
  if (p0_node->Terminal()) {
    if (num_remaining == 1) {
      int sum_other_contributions = 0;
      int remaining_p = -1;
      for (int p = 0; p < num_players_; ++p) {
	if (folded[p]) {
	  sum_other_contributions += contributions[p];
	} else {
	  remaining_p = p;
	}
      }
      outcomes[remaining_p] = sum_other_contributions;
    } else {
      // Showdown
      // Temporary?
      if (num_players_ == 2 && (contributions[0] != contributions[1] ||
				contributions[0] != p0_node->LastBetTo())) {
	fprintf(stderr, "Mismatch %i %i %i\n", contributions[0], contributions[1],
		p0_node->LastBetTo());
	fprintf(stderr, "TID: %u\n", p0_node->TerminalID());
	exit(-1);
      }

      // Find the best hand value of anyone remaining in the hand, and the
      // total pot size which includes contributions from remaining players
      // and players who folded earlier.
      int best_hv = 0;
      int pot_size = 0;
      for (int p = 0; p < num_players_; ++p) {
	pot_size += contributions[p];
	if (! folded[p]) {
	  int hv = hvs_[p];
	  if (hv > best_hv) best_hv = hv;
	}
      }

      // Determine if we won, the number of winners, and the total contribution
      // of all winners.
      int num_winners = 0;
      int winner_contributions = 0;
      for (int p = 0; p < num_players_; ++p) {
	if (! folded[p] && hvs_[p] == best_hv) {
	  winners_[p] = true;
	  ++num_winners;
	  winner_contributions += contributions[p];
	} else {
	  winners_[p] = false;
	}
      }
      
      for (int p = 0; p < num_players_; ++p) {
	if (winners_[p]) {
	  outcomes[p] = ((double)(pot_size - winner_contributions)) / ((double)num_winners);
	} else if (! folded[p]) {
	  outcomes[p] = -(int)contributions[p];
	}
      }
    }
    return;
  } else {
    int orig_pa = p0_node->PlayerActing();
    Node *node = nodes[orig_pa];
    int nt = node->NonterminalID();
    int st = node->Street();
    int num_succs = node->NumSuccs();
    // Find the next player to act.  Start with the first candidate and move
    // forward until we find someone who has not folded.  The first candidate
    // is either the last player plus one, or, if we are starting a new
    // betting round, the first player to act on that street.
    int actual_pa;
    if (st > last_st) actual_pa = Game::FirstToAct(st);
    else              actual_pa = last_player_acting + 1;
    while (true) {
      if (actual_pa == num_players_) actual_pa = 0;
      if (! folded[actual_pa]) break;
      ++actual_pa;
    }
    
    int dsi = node->DefaultSuccIndex();
    int bd = boards_[st];
    int raw_hcp = raw_hcps_[actual_pa][st];
    int num_hole_card_pairs = Game::NumHoleCardPairs(st);
    int a_offset, b_offset;
    // If card abstraction, hcp on river should be raw.  If no card
    // abstraction, hcp on river should be sorted.  Right?
    if (a_buckets_->None(st)) {
      int hcp = st == Game::MaxStreet() ? sorted_hcps_[bd][raw_hcp] : raw_hcp;
      a_offset = bd * num_hole_card_pairs * num_succs + hcp * num_succs;
    } else {
      unsigned int h = ((unsigned int)bd) * ((unsigned int)num_hole_card_pairs) + raw_hcp;
      int b = a_buckets_->Bucket(st, h);
      a_offset = b * num_succs;
    }
    if (b_buckets_->None(st)) {
      // Don't support full hold'em with no buckets.  Would get int overflow if we tried to do that.
      int hcp;
      hcp = st == Game::MaxStreet() ? sorted_hcps_[bd][raw_hcp] : raw_hcp;
      b_offset = bd * num_hole_card_pairs * num_succs + hcp * num_succs;
    } else {
      unsigned int h = ((unsigned int)bd) * ((unsigned int)num_hole_card_pairs) + raw_hcp;
      int b = b_buckets_->Bucket(st, h);
      b_offset = b * num_succs;
    }
    double r;
    // r = RandZeroToOne();
    drand48_r(&rand_bufs_[actual_pa], &r);
    
    double cum = 0;
    unique_ptr<double []> probs(new double[num_succs]);
    if (actual_pa == b_pos) {
      b_probs_[orig_pa]->RMProbs(st, orig_pa, nt, b_offset, num_succs, dsi, probs.get());
      // b_probs_[orig_pa]->Probs(orig_pa, st, nt, b_offset, num_succs, dsi, probs.get());
#if 0
      fprintf(stderr, "b pa %u st %u nt %u bd %u nhcp %u hcp %u b_offset %u "
	      "probs %f %f r %f\n", orig_pa, st, nt, bd, num_hole_card_pairs,
	      hcp, b_offset, probs[0], probs[1], r);
#endif
    } else {
      a_probs_[orig_pa]->RMProbs(st, orig_pa, nt, a_offset, num_succs, dsi, probs.get());
      // a_probs_[orig_pa]->Probs(orig_pa, st, nt, a_offset, num_succs, dsi, probs.get());
#if 0
      fprintf(stderr, "a pa %u st %u nt %u bd %u nhcp %u hcp %u b_offset %u "
	      "probs %f %f r %f\n", orig_pa, st, nt, bd, num_hole_card_pairs,
	      hcp, a_offset, probs[0], probs[1], r);
#endif
    }
    int s;
    for (s = 0; s < num_succs - 1; ++s) {
      // Look up probabilities with orig_pa which may be different from
      // actual_pa.
      double prob = probs[s];
      cum += prob;
      if (r < cum) break;
    }
    if (s == node->CallSuccIndex()) {
      unique_ptr<Node * []> succ_nodes(new Node *[num_players_]);
      for (int p = 0; p < num_players_; ++p) {
	int csi = nodes[p]->CallSuccIndex();
	succ_nodes[p] = nodes[p]->IthSucc(csi);
      }
      // fprintf(stderr, "Call\n");
      contributions[actual_pa] = last_bet_to;
      Play(succ_nodes.get(), b_pos, contributions, last_bet_to, folded, num_remaining, actual_pa,
	   st, outcomes);
    } else if (s == node->FoldSuccIndex()) {
      unique_ptr<Node * []> succ_nodes(new Node *[num_players_]);
      for (int p = 0; p < num_players_; ++p) {
	int fsi = nodes[p]->FoldSuccIndex();
	succ_nodes[p] = nodes[p]->IthSucc(fsi);
      }
      // fprintf(stderr, "Fold\n");
      folded[actual_pa] = true;
      outcomes[actual_pa] = -(int)contributions[actual_pa];
      Play(succ_nodes.get(), b_pos, contributions, last_bet_to, folded, num_remaining - 1,
	   actual_pa, st, outcomes);
    } else {
      Node *my_succ = node->IthSucc(s);
      int new_bet_to = my_succ->LastBetTo();
      unique_ptr<Node * []> succ_nodes(new Node *[num_players_]);
      for (int p = 0; p < num_players_; ++p) {
	int ps;
	Node *p_node = nodes[p];
	int p_num_succs = p_node->NumSuccs();
	for (ps = 0; ps < p_num_succs; ++ps) {
	  if (ps == p_node->CallSuccIndex() || ps == p_node->FoldSuccIndex()) {
	    continue;
	  }
	  if (p_node->IthSucc(ps)->LastBetTo() == new_bet_to) break;
	}
	if (ps == p_num_succs) {
	  fprintf(stderr, "No matching succ\n");
	  exit(-1);
	}
	succ_nodes[p] = nodes[p]->IthSucc(ps);
      }
      // fprintf(stderr, "Bet\n");
      contributions[actual_pa] = new_bet_to;
      Play(succ_nodes.get(), b_pos, contributions, new_bet_to, folded, num_remaining, actual_pa,
	   st, outcomes);
    }
  }
}

static int PrecedingPlayer(int p) {
  if (p == 0) return Game::NumPlayers() - 1;
  else        return p - 1;
}

// Play one hand of duplicate, which is a pair of regular hands.  Return
// outcome from A's perspective.
void Player::PlayDuplicateHand(unsigned long long int h, const Card *cards, bool deterministic,
			       double *a_sum, double *b_sum) {
  unique_ptr<double []> outcomes(new double[num_players_]);
  unique_ptr<int []> contributions(new int[num_players_]);
  unique_ptr<bool []> folded(new bool[num_players_]);
  // Assume the big blind is last to act preflop
  // Assume the small blind is prior to the big blind
  int big_blind_p = PrecedingPlayer(Game::FirstToAct(0));
  int small_blind_p = PrecedingPlayer(big_blind_p);
  *a_sum = 0;
  *b_sum = 0;
  for (int b_pos = 0; b_pos < num_players_; ++b_pos) {
    if (deterministic) {
      // Reseed the RNG again before play within this loop.  This ensure
      // that if we play a system against itself, the duplicate outcome will
      // always be zero.
      //
      // This has a big impact on the average P1 outcome - why?  Too much
      // coordination between the RNG for dealing the cards and the RNG for
      // actions?
      //
      // Temporary - to match play_agents
      // SeedRand(h);
      // Generate a separate seed for each player that depends on the hand
      // index.
      for (int p = 0; p < num_players_; ++p) {
	srand48_r(h * num_players_ + p, &rand_bufs_[p]);
      }
    }
    
    for (int p = 0; p < num_players_; ++p) {
      folded[p] = false;
      if (p == small_blind_p) {
	contributions[p] = Game::SmallBlind();
      } else if (p == big_blind_p) {
	contributions[p] = Game::BigBlind();
      } else {
	contributions[p] = 0;
      }
    }
    unique_ptr<Node * []> nodes(new Node *[num_players_]);
    for (int p = 0; p < num_players_; ++p) {
      nodes[p] = betting_trees_[p]->Root();
    }
    Play(nodes.get(), b_pos, contributions.get(), Game::BigBlind(), folded.get(), num_players_,
	 1000, -1, outcomes.get());
    for (int p = 0; p < num_players_; ++p) {
      if (p == b_pos) {
	*b_sum += outcomes[p];
      } else {
	*a_sum += outcomes[p];
      }
      sum_pos_outcomes_[p] += outcomes[p];
    }
  }
}

void Player::DealNCards(Card *cards, int n) {
  int max_card = Game::MaxCard();
  for (int i = 0; i < n; ++i) {
    Card c;
    while (true) {
      // c = RandBetween(0, max_card);
      double r;
      drand48_r(&rand_bufs_[0], &r);
      c = (max_card + 1) * r;
      int j;
      for (j = 0; j < i; ++j) {
	if (cards[j] == c) break;
      }
      if (j == i) break;
    }
    cards[i] = c;
  }
}

void Player::SetHCPsAndBoards(Card **raw_hole_cards, const Card *raw_board) {
  int max_street = Game::MaxStreet();
  for (int st = 0; st <= max_street; ++st) {
    if (st == 0) {
      for (int p = 0; p < num_players_; ++p) {
	raw_hcps_[p][0] = HCPIndex(st, raw_hole_cards[p]);
      }
    } else {
      // Store the hole cards *after* the board cards
      int num_hole_cards = Game::NumCardsForStreet(0);
      int num_board_cards = Game::NumBoardCards(st);
      for (int p = 0; p < num_players_; ++p) {
	Card canon_board[5];
	Card canon_hole_cards[2];
	CanonicalizeCards(raw_board, raw_hole_cards[p], st, canon_board, canon_hole_cards);
	// Don't need to do this repeatedly
	if (p == 0) {
	  boards_[st] = BoardTree::LookupBoard(canon_board, st);
	}
	Card canon_cards[7];
	for (int i = 0; i < num_board_cards; ++i) {
	  canon_cards[num_hole_cards + i] = canon_board[i];
	}
	for (int i = 0; i < num_hole_cards; ++i) {
	  canon_cards[i] = canon_hole_cards[i];
	}
	raw_hcps_[p][st] = HCPIndex(st, canon_cards);
      }
    }
  }
}

void Player::Go(unsigned long long int num_duplicate_hands, bool deterministic) {
  double sum_a_outcomes = 0, sum_b_outcomes = 0;
  double sum_sqd_a_outcomes = 0, sum_sqd_b_outcomes = 0;
  int max_street = Game::MaxStreet();
  int num_board_cards = Game::NumBoardCards(max_street);
  Card cards[100], hand_cards[7];
  Card **hole_cards = new Card *[num_players_];
  for (int p = 0; p < num_players_; ++p) {
    hole_cards[p] = new Card[2];
  }
  if (! deterministic) InitRand();
  for (unsigned long long int h = 0; h < num_duplicate_hands; ++h) {
    if (deterministic) {
      // Seed just as we do in play_agents so we can get the same cards and
      // compare results.
      // SeedRand(h);
      srand48_r(h, &rand_bufs_[0]);
    }
    // Assume 2 hole cards
    DealNCards(cards, num_board_cards + 2 * num_players_);
#if 0
    OutputNCards(cards + 2 * num_players_, num_board_cards);
    printf("\n");
    OutputTwoCards(cards);
    printf("\n");
    OutputTwoCards(cards + 2);
    printf("\n");
    fflush(stdout);
#endif
    for (int p = 0; p < num_players_; ++p) {
      SortCards(cards + 2 * p, 2);
    }
    int num = 2 * num_players_;
    for (int st = 1; st <= max_street; ++st) {
      int num_street_cards = Game::NumCardsForStreet(st);
      SortCards(cards + num, num_street_cards);
      num += num_street_cards;
    }
    for (int i = 0; i < num_board_cards; ++i) {
      hand_cards[i+2] = cards[i + 2 * num_players_];
    }
    for (int p = 0; p < num_players_; ++p) {
      hand_cards[0] = cards[2 * p];
      hand_cards[1] = cards[2 * p + 1];
      hvs_[p] = HandValueTree::Val(hand_cards);
      hole_cards[p][0] = cards[2 * p];
      hole_cards[p][1] = cards[2 * p + 1];
    }
    
    SetHCPsAndBoards(hole_cards, cards + 2 * num_players_);

    // PlayDuplicateHand() returns the result of a duplicate hand (which is
    // N hands if N is the number of players)
    double a_outcome, b_outcome;
    PlayDuplicateHand(h, cards, deterministic, &a_outcome, &b_outcome);
    sum_a_outcomes += a_outcome;
    sum_b_outcomes += b_outcome;
    sum_sqd_a_outcomes += a_outcome * a_outcome;
    sum_sqd_b_outcomes += b_outcome * b_outcome;
  }
  for (int p = 0; p < num_players_; ++p) {
    delete [] hole_cards[p];
  }
  delete [] hole_cards;
#if 0
  unsigned long long int num_a_hands =
    (num_players_ - 1) * num_players_ * num_duplicate_hands;
  double mean_a_outcome = sum_a_outcomes / (double)num_a_hands;
#endif
  // Divide by num_players because we evaluate B that many times (once for
  // each position).
  unsigned long long int num_b_hands = num_duplicate_hands * num_players_;
  double mean_b_outcome = sum_b_outcomes / (double)num_b_hands;
  // Need to divide by two to convert from small blind units to big blind units
  // Multiply by 1000 to go from big blinds to milli-big-blinds
  double b_mbb_g = (mean_b_outcome / 2.0) * 1000.0;
  printf("Avg B outcome: %f (%.1f mbb/g) over %llu dup hands\n", mean_b_outcome, b_mbb_g,
	 num_duplicate_hands);
  // Variance is the mean of the squares minus the square of the means
  double var_b =
    (sum_sqd_b_outcomes / ((double)num_b_hands)) -
    (mean_b_outcome * mean_b_outcome);
  double stddev_b = sqrt(var_b);
  double match_stddev = stddev_b * sqrt(num_b_hands);
  double match_lower = sum_b_outcomes - 1.96 * match_stddev;
  double match_upper = sum_b_outcomes + 1.96 * match_stddev;
  double mbb_lower =
    ((match_lower / (num_b_hands)) / 2.0) * 1000.0;
  double mbb_upper =
    ((match_upper / (num_b_hands)) / 2.0) * 1000.0;
  printf("MBB confidence interval: %f-%f\n", mbb_lower, mbb_upper);
  fflush(stdout);

  for (int p = 0; p < num_players_; ++p) {
    double avg_outcome =
      sum_pos_outcomes_[p] / (double)(num_players_ * num_duplicate_hands);
    printf("Avg P%u outcome: %f\n", p, avg_outcome);
    fflush(stdout);
  }
}

Player::Player(const BettingAbstraction &ba, const CardAbstraction &a_ca,
	       const CardAbstraction &b_ca, const CFRConfig &a_cc, const CFRConfig &b_cc, int a_it,
	       int b_it) {
  a_buckets_ = new Buckets(a_ca, false);
  if (strcmp(a_ca.CardAbstractionName().c_str(), b_ca.CardAbstractionName().c_str())) {
    b_buckets_ = new Buckets(b_ca, false);
  } else {
    b_buckets_ = a_buckets_;
  }
  num_players_ = Game::NumPlayers();
  hvs_.reset(new int[num_players_]);
  winners_.reset(new bool[num_players_]);
  sum_pos_outcomes_.reset(new double[num_players_]);
  BoardTree::Create();
  BoardTree::CreateLookup();

  asymmetric_ = ba.Asymmetric();
  betting_trees_ = new BettingTree *[num_players_];
  if (asymmetric_) {
    for (int asym_p = 0; asym_p < num_players_; ++asym_p) {
      betting_trees_[asym_p] = BettingTree::BuildAsymmetricTree(ba, asym_p);
    }
  } else {
    betting_trees_[0] = BettingTree::BuildTree(ba);
    for (int asym_p = 1; asym_p < num_players_; ++asym_p) {
      betting_trees_[asym_p] = betting_trees_[0];
    }
  }
  
  a_probs_ = new CFRValues *[num_players_];
  b_probs_ = new CFRValues *[num_players_];
  for (int p = 0; p < num_players_; ++p) {
    unique_ptr<bool []> players(new bool[num_players_]);
    for (int p1 = 0; p1 < num_players_; ++p1) {
      players[p1] = (p1 == p);
    }
    a_probs_[p] = new CFRValues(players.get(), nullptr, 0, 0, *a_buckets_, betting_trees_[p]);
    b_probs_[p] = new CFRValues(players.get(), nullptr, 0, 0, *b_buckets_, betting_trees_[p]);
  }

  char dir[500], buf[100];
  for (int p = 0; p < num_players_; ++p) {
    sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::OldCFRBase(),
	    Game::GameName().c_str(), Game::NumPlayers(),
	    a_ca.CardAbstractionName().c_str(),
	    Game::NumRanks(), Game::NumSuits(), Game::MaxStreet(),
	    ba.BettingAbstractionName().c_str(),
	    a_cc.CFRConfigName().c_str());
    if (asymmetric_) {
      sprintf(buf, ".p%u", p);
      strcat(dir, buf);
    }
    a_probs_[p]->Read(dir, a_it, betting_trees_[p]->Root(), "x", -1, true);
    // fprintf(stderr, "Read A P%u probs\n", p);
    sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::OldCFRBase(), Game::GameName().c_str(),
	    Game::NumPlayers(), b_ca.CardAbstractionName().c_str(), Game::NumRanks(),
	    Game::NumSuits(), Game::MaxStreet(), ba.BettingAbstractionName().c_str(),
	    b_cc.CFRConfigName().c_str());
    if (asymmetric_) {
      sprintf(buf, ".p%u", p);
      strcat(dir, buf);
    }
    b_probs_[p]->Read(dir, b_it, betting_trees_[p]->Root(), "x", -1, true);
    // fprintf(stderr, "Read B P%u probs\n", p);
  }

  int max_street = Game::MaxStreet();
  boards_ = new int[max_street + 1];
  boards_[0] = 0;
  raw_hcps_ = new int *[num_players_];
  for (int p = 0; p < num_players_; ++p) {
    raw_hcps_[p] = new int[max_street + 1];
  }

  if (a_buckets_->None(max_street) || b_buckets_->None(max_street)) {
    int num_hole_card_pairs = Game::NumHoleCardPairs(max_street);
    int num_boards = BoardTree::NumBoards(max_street);
    sorted_hcps_ = new unsigned short *[num_boards];
    Card cards[7];
    int num_hole_cards = Game::NumCardsForStreet(0);
    int num_board_cards = Game::NumBoardCards(max_street);
    for (int bd = 0; bd < num_boards; ++bd) {
      const Card *board = BoardTree::Board(max_street, bd);
      for (int i = 0; i < num_board_cards; ++i) {
	cards[i + num_hole_cards] = board[i];
      }
      int sg = BoardTree::SuitGroups(max_street, bd);
      CanonicalCards hands(2, board, num_board_cards, sg, false);
      hands.SortByHandStrength(board);
      sorted_hcps_[bd] = new unsigned short[num_hole_card_pairs];
      for (int shcp = 0; shcp < num_hole_card_pairs; ++shcp) {
	const Card *hole_cards = hands.Cards(shcp);
	for (int i = 0; i < num_hole_cards; ++i) {
	  cards[i] = hole_cards[i];
	}
	int rhcp = HCPIndex(max_street, cards);
	sorted_hcps_[bd][rhcp] = shcp;
      }
    }
    fprintf(stderr, "Created sorted_hcps_\n");
  } else {
    sorted_hcps_ = nullptr;
    fprintf(stderr, "Not creating sorted_hcps_\n");
  }

  for (int p = 0; p < num_players_; ++p) {
    sum_pos_outcomes_[p] = 0;
  }

  rand_bufs_ = new drand48_data[num_players_];
}

Player::~Player(void) {
  delete [] rand_bufs_;
  if (sorted_hcps_) {
    int max_street = Game::MaxStreet();
    int num_boards = BoardTree::NumBoards(max_street);
    for (int bd = 0; bd < num_boards; ++bd) {
      delete [] sorted_hcps_[bd];
    }
    delete [] sorted_hcps_;
  }
  delete [] boards_;
  for (int p = 0; p < num_players_; ++p) {
    delete [] raw_hcps_[p];
  }
  delete [] raw_hcps_;
  if (b_buckets_ != a_buckets_) delete b_buckets_;
  delete a_buckets_;
  for (int p = 0; p < num_players_; ++p) {
    delete a_probs_[p];
    delete b_probs_[p];
  }
  delete [] a_probs_;
  delete [] b_probs_;
  if (asymmetric_) {
    for (int asym_p = 0; asym_p < num_players_; ++asym_p) {
      delete betting_trees_[asym_p];
    }
  } else {
    delete betting_trees_[0];
  }
  delete [] betting_trees_;
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <A card params> <B card params> "
	  "<betting abstraction params> <A CFR params> <B CFR params> <A it> <B it> "
	  "<num duplicate hands> [determ|nondeterm]\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 11) Usage(argv[0]);
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
  unique_ptr<Params> betting_params = CreateBettingAbstractionParams();
  betting_params->ReadFromFile(argv[4]);
  unique_ptr<BettingAbstraction>
    betting_abstraction(new BettingAbstraction(*betting_params));
  unique_ptr<Params> a_cfr_params = CreateCFRParams();
  a_cfr_params->ReadFromFile(argv[5]);
  unique_ptr<CFRConfig>
    a_cfr_config(new CFRConfig(*a_cfr_params));
  unique_ptr<Params> b_cfr_params = CreateCFRParams();
  b_cfr_params->ReadFromFile(argv[6]);
  unique_ptr<CFRConfig>
    b_cfr_config(new CFRConfig(*b_cfr_params));

  int a_it, b_it;
  if (sscanf(argv[7], "%i", &a_it) != 1) Usage(argv[0]);
  if (sscanf(argv[8], "%i", &b_it) != 1) Usage(argv[0]);
  unsigned long long int num_duplicate_hands;
  if (sscanf(argv[9], "%llu", &num_duplicate_hands) != 1) Usage(argv[0]);
  string darg = argv[10];
  bool deterministic;
  if (darg == "determ")         deterministic = true;
  else if (darg == "nondeterm") deterministic = false;
  else                          Usage(argv[0]);

  if (deterministic) {
    fprintf(stderr, "WARNING: have seen inaccurate results with determ\n");
    exit(-1);
  }
  
  HandValueTree::Create();

  // Leave this in if we don't want reproducibility
  InitRand();

  Player *player = new Player(*betting_abstraction, *a_card_abstraction, *b_card_abstraction,
			      *a_cfr_config, *b_cfr_config, a_it, b_it);
  player->Go(num_duplicate_hands, deterministic);
  delete player;
}
