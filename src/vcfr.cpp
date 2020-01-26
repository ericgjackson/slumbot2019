#include <math.h> // lrint()
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // usleep()

#include <memory>
#include <string>
#include <vector>

#include "betting_tree.h"
#include "betting_trees.h"
#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "card_abstraction.h"
#include "cfr_config.h"
#include "cfr_street_values.h"
#include "cfr_utils.h"
#include "cfr_values.h"
#include "hand_tree.h"
#include "vcfr_state.h"
#include "vcfr.h"

using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

template <>
void VCFR::UpdateRegrets<int>(Node *node, double *vals, shared_ptr<double []> *succ_vals,
			      int *regrets) {
  int st = node->Street();
  int num_succs = node->NumSuccs();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);

  int floor = regret_floors_[st];
  int ceiling = regret_ceilings_[st];
  if (nn_regrets_) {
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      int *my_regrets = regrets + i * num_succs;
      for (int s = 0; s < num_succs; ++s) {
	double d = succ_vals[s][i] - vals[i];
	// Need different implementation for doubles
	int di = lrint(d * regret_scaling_[st]);
	int ri = my_regrets[s] + di;
	if (ri < floor) {
	  my_regrets[s] = floor;
	} else if (ri > ceiling) {
	  my_regrets[s] = ceiling;
	} else {
	  my_regrets[s] = ri;
	}
      }
    }
  } else {
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      int *my_regrets = regrets + i * num_succs;
      bool overflow = false;
      for (int s = 0; s < num_succs; ++s) {
	double d = succ_vals[s][i] - vals[i];
	my_regrets[s] += lrint(d * regret_scaling_[st]);
	if (my_regrets[s] < -2000000000 || my_regrets[s] > 2000000000) {
	  overflow = true;
	}
      }
      if (overflow) {
	for (int s = 0; s < num_succs; ++s) {
	  my_regrets[s] /= 2;
	}
      }
    }
  }
}

// This implementation does not round regrets to ints, nor do scaling.
template <>
void VCFR::UpdateRegrets<double>(Node *node, double *vals, shared_ptr<double []> *succ_vals,
				 double *regrets) {
  int st = node->Street();
  int num_succs = node->NumSuccs();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  
  double floor = regret_floors_[st];
  double ceiling = regret_ceilings_[st];
  if (nn_regrets_) {
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      double *my_regrets = regrets + i * num_succs;
      for (int s = 0; s < num_succs; ++s) {
	double newr = my_regrets[s] + succ_vals[s][i] - vals[i];
	if (newr < floor) {
	  my_regrets[s] = floor;
	} else if (newr > ceiling) {
	  my_regrets[s] = ceiling;
	} else {
	  my_regrets[s] = newr;
	}
      }
    }
  } else {
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      double *my_regrets = regrets + i * num_succs;
      for (int s = 0; s < num_succs; ++s) {
	my_regrets[s] += succ_vals[s][i] - vals[i];
      }
    }
  }
}

// This is ugly, but I can't figure out a better way.
void VCFR::UpdateRegrets(Node *node, int lbd, double *vals, shared_ptr<double []> *succ_vals) {
  int pa = node->PlayerActing();
  int st = node->Street();
  int nt = node->NonterminalID();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  int num_succs = node->NumSuccs();
  CFRStreetValues<double> *d_street_values;
  CFRStreetValues<int> *i_street_values;
  AbstractCFRStreetValues *street_values = regrets_->StreetValues(st);
  if ((d_street_values =
       dynamic_cast<CFRStreetValues<double> *>(street_values))) {
    double *board_regrets = d_street_values->AllValues(pa, nt) +
      lbd * num_hole_card_pairs * num_succs;
    UpdateRegrets(node, vals, succ_vals, board_regrets);
  } else if ((i_street_values =
	      dynamic_cast<CFRStreetValues<int> *>(street_values))) {
    int *board_regrets = i_street_values->AllValues(pa, nt) +
      lbd * num_hole_card_pairs * num_succs;
    UpdateRegrets(node, vals, succ_vals, board_regrets);
  }
}

void VCFR::UpdateRegretsBucketed(Node *node, int *street_buckets, double *vals,
				 shared_ptr<double []> *succ_vals, int *regrets) {
  int st = node->Street();
  int num_succs = node->NumSuccs();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);

  int floor = regret_floors_[st];
  int ceiling = regret_ceilings_[st];
  if (nn_regrets_) {
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      int b = street_buckets[i];
      int *my_regrets = regrets + b * num_succs;
      for (int s = 0; s < num_succs; ++s) {
	double d = succ_vals[s][i] - vals[i];
	// Need different implementation for doubles
	int di = lrint(d * regret_scaling_[st]);
	int ri = my_regrets[s] + di;
	if (ri < floor) {
	  my_regrets[s] = floor;
	} else if (ri > ceiling) {
	  my_regrets[s] = ceiling;
	} else {
	  my_regrets[s] = ri;
	}
      }
    }
  } else {
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      int b = street_buckets[i];
      int *my_regrets = regrets + b * num_succs;
      bool overflow = false;
      for (int s = 0; s < num_succs; ++s) {
	double d = succ_vals[s][i] - vals[i];
	my_regrets[s] += lrint(d * regret_scaling_[st]);
	if (my_regrets[s] < -2000000000 || my_regrets[s] > 2000000000) {
	  overflow = true;
	}
      }
      if (overflow) {
	for (int s = 0; s < num_succs; ++s) {
	  my_regrets[s] /= 2;
	}
      }
    }
  }
}

// This implementation does not round regrets to ints, nor do scaling.
void VCFR::UpdateRegretsBucketed(Node *node, int *street_buckets, double *vals,
				 shared_ptr<double []> *succ_vals, double *regrets) {
  int st = node->Street();
  int num_succs = node->NumSuccs();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  
  double floor = regret_floors_[st];
  double ceiling = regret_ceilings_[st];
  if (nn_regrets_) {
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      int b = street_buckets[i];
      double *my_regrets = regrets + b * num_succs;
      for (int s = 0; s < num_succs; ++s) {
	double newr = my_regrets[s] + succ_vals[s][i] - vals[i];
	if (newr < floor) {
	  my_regrets[s] = floor;
	} else if (newr > ceiling) {
	  my_regrets[s] = ceiling;
	} else {
	  my_regrets[s] = newr;
	}
      }
    }
  } else {
    for (int i = 0; i < num_hole_card_pairs; ++i) {
      int b = street_buckets[i];
      double *my_regrets = regrets + b * num_succs;
      for (int s = 0; s < num_succs; ++s) {
	my_regrets[s] += succ_vals[s][i] - vals[i];
      }
    }
  }
}

void VCFR::UpdateRegretsBucketed(Node *node, int *street_buckets, double *vals,
				 shared_ptr<double []> *succ_vals) {
  int pa = node->PlayerActing();
  int st = node->Street();
  int nt = node->NonterminalID();
  CFRStreetValues<double> *d_street_values;
  CFRStreetValues<int> *i_street_values;
  AbstractCFRStreetValues *street_values = regrets_->StreetValues(st);
  if ((d_street_values =
       dynamic_cast<CFRStreetValues<double> *>(street_values))) {
    double *d_regrets = d_street_values->AllValues(pa, nt);
    UpdateRegretsBucketed(node, street_buckets, vals, succ_vals, d_regrets);
  } else if ((i_street_values =
	      dynamic_cast<CFRStreetValues<int> *>(street_values))) {
    int *i_regrets = i_street_values->AllValues(pa, nt);
    UpdateRegretsBucketed(node, street_buckets, vals, succ_vals, i_regrets);
  }
}

shared_ptr<double []> VCFR::OurChoice(Node *p0_node, Node *p1_node, int gbd, VCFRState *state) {
  int pa = p0_node->PlayerActing();
  Node *node = pa == 0 ? p0_node : p1_node;
  Node *responding_node = pa == 0 ? p1_node : p0_node;
  int st = node->Street();
  int num_succs = node->NumSuccs();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  int nt = node->NonterminalID();
  int lbd = state->LocalBoardIndex(st, gbd);
  unique_ptr<int []> succ_mapping = GetSuccMapping(node, responding_node);
  shared_ptr<double []> vals;
  unique_ptr< shared_ptr<double []> []> succ_vals(new shared_ptr<double []> [num_succs]);
  for (int s = 0; s < num_succs; ++s) {
    int p0_s = pa == 0 ? s : succ_mapping[s];
    int p1_s = pa == 0 ? succ_mapping[s] : s;
    VCFRState succ_state(*state, node, s);
    succ_vals[s] = Process(p0_node->IthSucc(p0_s), p1_node->IthSucc(p1_s), gbd, &succ_state, st);
  }
  if (num_succs == 1) {
    vals = succ_vals[0];
  } else {
    int *street_buckets = state->StreetBuckets(st);
    vals.reset(new double[num_hole_card_pairs]);
    for (int i = 0; i < num_hole_card_pairs; ++i) vals[i] = 0;
    if (best_response_streets_[st]) {
      for (int i = 0; i < num_hole_card_pairs; ++i) {
	double max_val = succ_vals[0][i];
	for (int s = 1; s < num_succs; ++s) {
	  double sv = succ_vals[s][i];
	  if (sv > max_val) {max_val = sv;}
	}
	vals[i] = max_val;
      }
    } else {
      int dsi = node->DefaultSuccIndex();
      bool bucketed = ! buckets_.None(st) &&
	node->LastBetTo() < card_abstraction_.BucketThreshold(st);
      if (bucketed && ! value_calculation_) {
	// This is true when we are running CFR+ on a bucketed system.  We don't want to get the
	// current strategy from the regrets during the iteration, because the regrets for each
	// bucket are in an intermediate state.  Instead we compute the current strategy once at the
	// beginning of each iteration.  current_strategy_ always contains doubles.
	CFRStreetValues<double> *street_values =
	  dynamic_cast< CFRStreetValues<double> *>(
			current_strategy_->StreetValues(st));
	for (int i = 0; i < num_hole_card_pairs; ++i) {
	  int b = street_buckets[i];
	  double *current_probs =
	    street_values->AllValues(pa, nt) + b * num_succs;
	  for (int s = 0; s < num_succs; ++s) {
	    vals[i] += succ_vals[s][i] * current_probs[s];
	  }
	}
      } else {
	AbstractCFRStreetValues *street_values;
	if (value_calculation_) {
	  street_values = sumprobs_->StreetValues(st);
	} else {
	  street_values = regrets_->StreetValues(st);
	}
	unique_ptr<double []> current_probs(new double[num_succs]);
	if (bucketed) {
	  street_values->ComputeOurValsBucketed(pa, nt, num_hole_card_pairs, num_succs, dsi,
						succ_vals.get(), street_buckets, vals);
	} else {
	  street_values->ComputeOurVals(pa, nt, num_hole_card_pairs, num_succs, dsi,
					succ_vals.get(), lbd, vals);
#if 0
	  for (int i = 0; i < num_hole_card_pairs; ++i) {
	    int offset = lbd * num_hole_card_pairs * num_succs + i * num_succs;
	    street_values->RMProbs(pa, nt, offset, num_succs, dsi, current_probs.get());
	    for (int s = 0; s < num_succs; ++s) {
	      vals[i] += succ_vals[s][i] * current_probs[s];
	    }
	  }
#endif
	}
      }
      if (! value_calculation_ && ! pre_phase_) {
	if (bucketed) {
	  UpdateRegretsBucketed(node, state->StreetBuckets(st), vals.get(), succ_vals.get());
	} else {
	  // Need values for current board if this is unabstracted system
	  UpdateRegrets(node, lbd, vals.get(), succ_vals.get());
	}
      }
    }
  }

  return vals;
}

shared_ptr<double []> VCFR::OppChoice(Node *p0_node, Node *p1_node, int gbd, VCFRState *state) {
  int pa = p0_node->PlayerActing();
  Node *node = pa == 0 ? p0_node : p1_node;
  Node *responding_node = pa == 0 ? p1_node : p0_node;
  int st = node->Street();
  int num_succs = node->NumSuccs();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  const CanonicalCards *hands = state->Hands(st, gbd);
  int lbd = state->LocalBoardIndex(st, gbd);
  int num_hole_cards = Game::NumCardsForStreet(0);
  int max_card1 = Game::MaxCard() + 1;
  int num_enc;
  if (num_hole_cards == 1) num_enc = max_card1;
  else                     num_enc = max_card1 * max_card1;

  const shared_ptr<double []> &opp_probs = state->OppProbs();
  unique_ptr<shared_ptr<double []> []> succ_opp_probs(new shared_ptr<double []> [num_succs]);
  if (num_succs == 1) {
    succ_opp_probs[0].reset(new double[num_enc]);
    for (int i = 0; i < num_enc; ++i) {
      succ_opp_probs[0][i] = opp_probs[i];
    }
  } else {
    int *street_buckets = state->StreetBuckets(st);
    for (int s = 0; s < num_succs; ++s) {
      succ_opp_probs[s].reset(new double[num_enc]);
      for (int i = 0; i < num_enc; ++i) succ_opp_probs[s][i] = 0;
    }

    int dsi = node->DefaultSuccIndex();
    bool bucketed = ! buckets_.None(st) &&
      node->LastBetTo() < card_abstraction_.BucketThreshold(st);

    // At most one of d_sumprob_values, i_sumprob_vals and c_sumprob_vals is non-null.
    // All may be null.
    CFRStreetValues<double> *d_sumprob_values = nullptr;
    CFRStreetValues<int> *i_sumprob_values = nullptr;
    CFRStreetValues<unsigned char> *c_sumprob_values = nullptr;
    AbstractCFRStreetValues *sumprob_values = nullptr;
    if (sumprobs_ && sumprob_streets_[st]) {
      sumprob_values = sumprobs_->StreetValues(st);
      if (sumprob_values == nullptr) {
	fprintf(stderr, "No sumprobs values for street %u?!?\n", st);
	exit(-1);
      }
      if ((d_sumprob_values =
	   dynamic_cast<CFRStreetValues<double> *>(sumprob_values)) ==
	  nullptr) {
	if ((i_sumprob_values =
	     dynamic_cast<CFRStreetValues<int> *>(sumprob_values)) ==
	    nullptr) {
	  if ((c_sumprob_values =
	       dynamic_cast<CFRStreetValues<unsigned char> *>(sumprob_values)) ==
	      nullptr) {
	    fprintf(stderr, "sumprobs not doubles, ints or chars?!?\n");
	    exit(-1);
	  }
	}
      }
    }

    if (value_calculation_) {
      // For example, RGBR calculation
      if (br_current_) {
	fprintf(stderr, "br_current_ not handled currently\n");
	exit(-1);
      } else {
	if (d_sumprob_values) {
	  ProcessOppProbs(node, lbd, hands, bucketed, street_buckets, opp_probs.get(),
			  succ_opp_probs.get(), *d_sumprob_values, dsi, it_, soft_warmup_,
			  hard_warmup_, sumprob_scaling_[st], (CFRStreetValues<int> *)nullptr);
	} else if (i_sumprob_values) {
	  ProcessOppProbs(node, lbd, hands, bucketed, street_buckets, opp_probs.get(),
			  succ_opp_probs.get(), *i_sumprob_values, dsi, it_, soft_warmup_,
			  hard_warmup_, sumprob_scaling_[st], (CFRStreetValues<int> *)nullptr);
	} else if (c_sumprob_values) {
	  ProcessOppProbs(node, lbd, hands, bucketed, street_buckets, opp_probs.get(),
			  succ_opp_probs.get(), *c_sumprob_values, dsi, it_, soft_warmup_,
			  hard_warmup_, sumprob_scaling_[st], (CFRStreetValues<int> *)nullptr);
	} else {
	  fprintf(stderr, "value_calculation_ and ! br_current_ requires sumprobs\n");
	  exit(-1);
	}
      }
    } else if (bucketed) {
      // This is true when we are running CFR+ on a bucketed system.  We
      // don't want to get the current strategy from the regrets during the
      // iteration, because the regrets for each bucket are in an intermediate
      // state.  Instead we compute the current strategy once at the
      // beginning of each iteration.
      // current_strategy_ always contains doubles
      CFRStreetValues<double> *street_values =
	dynamic_cast< CFRStreetValues<double> *>(current_strategy_->StreetValues(st));
      int nt = node->NonterminalID();
      double *current_probs = street_values->AllValues(pa, nt);
      if (d_sumprob_values) {
	ProcessOppProbs(node, hands, street_buckets, opp_probs.get(), succ_opp_probs.get(),
			current_probs, it_, soft_warmup_, hard_warmup_, sumprob_scaling_[st],
			d_sumprob_values);
      } else {
	ProcessOppProbs(node, hands, street_buckets, opp_probs.get(), succ_opp_probs.get(),
			current_probs, it_, soft_warmup_, hard_warmup_, sumprob_scaling_[st],
			i_sumprob_values);
      }
    } else {
      // Such a mess!
      AbstractCFRStreetValues *cs_values;
      if (value_calculation_ && ! br_current_) {
	if (sumprobs_.get() == nullptr) {
	  fprintf(stderr, "VCFR::OppChoice() null sumprobs?!?\n");
	  exit(-1);
	}
	// value_calculation_ now handled above
	cs_values = sumprobs_->StreetValues(st);
      } else {
	if (sumprobs_.get() == nullptr) {
	  fprintf(stderr, "VCFR::OppChoice() null regrets?!?\n");
	  exit(-1);
	}
	cs_values = regrets_->StreetValues(st);
      }
      CFRStreetValues<double> *d_cs_values;
      CFRStreetValues<int> *i_cs_values;
      if ((d_cs_values =
	   dynamic_cast<CFRStreetValues<double> *>(cs_values))) {
	if (d_sumprob_values) {
	  ProcessOppProbs(node, lbd, hands, bucketed, street_buckets, opp_probs.get(),
			  succ_opp_probs.get(), *d_cs_values, dsi, it_, soft_warmup_, hard_warmup_,
			  sumprob_scaling_[st], d_sumprob_values);
	} else {
	  ProcessOppProbs(node, lbd, hands, bucketed, street_buckets, opp_probs.get(),
			  succ_opp_probs.get(), *d_cs_values, dsi, it_, soft_warmup_, hard_warmup_,
			  sumprob_scaling_[st], i_sumprob_values);
	}
      } else {
	i_cs_values = dynamic_cast<CFRStreetValues<int> *>(cs_values);
	if (i_cs_values == nullptr) {
	  fprintf(stderr, "Neither int nor double cs values?!?\n");
	  exit(-1);
	}
	if (d_sumprob_values) {
	  ProcessOppProbs(node, lbd, hands, bucketed, street_buckets, opp_probs.get(),
			  succ_opp_probs.get(), *i_cs_values, dsi, it_, soft_warmup_, hard_warmup_,
			  sumprob_scaling_[st], d_sumprob_values);
	} else {
	  ProcessOppProbs(node, lbd, hands, bucketed, street_buckets, opp_probs.get(),
			  succ_opp_probs.get(), *i_cs_values, dsi, it_, soft_warmup_, hard_warmup_,
			  sumprob_scaling_[st], i_sumprob_values);
	}
      }
    }
  }

  unique_ptr<int []> succ_mapping = GetSuccMapping(node, responding_node);
  shared_ptr<double []> vals;
  for (int s = 0; s < num_succs; ++s) {
    // We can't prune now.  Is that a big problem?
#if 0
    shared_ptr<double []> succ_total_card_probs(new double[max_card1]);
    CommonBetResponseCalcs(st, hands, succ_opp_probs[s].get(), &succ_sum_opp_probs,
			   succ_total_card_probs.get());
    if (prune_ && succ_sum_opp_probs == 0) {
      continue;
    }
#endif
    int p0_s = pa == 0 ? s : succ_mapping[s];
    int p1_s = pa == 0 ? succ_mapping[s] : s;
    VCFRState succ_state(*state, node, s, succ_opp_probs[s]);
    shared_ptr<double []> succ_vals = Process(p0_node->IthSucc(p0_s), p1_node->IthSucc(p1_s), gbd,
					      &succ_state, st);
    if (vals == nullptr) {
      vals = succ_vals;
    } else {
      for (int i = 0; i < num_hole_card_pairs; ++i) {
	vals[i] += succ_vals[i];
      }
    }
  }
  if (vals == nullptr) {
    // This can happen if there were non-zero opp probs on the prior street,
    // but the board cards just dealt blocked all the opponent hands with
    // non-zero probability.
    vals.reset(new double[num_hole_card_pairs]);
    for (int i = 0; i < num_hole_card_pairs; ++i) vals[i] = 0;
  }

  return vals;
}

using std::queue;
using std::shared_ptr;
using std::unique_ptr;

Request::Request(RequestType t, Node *p0_node, Node *p1_node, int gbd, const VCFRState *pred_state,
		 int *prev_canons) : request_type_(t), p0_node_(p0_node), p1_node_(p1_node),
				     gbd_(gbd), pred_state_(pred_state), prev_canons_(prev_canons) {
}

VCFRWorker::VCFRWorker(VCFR *vcfr) : vcfr_(vcfr) {
}

// Should be sure to call this while the worker is idle
void VCFRWorker::Reset(int pst) {
  int num_prev_hole_card_pairs = Game::NumHoleCardPairs(pst);
  vals_.reset(new double[num_prev_hole_card_pairs]);
  for (int i = 0; i < num_prev_hole_card_pairs; ++i) vals_[i] = 0;
}

void VCFRWorker::HandleRequest(const Request &request) {
  Node *p0_node = request.P0Node();
  Node *p1_node = request.P1Node();
  int ngbd = request.GBD();
  const VCFRState &pred_state = request.PredState();
  int *prev_canons = request.PrevCanons();
  int nst = p0_node->Street();
  if (vals_.get() == nullptr) {
    fprintf(stderr, "vals_ uninitialized\n");
    exit(-1);
  }
  shared_ptr<double []> bd_vals = vcfr_->ProcessSubgame(p0_node, p1_node, ngbd, pred_state);
  const CanonicalCards *hands = pred_state.Hands(nst, ngbd);
  int board_variants = BoardTree::NumVariants(nst, ngbd);
  int num_hands = hands->NumRaw();
  int max_card1 = Game::MaxCard() + 1;
  for (int nhcp = 0; nhcp < num_hands; ++nhcp) {
    const Card *cards = hands->Cards(nhcp);
    Card hi = cards[0];
    Card lo = cards[1];
    int enc = hi * max_card1 + lo;
    int prev_canon = prev_canons[enc];
    vals_[prev_canon] += board_variants * bd_vals[nhcp];
  }
}

void VCFRWorker::MainLoop(void) {
  queue<Request> *request_queue = vcfr_->GetRequestQueue();
  pthread_mutex_t *queue_mutex = vcfr_->GetQueueMutex();
  pthread_cond_t *queue_not_empty = vcfr_->GetQueueNotEmpty();
  pthread_cond_t *queue_not_full = vcfr_->GetQueueNotFull();

  while (true) {
    pthread_mutex_lock(queue_mutex);

    // Wait while the queue is empty.
    while (request_queue->empty()) {
      pthread_cond_wait(queue_not_empty, queue_mutex);
    }

    Request request = request_queue->front();
    request_queue->pop();

    pthread_cond_signal(queue_not_full);
    pthread_mutex_unlock(queue_mutex);
    if (request.GetRequestType() == RequestType::QUIT) {
      vcfr_->IncrementNumDone();
      break;
    }

    HandleRequest(request);
    vcfr_->IncrementNumDone();
  }
}

static void *worker_thread_run(void *v_w) {
  VCFRWorker *w = (VCFRWorker *)v_w;
  w->MainLoop();
  return NULL;
}

void VCFRWorker::Run(void) {
  pthread_create(&pthread_id_, NULL, worker_thread_run, this);
}

void VCFRWorker::Join(void) {
  pthread_join(pthread_id_, NULL);
}

void VCFR::SpawnWorkers(void) {
  workers_.reset(new unique_ptr<VCFRWorker>[num_threads_]);
  for (int i = 0; i < num_threads_; ++i) {
    workers_[i].reset(new VCFRWorker(this));
    workers_[i]->Run();
  }
}

void VCFR::IncrementNumDone(void) {
  pthread_mutex_lock(&num_done_mutex_);
  ++num_done_;
  pthread_mutex_unlock(&num_done_mutex_);
}

void VCFR::Split(Node *p0_node, Node *p1_node, int pgbd, VCFRState *state, int *prev_canons,
		 double *vals) {
  int nst = p0_node->Street();
  int pst = nst - 1;

  // Reset the workers; i.e., initialize all values to zero
  // Can only do this while workers are idle
  for (int t = 0; t < num_threads_; ++t) {
    workers_[t]->Reset(pst);
  }
  
  num_done_ = 0;
  int ngbd_begin = BoardTree::SuccBoardBegin(pst, pgbd, nst);
  int ngbd_end = BoardTree::SuccBoardEnd(pst, pgbd, nst);
  for (int ngbd = ngbd_begin; ngbd < ngbd_end; ++ngbd) {
    // Push onto the queue under mutex protection
    pthread_mutex_lock(&queue_mutex_);
    // Wait until there’s room in the queue.
    while (request_queue_.size() == kRequestQueueMaxSize) {
      pthread_cond_wait(&queue_not_full_, &queue_mutex_);
    }
    Request request(RequestType::PROCESS, p0_node, p1_node, ngbd, state, prev_canons);
    request_queue_.push(request);
    // Inform waiting threads that queue has a request
    pthread_cond_signal(&queue_not_empty_);
    pthread_mutex_unlock(&queue_mutex_);
  }

  // There should be a better way to do this without a busy loop
  int num_requests = ngbd_end - ngbd_begin;
  while (true) {
    // No mutex needed, right?
    if (num_done_ == num_requests) break;
    usleep(1);
  }

  int num_prev_hole_card_pairs = Game::NumHoleCardPairs(pst);
  for (int t = 0; t < num_threads_; ++t) {
    double *t_vals = workers_[t]->Vals();
    for (int i = 0; i < num_prev_hole_card_pairs; ++i) {
      vals[i] += t_vals[i];
    }
  }
}

void VCFR::SetStreetBuckets(int st, int gbd, VCFRState *state) {
  if (buckets_.None(st)) return;
  int num_board_cards = Game::NumBoardCards(st);
  const Card *board = BoardTree::Board(st, gbd);
  Card cards[7];
  for (int i = 0; i < num_board_cards; ++i) {
    cards[i + 2] = board[i];
  }
  const CanonicalCards *hands = state->Hands(st, gbd);
  int max_street = Game::MaxStreet();
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  int *street_buckets = state->StreetBuckets(st);
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    unsigned int h;
    if (st == max_street) {
      // Hands on final street were reordered by hand strength, but
      // bucket lookup requires the unordered hole card pair index
      const Card *hole_cards = hands->Cards(i);
      cards[0] = hole_cards[0];
      cards[1] = hole_cards[1];
      int hcp = HCPIndex(st, cards);
      h = ((unsigned int)gbd) * ((unsigned int)num_hole_card_pairs) + hcp;
    } else {
      h = ((unsigned int)gbd) * ((unsigned int)num_hole_card_pairs) + i;
    }
    street_buckets[i] = buckets_.Bucket(st, h);
  }
}

shared_ptr<double []> VCFR::StreetInitial(Node *p0_node, Node *p1_node, int pgbd,
					  VCFRState *state) {
  int nst = p0_node->Street();
  int pst = nst - 1;
  int prev_num_hole_card_pairs = Game::NumHoleCardPairs(pst);
#if 0
  // Move this to CFRP class
  if (nst == subgame_street_ && ! subgame_) {
    if (pre_phase_) {
      SpawnSubgame(p0_node, p1_node, plbd, state->ActionSequence(), state->OppProbs());
      // Code expects values to be returned so we return all zeroes
      shared_ptr<double []> vals(new double[prev_num_hole_card_pairs]);
      for (int i = 0; i < prev_num_hole_card_pairs; ++i) vals[i] = 0;
      return vals;
    } else {
      int pa = p0_node->PlayerActing();
      int nt = pa == 0 ? p0_node->NonterminalID() : p1_node->NonterminalID();
      double *final_vals = final_vals_[pa][nt][plbd];
      if (final_vals == nullptr) {
	fprintf(stderr, "No final vals for %u %u %u?!?\n", pa, nt, plbd);
	exit(-1);
      }
      final_vals_[pa][nt][plbd] = nullptr;
      return final_vals;
    }
  }
#endif
  const CanonicalCards *pred_hands = state->Hands(pst, pgbd);
  Card max_card = Game::MaxCard();
  int num_encodings = (max_card + 1) * (max_card + 1);
  unique_ptr<int []> prev_canons(new int[num_encodings]);
  shared_ptr<double []> vals(new double[prev_num_hole_card_pairs]);
  for (int i = 0; i < prev_num_hole_card_pairs; ++i) vals[i] = 0;
  for (int ph = 0; ph < prev_num_hole_card_pairs; ++ph) {
    if (pred_hands->NumVariants(ph) > 0) {
      const Card *prev_cards = pred_hands->Cards(ph);
      int prev_encoding = prev_cards[0] * (max_card + 1) +
	prev_cards[1];
      prev_canons[prev_encoding] = ph;
    }
  }
  for (int ph = 0; ph < prev_num_hole_card_pairs; ++ph) {
    if (pred_hands->NumVariants(ph) == 0) {
      const Card *prev_cards = pred_hands->Cards(ph);
      int prev_encoding = prev_cards[0] * (max_card + 1) +
	prev_cards[1];
      int pc = prev_canons[pred_hands->Canon(ph)];
      prev_canons[prev_encoding] = pc;
    }
  }

  if (nst == split_street_ && subgame_street_ == -1 && num_threads_ > 1) {
    // By default, split on the flop.
    Split(p0_node, p1_node, pgbd, state, prev_canons.get(), vals.get());
  } else {
    int ngbd_begin = BoardTree::SuccBoardBegin(pst, pgbd, nst);
    int ngbd_end = BoardTree::SuccBoardEnd(pst, pgbd, nst);
    for (int ngbd = ngbd_begin; ngbd < ngbd_end; ++ngbd) {
      const CanonicalCards *hands = state->Hands(nst, ngbd);
      SetStreetBuckets(nst, ngbd, state);
      // I can pass unset values for sum_opp_probs and total_card_probs.  I
      // know I will come across an opp choice node before getting to a terminal
      // node.
      shared_ptr<double []> next_vals = Process(p0_node, p1_node, ngbd, state, nst);

      int board_variants = BoardTree::NumVariants(nst, ngbd);
      int num_next_hands = hands->NumRaw();
      for (int nh = 0; nh < num_next_hands; ++nh) {
	const Card *cards = hands->Cards(nh);
	Card hi = cards[0];
	Card lo = cards[1];
	int enc = hi * (max_card + 1) + lo;
	int prev_canon = prev_canons[enc];
	vals[prev_canon] += board_variants * next_vals[nh];
      }
    }
  }
  
  // Scale down the values of the previous-street canonical hands
  double scale_down = Game::StreetPermutations(nst);
  for (int ph = 0; ph < prev_num_hole_card_pairs; ++ph) {
    int prev_hand_variants = pred_hands->NumVariants(ph);
    if (prev_hand_variants > 0) {
      // Is this doing the right thing?
      vals[ph] /= scale_down * prev_hand_variants;
    }
  }
  // Copy the canonical hand values to the non-canonical
  for (int ph = 0; ph < prev_num_hole_card_pairs; ++ph) {
    if (pred_hands->NumVariants(ph) == 0) {
      vals[ph] = vals[prev_canons[pred_hands->Canon(ph)]];
    }
  }

  return vals;
}

void VCFR::InitializeOppData(VCFRState *state, int st, int gbd) {
  if (state->SumOppProbs() != -1) return;
  const CanonicalCards *hands = state->Hands(st, gbd);
  state->AllocateTotalCardProbs();
  double sum_opp_probs;
  CommonBetResponseCalcs(st, hands, state->OppProbs().get(), &sum_opp_probs,
			 state->TotalCardProbs().get());
  state->SetSumOppProbs(sum_opp_probs);
}

shared_ptr<double []> VCFR::Process(Node *p0_node, Node *p1_node, int gbd, VCFRState *state,
				    int last_st) {
  int st = p0_node->Street();
  if (p0_node->Terminal()) {
    InitializeOppData(state, st, gbd);
    if (p0_node->NumRemaining() == 1) {
      return Fold(p0_node, state->P(), state->Hands(st, gbd), state->OppProbs().get(),
		  state->SumOppProbs(), state->TotalCardProbs().get());
    } else {
      return Showdown(p0_node, state->Hands(st, gbd), state->OppProbs().get(),
		      state->SumOppProbs(), state->TotalCardProbs().get());
    }
  }
  if (st > last_st) {
    return StreetInitial(p0_node, p1_node, gbd, state);
  }
  shared_ptr<double []> vals;
  if (p0_node->PlayerActing() == state->P()) {
    vals = OurChoice(p0_node, p1_node, gbd, state);
  } else {
    vals = OppChoice(p0_node, p1_node, gbd, state);
  }
  return vals;
}

// Must be called on the root of the entire tree
shared_ptr<double []> VCFR::ProcessRoot(const BettingTrees *betting_trees, int p,
					HandTree *hand_tree) {
  VCFRState state(p, hand_tree);
  SetStreetBuckets(0, 0, &state);
  return Process(betting_trees->Root(), betting_trees->Root(), 0, &state, 0);
}

// Two implementations of ProcessSubgame().  One if you have a VCFRState object to work from,
// one if you don't.
shared_ptr<double []> VCFR::ProcessSubgame(Node *p0_node, Node *p1_node, int gbd, 
					   const VCFRState &pred_state) {
  // It's important to create a new state object, I think.  In the case of multithreading, we don't
  // want multiple threads modifying the same state object.
  VCFRState state(pred_state.P(), pred_state.OppProbs(), pred_state.GetHandTree(),
		  pred_state.ActionSequence());
  int st = p0_node->Street();
  SetStreetBuckets(st, gbd, &state);
  return Process(p0_node, p1_node, gbd, &state, st);
}

shared_ptr<double []> VCFR::ProcessSubgame(Node *p0_node, Node *p1_node, int gbd, int p,
					   shared_ptr<double []> opp_probs,
					   const HandTree *hand_tree,
					   const string &action_sequence) {
  VCFRState state(p, opp_probs, hand_tree, action_sequence);
  int st = p0_node->Street();
  SetStreetBuckets(st, gbd, &state);
  return Process(p0_node, p1_node, gbd, &state, st);
}

void VCFR::SetCurrentStrategy(Node *node) {
  if (node->Terminal()) return;
  if (value_calculation_) {
    fprintf(stderr, "Don't call SetCurrentStrategy when doing value calculation?\n");
    exit(-1);
  }
  int num_succs = node->NumSuccs();
  int st = node->Street();
  int nt = node->NonterminalID();
  int dsi = node->DefaultSuccIndex();
  int pa = node->PlayerActing();

  // In RGBR calculation, for example, only want to set for opp
  if (current_strategy_->StreetValues(st)->Players(pa) && ! buckets_.None(st) &&
      node->LastBetTo() < card_abstraction_.BucketThreshold(st) &&
      num_succs > 1) {
    int num_buckets = buckets_.NumBuckets(st);
    // Only need to support regrets
#if 0
    AbstractCFRStreetValues *street_values;
    if (value_calculation_ && ! br_current_) {
      // Shouldn't get here
      street_values = sumprobs_->StreetValues(st);
    } else {
      street_values = regrets_->StreetValues(st);
    }
#endif
    AbstractCFRStreetValues *street_regrets = regrets_->StreetValues(st);
    // Current strategy is always doubles
    CFRStreetValues<double> *d_current_strategy_vals =
      dynamic_cast<CFRStreetValues<double> *>(
	       current_strategy_->StreetValues(st));
    double *all_cs_probs = d_current_strategy_vals->AllValues(pa, nt);
    street_regrets->SetCurrentAbstractedStrategy(pa, nt, num_buckets, num_succs, dsi,
						 all_cs_probs);
#if 0
    unique_ptr<double []> current_probs(new double[num_succs]);
    for (int b = 0; b < num_buckets; ++b) {
      street_values->RMProbs(pa, nt, b * num_succs, num_succs, dsi,
			     current_probs.get());
      d_current_strategy_vals->Set(p, nt, b, num_succs, current_probs.get());
    }
#endif
  }
  for (int s = 0; s < num_succs; ++s) {
    SetCurrentStrategy(node->IthSucc(s));
  }
}

VCFR::VCFR(const CardAbstraction &ca, const CFRConfig &cc, const Buckets &buckets,
	   int num_threads) :
  card_abstraction_(ca), cfr_config_(cc), buckets_(buckets) {
  num_threads_ = num_threads;
  subgame_street_ = cfr_config_.SubgameStreet();
  split_street_ = 1; // Default
  soft_warmup_ = cfr_config_.SoftWarmup();
  hard_warmup_ = cfr_config_.HardWarmup();
  nn_regrets_ = cfr_config_.NNR();
  br_current_ = false;
  prob_method_ = ProbMethod::REGRET_MATCHING;
  value_calculation_ = false;
  // Whether we prune branches if no opponent hand reaches.  Normally true,
  // but false when calculating CBRs.
  prune_ = true;
  pre_phase_ = false;

  int max_street = Game::MaxStreet();
  best_response_streets_.reset(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    best_response_streets_[st] = false;
  }
  
  sumprob_streets_.reset(new bool[max_street + 1]);
  const vector<int> &ssv = cfr_config_.SumprobStreets();
  int num_ssv = ssv.size();
  if (num_ssv == 0) {
    for (int st = 0; st <= max_street; ++st) {
      sumprob_streets_[st] = true;
    }
  } else {
    for (int st = 0; st <= max_street; ++st) {
      sumprob_streets_[st] = false;
    }
    for (int i = 0; i < num_ssv; ++i) {
      int st = ssv[i];
      sumprob_streets_[st] = true;
    }
  }

  regret_floors_.reset(new int[max_street + 1]);
  const vector<int> &fv = cfr_config_.RegretFloors();
  if (fv.size() == 0) {
    for (int s = 0; s <= max_street; ++s) {
      regret_floors_[s] = 0;
    }
  } else {
    if ((int)fv.size() < max_street + 1) {
      fprintf(stderr, "Regret floor vector too small\n");
      exit(-1);
    }
    for (int s = 0; s <= max_street; ++s) {
      if (fv[s] == 1) regret_floors_[s] = kMinInt;
      else            regret_floors_[s] = fv[s];
    }
  }

  regret_ceilings_.reset(new int[max_street + 1]);
  const vector<int> &cv = cfr_config_.RegretCeilings();
  if (cv.size() == 0) {
    for (int s = 0; s <= max_street; ++s) {
      regret_ceilings_[s] = kMaxInt;
    }
  } else {
    if ((int)cv.size() < max_street + 1) {
      fprintf(stderr, "Regret ceiling vector too small\n");
      exit(-1);
    }
    for (int s = 0; s <= max_street; ++s) {
      if (cv[s] == 0) regret_ceilings_[s] = kMaxInt;
      else            regret_ceilings_[s] = cv[s];
    }
  }

  regret_scaling_.reset(new double[max_street + 1]);
  sumprob_scaling_.reset(new double[max_street + 1]);
  const vector<double> &rv = cfr_config_.RegretScaling();
  if (rv.size() == 0) {
    for (int s = 0; s <= max_street; ++s) {
      regret_scaling_[s] = 1.0;
    }
  } else {
    if ((int)rv.size() < max_street + 1) {
      fprintf(stderr, "Regret scaling vector too small\n");
      exit(-1);
    }
    for (int s = 0; s <= max_street; ++s) {
      regret_scaling_[s] = rv[s];
    }
  }
  const vector<double> &sv = cfr_config_.SumprobScaling();
  if (sv.size() == 0) {
    for (int s = 0; s <= max_street; ++s) {
      sumprob_scaling_[s] = 1.0;
    }
  } else {
    if ((int)sv.size() < max_street + 1) {
      fprintf(stderr, "Sumprob scaling vector too small\n");
      exit(-1);
    }
    for (int s = 0; s <= max_street; ++s) {
      sumprob_scaling_[s] = sv[s];
    }
  }

  pthread_mutex_init(&queue_mutex_, NULL);
  pthread_mutex_init(&num_done_mutex_, NULL);
  pthread_cond_init(&queue_not_empty_, NULL);
  pthread_cond_init(&queue_not_full_, NULL);
  SpawnWorkers();
}

VCFR::~VCFR(void) {
  num_done_ = 0;
  // Add num_threads_ quit requests to the queue
  for (int t = 0; t < num_threads_; ++t) {
    pthread_mutex_lock(&queue_mutex_);
    // Wait until there’s room in the queue.
    while (request_queue_.size() == kRequestQueueMaxSize) {
      pthread_cond_wait(&queue_not_full_, &queue_mutex_);
    }
    Request request(RequestType::QUIT, nullptr, nullptr, -1, nullptr, nullptr);
    request_queue_.push(request);
    // Inform waiting threads that queue has a request
    pthread_cond_signal(&queue_not_empty_);
    pthread_mutex_unlock(&queue_mutex_);
  }
  // There should be a better way to do this without a busy loop
  while (true) {
    // No mutex needed, right?
    if (num_done_ == num_threads_) break;
    usleep(1);
  }
  for (int t = 0; t < num_threads_; ++t) {
    workers_[t]->Join();
  }
  pthread_mutex_destroy(&queue_mutex_);
  pthread_mutex_destroy(&num_done_mutex_);
  pthread_cond_destroy(&queue_not_empty_);
  pthread_cond_destroy(&queue_not_full_);
}

