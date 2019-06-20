// This is an implementation of CFR+.
//
// TODO: Remove old trunk files.
// TODO: skip unreachable subgames.  Will need to copy unaltered regret and
//       sumprob files to new iteration.  Subgame only unreachable if not
//       reachable for *any* flop board.  That may not be very common.  Could
//       also write out 0/1 for each flop board for reachability.  What does
//       Alberta do?
// TODO: skip river if no opp hands reach.  How does Alberta do it?
//
// Where does final_vals_ get constructed?  Is all the subgame stuff unfinished?

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "betting_abstraction.h"
#include "betting_trees.h"
#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "card_abstraction.h"
#include "cards.h"
#include "cfr_config.h"
#include "cfr_utils.h" // DeleteOldFiles()
#include "cfr_values.h"
// #include "cfrp_subgame.h"
#include "cfrp.h"
#include "constants.h"
#include "files.h"
#include "game.h"
#include "hand_tree.h"
#include "hand_value_tree.h"
#include "io.h"
#include "split.h"
#include "vcfr_state.h"

using std::shared_ptr;
using std::string;
using std::unique_ptr;

// Called from run_cfrp
void CFRP::Initialize(const BettingAbstraction &ba, int target_p) {
  asymmetric_ = ba.Asymmetric();
  betting_abstraction_name_ = ba.BettingAbstractionName();
  if (asymmetric_) {
    target_p_ = target_p;
  } else {
    target_p_ = -1;
  }
  if (asymmetric_) {
    betting_trees_.reset(new BettingTrees(ba, target_p_));
  } else {
    betting_trees_.reset(new BettingTrees(ba));
  }

  unique_ptr<bool []> streets;
  int max_street = Game::MaxStreet();
  if (subgame_street_ >= 0 && subgame_street_ <= max_street) {
    streets.reset(new bool[max_street + 1]);
    for (int st = 0; st <= max_street; ++st) {
      streets[st] = st < subgame_street_;
    }
  }
  regrets_.reset(new CFRValues(nullptr, streets.get(), 0, 0, buckets_,
			       betting_trees_->GetBettingTree()));
  
  // Should honor sumprobs_streets_
  if (asymmetric_) {
    int num_players = Game::NumPlayers();
    unique_ptr<bool []> players(new bool [num_players]);
    for (int p = 0; p < num_players; ++p) players[p] = false;
    players[target_p_] = true;
    sumprobs_.reset(new CFRValues(players.get(), streets.get(), 0, 0, buckets_,
				  betting_trees_->GetBettingTree()));
  } else {
    sumprobs_.reset(new CFRValues(nullptr, streets.get(), 0, 0, buckets_,
				  betting_trees_->GetBettingTree()));
  }

  unique_ptr<bool []> bucketed_streets(new bool[max_street + 1]);
  bucketed_ = false;
  for (int st = 0; st <= max_street; ++st) {
    bucketed_streets[st] = ! buckets_.None(st);
    if (bucketed_streets[st]) bucketed_ = true;
  }
  if (bucketed_) {
    // Hmm, we only want to allocate this for the streets that need it
    current_strategy_.reset(new CFRValues(nullptr, bucketed_streets.get(), 0, 0, buckets_,
					  betting_trees_->GetBettingTree()));
  } else {
    current_strategy_.reset(nullptr);
  }
}

CFRP::CFRP(const CardAbstraction &ca, const CFRConfig &cc, const Buckets &buckets,
	   int num_threads) :
  VCFR(ca, cc, buckets, num_threads) {
  BoardTree::Create();
  HandValueTree::Create();

  int max_street = Game::MaxStreet();
  hand_tree_.reset(new HandTree(0, 0, max_street));

  it_ = 0;
  if (subgame_street_ >= 0 && subgame_street_ <= max_street) {
    // Currently don't want to prune in the trunk when running CFR+ in the
    // trunk.  If we do this, then no regret files get written out for
    // unreached subgames.  And then we fail when we try to read those regret
    // files on the next iteration.
    //
    // A better (faster) solution might be to be robust to this situation.
    // We could copy the regret files from it N to it N+1 when the subgame
    // is not reached on iteration N+1.
    prune_ = false;
  }
}

// Temporarily comment out subgame stuff
#if 0
void CFRP::Post(int t) {
  // Set subgame_running_[t] to false *before* sem_post.  If we didn't do that,
  // we might break out of sem_wait in main thread and find no threads with
  // available status.
  // The following is possible.  Threads 1 and 2 finish at near the same time.
  // Thread 2 posts, thread 1 sets subgame_running[1] to false, the manager
  // chooses thread 1 to work on next.  That's fine, I think.
  subgame_running_[t] = false;
  int ret = sem_post(&available_);
  if (ret != 0) {
    fprintf(stderr, "sem_post failed\n");
    exit(-1);
  }
}

static int g_num_active = 0;

// This breaks if we get Post() before call to WaitForFinalSubgames(), but
// not corresponding join.  Saw num_remaining two but num active three.
void CFRP::WaitForFinalSubgames(void) {
  int num_remaining = 0;
  for (int t = 0; t < num_threads_; ++t) {
    // This was buggy
    // if (subgame_running_[t]) ++num_remaining;
    if (active_subgames_[t]) ++num_remaining;
  }
  if (num_remaining != g_num_active) {
    fprintf(stderr, "Expect num_remaining %u to match num_active %u\n",
	    num_remaining, g_num_active);
    exit(-1);
  }
  while (num_remaining > 0) {
    while (sem_wait(&available_) == EINTR) ;
    int t;
    for (t = 0; t < num_threads_; ++t) {
      if (! subgame_running_[t] && active_subgames_[t]) {
	CFRPSubgame *subgame = active_subgames_[t];
	if (subgame == nullptr) {
	  fprintf(stderr, "Subgame finished, but no subgame object?!?\n");
	  exit(-1);
	}
	pthread_join(pthread_ids_[t], NULL);
	Node *root = subgame->Root();
	int p = root->PlayerActing();
	int nt = root->NonterminalID();
	int root_bd = subgame->RootBd();
	final_vals_[p][nt][root_bd] = subgame->FinalVals();
	delete subgame;
	active_subgames_[t] = nullptr;
	--num_remaining;
	--g_num_active;
	break;
      }
    }
    // It's possible for sem_wait() to return even though there is no thread
    // ready to be joined.
  }
  if (g_num_active > 0) {
    fprintf(stderr, "Num active %u at end of WaitForFinalSubgames()\n",
	    g_num_active);
    exit(-1);
  }
}

static void *thread_run(void *v_sg) {
  CFRPSubgame *sg = (CFRPSubgame *)v_sg;
  sg->Go();
  return NULL;
}

void CFRP::SpawnSubgame(Node *node, int bd, const string &action_sequence, int p,
			const shared_ptr<double []> &opp_probs) {
  CFRPSubgame *subgame =
    new CFRPSubgame(card_abstraction_, betting_abstraction_, cfr_config_, buckets_, node, bd,
		    action_sequence, p, this);
  subgame->SetBestResponseStreets(best_response_streets_.get());
  subgame->SetBRCurrent(br_current_);
  subgame->SetValueCalculation(value_calculation_);
  // Wait for thread to be available
  while (sem_wait(&available_) == EINTR) ;

  // Find a thread that is not working
  int t;
  for (t = 0; t < num_threads_; ++t) {
    if (! subgame_running_[t]) break;
  }
  if (t == num_threads_) {
    fprintf(stderr, "sem_wait returned but no thread available\n");
    exit(-1);
  }
  CFRPSubgame *old_subgame = active_subgames_[t];
  if (old_subgame) {
    pthread_join(pthread_ids_[t], NULL);
    Node *root = old_subgame->Root();
    int p = root->PlayerActing();
    int nt = root->NonterminalID();
    int root_bd = old_subgame->RootBd();
    final_vals_[p][nt][root_bd] = old_subgame->FinalVals();
    delete old_subgame;
    active_subgames_[t] = nullptr;
    --g_num_active;
    if (num_threads_ == 1 && g_num_active != 0) {
      fprintf(stderr, "num_active %i\n", g_num_active);
      exit(-1);
    }
  }

  // Launch the current subgame
  subgame_running_[t] = true;
  active_subgames_[t] = subgame;
  // I could pass these into the constructor, no?
  subgame->SetTargetP(target_p_);
  subgame->SetIt(it_);
  subgame->SetOppProbs(opp_probs);
  subgame->SetThreadIndex(t);
  subgame->SetLastCheckpointIt(last_checkpoint_it_);
  ++g_num_active;
  if (num_threads_ == 1 && g_num_active != 1) {
    fprintf(stderr, "num_active %i\n", g_num_active);
    exit(-1);
  }
  pthread_create(&pthread_ids_[t], NULL, thread_run, subgame);
}
#endif

void CFRP::FloorRegrets(Node *node, int p) {
  if (node->Terminal()) return;
  int st = node->Street();
  int num_succs = node->NumSuccs();
  if (node->PlayerActing() == p && ! buckets_.None(st) && num_succs > 1) {
    int nt = node->NonterminalID();
    if (nn_regrets_ && regret_floors_[st] >= 0) {
      regrets_->StreetValues(st)->Floor(p, nt, num_succs, regret_floors_[st]);
    }
  }
  for (int s = 0; s < num_succs; ++s) {
    FloorRegrets(node->IthSucc(s), p);
  }
}

// Do trunk in main thread
void CFRP::HalfIteration(int p) {
  fprintf(stderr, "P%u half iteration\n", p);
  if (current_strategy_.get() != nullptr) {
    SetCurrentStrategy(betting_trees_->Root());
  }

#if 0
  // Temporarily comment out subgame stuff
  if (subgame_street_ >= 0 && subgame_street_ <= Game::MaxStreet()) {
    // subgame_running_ should be false for all threads
    // active_subgames_ should be nullptr for all threads
    for (int t = 0; t < num_threads_; ++t) {
      int ret = sem_post(&available_);
      if (ret != 0) {
	fprintf(stderr, "sem_post failed\n");
	exit(-1);
      }
    }
  }
#endif

  // if (subgame_street_ >= 0 && subgame_street_ <= Game::MaxStreet()) pre_phase_ = true;
  shared_ptr<double []> vals = ProcessRoot(betting_trees_.get(), p, hand_tree_.get());
#if 0
  if (subgame_street_ >= 0 && subgame_street_ <= Game::MaxStreet()) {
    WaitForFinalSubgames();
    pre_phase_ = false;
    vals = ProcessRoot(betting_trees_.get(), p, hand_tree_.get());
  }
#endif
#if 0
  int num_hole_card_pairs = Game::NumHoleCardPairs(0);
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    fprintf(stderr, "vals[%u] %f\n", i, vals[i]);
  }
#endif

#if 0
  int num_hole_card_pairs = Game::NumHoleCardPairs(0);
  int num_opp_hole_card_pairs = 50 * 49 / 2;
  double sum_vals = 0;
  for (int i = 0; i < num_hole_card_pairs; ++i) {
    sum_vals += vals[i] / num_opp_hole_card_pairs;
  }
  double avg_val = sum_vals / num_hole_card_pairs;
  fprintf(stderr, "%s avg val %f\n", p1 ? "P1" : "P2", avg_val);
#endif

  if (nn_regrets_ && bucketed_) {
    FloorRegrets(betting_trees_->Root(), p);
  }
}

void CFRP::Checkpoint(int it) {
  char dir[500];
  sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::NewCFRBase(),
	  Game::GameName().c_str(), Game::NumPlayers(),
	  card_abstraction_.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(), betting_abstraction_name_.c_str(),
	  cfr_config_.CFRConfigName().c_str());
  if (asymmetric_) {
    char buf[100];
    sprintf(buf, ".p%u", target_p_);
    strcat(dir, buf);
  }
  Mkdir(dir);
  regrets_->Write(dir, it, betting_trees_->Root(), "x", -1, false);
  sumprobs_->Write(dir, it, betting_trees_->Root(), "x", -1, true);
}

void CFRP::ReadFromCheckpoint(int it) {
  char dir[500];
  sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::OldCFRBase(),
	  Game::GameName().c_str(), Game::NumPlayers(),
	  card_abstraction_.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(), betting_abstraction_name_.c_str(),
	  cfr_config_.CFRConfigName().c_str());
  if (asymmetric_) {
    char buf[100];
    sprintf(buf, ".p%u", target_p_);
    strcat(dir, buf);
  }
  regrets_->Read(dir, it, betting_trees_->GetBettingTree(), "x", -1, false, false);
  sumprobs_->Read(dir, it, betting_trees_->GetBettingTree(), "x", -1, true, false);
}

void CFRP::Run(int start_it, int end_it) {
  if (start_it == 0) {
    fprintf(stderr, "CFR starts from iteration 1\n");
    exit(-1);
  }
  DeleteOldFiles(card_abstraction_, betting_abstraction_name_, cfr_config_, end_it);

  if (start_it > 1) {
    ReadFromCheckpoint(start_it - 1);
    last_checkpoint_it_ = start_it - 1;
  } else {
    bool double_regrets = cfr_config_.DoubleRegrets();
    bool double_sumprobs = cfr_config_.DoubleSumprobs();
    if (double_regrets) {
      regrets_->AllocateAndClear(betting_trees_->GetBettingTree(), CFRValueType::CFR_DOUBLE, false,
				 -1);
    } else {
      regrets_->AllocateAndClear(betting_trees_->GetBettingTree(), CFRValueType::CFR_INT, false,
				 -1);
    }
    if (double_sumprobs) {
      sumprobs_->AllocateAndClear(betting_trees_->GetBettingTree(), CFRValueType::CFR_DOUBLE, false,
				  -1);
    } else {
      sumprobs_->AllocateAndClear(betting_trees_->GetBettingTree(), CFRValueType::CFR_INT, false,
				  -1);
    }
  }
  if (bucketed_) {
    // Current strategy always uses doubles
    current_strategy_->AllocateAndClear(betting_trees_->GetBettingTree(), CFRValueType::CFR_DOUBLE,
					false, -1);
  }

  if (subgame_street_ >= 0 && subgame_street_ <= Game::MaxStreet()) {
    prune_ = false;
  }
  
  for (it_ = start_it; it_ <= end_it; ++it_) {
    fprintf(stderr, "It %u\n", it_);
    HalfIteration(1);
    HalfIteration(0);
  }

  Checkpoint(end_it);
}
