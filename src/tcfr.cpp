// Should initialize buckets outside for either all streets or all streets
// but river if using HVB.
//
// Targeted CFR.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // sleep()

#include <algorithm>
#include <string>

#include "betting_abstraction.h"
#include "betting_tree.h"
#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "card_abstraction.h"
#include "cfr_config.h"
#include "constants.h"
#include "files.h"
#include "game.h"
#include "hand_tree.h"
#include "hand_value_tree.h"
#include "io.h"
#include "nonterminal_ids.h"
#include "rand.h"
#include "regret_compression.h"
#include "split.h"
#include "tcfr.h"

using namespace std;

// #define BC 1
#define SWITCH 1

TCFRThread::TCFRThread(const BettingAbstraction &ba, const CFRConfig &cc, const Buckets &buckets,
		       int batch_index, int thread_index, int num_threads, unsigned char *data,
		       int target_player, float *rngs, unsigned int *uncompress,
		       unsigned int *short_uncompress, unsigned int *pruning_thresholds,
		       bool **sumprob_streets, const double *boost_thresholds, const bool *freeze,
		       unsigned char *hvb_table, unsigned char ***cards_to_indices,
		       int num_raw_boards, const int *board_table, int batch_size,
		       unsigned long long int *total_its) :
  betting_abstraction_(ba), cfr_config_(cc), buckets_(buckets) {
  batch_index_ = batch_index;
  thread_index_ = thread_index;
  num_threads_ = num_threads;
  data_ = data;
  asymmetric_ = betting_abstraction_.Asymmetric();
  num_players_ = Game::NumPlayers();
  target_player_ = target_player;
  rngs_ = rngs;
  rng_index_ = RandZeroToOne() * kNumPregenRNGs;
  uncompress_ = uncompress;
  short_uncompress_ = short_uncompress;
  pruning_thresholds_ = pruning_thresholds;
  sumprob_streets_ = sumprob_streets;
  boost_thresholds_ = boost_thresholds;
  freeze_ = freeze;
  hvb_table_ = hvb_table;
  cards_to_indices_ = cards_to_indices;
  num_raw_boards_ = num_raw_boards;
  board_table_ = board_table;
  batch_size_ = batch_size;
  total_its_ = total_its;
  
  max_street_ = Game::MaxStreet();
  char_quantized_streets_.reset(new bool[max_street_ + 1]);
  for (int st = 0; st <= max_street_; ++st) {
    char_quantized_streets_[st] = cfr_config_.CharQuantizedStreet(st);
  }
  short_quantized_streets_.reset(new bool[max_street_ + 1]);
  for (int st = 0; st <= max_street_; ++st) {
    short_quantized_streets_[st] = cfr_config_.ShortQuantizedStreet(st);
  }
  scaled_streets_ = new bool[max_street_ + 1];
  for (int st = 0; st <= max_street_; ++st) {
    scaled_streets_[st] = cfr_config_.ScaledStreet(st);
  }
  explore_ = cfr_config_.Explore();
  full_only_avg_update_ = true; // cfr_config_.FullOnlyAvgUpdate();
  deal_twice_ = cfr_config_.DealTwice();
  canon_bds_ = new int[max_street_ + 1];
  canon_bds_[0] = 0;
  hi_cards_ = new int[num_players_];
  lo_cards_ = new int[num_players_];
  hole_cards_ = new int[num_players_ * 2];
  hvs_ = new int[num_players_];
  hand_buckets_ = new int[num_players_ * (max_street_ + 1)];
  winners_ = new int[num_players_];
  contributions_ = new int[num_players_];
  folded_ = new bool[num_players_];
  succ_value_stack_ = new T_VALUE *[kStackDepth];
  succ_iregret_stack_ = new int *[kStackDepth];
  for (unsigned int i = 0; i < kStackDepth; ++i) {
    succ_value_stack_[i] = new T_VALUE[kMaxSuccs];
    succ_iregret_stack_[i] = new int[kMaxSuccs];
  }
  if (cfr_config_.NNR()) {
    fprintf(stderr, "NNR not supported\n");
    exit(-1);
  }
  const vector<int> &fv = cfr_config_.RegretFloors();
  if (fv.size() > 0) {
    fprintf(stderr, "Regret floors not supported\n");
    exit(-1);
  }
  sumprob_ceilings_ = new unsigned int[max_street_ + 1];
  const vector<int> &cv = cfr_config_.SumprobCeilings();
  if (cv.size() == 0) {
    for (int st = 0; st <= max_street_; ++st) {
      // Set ceiling below maximum value for a *signed* integer, even though
      // sumprobs are unsigned.  We use the same code (e.g., in CFRValues) to
      // read in regrets and sumprobs, and regrets are signed.
      // Allow a little headroom to avoid overflow
      sumprob_ceilings_[st] = 2000000000U;
    }
  } else {
    if ((int)cv.size() != max_street_ + 1) {
      fprintf(stderr, "Sumprob ceiling vector wrong size\n");
      exit(-1);
    }
    for (int st = 0; st <= max_street_; ++st) {
      sumprob_ceilings_[st] = cv[st];
    }
  }

  if (hvb_table_) {
    bytes_per_hand_ = 4ULL;
    if (buckets_.NumBuckets(max_street_) <= 65536) bytes_per_hand_ += 2;
    else                                           bytes_per_hand_ += 4;
  } else {
    bytes_per_hand_ = 0ULL;
  }

  full_ = new bool[max_street_ + 1];
  close_thresholds_.reset(new unsigned int[max_street_ + 1]);
  for (int st = 0; st <= max_street_; ++st) {
    // CFRConfig currently does not support a per-street close threshold
    close_thresholds_[st] = cfr_config_.CloseThreshold();
  }

  active_mod_ = cfr_config_.ActiveMod();
  if (active_mod_ == 0) {
    fprintf(stderr, "Must set ActiveMod\n");
    exit(-1);
  }
  num_active_conditions_ = cfr_config_.NumActiveConditions();
  if (num_active_conditions_ > 0) {
    num_active_streets_ = new int[num_active_conditions_];
    num_active_rems_ = new int[num_active_conditions_];
    active_streets_ = new int *[num_active_conditions_];
    active_rems_ = new int *[num_active_conditions_];
    for (int c = 0; c < num_active_conditions_; ++c) {
      num_active_streets_[c] = cfr_config_.NumActiveStreets(c);
      num_active_rems_[c] = cfr_config_.NumActiveRems(c);
      active_streets_[c] = new int[num_active_streets_[c]];
      for (int i = 0; i < num_active_streets_[c]; ++i) {
	active_streets_[c][i] = cfr_config_.ActiveStreet(c, i);
      }
      active_rems_[c] = new int[num_active_rems_[c]];
      for (int i = 0; i < num_active_rems_[c]; ++i) {
	active_rems_[c][i] = cfr_config_.ActiveRem(c, i);
      }
    }
  } else {
    num_active_streets_ = NULL;
    num_active_rems_ = NULL;
    active_streets_ = NULL;
    active_rems_ = NULL;
  }

  srand48_r(batch_index_ * num_threads_ + thread_index_, &rand_buf_);
}

TCFRThread::~TCFRThread(void) {
  for (int c = 0; c < num_active_conditions_; ++c) {
    delete [] active_streets_[c];
    delete [] active_rems_[c];
  }
  delete [] num_active_streets_;
  delete [] num_active_rems_;
  delete [] active_streets_;
  delete [] active_rems_;
  delete [] full_;
  delete [] sumprob_ceilings_;
  delete [] scaled_streets_;
  for (int i = 0; i < kStackDepth; ++i) {
    delete [] succ_value_stack_[i];
    delete [] succ_iregret_stack_[i];
  }
  delete [] succ_value_stack_;
  delete [] succ_iregret_stack_;
  delete [] hand_buckets_;
  delete [] canon_bds_;
  delete [] hi_cards_;
  delete [] lo_cards_;
  delete [] hole_cards_;
  delete [] hvs_;
  delete [] winners_;
  delete [] contributions_;
  delete [] folded_;
}

void TCFRThread::HVBDealHand(void) {
  double r;
  drand48_r(&rand_buf_, &r);
#ifdef BC
  unsigned int num_boards = BoardTree::NumBoards(max_street_);
  unsigned int msbd = r * num_boards;
  board_count_ = BoardTree::BoardCount(max_street_, msbd);
#else
  unsigned int msbd = board_table_[(int)(r * num_raw_boards_)];
#endif
  canon_bds_[max_street_] = msbd;
  for (int st = 1; st < max_street_; ++st) {
    canon_bds_[st] = BoardTree::PredBoard(msbd, st);
  }
  const Card *board = BoardTree::Board(max_street_, msbd);
  unsigned int num_ms_board_cards = Game::NumBoardCards(max_street_);
  unsigned int end_cards = Game::MaxCard() + 1;

  for (int p = 0; p < num_players_; ++p) {
    int c1, c2;
    while (true) {
      drand48_r(&rand_buf_, &r);
      c1 = end_cards * r;
      if (InCards(c1, board, num_ms_board_cards)) continue;
      if (InCards(c1, hole_cards_, 2 * p)) continue;
      break;
    }
    hole_cards_[2 * p] = c1;
    while (true) {
      drand48_r(&rand_buf_, &r);
      c2 = end_cards * r;
      if (InCards(c2, board, num_ms_board_cards)) continue;
      if (InCards(c2, hole_cards_, 2 * p + 1)) continue;
      break;
    }
    hole_cards_[2 * p + 1] = c2;
    if (c1 > c2) {hi_cards_[p] = c1; lo_cards_[p] = c2;}
    else         {hi_cards_[p] = c2; lo_cards_[p] = c1;}
  }
  
  for (int p = 0; p < num_players_; ++p) hvs_[p] = 0;

  for (int st = 0; st <= max_street_; ++st) {
    int num_hole_card_pairs = Game::NumHoleCardPairs(st);
    int bd = canon_bds_[st];
    unsigned int base = ((unsigned int)bd) * ((unsigned int)num_hole_card_pairs);
    for (int p = 0; p < num_players_; ++p) {
      unsigned char hi = cards_to_indices_[st][bd][hi_cards_[p]];
      unsigned char li = cards_to_indices_[st][bd][lo_cards_[p]];
      // The sum from 1... hi_index - 1 is the number of hole card pairs
      // containing a high card less than hi.
      unsigned int hcp = (hi - 1) * hi / 2 + li;
      unsigned int h = base + hcp;

      if (st == max_street_) {
	unsigned char *ptr = &hvb_table_[h * bytes_per_hand_];
	if (buckets_.NumBuckets(max_street_) <= 65536) {
	  hand_buckets_[p * (max_street_ + 1) + max_street_] =
	    *(unsigned short *)ptr;
	  ptr += 2;
	} else {
	  hand_buckets_[p * (max_street_ + 1) + max_street_] =
	    *(int *)ptr;
	  ptr += 4;
	}
	hvs_[p] = *(unsigned int *)ptr;
      } else {
	hand_buckets_[p * (max_street_ + 1) + st] = buckets_.Bucket(st, h);
      }
    }
  }
}

// Our old implementation which is a bit slower.
void TCFRThread::NoHVBDealHand(void) {
  double r;
  drand48_r(&rand_buf_, &r);
#ifdef BC
  unsigned int num_boards = BoardTree::NumBoards(max_street_);
  unsigned int msbd = r * num_boards;
  board_count_ = BoardTree::BoardCount(max_street_, msbd);
#else
  unsigned int msbd = board_table_[(int)(r * num_raw_boards_)];
#endif
  canon_bds_[max_street_] = msbd;
  for (int st = 1; st < max_street_; ++st) {
    canon_bds_[st] = BoardTree::PredBoard(msbd, st);
  }
  const Card *board = BoardTree::Board(max_street_, msbd);
  Card cards[7];
  unsigned int num_ms_board_cards = Game::NumBoardCards(max_street_);
  for (unsigned int i = 0; i < num_ms_board_cards; ++i) {
    cards[i+2] = board[i];
  }
  int end_cards = Game::MaxCard() + 1;

  for (int p = 0; p < num_players_; ++p) {
    int c1, c2;
    while (true) {
      drand48_r(&rand_buf_, &r);
      c1 = end_cards * r;
      if (InCards(c1, board, num_ms_board_cards)) continue;
      if (InCards(c1, hole_cards_, 2 * p)) continue;
      break;
    }
    hole_cards_[2 * p] = c1;
    while (true) {
      drand48_r(&rand_buf_, &r);
      c2 = end_cards * r;
      if (InCards(c2, board, num_ms_board_cards)) continue;
      if (InCards(c2, hole_cards_, 2 * p + 1)) continue;
      break;
    }
    hole_cards_[2 * p + 1] = c2;
    if (c1 > c2) {hi_cards_[p] = c1; lo_cards_[p] = c2;}
    else         {hi_cards_[p] = c2; lo_cards_[p] = c1;}
  }


  for (int p = 0; p < num_players_; ++p) hvs_[p] = 0;

  for (int st = 0; st <= max_street_; ++st) {
    int bd = canon_bds_[st];
    int num_hole_card_pairs = Game::NumHoleCardPairs(st);
    for (int p = 0; p < num_players_; ++p) {
      cards[0] = hi_cards_[p];
      cards[1] = lo_cards_[p];
      unsigned int hcp = HCPIndex(st, cards);
      unsigned int h = ((unsigned int)bd) * ((unsigned int)num_hole_card_pairs) + hcp;
      hand_buckets_[p * (max_street_ + 1) + st] = buckets_.Bucket(st, h);
      if (st == max_street_) {
	hvs_[p] = HandValueTree::Val(cards);
      }
    }
  }
}

static int PrecedingPlayer(unsigned int p) {
  if (p == 0) return Game::NumPlayers() - 1;
  else        return p - 1;
}

static double **g_preflop_vals = nullptr;
static unsigned long long int **g_preflop_nums = nullptr;

void TCFRThread::Run(void) {
  process_count_ = 0ULL;
  full_process_count_ = 0ULL;
  it_ = 1;
  unique_ptr<long long int []> sum_values(new long long int[num_players_]);
  unique_ptr<long long int []> denoms(new long long int[num_players_]);
  for (int p = 0; p < num_players_; ++p) {
    sum_values[p] = 0LL;
    denoms[p] = 0LL;
  }
  
  while (1) {
    if (*total_its_ >= ((unsigned long long int)batch_size_) * num_threads_) {
      fprintf(stderr, "Thread %u performed %llu iterations\n", thread_index_, it_);
      break;
    }

    if (! deal_twice_) {
      if (hvb_table_) HVBDealHand();
      else            NoHVBDealHand();
    }

    if (it_ % 10000000 == 1 && thread_index_ == 0) {
      fprintf(stderr, "It %llu\n", it_);
    }

    all_full_ = false;

    for (int st = 0; st <= max_street_; ++st) full_[st] = false;
    int rem = it_ % active_mod_;
    int c;
    for (c = 0; c < num_active_conditions_; ++c) {
      int num = num_active_rems_[c];
      for (int i = 0; i < num; ++i) {
	int this_rem = active_rems_[c][i];
	if (rem == this_rem) {
	  goto BREAKOUT;
	}
      }
    }
  BREAKOUT:
    if (c == num_active_conditions_) {
      all_full_ = true;
    } else {
      int num = num_active_streets_[c];
      for (int i = 0; i < num; ++i) {
	int st = active_streets_[c][i];
	full_[st] = true;
      }
    }

    int start, end, incr;
#ifdef SWITCH
    if ((it_ / active_mod_) % 2 == 0) {
      start = 0;
      end = num_players_;
      incr = 1;
    } else {
      start = num_players_ - 1;
      end = -1;
      incr = -1;
    }
#else
    start = 0;
    end = num_players_;
    incr = 1;
#endif
    for (int p = start; p != end; p += incr) {
      if (freeze_[p]) continue;
      p_ = p;
      if (deal_twice_) {
	if (hvb_table_) HVBDealHand();
	else            NoHVBDealHand();
      }
      stack_index_ = 0;
      // Assume the big blind is last to act preflop
      // Assume the small blind is prior to the big blind
      int big_blind_p = PrecedingPlayer(Game::FirstToAct(0));
      int small_blind_p = PrecedingPlayer(big_blind_p);
      for (int p = 0; p < num_players_; ++p) {
	folded_[p] = false;
	if (p == small_blind_p) {
	  contributions_[p] = Game::SmallBlind();
	} else if (p == big_blind_p) {
	  contributions_[p] = Game::BigBlind();
	} else {
	  contributions_[p] = 0;
	}
      }
      T_VALUE val = Process(data_, 1000, -1);
      sum_values[p_] += val;
#ifdef BC
      denoms[p_] += board_count_;
#else
      ++denoms[p_];
#endif
      unsigned int b = hand_buckets_[p_ * (max_street_ + 1)];
      g_preflop_vals[p_][b] += val;
#ifdef BC
      g_preflop_nums[p_][b] += board_count_;
#else
      ++g_preflop_nums[p_][b];
#endif
    }

    ++it_;
    if (it_ % 10000000 == 0 && thread_index_ == 0) {
      for (int p = 0; p < num_players_; ++p) {
	fprintf(stderr, "It %llu avg P%u val %f\n", it_, p, sum_values[p] / (double)denoms[p]);
      }
    }
    if (num_threads_ == 1) {
      ++*total_its_;
    } else {
      if (it_ % 1000 == 0) {
	// To reduce the chance of multiple threads trying to update total_its_
	// at the same time, only update every 1000 iterations.
	*total_its_ += 1000;
      }
    }
  }
  // fprintf(stderr, "Thread %i done; performed %llu iterations\n", thread_index_, it_);
  if (thread_index_ == 0) {
    for (int p = 0; p < num_players_; ++p) {
      fprintf(stderr, "Thread %i avg P%u val %f\n", thread_index_, p,
	      sum_values[p] / (double)denoms[p]);
    }
  }
}

static void *thread_run(void *v_t) {
  TCFRThread *t = (TCFRThread *)v_t;
  t->Run();
  return NULL;
}

void TCFRThread::RunThread(void) {
  pthread_create(&pthread_id_, NULL, thread_run, this);
}

void TCFRThread::Join(void) {
  pthread_join(pthread_id_, NULL); 
}

int TCFRThread::Round(double d) {
  double rnd = rngs_[rng_index_++];
  if (rng_index_ == kNumPregenRNGs) rng_index_ = 0;
  if (d < 0) {
    int below = d;
    double rem = below - d;
    if (rnd < rem) {
      return below - 1;
    } else {
      return below;
    }
  } else {
    int below = d;
    double rem = d - below;
    if (rnd < rem) {
      return below + 1;
    } else {
      return below;
    }
  }
}

T_VALUE TCFRThread::Process(unsigned char *ptr, int last_player_acting, int last_st) {
  ++process_count_;
  if (all_full_) {
    ++full_process_count_;
  }
  unsigned char first_byte = ptr[0];
  if (first_byte == 1) {
    // Terminal node - could be showdown or fold

    // Find the best hand value of anyone remaining in the hand, and the
    // total pot size which includes contributions from remaining players
    // and players who folded earlier.
    int best_hv = 0;
    int pot_size = 0;
    int num_remaining = 0;
    for (int p = 0; p < num_players_; ++p) {
      pot_size += contributions_[p];
      if (! folded_[p]) {
	++num_remaining;
	int hv = hvs_[p];
	if (hv > best_hv) best_hv = hv;
      }
    }

    if (num_remaining == 1) {
      // A fold node.  Everyone has folded except one player.  We know the
      // one remaining player is the target player (p_) because when the
      // target player folds we return earlier and don't get here.
      // We want the pot size minus our contribution.  This is what we win.
#ifdef BC
      return (pot_size - contributions_[p_]) * board_count_;
#else
      return pot_size - contributions_[p_];
#endif
    } else {
      // Showdown
      
      // Determine if we won, the number of winners, and the total contribution
      // of all winners.
      int num_winners = 0;
      int winner_contributions = 0;
      bool we_win = false;
      for (int p = 0; p < num_players_; ++p) {
	if (! folded_[p] && hvs_[p] == best_hv) {
	  winners_[num_winners++] = p;
	  winner_contributions += contributions_[p];
	  we_win |= (p == p_);
	}
      }
      
      int ret;
      if (we_win) {
	// Our winnings is:
	// a) The total pot
	// b) Minus the contributions of the winners
	// c) Divided by the number of winners
	double winnings =
	  ((double)(pot_size - winner_contributions)) /
	  ((double)num_winners);
	// Normally the winnings are a whole number, but not always.
#ifdef BC
	ret = Round(winnings * board_count_);
#else
	ret = Round(winnings);
#endif
      } else {
	// If we lose at showdown, we lose the amount we contributed to the pot.
#ifdef BC
	ret = -contributions_[p_] * board_count_;
#else
	ret = -contributions_[p_];
#endif
      }
      return ret;
    }
  } else { // Nonterminal node
    int st = ptr[1];
    int num_succs = ptr[2];
    // Find the next player to act.  Start with the first candidate and move
    // forward until we find someone who has not folded.  The first candidate
    // is either the last player plus one, or, if we are starting a new
    // betting round, the first player to act on that street.
    int player_acting;
    if (st > last_st) {
      player_acting = Game::FirstToAct(st);
    } else {
      player_acting = last_player_acting + 1;
    }
    while (true) {
      if (player_acting == num_players_) player_acting = 0;
      if (! folded_[player_acting]) break;
      ++player_acting;
    }
    if (num_succs == 1) {
      unsigned long long int succ_offset =
	*((unsigned long long int *)(SUCCPTR(ptr)));
      return Process(data_ + succ_offset, player_acting, st);
    }
    int default_succ_index = 0;
    int fold_succ_index = ptr[3];
    int call_succ_index = ptr[4];
    // unsigned int player_acting = ptr[5];
    if (player_acting == p_) {
      // Our choice
      int our_bucket = hand_buckets_[p_ * (max_street_ + 1) + st];

      int size_bucket_data;
      if (char_quantized_streets_[st]) {
	size_bucket_data = num_succs;
      } else if (short_quantized_streets_[st]) {
	size_bucket_data = num_succs * 2;
      } else {
	size_bucket_data = num_succs * sizeof(T_REGRET);
      }
      if (sumprob_streets_[p_][st]) {
	if (! asymmetric_ || target_player_ == player_acting) {
	  size_bucket_data += num_succs * sizeof(T_SUM_PROB);
	}
      }
      unsigned char *ptr1 = SUCCPTR(ptr) + num_succs * 8;
      ptr1 += our_bucket * size_bucket_data;
      // ptr1 has now skipped past prior buckets

      int min_s = -1;
      // min_r2 is second best regret
      unsigned int min_r = kMaxUnsignedInt, min_r2 = kMaxUnsignedInt;

      if (char_quantized_streets_[st]) {
	unsigned char *bucket_regrets = ptr1;
	unsigned char min_qr = 255, min_qr2 = 255;
	for (int s = 0; s < num_succs; ++s) {
	  // There should always be one action with regret 0
	  unsigned char qr = bucket_regrets[s];
	  if (qr < min_qr) {
	    min_s = s;
	    min_qr2 = min_qr;
	    min_qr = qr;
	  } else if (qr < min_qr2) {
	    min_qr2 = qr;
	  }
	}
	min_r = uncompress_[min_qr];
	min_r2 = uncompress_[min_qr2];
      } else if (short_quantized_streets_[st]) {
	unsigned short *bucket_regrets = (unsigned short *)ptr1;
	unsigned short min_qr = 65535, min_qr2 = 65535;
	for (int s = 0; s < num_succs; ++s) {
	  // There should always be one action with regret 0
	  unsigned short qr = bucket_regrets[s];
	  if (qr < min_qr) {
	    min_s = s;
	    min_qr2 = min_qr;
	    min_qr = qr;
	  } else if (qr < min_qr2) {
	    min_qr2 = qr;
	  }
	}
	min_r = short_uncompress_[min_qr];
	min_r2 = short_uncompress_[min_qr2];
      } else {
	T_REGRET *bucket_regrets = (T_REGRET *)ptr1;
	for (int s = 0; s < num_succs; ++s) {
	  // There should always be one action with regret 0
	  T_REGRET r = bucket_regrets[s];
	  if (r < min_r) {
	    min_s = s;
	    min_r2 = min_r;
	    min_r = r;
	  } else if (r < min_r2) {
	    min_r2 = r;
	  }
	}
      }

      bool recurse_on_all;
      if (all_full_) {
	recurse_on_all = true;
      } else {
	// Could consider only recursing on close children.
	bool close = ((min_r2 - min_r) < close_thresholds_[st]);
	recurse_on_all = full_[st] || close;
      }

      T_VALUE *succ_values = succ_value_stack_[stack_index_];
      unsigned int pruning_threshold = pruning_thresholds_[st];
      T_VALUE val;
      if (! recurse_on_all) {
	int s = min_s;
	if (explore_ > 0) {
	  double thresh = explore_ * num_succs;
	  double rnd = rngs_[rng_index_++];
	  if (rng_index_ == kNumPregenRNGs) rng_index_ = 0;
	  if (rnd < thresh) {
	    s = rnd / explore_;
	  }
	}
	if (s == fold_succ_index) {
#ifdef BC
	  val = -contributions_[p_] * board_count_;
#else
	  val = -contributions_[p_];
#endif
	} else {
	  unsigned long long int succ_offset =
	    *((unsigned long long int *)(SUCCPTR(ptr) + s * 8));
	  int old_contribution = contributions_[p_];
	  if (s == call_succ_index) {
	    int last_bet_to = (int)*(unsigned short *)(ptr + 6);
	    contributions_[p_] = last_bet_to;
	  } else {
	    // bet_to amount is store in the child's data
	    unsigned char *succ_ptr = data_ + succ_offset;
	    int bet_to = (int)*(unsigned short *)(succ_ptr + 6);
	    contributions_[p_] = bet_to;
	  }
	  val = Process(data_ + succ_offset, player_acting, st);
	  contributions_[p_] = old_contribution;
	}
      } else { // Recursing on all succs
	for (int s = 0; s < num_succs; ++s) {
	  bool prune = false;
	  if (! char_quantized_streets_[st] && ! short_quantized_streets_[st]) {
	    T_REGRET *bucket_regrets = (T_REGRET *)ptr1;
	    prune = (bucket_regrets[s] >= pruning_threshold);
	  }
	  if (s == fold_succ_index || ! prune) {
	    if (s == fold_succ_index) {
#ifdef BC
	      succ_values[s] = -contributions_[p_] * board_count_;
#else
	      succ_values[s] = -contributions_[p_];
#endif
	    } else {
	      unsigned long long int succ_offset =
		*((unsigned long long int *)(SUCCPTR(ptr) + s * 8));
	      int old_contribution = contributions_[p_];
	      if (s == call_succ_index) {
		int last_bet_to = (int)*(unsigned short *)(ptr + 6);
		contributions_[p_] = last_bet_to;
	      } else {
		// bet_to amount is store in the child's data
		unsigned char *succ_ptr = data_ + succ_offset;
		int bet_to = (int)*(unsigned short *)(succ_ptr + 6);
		contributions_[p_] = bet_to;
	      }
	      ++stack_index_;
	      succ_values[s] = Process(data_ + succ_offset, player_acting, st);
	      --stack_index_;
	      contributions_[p_] = old_contribution;
	    }
	  }
	}
	val = succ_values[min_s];

	int *succ_iregrets = succ_iregret_stack_[stack_index_];
	int min_regret = kMaxInt;
	for (int s = 0; s < num_succs; ++s) {
	  int ucr;
	  if (char_quantized_streets_[st]) {
	    unsigned char *bucket_regrets = ptr1;
	    ucr = uncompress_[bucket_regrets[s]];
	  } else if (short_quantized_streets_[st]) {
	    unsigned short *bucket_regrets = (unsigned short *)ptr1;
	    ucr = short_uncompress_[bucket_regrets[s]];
	  } else {
	    T_REGRET *bucket_regrets = (T_REGRET *)ptr1;
	    if (s != fold_succ_index &&
		bucket_regrets[s] >= pruning_threshold) {
	      continue;
	    }
	    ucr = bucket_regrets[s];
	  }
	  int i_regret;
	  if (scaled_streets_[st]) {
	    int incr = succ_values[s] - val;
	    double scaled = incr * 0.005;
	    int trunc = scaled;
	    double rnd = rngs_[rng_index_++];
	    if (rng_index_ == kNumPregenRNGs) rng_index_ = 0;
	    if (scaled < 0) {
	      double rem = trunc - scaled;
	      if (rnd < rem) incr = trunc - 1;
	      else           incr = trunc;
	    } else {
	      double rem = scaled - trunc;
	      if (rnd < rem) incr = trunc + 1;
	      else           incr = trunc;
	    }
	    i_regret = ucr - incr;
	  } else {
	    i_regret = ucr - (succ_values[s] - val);
	  }
	  if (s == 0 || i_regret < min_regret) min_regret = i_regret;
	  succ_iregrets[s] = i_regret;
	}
	int offset = -min_regret;
	for (int s = 0; s < num_succs; ++s) {
	  // Assume no pruning if quantization for now
	  if (! char_quantized_streets_[st] && ! short_quantized_streets_[st]) {
	    T_REGRET *bucket_regrets = (T_REGRET *)ptr1;
	    if (s != fold_succ_index && bucket_regrets[s] >= pruning_threshold) {
	      continue;
	    }
	  }
	  int i_regret = succ_iregrets[s];
	  unsigned int r = (unsigned int)(i_regret + offset);
	  if (char_quantized_streets_[st]) {
	    unsigned char *bucket_regrets = ptr1;
	    double rnd = rngs_[rng_index_++];
	    if (rng_index_ == kNumPregenRNGs) rng_index_ = 0;
	    bucket_regrets[s] = CompressRegret(r, rnd, uncompress_);
	  } else if (short_quantized_streets_[st]) {
	    unsigned short *bucket_regrets = (unsigned short *)ptr1;
	    double rnd = rngs_[rng_index_++];
	    if (rng_index_ == kNumPregenRNGs) rng_index_ = 0;
	    bucket_regrets[s] = CompressRegretShort(r, rnd, short_uncompress_);
	  } else {
	    T_REGRET *bucket_regrets = (T_REGRET *)ptr1;
	    // Try capping instead of dividing by two.  Make sure to apply
	    // cap after adding offset.
	    if (r > 2000000000) r = 2000000000;
	    bucket_regrets[s] = r;
	  }
	}
      }
      return val;
    } else {
      // Opp choice
      unsigned int opp_bucket = hand_buckets_[player_acting * (max_street_ + 1) + st];

      unsigned char *ptr1 = SUCCPTR(ptr) + num_succs * 8;
      unsigned int size_bucket_data;
      if (char_quantized_streets_[st]) {
	size_bucket_data = num_succs;
      } else if (short_quantized_streets_[st]) {
	size_bucket_data = num_succs * 2;
      } else {
	size_bucket_data = num_succs * sizeof(T_REGRET);
      }
      if (sumprob_streets_[player_acting][st]) {
	if (! asymmetric_ || target_player_ == player_acting) {
	  size_bucket_data += num_succs * sizeof(T_SUM_PROB);
	}
      }

      ptr1 += opp_bucket * size_bucket_data;
      // ptr1 has now skipped past prior buckets

      // ss = "sampled succ"
      int ss;
      
      if (freeze_[player_acting]) {
	// If this player is frozen, then we play according to the average strategy (sumprobs),
	// not the current strategy (regrets).  We still sample just one succ.
	T_SUM_PROB *bucket_sum_probs;
	if (char_quantized_streets_[st]) {
	  bucket_sum_probs = (T_SUM_PROB *)(ptr1 + num_succs);
	} else if (short_quantized_streets_[st]) {
	  bucket_sum_probs = (T_SUM_PROB *)(ptr1 + num_succs * 2);
	} else {
	  bucket_sum_probs =
	    (T_SUM_PROB *)(ptr1 + num_succs * sizeof(T_REGRET));
	}
	unsigned long long int sum_sumprobs = 0;
	for (int s = 0; s < num_succs; ++s) {
	  sum_sumprobs += bucket_sum_probs[s];
	}
	if (sum_sumprobs == 0) {
	  ss = default_succ_index;
	} else {
	  double d_sum_sumprobs = sum_sumprobs;
	  double cum = 0;
	  double rnd = rngs_[rng_index_++];
	  if (rng_index_ == kNumPregenRNGs) rng_index_ = 0;
	  ss = 0;
	  for (ss = 0; ss < num_succs - 1; ++ss) {
	    double prob = bucket_sum_probs[ss] / d_sum_sumprobs;
	    cum += prob;
	    if (rnd < cum) break;
	  }
	}
      } else {
	ss = default_succ_index;
	// There should always be one action with regret 0
	if (char_quantized_streets_[st]) {
	  unsigned char *bucket_regrets = ptr1;
	  for (int s = 0; s < num_succs; ++s) {
	    if (bucket_regrets[s] == 0) {
	      ss = s;
	      break;
	    }
	  }
	} else if (short_quantized_streets_[st]) {
	  unsigned short *bucket_regrets = (unsigned short *)ptr1;
	  for (int s = 0; s < num_succs; ++s) {
	    if (bucket_regrets[s] == 0) {
	      ss = s;
	      break;
	    }
	  }
	} else {
	  T_REGRET *bucket_regrets = (T_REGRET *)ptr1;
	  for (int s = 0; s < num_succs; ++s) {
	    if (bucket_regrets[s] == 0) {
	      ss = s;
	      break;
	    }
	  }
	}
	
	if (explore_ > 0) {
	  double thresh = explore_ * num_succs;
	  double rnd = rngs_[rng_index_++];
	  if (rng_index_ == kNumPregenRNGs) rng_index_ = 0;
	  if (rnd < thresh) {
	    ss = rnd / explore_;
	  }
	}

	// Update sum-probs
	if (sumprob_streets_[player_acting][st] && (all_full_ || ! full_only_avg_update_) &&
	    (! asymmetric_ || target_player_ == player_acting)) {
	  T_SUM_PROB *these_sum_probs;
	  if (char_quantized_streets_[st]) {
	    these_sum_probs = (T_SUM_PROB *)(ptr1 + num_succs);
	  } else if (short_quantized_streets_[st]) {
	    these_sum_probs = (T_SUM_PROB *)(ptr1 + num_succs * 2);
	  } else {
	    these_sum_probs = (T_SUM_PROB *)(ptr1 + num_succs * sizeof(T_REGRET));
	  }
	  T_SUM_PROB ceiling = sumprob_ceilings_[st];
	  these_sum_probs[ss] += 1;
	  bool sum_prob_too_extreme = false;
	  if (these_sum_probs[ss] > ceiling) {
	    sum_prob_too_extreme = true;
	  }
	  if (sum_prob_too_extreme) {
	    for (int s = 0; s < num_succs; ++s) {
	      these_sum_probs[s] /= 2;
	    }
	  }
	  T_SUM_PROB *action_sumprobs = nullptr;
	  if (boost_thresholds_[st] > 0) {
	    int num_buckets = buckets_.NumBuckets(st);
	    action_sumprobs = (T_SUM_PROB *)
	      (SUCCPTR(ptr) + num_succs * 8 + num_buckets * size_bucket_data);
	    action_sumprobs[ss] += 1;
	    if (action_sumprobs[ss] > 2000000000) {
	      for (int s = 0; s < num_succs; ++s) {
		action_sumprobs[s] /= 2;
	      }
	    }
	  }
	  // Only start after iteration 10m.  (Assume any previous batches would
	  // have at least ten million iterations.)  Only adjust in thread 0.
	  if ((thread_index_ == 0) && (batch_index_ > 0 || it_ > 10000000) &&
	      boost_thresholds_[st] > 0) {
	    unsigned long long int sum = 0LL;
	    for (int s = 0; s < num_succs; ++s) {
	      sum += action_sumprobs[s];
	    }
	    for (int s = 0; s < num_succs; ++s) {
	      if (action_sumprobs[s] < boost_thresholds_[st] * sum) {
#if 0
		fprintf(stderr, "Boosting st %u pa %u s %u sum %llu asp %u offset %llu",
			st, player_acting, s, sum, action_sumprobs[s],
			(unsigned long long int)(ptr - data_));
		if (ptr == data_) {
		  fprintf(stderr, " root");
		}
		fprintf(stderr, "\n");
#endif
		int num_buckets = buckets_.NumBuckets(st);
		for (int b = 0; b < num_buckets; ++b) {
		  T_REGRET *bucket_regrets = (T_REGRET *)
		    (SUCCPTR(ptr) + num_succs * 8 + b * size_bucket_data);
		  // In FTL systems, positive regret is bad.  Want to *subtract*
		  // to make action more likely to be taken.
		  static const unsigned int kAdjust = 1000;
		  if (bucket_regrets[s] < kAdjust) {
		    bucket_regrets[s] = 0;
		  } else {
		    bucket_regrets[s] -= kAdjust;
		  }
		}
	      }
	    }
	  }
	}
      }

      int fold_succ_index = ptr[3];
      int call_succ_index = ptr[4];
      unsigned long long int succ_offset =
	*((unsigned long long int *)(SUCCPTR(ptr) + ss * 8));
      int old_contribution;
      if (ss == fold_succ_index) {
	folded_[player_acting] = true;
      } else if (ss == call_succ_index) {
	old_contribution = contributions_[player_acting];
	int last_bet_to = (int)*(unsigned short *)(ptr + 6);
	contributions_[player_acting] = last_bet_to;
      } else {
	old_contribution = contributions_[player_acting];
	// bet_to amount is store in the child's data
	unsigned char *succ_ptr = data_ + succ_offset;
	int bet_to = (int)*(unsigned short *)(succ_ptr + 6);
	contributions_[player_acting] = bet_to;
      }
      ++stack_index_;
      T_VALUE ret = Process(data_ + succ_offset, player_acting, st);
      --stack_index_;
      if (ss == fold_succ_index) {
	folded_[player_acting] = false;
      } else {
	contributions_[player_acting] = old_contribution;
      }
      return ret;
    }
  }
}

void TCFR::ReadRegrets(unsigned char *ptr, Node *node, Reader ***readers, bool ***seen) {
  unsigned char first_byte = ptr[0];
  // Terminal node
  if (first_byte != 0) return;
  int num_succs = ptr[2];
  if (num_succs > 1) {
    int pa = ptr[5];
    int st = ptr[1];
    int nt = node->NonterminalID();
    if (seen[st][pa][nt]) return;
    seen[st][pa][nt] = true;
    Reader *reader = readers[pa][st];
    int num_buckets = buckets_.NumBuckets(st);
    unsigned char *ptr1 = SUCCPTR(ptr) + num_succs * 8;
    if (char_quantized_streets_[st]) {
      for (int b = 0; b < num_buckets; ++b) {
	for (int s = 0; s < num_succs; ++s) {
	  ptr1[s] = reader->ReadUnsignedCharOrDie();
	}
	ptr1 += num_succs;
	if (sumprob_streets_[pa][st] && (! asymmetric_ || target_player_ == pa)) {
	  ptr1 += num_succs * sizeof(T_SUM_PROB);
	}
      }
    } else if (short_quantized_streets_[st]) {
      for (int b = 0; b < num_buckets; ++b) {
	unsigned short *regrets = (unsigned short *)ptr1;
	for (int s = 0; s < num_succs; ++s) {
	  regrets[s] = reader->ReadUnsignedShortOrDie();
	}
	ptr1 += num_succs * 2;
	if (sumprob_streets_[pa][st] && (! asymmetric_ || target_player_ == pa)) {
	  ptr1 += num_succs * sizeof(T_SUM_PROB);
	}
      }
    } else {
      for (int b = 0; b < num_buckets; ++b) {
	T_REGRET *regrets = (T_REGRET *)ptr1;
	for (int s = 0; s < num_succs; ++s) {
	  regrets[s] = reader->ReadUnsignedIntOrDie();
	}
	ptr1 += num_succs * sizeof(T_REGRET);
	if (sumprob_streets_[pa][st] && (! asymmetric_ || target_player_ == pa)) {
	  ptr1 += num_succs * sizeof(T_SUM_PROB);
	}
      }
    }
  }
  for (int s = 0; s < num_succs; ++s) {
    unsigned long long int succ_offset =
      *((unsigned long long int *)(SUCCPTR(ptr) + s * 8));
    ReadRegrets(data_ + succ_offset, node->IthSucc(s), readers, seen);
  }
}

void TCFR::WriteRegrets(unsigned char *ptr, Node *node, Writer ***writers, bool ***seen) {
  unsigned char first_byte = ptr[0];
  // Terminal node
  if (first_byte != 0) return;
  int num_succs = ptr[2];
  if (num_succs > 1) {
    int pa = ptr[5];
    int st = ptr[1];
    int nt = node->NonterminalID();
    if (seen[st][pa][nt]) return;
    seen[st][pa][nt] = true;
    Writer *writer = writers[pa][st];
    int num_buckets = buckets_.NumBuckets(st);
    unsigned char *ptr1 = SUCCPTR(ptr) + num_succs * 8;
    if (char_quantized_streets_[st]) {
      for (int b = 0; b < num_buckets; ++b) {
	for (int s = 0; s < num_succs; ++s) {
	  writer->WriteUnsignedChar(ptr1[s]);
	}
	ptr1 += num_succs;
	if (sumprob_streets_[pa][st] && (! asymmetric_ || target_player_ == pa)) {
	  ptr1 += num_succs * sizeof(T_SUM_PROB);
	}
      }
    } else if (short_quantized_streets_[st]) {
      for (int b = 0; b < num_buckets; ++b) {
	unsigned short *regrets = (unsigned short *)ptr1;
	for (int s = 0; s < num_succs; ++s) {
	  writer->WriteUnsignedShort(regrets[s]);
	}
	ptr1 += num_succs * 2;
	if (sumprob_streets_[pa][st] && (! asymmetric_ || target_player_ == pa)) {
	  ptr1 += num_succs * sizeof(T_SUM_PROB);
	}
      }
    } else {
      for (int b = 0; b < num_buckets; ++b) {
	T_REGRET *regrets = (T_REGRET *)ptr1;
	for (int s = 0; s < num_succs; ++s) {
	  writer->WriteUnsignedInt(regrets[s]);
	}
	ptr1 += num_succs * sizeof(T_REGRET);
	if (sumprob_streets_[pa][st] && (! asymmetric_ || target_player_ == pa)) {
	  ptr1 += num_succs * sizeof(T_SUM_PROB);
	}
      }
    }
  }
  for (int s = 0; s < num_succs; ++s) {
    unsigned long long int succ_offset =
      *((unsigned long long int *)(SUCCPTR(ptr) + s * 8));
    WriteRegrets(data_ + succ_offset, node->IthSucc(s), writers, seen);
  }
}

void TCFR::ReadSumprobs(unsigned char *ptr, Node *node, Reader ***readers, bool ***seen) {
  unsigned char first_byte = ptr[0];
  // Terminal node
  if (first_byte != 0) return;
  int num_succs = ptr[2];
  if (num_succs > 1) {
    int pa = ptr[5];
    int st = ptr[1];
    int nt = node->NonterminalID();
    if (seen[st][pa][nt]) return;
    seen[st][pa][nt] = true;
    int num_buckets = buckets_.NumBuckets(st);
    unsigned char *ptr1 = SUCCPTR(ptr) + num_succs * 8;
    if (sumprob_streets_[pa][st] && (! asymmetric_ || target_player_ == pa)) {
      Reader *reader = readers[pa][st];
      if (char_quantized_streets_[st]) {
	for (int b = 0; b < num_buckets; ++b) {
	  T_SUM_PROB *sum_probs = (T_SUM_PROB *)(ptr1 + num_succs);
	  for (int s = 0; s < num_succs; ++s) {
	    // Temporary for the case where I am starting to maintain
	    // sumprobs in the middle of running CFR.
	    if (reader == nullptr) {
	      sum_probs[s] = 0;
	    } else {
	      sum_probs[s] = reader->ReadUnsignedIntOrDie();
	    }
	  }
	  ptr1 += num_succs * (1 + sizeof(T_SUM_PROB));
	}
      } else if (short_quantized_streets_[st]) {
	for (int b = 0; b < num_buckets; ++b) {
	  T_SUM_PROB *sum_probs = (T_SUM_PROB *)(ptr1 + num_succs * 2);
	  for (int s = 0; s < num_succs; ++s) {
	    // Temporary for the case where I am starting to maintain
	    // sumprobs in the middle of running CFR.
	    if (reader == nullptr) {
	      sum_probs[s] = 0;
	    } else {
	      sum_probs[s] = reader->ReadUnsignedIntOrDie();
	    }
	  }
	  ptr1 += num_succs * (2 + sizeof(T_SUM_PROB));
	}
      } else {
	for (int b = 0; b < num_buckets; ++b) {
	  T_SUM_PROB *sumprobs =
	    (T_SUM_PROB *)(ptr1 + num_succs * sizeof(T_REGRET));
	  for (int s = 0; s < num_succs; ++s) {
	    // Temporary for the case where I am starting to maintain
	    // sumprobs in the middle of running CFR.
	    if (reader == nullptr) {
	      sumprobs[s] = 0;
	    } else {
	      sumprobs[s] = reader->ReadUnsignedIntOrDie();
	    }
	  }
	  ptr1 += num_succs * (sizeof(T_REGRET) + sizeof(T_SUM_PROB));
	}
	if (boost_thresholds_[st] > 0) {
	  unique_ptr<unsigned long long int []>
	    succ_total_sumprobs(new unsigned long long int[num_succs]);
	  for (int s = 0; s < num_succs; ++s) {
	    succ_total_sumprobs[s] = 0;
	  }
	  unsigned char *ptr2 = SUCCPTR(ptr) + num_succs * 8;
	  for (int b = 0; b < num_buckets; ++b) {
	    T_SUM_PROB *sumprobs =
	      (T_SUM_PROB *)(ptr2 + num_succs * sizeof(T_REGRET));
	    for (int s = 0; s < num_succs; ++s) {
	      succ_total_sumprobs[s] += sumprobs[s];
	    }
	    ptr2 += num_succs * (sizeof(T_REGRET) + sizeof(T_SUM_PROB));
	  }
	  while (true) {
	    bool too_high = false;
	    for (int s = 0; s < num_succs; ++s) {
	      if (succ_total_sumprobs[s] > 2000000000) {
		too_high = true;
		break;
	      }
	    }
	    if (! too_high) break;
	    for (int s = 0; s < num_succs; ++s) {
	      succ_total_sumprobs[s] /= 2;
	    }
	  }
	  int *action_sumprobs = (int *)
	    (SUCCPTR(ptr) + num_succs * 8 + num_buckets * num_succs *
	     (sizeof(T_REGRET) + sizeof(T_SUM_PROB)));
	  for (int s = 0; s < num_succs; ++s) {
	    action_sumprobs[s] = succ_total_sumprobs[s];
	  }
	}
      }
    }
  }
  for (int s = 0; s < num_succs; ++s) {
    unsigned long long int succ_offset =
      *((unsigned long long int *)(SUCCPTR(ptr) + s * 8));
    ReadSumprobs(data_ + succ_offset, node->IthSucc(s), readers, seen);
  }
}

void TCFR::WriteSumprobs(unsigned char *ptr, Node *node, Writer ***writers, bool ***seen) {
  unsigned char first_byte = ptr[0];
  // Terminal node
  if (first_byte != 0) return;
  int num_succs = ptr[2];
  if (num_succs > 1) {
    int pa = ptr[5];
    int st = ptr[1];
    int nt = node->NonterminalID();
    if (seen[st][pa][nt]) return;
    seen[st][pa][nt] = true;
    if (sumprob_streets_[pa][st] && (! asymmetric_ || target_player_ == pa)) {
      Writer *writer = writers[pa][st];
      int num_buckets = buckets_.NumBuckets(st);
      unsigned char *ptr1 = SUCCPTR(ptr) + num_succs * 8;
      if (char_quantized_streets_[st]) {
	for (int b = 0; b < num_buckets; ++b) {
	  T_SUM_PROB *sum_probs = (T_SUM_PROB *)(ptr1 + num_succs);
	  for (int s = 0; s < num_succs; ++s) {
	    writer->WriteUnsignedInt(sum_probs[s]);
	  }
	  ptr1 += num_succs * (1 + sizeof(T_SUM_PROB));
	}
      } else if (short_quantized_streets_[st]) {
	for (int b = 0; b < num_buckets; ++b) {
	  T_SUM_PROB *sum_probs = (T_SUM_PROB *)(ptr1 + num_succs * 2);
	  for (int s = 0; s < num_succs; ++s) {
	    writer->WriteUnsignedInt(sum_probs[s]);
	  }
	  ptr1 += num_succs * (2 + sizeof(T_SUM_PROB));
	}
      } else {
	for (int b = 0; b < num_buckets; ++b) {
	  T_SUM_PROB *sum_probs =
	    (T_SUM_PROB *)(ptr1 + num_succs * sizeof(T_REGRET));
	  for (int s = 0; s < num_succs; ++s) {
	    writer->WriteUnsignedInt(sum_probs[s]);
	  }
	  ptr1 += num_succs * (sizeof(T_REGRET) + sizeof(T_SUM_PROB));
	}
      }
    }
  }
  for (int s = 0; s < num_succs; ++s) {
    unsigned long long int succ_offset =
      *((unsigned long long int *)(SUCCPTR(ptr) + s * 8));
    WriteSumprobs(data_ + succ_offset, node->IthSucc(s), writers, seen);
  }
}

void TCFR::Read(int batch_index) {
  char dir[500], buf[500];
  sprintf(dir, "%s/%s.%u.%s.%i.%i.%i.%s.%s", Files::OldCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), card_abstraction_.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(),
	  betting_abstraction_.BettingAbstractionName().c_str(),
	  cfr_config_.CFRConfigName().c_str());
  if (asymmetric_) {
    char buf2[20];
    sprintf(buf2, ".p%u", target_player_);
    strcat(dir, buf2);
  }
  int num_players = Game::NumPlayers();
  Reader ***regret_readers = new Reader **[num_players];
  for (int p = 0; p < num_players; ++p) {
    regret_readers[p] = new Reader *[max_street_ + 1];
    for (int st = 0; st <= max_street_; ++st) {
      char suffix;
      if (char_quantized_streets_[st]) {
	suffix = 'c';
      } else if (short_quantized_streets_[st]) {
	suffix = 's';
      } else {
	suffix = 'i';
      }
      sprintf(buf, "%s/regrets.x.0.0.%u.%u.p%u.%c", dir, st, batch_index, p, suffix);
      regret_readers[p][st] = new Reader(buf);
    }
  }
  Reader ***sum_prob_readers = new Reader **[num_players];
  for (int p = 0; p < num_players; ++p) {
    sum_prob_readers[p] = new Reader *[max_street_ + 1];
    for (int st = 0; st <= max_street_; ++st) {
      if (! sumprob_streets_[p][st] || (asymmetric_ && p != target_player_)) {
	sum_prob_readers[p][st] = NULL;
	continue;
      }
      sprintf(buf, "%s/sumprobs.x.0.0.%u.%u.p%u.i", dir, st, batch_index, p);
      sum_prob_readers[p][st] = new Reader(buf);
    }
  }
  bool ***seen = new bool **[max_street_ + 1];
  for (int st = 0; st <= max_street_; ++st) {
    seen[st] = new bool *[num_players];
    for (int p = 0; p < num_players; ++p) {
      int num_nt = betting_tree_->NumNonterminals(p, st);
      seen[st][p] = new bool[num_nt];
      for (int i = 0; i < num_nt; ++i) {
	seen[st][p][i] = false;
      }
    }
  }

  ReadRegrets(data_, betting_tree_->Root(), regret_readers, seen);
  for (int st = 0; st <= max_street_; ++st) {
    for (int p = 0; p < num_players; ++p) {
      int num_nt = betting_tree_->NumNonterminals(p, st);
      for (int i = 0; i < num_nt; ++i) {
	seen[st][p][i] = false;
      }
    }
  }
  ReadSumprobs(data_, betting_tree_->Root(), sum_prob_readers, seen);
  for (int st = 0; st <= max_street_; ++st) {
    for (int p = 0; p < num_players; ++p) {
      delete [] seen[st][p];
    }
    delete [] seen[st];
  }
  delete [] seen;
  for (int p = 0; p <= 1; ++p) {
    for (int st = 0; st <= max_street_; ++st) {
      if (! regret_readers[p][st]->AtEnd()) {
	fprintf(stderr, "Regret reader didn't get to EOF\n");
	exit(-1);
      }
      delete regret_readers[p][st];
    }
    delete [] regret_readers[p];
  }
  delete [] regret_readers;
  for (int p = 0; p <= 1; ++p) {
    for (int st = 0; st <= max_street_; ++st) {
      if (! sumprob_streets_[p][st]) continue;
      if (sum_prob_readers[p][st] && ! sum_prob_readers[p][st]->AtEnd()) {
	fprintf(stderr, "Sumprob reader didn't get to EOF\n");
	exit(-1);
      }
      delete sum_prob_readers[p][st];
    }
    delete [] sum_prob_readers[p];
  }
  delete [] sum_prob_readers;
}

void TCFR::Write(int batch_index) {
  char dir[500], buf[500];
  sprintf(dir, "%s/%s.%u.%s.%i.%i.%i.%s.%s", Files::NewCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), card_abstraction_.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(),
	  betting_abstraction_.BettingAbstractionName().c_str(), 
	  cfr_config_.CFRConfigName().c_str());
  if (asymmetric_) {
    char buf2[20];
    sprintf(buf2, ".p%u", target_player_);
    strcat(dir, buf2);
  }
  Mkdir(dir);
  int num_players = Game::NumPlayers();
  Writer ***regret_writers = new Writer **[num_players];
  for (int p = 0; p < num_players; ++p) {
    regret_writers[p] = new Writer *[max_street_ + 1];
    for (int st = 0; st <= max_street_; ++st) {
      char suffix;
      if (char_quantized_streets_[st]) {
	suffix = 'c';
      } else if (short_quantized_streets_[st]) {
	suffix = 's';
      } else {
	suffix = 'i';
      }
      sprintf(buf, "%s/regrets.x.0.0.%u.%u.p%u.%c", dir, st, batch_index, p, suffix);
      regret_writers[p][st] = new Writer(buf);
    }
  }
  bool ***seen = new bool **[max_street_ + 1];
  for (int st = 0; st <= max_street_; ++st) {
    seen[st] = new bool *[num_players];
    for (int p = 0; p < num_players; ++p) {
      int num_nt = betting_tree_->NumNonterminals(p, st);
      seen[st][p] = new bool[num_nt];
      for (int i = 0; i < num_nt; ++i) {
	seen[st][p][i] = false;
      }
    }
  }
  WriteRegrets(data_, betting_tree_->Root(), regret_writers, seen);
  for (int p = 0; p < num_players; ++p) {
    for (int st = 0; st <= max_street_; ++st) {
      delete regret_writers[p][st];
    }
    delete [] regret_writers[p];
  }
  delete [] regret_writers;

  Writer ***sum_prob_writers = new Writer **[num_players];
  for (int p = 0; p < num_players; ++p) {
    sum_prob_writers[p] = new Writer *[max_street_ + 1];
    for (int st = 0; st <= max_street_; ++st) {
      if (! sumprob_streets_[p][st] || (asymmetric_ && p != target_player_)) {
	sum_prob_writers[p][st] = nullptr;
	continue;
      }
      sprintf(buf, "%s/sumprobs.x.0.0.%u.%u.p%u.i", dir, st, batch_index, p);
      sum_prob_writers[p][st] = new Writer(buf);
    }
  }
  for (int st = 0; st <= max_street_; ++st) {
    for (int p = 0; p < num_players; ++p) {
      int num_nt = betting_tree_->NumNonterminals(p, st);
      for (int i = 0; i < num_nt; ++i) {
	seen[st][p][i] = false;
      }
    }
  }
  WriteSumprobs(data_, betting_tree_->Root(), sum_prob_writers, seen);
  for (int st = 0; st <= max_street_; ++st) {
    for (int p = 0; p < num_players; ++p) {
      delete [] seen[st][p];
    }
    delete [] seen[st];
  }
  delete [] seen;
  for (int p = 0; p < num_players; ++p) {
    for (int st = 0; st <= max_street_; ++st) {
      if (! sumprob_streets_[p][st]) continue;
      delete sum_prob_writers[p][st];
    }
    delete [] sum_prob_writers[p];
  }
  delete [] sum_prob_writers;
}

void TCFR::Run(void) {
  // Temporary?
  int num_players = Game::NumPlayers();
  g_preflop_vals = new double *[num_players];
  g_preflop_nums = new unsigned long long int *[num_players];
  int num_preflop_buckets = buckets_.NumBuckets(0);
  for (int p = 0; p < num_players; ++p) {
    g_preflop_vals[p] = new double[num_preflop_buckets];
    g_preflop_nums[p] = new unsigned long long int[num_preflop_buckets];
    for (int b = 0; b < num_preflop_buckets; ++b) {
      g_preflop_vals[p][b] = 0;
      g_preflop_nums[p][b] = 0ULL;
    }
  }
  total_its_ = 0ULL;

  for (int i = 1; i < num_cfr_threads_; ++i) {
    cfr_threads_[i]->RunThread();
  }
  // Execute thread 0 in main execution thread
  fprintf(stderr, "Starting thread 0 in main thread\n");
  cfr_threads_[0]->Run();
  fprintf(stderr, "Finished main thread\n");
  for (int i = 1; i < num_cfr_threads_; ++i) {
    cfr_threads_[i]->Join();
    fprintf(stderr, "Joined thread %i\n", i);
  }

#if 0
  // Temporary?
  int max_card = Game::MaxCard();
  Card hole_cards[2];
  for (int p = 0; p < num_players; ++p) {
    for (Card hi = 1; hi <= max_card; ++hi) {
      hole_cards[0] = hi;
      for (Card lo = 0; lo < hi; ++lo) {
	hole_cards[1] = lo;
	int hcp = HCPIndex(0, hole_cards);
	int b = buckets_.Bucket(0, hcp);
	double val = g_preflop_vals[p][b];
	unsigned long long int num = g_preflop_nums[p][b];
	if (num > 0) {
	  printf("P%u %f ", p, val / (double)num);
	  OutputTwoCards(hi, lo);
	  printf(" (%u)\n", b);
	  g_preflop_nums[p][b] = 0;
	}
      }
    }
  }
  fflush(stdout);
#endif

  for (int p = 0; p < num_players; ++p) {
    delete [] g_preflop_vals[p];
    delete [] g_preflop_nums[p];
  }
  delete [] g_preflop_vals;
  delete [] g_preflop_nums;
}

void TCFR::RunBatch(int batch_size) {
  SeedRand(batch_index_);
  fprintf(stderr, "Seeding to %i\n", batch_index_);
  for (int i = 0; i < kNumPregenRNGs; ++i) {
    rngs_[i] = RandZeroToOne();
    // A hack.  We can end up generating 1.0 as an RNG because we are casting
    // doubles to floats.  Code elsewhere assumes RNGs are strictly less than
    // 1.0.
    if (rngs_[i] >= 1.0) rngs_[i] = 0.99999;
  }

  cfr_threads_ = new TCFRThread *[num_cfr_threads_];
  for (int i = 0; i < num_cfr_threads_; ++i) {
    TCFRThread *cfr_thread =
      new TCFRThread(betting_abstraction_, cfr_config_, buckets_, batch_index_, i, 
		     num_cfr_threads_, data_, target_player_, rngs_, uncompress_, short_uncompress_,
		     pruning_thresholds_, sumprob_streets_, boost_thresholds_.get(),
		     freeze_.get(), hvb_table_, cards_to_indices_, num_raw_boards_,
		     board_table_.get(), batch_size, &total_its_);
    cfr_threads_[i] = cfr_thread;
  }

  fprintf(stderr, "Running batch %i\n", batch_index_);
  Run();
  fprintf(stderr, "Finished running batch %i\n", batch_index_);

  for (int i = 0; i < num_cfr_threads_; ++i) {
    total_process_count_ += cfr_threads_[i]->ProcessCount();
  }
  for (int i = 0; i < num_cfr_threads_; ++i) {
    total_full_process_count_ += cfr_threads_[i]->FullProcessCount();
  }

  for (int i = 0; i < num_cfr_threads_; ++i) {
    delete cfr_threads_[i];
  }
  delete [] cfr_threads_;
  cfr_threads_ = NULL;
}

void TCFR::Run(int start_batch_index, int end_batch_index, int batch_size, int save_interval) {
  if ((end_batch_index - start_batch_index) % save_interval != 0) {
    fprintf(stderr, "Batches to execute should be multiple of save interval\n");
    exit(-1);
  }
  if (start_batch_index > 0) Read(start_batch_index - 1);

  bool some_frozen = false;
  for (int p = 0; p < num_players_; ++p) {
    if (freeze_[p]) some_frozen = true;
  }
  total_process_count_ = 0ULL;
  total_full_process_count_ = 0ULL;

  for (batch_index_ = start_batch_index; batch_index_ < end_batch_index; ++batch_index_) {
    RunBatch(batch_size);
    // In general, save every save_interval batches.  The logic is a little messy.  If the save
    // interval is > 1, then we don't want to save at batch 0.  But we do if the save interval
    // is 1.
    // Don't save if we are running CFR with frozen players as a sort of best-response calculation.
    if (! some_frozen && (batch_index_ - start_batch_index) % save_interval == 0 &&
	(batch_index_ > 0 || save_interval == 1)) {
      fprintf(stderr, "Process count: %llu\n", total_process_count_);
      fprintf(stderr, "Full process count: %llu\n", total_full_process_count_);
      Write(batch_index_);
      fprintf(stderr, "Checkpointed batch index %i\n", batch_index_);
      total_process_count_ = 0ULL;
      total_full_process_count_ = 0ULL;
    }
  }
}

// Returns a pointer to the allocation buffer after this node and all of its
// descendants.
// Note: we could use num-succs to encode terminal/nonterminal
// Terminal
//   Byte 0:      1
// Nonterminal
//   Byte 0:      0
//   Byte 1:      Street
//   Byte 2:      Num succs
//   Byte 3:      Fold succ index
//   Byte 4:      Call succ index
//   Byte 5:      Player acting
//   Bytes 6-7:   Last-bet-to
//   Byte 8:      Beginning of succ ptrs
// The next num-succs * 8 bytes are for the succ ptrs
// For each bucket
//   num-succs * sizeof(T_REGRET) for the regrets
//   If saving sumprobs:
//     num-succs * sizeof(T_SUM_PROB) for the sum-probs
// If boosting: num_succs * sizeof(T_SUM_PROB) for the action-sum-probs
unsigned char *TCFR::Prepare(unsigned char *ptr, Node *node, unsigned short last_bet_to,
			     unsigned long long int ***offsets) {
  if (node->Terminal()) {
    ptr[0] = 1;
    return ptr + 4;
  }
  int st = node->Street();
  int pa = node->PlayerActing();
  ptr[0] = 0;
  ptr[1] = st;
  int num_succs = node->NumSuccs();
  ptr[2] = num_succs;
  int fsi = 255;
  if (node->HasFoldSucc()) fsi = node->FoldSuccIndex();
  ptr[3] = fsi;
  int csi = 255;
  if (node->HasCallSucc()) csi = node->CallSuccIndex();
  ptr[4] = csi;
  ptr[5] = pa;
  *(unsigned short *)(ptr + 6) = last_bet_to;
  unsigned char *succ_ptr = ptr + 8;

  unsigned char *ptr1 = succ_ptr + num_succs * 8;
  if (num_succs > 1) {
    int num_buckets = buckets_.NumBuckets(st);
    for (int b = 0; b < num_buckets; ++b) {
      // Regrets
      if (char_quantized_streets_[st]) {
	for (int s = 0; s < num_succs; ++s) {
	  *ptr1 = 0;
	  ++ptr1;
	}
      } else if (short_quantized_streets_[st]) {
	for (int s = 0; s < num_succs; ++s) {
	  *(unsigned short *)ptr1 = 0;
	  ptr1 += 2;
	}
      } else {
	for (int s = 0; s < num_succs; ++s) {
	  *((T_REGRET *)ptr1) = 0;
	  ptr1 += sizeof(T_REGRET);
	}
      }
      // Sumprobs
      if (sumprob_streets_[pa][st] && (! asymmetric_ || target_player_ == pa)) {
	for (int s = 0; s < num_succs; ++s) {
	  *((T_SUM_PROB *)ptr1) = 0;
	  ptr1 += sizeof(T_SUM_PROB);
	}
      }
    }
    // Action sumprobs
    if (boost_thresholds_[st] > 0 && sumprob_streets_[pa][st] &&
	(! asymmetric_ || target_player_ == pa)) {
      for (int s = 0; s < num_succs; ++s) {
	*((T_SUM_PROB *)ptr1) = 0;
	ptr1 += sizeof(T_SUM_PROB);
      }
    }
  }

  for (int s = 0; s < num_succs; ++s) {
    unsigned long long int ull_offset = ptr1 - data_;
    Node *succ = node->IthSucc(s);
    if (! succ->Terminal()) {
      int succ_st = succ->Street();
      int succ_pa = succ->PlayerActing();
      int succ_nt = succ->NonterminalID();
      unsigned long long int succ_offset = offsets[succ_st][succ_pa][succ_nt];
      if (succ_offset != 999999999999) {
	// Temporary
	// Check last-bet-to's match
	unsigned short new_bet_to;
	if (s == fsi || s == csi) {
	  new_bet_to = last_bet_to;
	} else {
	  new_bet_to = succ->LastBetTo();
	}
	unsigned short old_bet_to =
	  *(unsigned short *)(data_ + succ_offset + 6);
	if (old_bet_to != new_bet_to) {
	  fprintf(stderr, "bet_to mismatch: %i %i\n", (int)old_bet_to,
		  (int)new_bet_to);
	  exit(-1);
	}
	*((unsigned long long int *)(succ_ptr + s * 8)) = succ_offset;
	continue;
      } else {
	offsets[succ_st][succ_pa][succ_nt] = ull_offset;
      }
    }
    *((unsigned long long int *)(succ_ptr + s * 8)) = ull_offset;
    if (s == fsi || s == csi) {
      ptr1 = Prepare(ptr1, succ, last_bet_to, offsets);
    } else {
      unsigned short new_bet_to = succ->LastBetTo();
      ptr1 = Prepare(ptr1, succ, new_bet_to, offsets);
    }
  }
  return ptr1;
}

void TCFR::MeasureTree(Node *node, bool ***seen, unsigned long long int *allocation_size) {
  if (node->Terminal()) {
    *allocation_size += 4;
    return;
  }

  int st = node->Street();
  int pa = node->PlayerActing();
  int nt = node->NonterminalID();
  if (seen[st][pa][nt]) return;
  seen[st][pa][nt] = true;
  
  // This is the number of bytes needed for everything else (e.g.,
  // num-succs).
  int this_sz = 8;

  int num_succs = node->NumSuccs();
  // Eight bytes per succ
  this_sz += num_succs * 8;
  if (num_succs > 1) {
    // A regret and a sum-prob for each bucket and succ
    int nb = buckets_.NumBuckets(st);
    if (char_quantized_streets_[st]) {
      this_sz += nb * num_succs;
    } else if (short_quantized_streets_[st]) {
      this_sz += nb * num_succs * 2;
    } else {
      this_sz += nb * num_succs * sizeof(T_REGRET);
    }
    if (sumprob_streets_[pa][st] && (! asymmetric_ || target_player_ == pa)) {
      this_sz += nb * num_succs * sizeof(T_SUM_PROB);
      if (boost_thresholds_[st] > 0) this_sz += num_succs * sizeof(T_SUM_PROB);
    }
  }

  *allocation_size += this_sz;

  for (int s = 0; s < num_succs; ++s) {
    MeasureTree(node->IthSucc(s), seen, allocation_size);
  }
}

// Allocate one contiguous block of memory that has successors, street,
// num-succs, regrets, sum-probs, showdown/fold flag, pot-size/2.
void TCFR::Prepare(void) {
  int max_street = Game::MaxStreet();
  bool ***seen = new bool **[max_street + 1];
  for (int st = 0; st <= max_street; ++st) {
    seen[st] = new bool *[num_players_];
    for (int pa = 0; pa < num_players_; ++pa) {
      int num_nt = betting_tree_->NumNonterminals(pa, st);
      seen[st][pa] = new bool[num_nt];
      for (int i = 0; i < num_nt; ++i) {
	seen[st][pa][i] = false;
      }
    }
  }
  // Use an unsigned long long int, but succs are four-byte
  unsigned long long int allocation_size = 0;
  MeasureTree(betting_tree_->Root(), seen, &allocation_size);

  for (int st = 0; st <= max_street; ++st) {
    for (int pa = 0; pa < num_players_; ++pa) {
      delete [] seen[st][pa];
    }
    delete [] seen[st];
  }
  delete [] seen;

  // Should get amount of RAM from method in Files class
  // if (allocation_size > 1180000000000ULL) {
  if (allocation_size > 32000000000ULL) {
    fprintf(stderr, "Allocation size %llu too big\n", allocation_size);
    exit(-1);
  }
  fprintf(stderr, "Allocation size: %llu\n", allocation_size);
  data_ = new unsigned char[allocation_size];
  if (data_ == NULL) {
    fprintf(stderr, "Could not allocate\n");
    exit(-1);
  }
  fprintf(stderr, "Allocated: %llu\n", allocation_size);

  unsigned long long int ***offsets =
    new unsigned long long int **[max_street + 1];
  for (int st = 0; st <= max_street; ++st) {
    offsets[st] = new unsigned long long int *[num_players_];
    for (int pa = 0; pa < num_players_; ++pa) {
      int num_nt = betting_tree_->NumNonterminals(pa, st);
      offsets[st][pa] = new unsigned long long int[num_nt];
      for (int i = 0; i < num_nt; ++i) {
	offsets[st][pa][i] = 999999999999;
      }
    }
  }
  unsigned char *end = Prepare(data_, betting_tree_->Root(), Game::BigBlind(), offsets);
  unsigned long long int sz = end - data_;
  if (sz != allocation_size) {
    fprintf(stderr, "Didn't fill expected number of bytes: sz %llu as %llu\n", sz, allocation_size);
    exit(-1);
  }

  for (int st = 0; st <= max_street; ++st) {
    for (int pa = 0; pa < num_players_; ++pa) {
      delete [] offsets[st][pa];
    }
    delete [] offsets[st];
  }
  delete [] offsets;
}

static int Factorial(int n) {
  if (n == 0) return 1;
  if (n == 1) return 1;
  return n * Factorial(n - 1);
}

TCFR::TCFR(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
	   const Buckets &buckets, int num_threads, int target_player) :
  card_abstraction_(ca), betting_abstraction_(ba), cfr_config_(cc),
  buckets_(buckets) {
  max_street_ = Game::MaxStreet();
#ifdef BC
  fprintf(stderr, "Sampling canonical boards; scaling updates by board count\n");
#else
  fprintf(stderr, "Sampling raw boards with right frequency\n");
#endif
  fprintf(stderr, "Full evaluation if regrets close; thresholds:");
  for (int st = 0; st <= max_street_; ++st) {
    fprintf(stderr, " %u", cfr_config_.CloseThreshold());
  }
  fprintf(stderr, "\n");
  if (cfr_config_.DealTwice()) {
    fprintf(stderr, "Dealing cards twice for each iteration\n");
  } else {
    fprintf(stderr, "Dealing cards once for each iteration\n");
  }
#ifdef SWITCH
  fprintf(stderr, "Switching the order of phases each iteration\n");
#endif
  time_t start_t = time(NULL);
  asymmetric_ = betting_abstraction_.Asymmetric();
  num_players_ = Game::NumPlayers();
  target_player_ = target_player;
  num_cfr_threads_ = num_threads;
  fprintf(stderr, "Num threads: %i\n", num_cfr_threads_);
  for (int st = 0; st <= max_street_; ++st) {
    if (buckets_.None(st)) {
      fprintf(stderr, "TCFR expects buckets on all streets\n");
      exit(-1);
    }
  }

  boost_thresholds_.reset(new double[max_street_+1]);
  const vector<double> &boost_thresholds = cc.BoostThresholds();
  if (boost_thresholds.size() == 0) {
    for (int st = 0; st <= max_street_; ++st) {
      boost_thresholds_[st] = 0;
    }
  } else {
    for (int st = 0; st <= max_street_; ++st) {
      if (st >= (int)boost_thresholds.size()) {
	fprintf(stderr, "Not enough boost thresholds specified\n");
	exit(-1);
      }
      boost_thresholds_[st] = boost_thresholds[st];
    }
  }
  
  freeze_.reset(new bool[num_players_]);
  for (int p = 0; p < num_players_; ++p) freeze_[p] = false;
  const vector<int> &freeze = cc.Freeze();
  for (int i = 0; i < (int)freeze.size(); ++i) {
    freeze_[freeze[i]] = true;
  }
  
  BoardTree::Create();
  BoardTree::BuildPredBoards();
  
  pruning_thresholds_ = new unsigned int[max_street_];
  const vector<unsigned int> &v = cfr_config_.PruningThresholds();
  for (int st = 0; st <= max_street_; ++st) {
    pruning_thresholds_[st] = v[st];
  }

  sumprob_streets_ = new bool *[num_players_];
  for (int p = 0; p < num_players_; ++p) {
    sumprob_streets_[p] = new bool[max_street_ + 1];
  }
  const vector<int> &ssv = cfr_config_.SumprobStreets();
  int num_ssv = ssv.size();
  if (num_ssv == 0) {
    for (int p = 0; p < num_players_; ++p) {
      for (int st = 0; st <= max_street_; ++st) {
	sumprob_streets_[p][st] = true;
      }
    }
  } else {
    for (int p = 0; p < num_players_; ++p) {
      for (int st = 0; st <= max_street_; ++st) sumprob_streets_[p][st] = false;
    }
    for (int i = 0; i < num_ssv; ++i) {
      int st = ssv[i];
      for (int p = 0; p < num_players_; ++p) {
	sumprob_streets_[p][st] = true;
      }
    }
  }

  char_quantized_streets_.reset(new bool[max_street_ + 1]);
  for (int st = 0; st <= max_street_; ++st) {
    char_quantized_streets_[st] = cfr_config_.CharQuantizedStreet(st);
  }
  short_quantized_streets_.reset(new bool[max_street_ + 1]);
  for (int st = 0; st <= max_street_; ++st) {
    short_quantized_streets_[st] = cfr_config_.ShortQuantizedStreet(st);
  }

  if (betting_abstraction_.Asymmetric()) {
    betting_tree_.reset(new BettingTree(betting_abstraction_, target_player_));
  } else {
    betting_tree_.reset(new BettingTree(betting_abstraction_));
  }

  BoardTree::BuildBoardCounts();
#ifdef BC
  num_raw_boards_ = 0;
  board_table_.reset(nullptr);
#else
  num_raw_boards_ = 1;
  int num_remaining = Game::NumCardsInDeck();
  for (int st = 1; st <= max_street_; ++st) {
    int num_street_cards = Game::NumCardsForStreet(st);
    int multiplier = 1;
    for (int n = (num_remaining - num_street_cards) + 1; n <= num_remaining; ++n) {
      multiplier *= n;
    }
    num_raw_boards_ *= multiplier / Factorial(num_street_cards);
    num_remaining -= num_street_cards;
  }
  board_table_.reset(new int[num_raw_boards_]);
  int num_boards = BoardTree::NumBoards(max_street_);
  int i = 0;
  for (int bd = 0; bd < num_boards; ++bd) {
    int ct = BoardTree::BoardCount(max_street_, bd);
    for (int j = 0; j < ct; ++j) {
      board_table_[i++] = bd;
    }
  }
  if (i != num_raw_boards_) {
    fprintf(stderr, "Num raw board mismatch: %u, %u\n", i, num_raw_boards_);
    exit(-1);
  }

  BoardTree::DeleteBoardCounts();
#endif

  Prepare();

  rngs_ = new float[kNumPregenRNGs];
  uncompress_ = new unsigned int[256];
  for (unsigned int c = 0; c <= 255; ++c) {
    uncompress_[c] = UncompressRegret(c);
  }
  short_uncompress_ = new unsigned int[65536];
  for (unsigned int c = 0; c <= 65535; ++c) {
    short_uncompress_[c] = UncompressRegretShort(c);
  }

  if (cfr_config_.HVBTable()) {
    // How much extra space does this require?  2.5b hands times 4 bytes
    // for hand value is 10 gigs - minus size of HandTree.
    char buf[500];
    string max_street_bucketing = card_abstraction_.Bucketing(max_street_);
    sprintf(buf, "%s/hvb.%s.%u.%u.%u.%s", Files::StaticBase(),
	    Game::GameName().c_str(), Game::NumRanks(), Game::NumSuits(),
	    max_street_, max_street_bucketing.c_str());
    int num_boards = BoardTree::NumBoards(max_street_);
    int num_hole_card_pairs = Game::NumHoleCardPairs(max_street_);
    long long int num_hands = ((long long int)num_boards) * ((long long int)num_hole_card_pairs);
    long long int bytes = 4;
    if (buckets_.NumBuckets(max_street_) <= 65536) bytes += 2;
    else                                           bytes += 4;
    long long int total_bytes = num_hands * bytes;
    Reader reader(buf);
    if (reader.FileSize() != total_bytes) {
      fprintf(stderr, "File size %lli expected %lli\n", reader.FileSize(), total_bytes);
    }
    hvb_table_ = new unsigned char[total_bytes];
    for (long long int i = 0; i < total_bytes; ++i) {
      if ((i % 1000000000LL) == 0) {
	fprintf(stderr, "i %lli/%lli\n", i, total_bytes);
      }
      hvb_table_[i] = reader.ReadUnsignedCharOrDie();
    }
  } else {
    HandValueTree::Create();
    hvb_table_ = NULL;
  }

  if (cfr_config_.HVBTable()) {
    cards_to_indices_ = new unsigned char **[max_street_ + 1];
    int max_card = Game::MaxCard();
    for (int st = 0; st <= max_street_; ++st) {
      int num_boards = BoardTree::NumBoards(st);
      int num_board_cards = Game::NumBoardCards(st);
      cards_to_indices_[st] = new unsigned char *[num_boards];
      for (int bd = 0; bd < num_boards; ++bd) {
	const Card *board = BoardTree::Board(st, bd);
	cards_to_indices_[st][bd] = new unsigned char[max_card + 1];
	int num_lower = 0;
	for (int c = 0; c <= max_card; ++c) {
	  // It's OK if we assign a value to a card that is on the board.
	  cards_to_indices_[st][bd][c] = c - num_lower;
	  for (int i = 0; i < num_board_cards; ++i) {
	    if (c == board[i]) {
	      ++num_lower;
	      break;
	    }
	  }
	}
      }
    }
  } else {
    cards_to_indices_ = NULL;
  }

  time_t end_t = time(NULL);
  double diff_sec = difftime(end_t, start_t);
  fprintf(stderr, "Initialization took %.1f seconds\n", diff_sec);
}

TCFR::~TCFR(void) {
  if (cards_to_indices_) {
    for (int st = 0; st <= max_street_; ++st) {
      int num_boards = BoardTree::NumBoards(st);
      for (int bd = 0; bd < num_boards; ++bd) {
	delete [] cards_to_indices_[st][bd];
      }
      delete [] cards_to_indices_[st];
    }
    delete [] cards_to_indices_;
  }
  delete [] hvb_table_;
  delete [] pruning_thresholds_;
  delete [] uncompress_;
  delete [] short_uncompress_;
  delete [] rngs_;
  delete [] data_;
  for (int p = 0; p < num_players_; ++p) {
    delete [] sumprob_streets_[p];
  }
  delete [] sumprob_streets_;
}
