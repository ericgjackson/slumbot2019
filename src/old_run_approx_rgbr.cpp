// Computes an approximate real-game best-response.  It is a sampling approximation of a lower
// bound to the true best-response value.
//
// Similar to a real best-response calculation, the code here computes the value a "responder" can
// achieve playing against a "target" player strategy.  There are two refinements:
// 1) The responder is limited in how he can respond (as compared to a true best-response).  So
// the value we get out is a lower bound on the true real-game best-response value.
// 2) We sample so that we only get an approximation of this lower bound value.
//
// run_approx_rgbr takes a street argument.  Prior to the given street the responder just plays
// according to the target strategy.  On the given street and later street the responder
// plays a best response.
//
// We also sample from the boards on the given street.
//
// Need to use resolve probs after resolving

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h> // gettimeofday()

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_tree.h"
#include "betting_trees.h"
#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "canonical.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "cfr_config.h"
#include "cfr_params.h"
#include "cfr_utils.h"
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
#include "subgame_utils.h"
#include "unsafe_eg_cfr.h"
#include "vcfr.h"
#include "vcfr_state.h"

using std::pair;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;

class PostResponder : public VCFR {
public:
  PostResponder(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
		const Buckets &buckets, int it, int street, bool quantize, bool resolve,
		const CardAbstraction &s_ca, const BettingAbstraction &s_ba, const CFRConfig &s_cc);
  ~PostResponder(void) {}
  void Initialize(int responder_p);
  shared_ptr<double []> ProcessRoot(const VCFRState &state);
  bool Resolving(void) const {return resolve_;}
  void Resolve(Node *node, int gbd);
private:
  const BettingAbstraction &subgame_betting_abstraction_;
  int it_;
  int street_;
  bool quantize_;
  bool resolve_;
  unique_ptr<Buckets> subgame_buckets_;
  unique_ptr<EGCFR> eg_cfr_;
  unique_ptr<BettingTrees> betting_trees_;
  int num_subgame_its_;
  unique_ptr<BettingTrees> subtrees_;
  int num_resolves_;
  double resolving_secs_;
};

class PreResponder : public VCFR {
public:
  PreResponder(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
	       const Buckets &buckets, int it, int street, int num_sampled_boards, bool quantize,
	       PostResponder *post_responder);
  ~PreResponder(void) {}
  void Initialize(int responder_p);
  double Go(int responder_p);
private:
  shared_ptr<double []> StreetInitial(Node *p0_node, Node *p1_node, int plbd,
				      const VCFRState &state);

  int it_;
  int street_;
  bool quantize_;
  PostResponder *post_responder_;
  unique_ptr<BettingTrees> betting_trees_;
  unique_ptr<HandTree> trunk_hand_tree_;
  unique_ptr<int []> board_samples_;
  unique_ptr<unique_ptr<bool []> []> has_continuation_;
};

PostResponder::PostResponder(const CardAbstraction &ca, const BettingAbstraction &ba,
			     const CFRConfig &cc, const Buckets &buckets, int it, int street,
			     bool quantize, bool resolve, const CardAbstraction &s_ca,
			     const BettingAbstraction &s_ba, const CFRConfig &s_cc) :
  VCFR(ca, ba, cc, buckets, 1), subgame_betting_abstraction_(s_ba), it_(it), street_(street),
  quantize_(quantize), resolve_(resolve) {
  value_calculation_ = true;
  int max_street = Game::MaxStreet();
  for (int st = street_; st <= max_street; ++st) {
    best_response_streets_[st] = true;
  }
  num_subgame_its_ = 200;
  num_resolves_ = 0;
  resolving_secs_ = 0;
  if (resolve_) {
    subgame_buckets_.reset(new Buckets(s_ca, false));
    eg_cfr_.reset(new UnsafeEGCFR(s_ca, ca, s_ba, ba, s_cc, cc, *subgame_buckets_, 1));
  }
}

void PostResponder::Initialize(int responder_p) {
  int target_p = responder_p^1;
  if (betting_abstraction_.Asymmetric()) {
    betting_trees_.reset(new BettingTrees(betting_abstraction_, target_p));
  } else {
    betting_trees_.reset(new BettingTrees(betting_abstraction_));
  }
  
  int max_street = Game::MaxStreet();
  unique_ptr<bool []> streets(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) streets[st] = st >= street_;
  
  int num_players = Game::NumPlayers();
  unique_ptr<bool []> players(new bool[num_players]);
  for (int p = 0; p < num_players; ++p) {
    players[p] = p == target_p;
  }
  sumprobs_.reset(new CFRValues(players.get(), streets.get(), 0, 0, buckets_,
				betting_trees_->GetBettingTree()));

  char dir[500];
  sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::OldCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), card_abstraction_.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(), betting_abstraction_.BettingAbstractionName().c_str(),
	  cfr_config_.CFRConfigName().c_str());
  if (betting_abstraction_.Asymmetric()) {
    char buf[100];
    sprintf(buf, ".p%u", target_p);
    strcat(dir, buf);
  }
  sumprobs_->Read(dir, it_, betting_trees_->GetBettingTree(), "x", -1, true, quantize_);
}

// Need reach_probs_.  Ugh.
void PostResponder::Resolve(Node *node, int gbd) {
  int st = node->Street();
  subtrees_.reset(CreateSubtrees(st, node->PlayerActing(), node->LastBetTo(), -1,
				 subgame_betting_abstraction_));
  struct timespec start, finish;
  clock_gettime(CLOCK_MONOTONIC, &start);
#if 0
  eg_cfr_->SolveSubgame(subtrees_.get(), gbd, reach_probs, action_sequence,
			resolve_hand_tree_.get(), nullptr, -1, true, num_subgame_its_);
#endif
  clock_gettime(CLOCK_MONOTONIC, &finish);
  resolving_secs_ += (finish.tv_sec - start.tv_sec);
  resolving_secs_ += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
  ++num_resolves_;
  // next_a_node = subtrees_->Root();
#if 0
  // Do I need to do anything for boards?  Esp. if resolving turn?
  for (int st1 = st; st1 <= max_street; ++st1) {
    int gbd;
    if (st1 == max_street) gbd = msbd_;
    else                   gbd = BoardTree::PredBoard(msbd_, st1);
    a_boards_[st1] = BoardTree::LocalIndex(st, root_bd, st1, gbd);
  }
#endif
}

// This is a corner case - invoked when street_ is 0 and we need to invoke the post
// responder on the root.
shared_ptr<double []> PostResponder::ProcessRoot(const VCFRState &state) {
  return Process(betting_trees_->Root(), betting_trees_->Root(), 0, state, 0);
}

PreResponder::PreResponder(const CardAbstraction &ca, const BettingAbstraction &ba,
			   const CFRConfig &cc, const Buckets &buckets, int it, int street,
			   int num_sampled_boards, bool quantize, PostResponder *post_responder) :
  VCFR(ca, ba, cc, buckets, 1), it_(it), street_(street), quantize_(quantize),
  post_responder_(post_responder) {
  int max_street = Game::MaxStreet();
  bool all_post_bucketed = true;
  for (int st = street_; st <= max_street; ++st) {
    if (buckets.None(st)) {
      all_post_bucketed = false;
      break;
    }
  }
  // If we are using buckets for every street >= street_, then we can create a hand tree
  // per sampled subgame.  Otherwise the trunk hand tree is created for the whole game.  Won't
  // work for the full game of Holdem, but we're not likely to build an unabstracted system
  // for full Holdem.
  if (all_post_bucketed) {
    trunk_hand_tree_.reset(new HandTree(0, 0, street_ - 1));
  } else {
    trunk_hand_tree_.reset(new HandTree(0, 0, max_street));
  }
  value_calculation_ = true;
  // best_response_streets_ default to false

  int num_boards = BoardTree::NumBoards(street_);
  board_samples_.reset(new int[num_boards]);
  if (street_ > 0) {
    has_continuation_.reset(new unique_ptr<bool []>[street_]);
    for (int st = 0; st < street_; ++st) {
      int num_boards = BoardTree::NumBoards(st);
      has_continuation_[st].reset(new bool[num_boards]);
    }
    has_continuation_[0][0] = true;
  }

  if (num_sampled_boards == 0 || num_sampled_boards > num_boards) {
    num_sampled_boards = num_boards;
  }
  if (num_sampled_boards == num_boards) {
    fprintf(stderr, "Sampling all boards on street %i\n", street_);
    for (int bd = 0; bd < num_boards; ++bd) {
      board_samples_[bd] = BoardTree::NumVariants(street_, bd);
    }
    for (int st = 1; st < street_; ++st) {
      int num_st_boards = BoardTree::NumBoards(st);
      for (int bd = 0; bd < num_st_boards; ++bd) {
	has_continuation_[st][bd] = true;
      }
    }
  } else {
    fprintf(stderr, "Sampling only some boards on street %i\n", street_);
    for (int bd = 0; bd < num_boards; ++bd) board_samples_[bd] = 0;
    for (int st = 1; st < street_; ++st) {
      int num_st_boards = BoardTree::NumBoards(st);
      for (int bd = 0; bd < num_st_boards; ++bd) {
	has_continuation_[st][bd] = false;
      }
    }
    struct drand48_data rand_buf;
    struct timeval time; 
    gettimeofday(&time, NULL);
    srand48_r((time.tv_sec * 1000) + (time.tv_usec / 1000), &rand_buf);
    vector< pair<double, int> > v;
    for (int bd = 0; bd < num_boards; ++bd) {
      int board_count = BoardTree::BoardCount(street_, bd);
      for (int i = 0; i < board_count; ++i) {
	double r;
	drand48_r(&rand_buf, &r);
	v.push_back(std::make_pair(r, bd));
      }
    }
    std::sort(v.begin(), v.end());
    for (int i = 0; i < num_sampled_boards; ++i) {
      int bd = v[i].second;
      // fprintf(stderr, "Sampling %i\n", bd);
      ++board_samples_[bd];
      if (street_ > 1) {
	const Card *board = BoardTree::Board(street_, bd);
	for (int st = 1; st < street_; ++st) {
	  int pbd = BoardTree::LookupBoard(board, st);
	  has_continuation_[st][pbd] = true;
	}
      }
    }
  }
}

// Three possibilities:
// 1) We are on street_
// 2) We are before street_
// 3) We are after street_
shared_ptr<double []> PreResponder::StreetInitial(Node *p0_node, Node *p1_node, int pbd,
						  const VCFRState &state) {
  int nst = p0_node->Street();
  int pst = nst - 1;
  int prev_num_hole_card_pairs = Game::NumHoleCardPairs(pst);
  if (nst <= street_) {
    // Unfortunate replication of code from VCFR::StreetInitial()
    const HandTree *prev_hand_tree = state.GetHandTree();
    const CanonicalCards *pred_hands = prev_hand_tree->Hands(pst, pbd);
    Card max_card = Game::MaxCard();
    int num_encodings = (max_card + 1) * (max_card + 1);
    unique_ptr<int []> prev_canons(new int[num_encodings]);
    for (int ph = 0; ph < prev_num_hole_card_pairs; ++ph) {
      if (pred_hands->NumVariants(ph) > 0) {
	const Card *prev_cards = pred_hands->Cards(ph);
	int prev_encoding = prev_cards[0] * (max_card + 1) + prev_cards[1];
	prev_canons[prev_encoding] = ph;
      }
    }
    for (int ph = 0; ph < prev_num_hole_card_pairs; ++ph) {
      if (pred_hands->NumVariants(ph) == 0) {
	const Card *prev_cards = pred_hands->Cards(ph);
	int prev_encoding = prev_cards[0] * (max_card + 1) + prev_cards[1];
	int pc = prev_canons[pred_hands->Canon(ph)];
	prev_canons[prev_encoding] = pc;
      }
    }

    shared_ptr<double []> vals(new double[prev_num_hole_card_pairs]);
    for (int i = 0; i < prev_num_hole_card_pairs; ++i) vals[i] = 0;
    // pbd is a global board index
    int ngbd_begin = BoardTree::SuccBoardBegin(pst, pbd, nst);
    int ngbd_end = BoardTree::SuccBoardEnd(pst, pbd, nst);

    int total_num_samples = 0;
    for (int ngbd = ngbd_begin; ngbd < ngbd_end; ++ngbd) {
      int num_samples;
      if (nst == street_) {
	num_samples = board_samples_[ngbd];
	if (num_samples == 0) continue;
      } else {
	if (! has_continuation_[nst][ngbd]) continue;
	num_samples = BoardTree::NumVariants(nst, ngbd);
      }
      total_num_samples += num_samples;
      shared_ptr<double []> next_vals;
      unique_ptr<HandTree> subgame_hand_tree;
      if (nst == street_) {
	int nlbd;
	const HandTree *next_hand_tree;
	unique_ptr<VCFRState> next_state;
	if (state.GetHandTree()->FinalSt() == Game::MaxStreet()) {
	  // The trunk hand tree is for the whole game
	  next_hand_tree = state.GetHandTree();
	  nlbd = ngbd;
	  // We could reuse state actually
	  next_state.reset(new VCFRState(state.P(), state.OppProbs(), next_hand_tree,
					 state.ActionSequence(), 0, 0));
	} else {
	  subgame_hand_tree.reset(new HandTree(nst, ngbd, Game::MaxStreet()));
	  next_hand_tree = subgame_hand_tree.get();
	  nlbd = 0;
	  next_state.reset(new VCFRState(state.P(), state.OppProbs(), next_hand_tree,
					 state.ActionSequence(), ngbd, street_));
	}
	SetStreetBuckets(nst, ngbd, *next_state);
	if (next_state->P() == 0) {
	  // P0 is the best responder.  P1 is the target.  Want to use P1's tree in the post
	  // phase.
	  if (post_responder_->Resolving()) {
	    // Make sure nlbd is 0?
	    post_responder_->Resolve(p1_node, ngbd);
	  }
	  next_vals = post_responder_->Process(p1_node, p1_node, nlbd, *next_state, nst);
	} else {
	  // The opposite
	  if (post_responder_->Resolving()) {
	    post_responder_->Resolve(p0_node, ngbd);
	  }
	  next_vals = post_responder_->Process(p0_node, p0_node, nlbd, *next_state, nst);
	}
      } else {
	int nlbd = BoardTree::LocalIndex(state.RootBdSt(), state.RootBd(), nst, ngbd);
	SetStreetBuckets(nst, ngbd, state);
	next_vals = Process(p0_node, p1_node, nlbd, state, nst);
      }
      const CanonicalCards *hands;
      if (subgame_hand_tree.get()) {
	hands = subgame_hand_tree->Hands(nst, 0);
      } else {
	hands = state.GetHandTree()->Hands(nst, ngbd);
      }
      int num_next_hands = hands->NumRaw();
      for (int nhcp = 0; nhcp < num_next_hands; ++nhcp) {
	const Card *cards = hands->Cards(nhcp);
	Card hi = cards[0];
	Card lo = cards[1];
	int enc = hi * (max_card + 1) + lo;
	int prev_canon = prev_canons[enc];
	// Aren't we going to be scaling by board variants at other StreetInitial nodes in
	// the post phase?
	// vals[prev_canon] += board_variants * next_vals[nh];
	vals[prev_canon] += num_samples * next_vals[nhcp];
      }
    }
    if (total_num_samples > 0) {
      double d_total_num_samples = total_num_samples;
      int num_board_permutations = Game::StreetPermutations3(nst);
      double overweighting = num_board_permutations / d_total_num_samples;
#if 0
      // Temporary - not appropriate if we are sampling
      if (overweighting != 1.0) {
	fprintf(stderr, "overweighting %f\n", overweighting);
	fprintf(stderr, "nst %i nbp %i tns %i pbd %i\n", nst, num_board_permutations,
		total_num_samples, pbd);
	exit(-1);
      }
#endif
      double scale_down = Game::StreetPermutations(nst);
      for (int ph = 0; ph < prev_num_hole_card_pairs; ++ph) {
	int prev_hand_variants = pred_hands->NumVariants(ph);
	if (prev_hand_variants > 0) {
	  vals[ph] *= overweighting / (scale_down * prev_hand_variants);
	}
      }
      // Copy the canonical hand values to the non-canonical
      for (int ph = 0; ph < prev_num_hole_card_pairs; ++ph) {
	if (pred_hands->NumVariants(ph) == 0) {
	  vals[ph] = vals[prev_canons[pred_hands->Canon(ph)]];
	}
      }
    }

    return vals;
  } else {
    return VCFR::StreetInitial(p0_node, p1_node, pbd, state);
  }
}

void PreResponder::Initialize(int responder_p) {
  // For asymmetric, we need both betting trees
  betting_trees_.reset(new BettingTrees(betting_abstraction_));

  int max_street = Game::MaxStreet();
  unique_ptr<bool []> streets(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) streets[st] = st < street_;

  if (betting_abstraction_.Asymmetric()) {
    sumprobs_.reset(new CFRValues(nullptr, streets.get(), 0, 0, buckets_, *betting_trees_));
  } else {
    sumprobs_.reset(new CFRValues(nullptr, streets.get(), 0, 0, buckets_,
				  betting_trees_->GetBettingTree()));
  }

  char dir[500];
  sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::OldCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), card_abstraction_.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(), betting_abstraction_.BettingAbstractionName().c_str(),
	  cfr_config_.CFRConfigName().c_str());
  if (betting_abstraction_.Asymmetric()) {
    sumprobs_->ReadAsymmetric(dir, it_, *betting_trees_, "x", -1, true, quantize_);
  } else {
    sumprobs_->Read(dir, it_, betting_trees_->GetBettingTree(), "x", -1, true, quantize_);
  }
}

double PreResponder::Go(int responder_p) {
  int num_hole_card_pairs = Game::NumHoleCardPairs(0);
  int num_remaining = Game::NumCardsInDeck() - Game::NumCardsForStreet(0);
  int num_opp_hole_card_pairs = num_remaining * (num_remaining - 1) / 2;

  shared_ptr<double []> vals;
  if (street_ == 0) {
    // Need to invoke the post responder directly
    HandTree hand_tree(0, 0, Game::MaxStreet());
    VCFRState state(responder_p, &hand_tree);
    SetStreetBuckets(0, 0, state);
    vals = post_responder_->ProcessRoot(state);
  } else {
    VCFRState state(responder_p, trunk_hand_tree_.get());
    SetStreetBuckets(0, 0, state);
    vals = Process(betting_trees_->Root(0), betting_trees_->Root(1), 0, state, 0);
  }
  double sum = 0;
  for (int hcp = 0; hcp < num_hole_card_pairs; ++hcp) {
    sum += vals[hcp];
  }
  double overall = sum / (num_hole_card_pairs * num_opp_hole_card_pairs);
  printf("P%i BR val: %f\n", responder_p, overall);
#if 0
  if (responder_p == 1) {
    Card aa_cards[2];
    aa_cards[0] = MakeCard(12, 3);
    aa_cards[1] = MakeCard(12, 2);
    int aa_hcp = HCPIndex(0, aa_cards);
    double aa_p1_val = vals[aa_hcp] / num_opp_hole_card_pairs;
    printf("AA P1: %f (hcp %i)\n", aa_p1_val, aa_hcp);
  }
#endif
  return overall;
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <card params> <betting abstraction params> <CFR params> "
	  "<it> [quantize|raw] <street> <num sampled boards> (<resolve card params> "
	  "<resolve betting params> <resolve CFR params>)\n", prog_name);
  fprintf(stderr, "\n");
  fprintf(stderr, "Specify 0 for <num sampled boards> to not sample\n");
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 9 && argc != 12) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unique_ptr<Params> card_params = CreateCardAbstractionParams();
  card_params->ReadFromFile(argv[2]);
  unique_ptr<CardAbstraction>
    card_abstraction(new CardAbstraction(*card_params));
  unique_ptr<Params> betting_params = CreateBettingAbstractionParams();
  betting_params->ReadFromFile(argv[3]);
  unique_ptr<BettingAbstraction>
    betting_abstraction(new BettingAbstraction(*betting_params));
  unique_ptr<Params> cfr_params = CreateCFRParams();
  cfr_params->ReadFromFile(argv[4]);
  unique_ptr<CFRConfig>
    cfr_config(new CFRConfig(*cfr_params));
  int it;
  if (sscanf(argv[5], "%i", &it) != 1) Usage(argv[0]);
  bool quantize;
  string qa = argv[6];
  if (qa == "quantize") quantize = true;
  else if (qa == "raw") quantize = false;
  else                  Usage(argv[0]);
  int street, num_sampled_boards;
  if (sscanf(argv[7], "%i", &street) != 1)             Usage(argv[0]);
  if (sscanf(argv[8], "%i", &num_sampled_boards) != 1) Usage(argv[0]);

  bool resolve = false;
  unique_ptr<CardAbstraction> subgame_card_abstraction;
  unique_ptr<BettingAbstraction> subgame_betting_abstraction;
  unique_ptr<CFRConfig> subgame_cfr_config;
  if (argc == 12) {
    resolve = true;
    unique_ptr<Params> subgame_card_params = CreateCardAbstractionParams();
    subgame_card_params->ReadFromFile(argv[9]);
    subgame_card_abstraction.reset(new CardAbstraction(*subgame_card_params));
    unique_ptr<Params> subgame_betting_params = CreateBettingAbstractionParams();
    subgame_betting_params->ReadFromFile(argv[10]);
    subgame_betting_abstraction.reset(new BettingAbstraction(*subgame_betting_params));
    unique_ptr<Params> subgame_cfr_params = CreateCFRParams();
    subgame_cfr_params->ReadFromFile(argv[11]);
    subgame_cfr_config.reset(new CFRConfig(*subgame_cfr_params));
  }
  
  int num_players = Game::NumPlayers();
  if (num_players != 2) {
    fprintf(stderr, "Only heads-up supported\n");
    exit(-1);
  }

  BoardTree::Create();
  BoardTree::CreateLookup();
  BoardTree::BuildBoardCounts();

  // Should get created when needed
  // HandValueTree::Create();

  Buckets buckets(*card_abstraction, false);
  PostResponder post_responder(*card_abstraction, *betting_abstraction, *cfr_config, buckets, it,
			       street, quantize, resolve, *subgame_card_abstraction,
			       *subgame_betting_abstraction, *subgame_cfr_config);
  PreResponder pre_responder(*card_abstraction, *betting_abstraction, *cfr_config, buckets, it,
			     street, num_sampled_boards, quantize, &post_responder);
  double gap = 0;
  for (int responder_p = 0; responder_p < 2; ++responder_p) {
    pre_responder.Initialize(responder_p);
    post_responder.Initialize(responder_p);
    double val = pre_responder.Go(responder_p);
    gap += val;
  }
  printf("Gap: %f\n", gap);
  printf("Exploitability: %.2f mbb/g\n", ((gap / 2.0) / num_players) * 1000.0);
}
