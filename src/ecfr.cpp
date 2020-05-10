#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "betting_abstraction.h"
#include "betting_tree.h"
#include "betting_trees.h"
#include "board_tree.h"
#include "buckets.h"
#include "card_abstraction.h"
#include "cards.h"
#include "cfr_config.h"
#include "ecfr.h"
#include "files.h"
#include "game.h"
#include "hand_tree.h"
#include "hand_value_tree.h"
#include "io.h"

using std::unique_ptr;

// #define FTL 1

static const double kUnset = -999999999.9;

ECFRNode::ECFRNode(Node *node, const Buckets &buckets) {
  terminal_ = node->Terminal();
  if (terminal_) {
    showdown_ = node->Showdown();
    last_bet_to_ = node->LastBetTo();
    player_acting_ = node->PlayerActing(); // At fold nodes, encodes player remaining
    return;
  }
  st_ = node->Street();
  player_acting_ = node->PlayerActing();
  int num_buckets = buckets.NumBuckets(st_);
  num_succs_ = node->NumSuccs();
  if (num_succs_ > 1) {
    // Only store regrets, etc. if there is more than one succ
    int num_values = num_buckets * num_succs_;
    regrets_.reset(new double[num_values]);
    sumprobs_.reset(new int[num_values]);
    for (int i = 0; i < num_values; ++i) {
      regrets_[i] = 0;
      sumprobs_[i] = 0;
    }
  }
  succs_.reset(new unique_ptr<ECFRNode>[num_succs_]);
  for (int s = 0; s < num_succs_; ++s) {
    succs_[s].reset(new ECFRNode(node->IthSucc(s), buckets));
  }
}

ECFRThread::ECFRThread(const CFRConfig &cfr_config, const Buckets &buckets, ECFRNode *root,
			 int seed, int batch_size, const int *board_table, int num_raw_boards,
			 unsigned long long int *total_its, int thread_index, int num_threads) :
  buckets_(buckets), root_(root), batch_size_(batch_size), board_table_(board_table),
  num_raw_boards_(num_raw_boards), total_its_(total_its), thread_index_(thread_index),
  num_threads_(num_threads) {
  srand48_r(seed, &rand_buf_);
  int max_street = Game::MaxStreet();
  canon_bds_.reset(new int[max_street + 1]);
  canon_bds_[0] = 0;
  int num_players = Game::NumPlayers();
  hole_cards_.reset(new int[num_players * 2]);
  hi_cards_.reset(new int[num_players]);
  lo_cards_.reset(new int[num_players]);
  hvs_.reset(new int[num_players]);
  hand_buckets_.reset(new int[num_players * (max_street + 1)]);
}

static void RMProbs(double *regrets, double *probs, int num_succs) {
  double sum_pos_regrets = 0;
  for (int s = 0; s < num_succs; ++s) {
    double r = regrets[s];
    if (r > 0) sum_pos_regrets += r;
  }
  if (sum_pos_regrets == 0) {
    probs[0] = 1.0;
    for (int s = 1; s < num_succs; ++s) probs[s] = 0;
  } else {
    for (int s = 0; s < num_succs; ++s) {
      double r = regrets[s];
      if (r <= 0) probs[s] = 0;
      else        probs[s] = r / sum_pos_regrets;
    }
  }
}

void ECFRThread::Deal(void) {
  double r;
  drand48_r(&rand_buf_, &r);
  unsigned int msbd = board_table_[(int)(r * num_raw_boards_)];
  int max_street = Game::MaxStreet();
  canon_bds_[max_street] = msbd;
  for (int st = 1; st < max_street; ++st) {
    canon_bds_[st] = BoardTree::PredBoard(msbd, st);
  }
  const Card *board = BoardTree::Board(max_street, msbd);
  Card cards[7];
  unsigned int num_ms_board_cards = Game::NumBoardCards(max_street);
  for (unsigned int i = 0; i < num_ms_board_cards; ++i) {
    cards[i+2] = board[i];
  }
  int end_cards = Game::MaxCard() + 1;

  int num_players = Game::NumPlayers();
  for (int p = 0; p < num_players; ++p) {
    int c1, c2;
    while (true) {
      drand48_r(&rand_buf_, &r);
      c1 = end_cards * r;
      if (InCards(c1, board, num_ms_board_cards)) continue;
      if (InCards(c1, hole_cards_.get(), 2 * p)) continue;
      break;
    }
    hole_cards_[2 * p] = c1;
    while (true) {
      drand48_r(&rand_buf_, &r);
      c2 = end_cards * r;
      if (InCards(c2, board, num_ms_board_cards)) continue;
      if (InCards(c2, hole_cards_.get(), 2 * p + 1)) continue;
      break;
    }
    hole_cards_[2 * p + 1] = c2;
    if (c1 > c2) {hi_cards_[p] = c1; lo_cards_[p] = c2;}
    else         {hi_cards_[p] = c2; lo_cards_[p] = c1;}
  }

  for (int p = 0; p < num_players; ++p) hvs_[p] = 0;

  for (int st = 0; st <= max_street; ++st) {
    int bd = canon_bds_[st];
    int num_hole_card_pairs = Game::NumHoleCardPairs(st);
    for (int p = 0; p < num_players; ++p) {
      cards[0] = hi_cards_[p];
      cards[1] = lo_cards_[p];
      unsigned int hcp = HCPIndex(st, cards);
      unsigned int h = ((unsigned int)bd) * ((unsigned int)num_hole_card_pairs) + hcp;
      int b = buckets_.Bucket(st, h);
      hand_buckets_[p * (max_street + 1) + st] = b;
      
      if (st == max_street) {
	hvs_[p] = HandValueTree::Val(cards);
      }
    }
  }
  if (hvs_[1] > hvs_[0])      p1_outcome_ = 1;
  else if (hvs_[1] < hvs_[0]) p1_outcome_ = -1;
  else                        p1_outcome_ = 0;
}

void ECFRThread::Run(void) {
  int num_players = Game::NumPlayers();
  it_ = 1;
  while (1) {
    if (*total_its_ >= ((unsigned long long int)batch_size_) * num_threads_) {
      fprintf(stderr, "Thread %i performed %i iterations\n", thread_index_, it_);
      break;
    }
    Deal();
    if (it_ % 10000000 == 1 && thread_index_ == 0) {
      fprintf(stderr, "It %i\n", it_);
    }
    for (p_ = 0; p_ < num_players; ++p_) {
      Process(root_);
    }
    ++it_;
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
}

// Handle sumprob overflow.
double ECFRThread::Process(ECFRNode *node) {
  if (node->Terminal()) {
    int last_bet_to = node->LastBetTo();
    if (node->Showdown()) {
      if (p_ == 0) {
	return (double)-(p1_outcome_ * last_bet_to);
      } else {
	return (double)(p1_outcome_ * last_bet_to);
      }
    } else {
      // Fold node
      int player_remaining = node->PlayerActing();
      double val;
      if (player_remaining == p_) val = (double)last_bet_to;
      else                        val = (double)-last_bet_to;
      return val;
    }
  }
  int pa = node->PlayerActing();
  if (pa == p_) {
    // Our choice
    int num_succs = node->NumSuccs();
    int st = node->Street();
    int b = hand_buckets_[p_ * (Game::MaxStreet() + 1) + st];
    double *regrets = node->Regrets() + b * num_succs;

#ifdef FTL
    int max_s = 0;
    double max_regret = regrets[0];
    for (int s = 1; s < num_succs; ++s) {
      double r = regrets[s];
      if (r > max_regret) {
	max_regret = r;
	max_s = s;
      }
    }
#else
    unique_ptr<double []> probs(new double[num_succs]);
    RMProbs(regrets, probs.get(), num_succs);
#endif

    unique_ptr<double []> succ_vals(new double[num_succs]);
    for (int s = 0; s < num_succs; ++s) {
      // fprintf(stderr, "st %i our choice recursing on succ %i/%i\n", node->Street(), s, num_succs);
      succ_vals[s] = Process(node->IthSucc(s));
    }
    
#ifdef FTL
    double val = succ_vals[max_s];
#else
    double val = 0;
    for (int s = 0; s < num_succs; ++s) {
      val += succ_vals[s] * probs[s];
    }
#endif

    // Update regrets
    for (int s = 0; s < num_succs; ++s) {
      regrets[s] += succ_vals[s] - val;
    }
    return val;
  } else {
    // Opp choice
    int num_succs = node->NumSuccs();
    int st = node->Street();
    int b = hand_buckets_[pa * (Game::MaxStreet() + 1) + st];
    double *regrets = node->Regrets() + b * num_succs;
#ifdef FTL
    int max_s = 0;
    double max_regret = regrets[0];
    for (int s = 1; s < num_succs; ++s) {
      double r = regrets[s];
      if (r > max_regret) {
	max_regret = r;
	max_s = s;
      }
    }
    int s = max_s;
#else
    unique_ptr<double []> probs(new double[num_succs]);
    RMProbs(regrets, probs.get(), num_succs);
    double r;
    drand48_r(&rand_buf_, &r);
    // fprintf(stderr, "st %i r %f (opp)\n", st, r);
    double cum = 0;
    int s;
    for (s = 0; s < num_succs - 1; ++s) {
      cum += probs[s];
      if (r < cum) break;
    }
#endif
    
    int *sumprobs = node->Sumprobs() + b * num_succs;
    sumprobs[s] += 1;
    if (sumprobs[s] >= 2000000000) {
      for (int s = 0; s < num_succs; ++s) sumprobs[s] /= 2;
    }
    return Process(node->IthSucc(s));
  }
}

static void *thread_run(void *v_t) {
  ECFRThread *t = (ECFRThread *)v_t;
  t->Run();
  return NULL;
}

void ECFRThread::RunThread(void) {
  pthread_create(&pthread_id_, NULL, thread_run, this);
}

void ECFRThread::Join(void) {
  pthread_join(pthread_id_, NULL); 
}

void ECFR::ReadRegrets(ECFRNode *node, unique_ptr<Reader> *readers) {
  if (node->Terminal()) return;
  int max_street = Game::MaxStreet();
  int st = node->Street();
  int pa = node->PlayerActing();
  int num_succs = node->NumSuccs();
  int index = pa * (max_street + 1) + st;
  Reader *reader = readers[index].get();
  int num_buckets = buckets_.NumBuckets(st);
  double *regrets = node->Regrets();
  int num_values = num_buckets * num_succs;
  for (int i = 0; i < num_values; ++i) {
    regrets[i] = reader->ReadDoubleOrDie();
  }
  for (int s = 0; s < num_succs; ++s) {
    ReadRegrets(node->IthSucc(s), readers);
  }
}

void ECFR::ReadSumprobs(ECFRNode *node, unique_ptr<Reader> *readers) {
  if (node->Terminal()) return;
  int max_street = Game::MaxStreet();
  int st = node->Street();
  int pa = node->PlayerActing();
  int num_succs = node->NumSuccs();
  int index = pa * (max_street + 1) + st;
  Reader *reader = readers[index].get();
  int num_buckets = buckets_.NumBuckets(st);
  int *sumprobs = node->Sumprobs();
  int num_values = num_buckets * num_succs;
  for (int i = 0; i < num_values; ++i) {
    sumprobs[i] = reader->ReadIntOrDie();
  }
  for (int s = 0; s < num_succs; ++s) {
    ReadSumprobs(node->IthSucc(s), readers);
  }
}

void ECFR::Read(int batch_index) {
  char dir[500], buf[500];
  sprintf(dir, "%s/%s.%u.%s.%i.%i.%i.%s.%s", Files::OldCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), card_abstraction_.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(),
	  betting_abstraction_.BettingAbstractionName().c_str(),
	  cfr_config_.CFRConfigName().c_str());
  int num_players = Game::NumPlayers();
  int max_street = Game::MaxStreet();
  int num_readers = num_players * (max_street + 1);
  
  unique_ptr< unique_ptr<Reader> []> regret_readers(new unique_ptr<Reader> [num_readers]);
  for (int p = 0; p < num_players; ++p) {
    for (int st = 0; st <= max_street; ++st) {
      int index = p * (max_street + 1) + st;
      sprintf(buf, "%s/regrets.x.0.0.%u.%u.p%u.d", dir, st, batch_index, p);
      regret_readers[index].reset(new Reader(buf));
    }
  }
  ReadRegrets(root_.get(), regret_readers.get());
  
  unique_ptr< unique_ptr<Reader> []> sumprob_readers(new unique_ptr<Reader> [num_readers]);
  for (int p = 0; p < num_players; ++p) {
    for (int st = 0; st <= max_street; ++st) {
      int index = p * (max_street + 1) + st;
      sprintf(buf, "%s/sumprobs.x.0.0.%u.%u.p%u.i", dir, st, batch_index, p);
      sumprob_readers[index].reset(new Reader(buf));
    }
  }
  ReadSumprobs(root_.get(), sumprob_readers.get());
}

void ECFR::WriteRegrets(ECFRNode *node, unique_ptr<Writer> *writers) {
  if (node->Terminal()) return;
  int max_street = Game::MaxStreet();
  int st = node->Street();
  int pa = node->PlayerActing();
  int num_succs = node->NumSuccs();
  int index = pa * (max_street + 1) + st;
  Writer *writer = writers[index].get();
  int num_buckets = buckets_.NumBuckets(st);
  double *regrets = node->Regrets();
  int num_values = num_buckets * num_succs;
  for (int i = 0; i < num_values; ++i) {
    writer->WriteDouble(regrets[i]);
  }
  for (int s = 0; s < num_succs; ++s) {
    WriteRegrets(node->IthSucc(s), writers);
  }
}

void ECFR::WriteSumprobs(ECFRNode *node, unique_ptr<Writer> *writers) {
  if (node->Terminal()) return;
  int max_street = Game::MaxStreet();
  int st = node->Street();
  int pa = node->PlayerActing();
  int num_succs = node->NumSuccs();
  int index = pa * (max_street + 1) + st;
  Writer *writer = writers[index].get();
  int num_buckets = buckets_.NumBuckets(st);
  int *sumprobs = node->Sumprobs();
  int num_values = num_buckets * num_succs;
  for (int i = 0; i < num_values; ++i) {
    writer->WriteInt(sumprobs[i]);
  }
  for (int s = 0; s < num_succs; ++s) {
    WriteSumprobs(node->IthSucc(s), writers);
  }
}

void ECFR::Write(int batch_index) {
  char dir[500], buf[500];
  sprintf(dir, "%s/%s.%u.%s.%i.%i.%i.%s.%s", Files::NewCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), card_abstraction_.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(),
	  betting_abstraction_.BettingAbstractionName().c_str(), 
	  cfr_config_.CFRConfigName().c_str());
  Mkdir(dir);
  int num_players = Game::NumPlayers();
  int max_street = Game::MaxStreet();
  int num_writers = num_players * (max_street + 1);
  
  unique_ptr< unique_ptr<Writer> []> regret_writers(new unique_ptr<Writer> [num_writers]);
  for (int p = 0; p < num_players; ++p) {
    for (int st = 0; st <= max_street; ++st) {
      int index = p * (max_street + 1) + st;
      sprintf(buf, "%s/regrets.x.0.0.%u.%u.p%u.d", dir, st, batch_index, p);
      regret_writers[index].reset(new Writer(buf));
    }
  }
  WriteRegrets(root_.get(), regret_writers.get());
  
  unique_ptr< unique_ptr<Writer> []> sumprob_writers(new unique_ptr<Writer> [num_writers]);
  for (int p = 0; p < num_players; ++p) {
    for (int st = 0; st <= max_street; ++st) {
      int index = p * (max_street + 1) + st;
      sprintf(buf, "%s/sumprobs.x.0.0.%u.%u.p%u.i", dir, st, batch_index, p);
      sumprob_writers[index].reset(new Writer(buf));
    }
  }
  WriteSumprobs(root_.get(), sumprob_writers.get());
}

void ECFR::Run(void) {
  total_its_ = 0ULL;
  for (int i = 1; i < num_cfr_threads_; ++i) {
    cfr_threads_[i]->RunThread();
  }
  cfr_threads_[0]->Run();
  fprintf(stderr, "Finished main thread\n");
  for (int i = 1; i < num_cfr_threads_; ++i) {
    cfr_threads_[i]->Join();
    fprintf(stderr, "Joined thread %i\n", i);
  }
}

void ECFR::RunBatch(int batch_index, int batch_size) {
  srand48_r(batch_index, &rand_buf_);
  cfr_threads_.reset(new unique_ptr<ECFRThread> [num_cfr_threads_]);
  for (int t = 0; t < num_cfr_threads_; ++t) {
    // Temporary
#if 0
    double r;
    drand48_r(&rand_buf_, &r);
    int seed = r * 100000.0;
#endif
    int seed = 0;
    cfr_threads_[t].reset(new ECFRThread(cfr_config_, buckets_, root_.get(), seed, batch_size,
					  board_table_.get(), num_raw_boards_, &total_its_, t,
					  num_cfr_threads_));
  }

  fprintf(stderr, "Running batch %i\n", batch_index);
  Run();
  fprintf(stderr, "Finished running batch %i\n", batch_index);
}

void ECFR::Run(int start_batch_index, int end_batch_index, int batch_size, int save_interval) {
  if ((end_batch_index - start_batch_index) % save_interval != 0) {
    fprintf(stderr, "Batches to execute should be multiple of save interval\n");
    exit(-1);
  }
  if (start_batch_index > 0) Read(start_batch_index - 1);

  for (int batch_index = start_batch_index; batch_index < end_batch_index; ++batch_index) {
    RunBatch(batch_index, batch_size);
    // In general, save every save_interval batches.  The logic is a little messy.  If the save
    // interval is > 1, then we don't want to save at batch 0.  But we do if the save interval
    // is 1.
    if ((batch_index - start_batch_index) % save_interval == 0 &&
	(batch_index > 0 || save_interval == 1)) {
      Write(batch_index);
      fprintf(stderr, "Checkpointed batch index %i\n", batch_index);
    }
  }
}

static int Factorial(int n) {
  if (n == 0) return 1;
  if (n == 1) return 1;
  return n * Factorial(n - 1);
}

ECFR::ECFR(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
	     const Buckets &buckets, int num_threads) :
  card_abstraction_(ca), betting_abstraction_(ba), cfr_config_(cc), buckets_(buckets),
  num_cfr_threads_(num_threads) {
  BettingTrees betting_trees(betting_abstraction_);
  root_.reset(new ECFRNode(betting_trees.Root(), buckets_));
  
  BoardTree::Create();
  BoardTree::BuildBoardCounts(); // Will get rid of these below
  int max_street = Game::MaxStreet();
  num_raw_boards_ = 1;
  int num_remaining = Game::NumCardsInDeck();
  for (int st = 1; st <= max_street; ++st) {
    int num_street_cards = Game::NumCardsForStreet(st);
    int multiplier = 1;
    for (int n = (num_remaining - num_street_cards) + 1; n <= num_remaining; ++n) {
      multiplier *= n;
    }
    num_raw_boards_ *= multiplier / Factorial(num_street_cards);
    num_remaining -= num_street_cards;
  }
  board_table_.reset(new int[num_raw_boards_]);
  int num_boards = BoardTree::NumBoards(max_street);
  int i = 0;
  for (int bd = 0; bd < num_boards; ++bd) {
    int ct = BoardTree::BoardCount(max_street, bd);
    for (int j = 0; j < ct; ++j) {
      board_table_[i++] = bd;
    }
  }
  if (i != num_raw_boards_) {
    fprintf(stderr, "Num raw board mismatch: %u, %u\n", i, num_raw_boards_);
    exit(-1);
  }
  BoardTree::DeleteBoardCounts();

  BoardTree::BuildPredBoards();
  HandValueTree::Create();
}
