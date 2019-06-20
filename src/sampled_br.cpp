// This is unfinished.  It may not be needed.
// Why is this better than just running CFR for a while with n-1/n players' strategies fixed?
// Do not support imperfect recall abstractions.

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

class SampledBR {
public:
  SampledBR(const BettingAbstraction &ba, const CardAbstraction &target_ca,
	    const CardAbstraction &responder_ca, const CFRConfig &cc, int it,
	    int responder_p);
  void Go(long long int num_samples, bool deterministic);
private:
  void AllocateCVs(Node *node);
  void DealNCards(Card *cards, int n);
  void SetHCPsAndBoards(Card **raw_hole_cards, const Card *raw_board);
  long long int Round(double d);
  void Terminal(Node *node, int *contributions, int last_bet_to, bool *folded, int num_remaining,
		int last_player_acting, int last_st);
  void RecurseOnSucc(Node *node, int s, int actual_pa, int *contributions, int last_bet_to,
		     bool *folded, int num_remaining, int last_player_acting);
  void Nonterminal(Node *node, int *contributions, int last_bet_to, bool *folded,
		   int num_remaining, int last_player_acting, int last_st);
  void Play(Node *node, int *contributions, int last_bet_to, bool *folded, int num_remaining,
	    int last_player_acting, int last_st);
  void PlayHand(const Card *cards, bool deterministic);
  double *ChooseBestSuccs(Node *node);
  void ChooseBestSuccs(void);
  
  int num_players_;
  int responder_p_;
  bool asymmetric_;
  BettingTree *betting_tree_;
  const Buckets *target_buckets_;
  const Buckets *responder_buckets_;
  CFRValues **probs_;
  unique_ptr<int []> boards_;
  int **raw_hcps_;
  unsigned short **sorted_hcps_;
  unique_ptr<int []> hvs_;
  struct drand48_data rand_buf_;
  // Index by terminal ID and bucket
  long long int **sum_terminal_cvs_;
  long long int **num_terminal_cvs_;
  unsigned char ***best_succs_;
};

// Doesn't handle reentrant trees currently
void SampledBR::AllocateCVs(Node *node) {
  if (node->Terminal()) {
    int st = node->Street();
    int num_buckets = responder_buckets_->NumBuckets(st);
    int tid = node->TerminalID();
    sum_terminal_cvs_[tid] = new long long int[num_buckets];
    num_terminal_cvs_[tid] = new long long int[num_buckets];
    for (int b = 0; b < num_buckets; ++b) {
      sum_terminal_cvs_[tid][b] = 0;
      num_terminal_cvs_[tid][b] = 0;
    }
    return;
  }
  int num_succs = node->NumSuccs();
  for (int s = 0; s < num_succs; ++s) {
    AllocateCVs(node->IthSucc(s));
  }
}

SampledBR::SampledBR(const BettingAbstraction &ba, const CardAbstraction &target_ca,
		     const CardAbstraction &responder_ca, const CFRConfig &cc, int it,
		     int responder_p) {
  responder_p_ = responder_p;
  target_buckets_ = new Buckets(target_ca, false);
  if (strcmp(target_ca.CardAbstractionName().c_str(), responder_ca.CardAbstractionName().c_str())) {
    responder_buckets_ = new Buckets(responder_ca, false);
  } else {
    responder_buckets_ = target_buckets_;
  }
  num_players_ = Game::NumPlayers();
  asymmetric_ = ba.Asymmetric();
  BoardTree::Create();
  BoardTree::CreateLookup();
  if (asymmetric_) {
    fprintf(stderr, "asymmetric not supported currently\n");
    exit(-1);
#if 0
    for (int asym_p = 0; asym_p < num_players_; ++asym_p) {
      betting_trees_[asym_p] = BettingTree::BuildAsymmetricTree(ba, asym_p);
    }
#endif
  } else {
    betting_tree_ = new BettingTree(ba);
#if 0
    for (int asym_p = 1; asym_p < num_players_; ++asym_p) {
      betting_trees_[asym_p] = betting_trees_[0];
    }
#endif
  }

  probs_ = new CFRValues *[num_players_];
  for (int p = 0; p < num_players_; ++p) {
    if (p == responder_p_) {
      probs_[p] = nullptr;
      continue;
    }
    unique_ptr<bool []> players(new bool[num_players_]);
    for (int p1 = 0; p1 < num_players_; ++p1) players[p1] = (p1 == p);
    probs_[p] = new CFRValues(players.get(), nullptr, 0, 0, *target_buckets_, betting_tree_);
  }

  char dir[500], buf[100];
  for (int p = 0; p < num_players_; ++p) {
    if (p == responder_p_) continue;
    sprintf(dir, "%s/%s.%u.%s.%u.%u.%u.%s.%s", Files::OldCFRBase(), Game::GameName().c_str(),
	    Game::NumPlayers(), target_ca.CardAbstractionName().c_str(), Game::NumRanks(),
	    Game::NumSuits(), Game::MaxStreet(), ba.BettingAbstractionName().c_str(),
	    cc.CFRConfigName().c_str());
    if (asymmetric_) {
      sprintf(buf, ".p%u", p);
      strcat(dir, buf);
    }
    probs_[p]->Read(dir, it, betting_tree_, "x", -1, true, false);
  }

  int max_street = Game::MaxStreet();
  boards_.reset(new int[max_street + 1]);
  boards_[0] = 0;
  raw_hcps_ = new int *[num_players_];
  for (int p = 0; p < num_players_; ++p) {
    raw_hcps_[p] = new int[max_street + 1];
  }
  hvs_.reset(new int[num_players_]);

  if (target_buckets_->None(max_street) || responder_buckets_->None(max_street)) {
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
  } else {
    sorted_hcps_ = nullptr;
  }

  int num_terminals = betting_tree_->NumTerminals();
  // Indexed by terminal ID and bucket
  sum_terminal_cvs_ = new long long int *[num_terminals];
  num_terminal_cvs_ = new long long int *[num_terminals];
  AllocateCVs(betting_tree_->Root());
  best_succs_ = new unsigned char **[max_street + 1];
  for (int st = 0; st <= max_street; ++st) {
    int num_nt = betting_tree_->NumNonterminals(st, responder_p_);
    int num_buckets = responder_buckets_->NumBuckets(st);
    best_succs_[st] = new unsigned char *[num_nt];
    for (int i = 0; i < num_nt; ++i) {
      best_succs_[st][i] = new unsigned char[num_buckets];
      for (int b = 0; b < num_buckets; ++b) {
	best_succs_[st][i][b] = 255;
      }
    }
  }
}

long long int SampledBR::Round(double d) {
  double rnd;
  drand48_r(&rand_buf_, &rnd);
  if (d < 0) {
    long long int below = d;
    double rem = below - d;
    if (rnd < rem) {
      return below - 1;
    } else {
      return below;
    }
  } else {
    long long int below = d;
    double rem = d - below;
    if (rnd < rem) {
      return below + 1;
    } else {
      return below;
    }
  }
}

// I don't understand when to use actual_pa and when to use responder_p_ and why
// node->PlayerActing() cannot be used.
void SampledBR::Terminal(Node *node, int *contributions, int last_bet_to, bool *folded,
			 int num_remaining, int last_player_acting, int last_st) {
  // fprintf(stderr, "Terminal\n");
  int st = node->Street();
  // Use unsigned ints here so that h can be beyond the maximum value of an int
  unsigned int bd = boards_[st];
  unsigned int raw_hcp = raw_hcps_[responder_p_][st];
  unsigned int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  unsigned int h = bd * num_hole_card_pairs + raw_hcp;
  int b = responder_buckets_->Bucket(st, h);
  int tid = node->TerminalID();
  
  if (num_remaining == 1) {
    // fprintf(stderr, "1 remaining\n");
    int sum_other_contributions = 0;
    int remaining_p = -1;
    for (int p = 0; p < num_players_; ++p) {
      if (folded[p]) {
	sum_other_contributions += contributions[p];
      } else {
	remaining_p = p;
      }
    }
    // This should be a node where responder_p_ is the only remaining player
    if (remaining_p != responder_p_) {
      fprintf(stderr, "remaining_p != responder_p_\n");
      exit(-1);
    }
    sum_terminal_cvs_[tid][b] += sum_other_contributions;
    ++num_terminal_cvs_[tid][b];
  } else {
    // fprintf(stderr, "Showdown num_rem %i\n", num_remaining);
    // fprintf(stderr, "tid %i\n", node->TerminalID());
    // Showdown
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
    // fprintf(stderr, "pot_size %i\n", pot_size);
    
    // Determine if we won, the number of winners, and the total contribution
    // of all winners.
    int num_winners = 0;
    int winner_contributions = 0;
    bool responder_is_winner = false;
    for (int p = 0; p < num_players_; ++p) {
      if (! folded[p] && hvs_[p] == best_hv) {
	if (p == responder_p_) responder_is_winner = true;
	++num_winners;
	winner_contributions += contributions[p];
      }
    }
    // fprintf(stderr, "num_winners %i\n", num_winners);
    // fprintf(stderr, "winner_contributions %i\n", winner_contributions);
    // fprintf(stderr, "responder_is_winner %i\n", (int)responder_is_winner);

    double d_cv;
    if (responder_is_winner) {
      d_cv = ((double)(pot_size - winner_contributions)) / ((double)num_winners);
    } else {
      d_cv = -(int)contributions[responder_p_];
    }
    // There's probably a rule for how to chop
    long long int lli_cv = Round(d_cv);
    // fprintf(stderr, "lli_cv %lli\n", lli_cv);
    // exit(0);
    sum_terminal_cvs_[tid][b] += lli_cv;
    ++num_terminal_cvs_[tid][b];
  }
}

void SampledBR::RecurseOnSucc(Node *node, int s, int actual_pa, int *contributions, int last_bet_to,
			      bool *folded, int num_remaining, int last_player_acting) {
  int st = node->Street();
  if (s == node->CallSuccIndex()) {
#if 0
    unique_ptr<Node * []> succ_nodes(new Node *[num_players_]);
    for (int p = 0; p < num_players_; ++p) {
      int csi = nodes[p]->CallSuccIndex();
      succ_nodes[p] = nodes[p]->IthSucc(csi);
    }
#endif
    contributions[actual_pa] = last_bet_to;
    Play(node->IthSucc(s), contributions, last_bet_to, folded, num_remaining, actual_pa, st);
  } else if (s == node->FoldSuccIndex()) {
#if 0
    unique_ptr<Node * []> succ_nodes(new Node *[num_players_]);
    for (int p = 0; p < num_players_; ++p) {
      int fsi = nodes[p]->FoldSuccIndex();
      succ_nodes[p] = nodes[p]->IthSucc(fsi);
    }
#endif
    folded[actual_pa] = true;
    Play(node->IthSucc(s), contributions, last_bet_to, folded, num_remaining - 1, actual_pa, st);
  } else {
    Node *my_succ = node->IthSucc(s);
    int new_bet_to = my_succ->LastBetTo();
    contributions[actual_pa] = new_bet_to;
#if 0
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
#endif
    Play(node->IthSucc(s), contributions, new_bet_to, folded, num_remaining, actual_pa, st);
  }
}

// What's the distinction between orig_pa and actual_pa?
void SampledBR::Nonterminal(Node *node, int *contributions, int last_bet_to, bool *folded,
			    int num_remaining, int last_player_acting, int last_st) {
  int node_pa = node->PlayerActing();
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
  if (actual_pa != node_pa) {
    fprintf(stderr, "Nonterminal() pa mismatch: %i %i\n", actual_pa, node_pa);
    exit(-1);
  }

  if (actual_pa == responder_p_) {
    for (int s = 0; s < num_succs; ++s) {
      // No need to recurse on fold succ.  1) We don't need to estimate CVs at terminal fold
      // nodes for responder; 2) we are not estimating CVs for any other player so no point
      // recursing for that reason either.
      if (s == node->FoldSuccIndex()) continue;
      RecurseOnSucc(node, s, actual_pa, contributions, last_bet_to, folded, num_remaining,
		    last_player_acting);
    }
  } else {
    int dsi = node->DefaultSuccIndex();
    int bd = boards_[st];
    int raw_hcp = raw_hcps_[actual_pa][st];
    int num_hole_card_pairs = Game::NumHoleCardPairs(st);
    int offset;
    // If card abstraction, hcp on river should be raw.  If no card
    // abstraction, hcp on river should be sorted.  Right?
    if (target_buckets_->None(st)) {
      int hcp = st == Game::MaxStreet() ? sorted_hcps_[bd][raw_hcp] : raw_hcp;
      offset = bd * num_hole_card_pairs * num_succs + hcp * num_succs;
    } else {
      unsigned int h = ((unsigned int)bd) * ((unsigned int)num_hole_card_pairs) + raw_hcp;
      int b = target_buckets_->Bucket(st, h);
      offset = b * num_succs;
    }
    double r;
    drand48_r(&rand_buf_, &r);
    unique_ptr<double []> probs(new double[num_succs]);
    probs_[node_pa]->RMProbs(st, node_pa, nt, offset, num_succs, dsi, probs.get());
    double cum = 0;
    int s;
    for (s = 0; s < num_succs - 1; ++s) {
      // Look up probabilities with orig_pa which may be different from
      // actual_pa.
      double prob = probs[s];
      cum += prob;
      if (r < cum) break;
    }
    RecurseOnSucc(node, s, actual_pa, contributions, last_bet_to, folded, num_remaining,
		  last_player_acting);
  }
}

void SampledBR::Play(Node *node, int *contributions, int last_bet_to, bool *folded,
		     int num_remaining, int last_player_acting, int last_st) {
  if (node->Terminal()) {
    Terminal(node, contributions, last_bet_to, folded, num_remaining, last_player_acting, last_st);
  } else {
    Nonterminal(node, contributions, last_bet_to, folded, num_remaining, last_player_acting,
		last_st);
  }
}

static int PrecedingPlayer(int p) {
  if (p == 0) return Game::NumPlayers() - 1;
  else        return p - 1;
}

void SampledBR::PlayHand(const Card *cards, bool deterministic) {
  unique_ptr<int []> contributions(new int[num_players_]);
  unique_ptr<bool []> folded(new bool[num_players_]);
  // Assume the big blind is last to act preflop
  // Assume the small blind is prior to the big blind
  int big_blind_p = PrecedingPlayer(Game::FirstToAct(0));
  int small_blind_p = PrecedingPlayer(big_blind_p);

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

#if 0
  unique_ptr<Node * []> nodes(new Node *[num_players_]);
  for (int p = 0; p < num_players_; ++p) {
    nodes[p] = betting_trees_[p]->Root();
  }
#endif
  
  Play(betting_tree_->Root(), contributions.get(), Game::BigBlind(), folded.get(), num_players_,
       1000, -1);
}

void SampledBR::DealNCards(Card *cards, int n) {
  int max_card = Game::MaxCard();
  for (int i = 0; i < n; ++i) {
    Card c;
    while (true) {
      // c = RandBetween(0, max_card);
      double r;
      drand48_r(&rand_buf_, &r);
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

void SampledBR::SetHCPsAndBoards(Card **raw_hole_cards, const Card *raw_board) {
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

// Assume perfect recall abstraction.
// Need to do something at street-initial nodes.
double *SampledBR::ChooseBestSuccs(Node *node) {
  int st = node->Street();
  int num_buckets = responder_buckets_->NumBuckets(st);
  double *cvs = new double[num_buckets];
  if (node->Terminal()) {
    int tid = node->TerminalID();
    for (int b = 0; b < num_buckets; ++b) {
      long long int num = num_terminal_cvs_[tid][b];
      if (num == 0LL) {
	cvs[b] = 0;
      } else {
	long long int sum = sum_terminal_cvs_[tid][b];
	cvs[b] = ((double)sum) / (double)num;
      }
    }
    return cvs;
  } else {
    int nt = node->NonterminalID();
    int num_succs = node->NumSuccs();
    unique_ptr<double * []> succ_cvs(new double *[num_succs]);
    for (int s = 0; s < num_succs; ++s) {
      succ_cvs[s] = ChooseBestSuccs(node->IthSucc(s));
    }
    double *cvs = new double[num_buckets];
    for (int b = 0; b < num_buckets; ++b) {
      double max_cv = succ_cvs[0][b];
      int max_s = 0;
      for (int s = 1; s < num_succs; ++s) {
	double cv = succ_cvs[s][b];
	if (cv > max_cv) {
	  max_cv = cv;
	  max_s = s;
	}
      }
      cvs[b] = max_cv;
      best_succs_[st][nt][b] = (unsigned char)max_s;
    }
    for (int s = 0; s < num_succs; ++s) {
      delete [] succ_cvs[s];
    }
    return cvs;
  }
}

void SampledBR::ChooseBestSuccs(void) {
  double *root_cvs = ChooseBestSuccs(betting_tree_->Root());
  delete [] root_cvs;
}

void SampledBR::Go(long long int num_samples, bool deterministic) {
  int max_street = Game::MaxStreet();
  int num_board_cards = Game::NumBoardCards(max_street);
  Card cards[100], hand_cards[7];
  Card **hole_cards = new Card *[num_players_];
  
  if (deterministic) {
    fprintf(stderr, "deterministic not supported yet\n");
    exit(-1);
  }
  if (! deterministic) {
    srand48_r(time(0), &rand_buf_);
  }

  for (long long int i = 0; i < num_samples; ++i) {
    // Assume 2 hole cards
    DealNCards(cards, num_board_cards + 2 * num_players_);
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
    
    PlayHand(cards, deterministic);
  }
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <target card params> <responder card params> "
	  "<betting abstraction params> <CFR params> <it> <num samples> [determ|nondeterm]\n",
	  prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 9) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unique_ptr<Params> target_card_params = CreateCardAbstractionParams();
  target_card_params->ReadFromFile(argv[2]);
  unique_ptr<CardAbstraction>
    target_card_abstraction(new CardAbstraction(*target_card_params));
  unique_ptr<Params> responder_card_params = CreateCardAbstractionParams();
  responder_card_params->ReadFromFile(argv[3]);
  unique_ptr<CardAbstraction>
    responder_card_abstraction(new CardAbstraction(*responder_card_params));
  unique_ptr<Params> betting_params = CreateBettingAbstractionParams();
  betting_params->ReadFromFile(argv[4]);
  unique_ptr<BettingAbstraction>
    betting_abstraction(new BettingAbstraction(*betting_params));
  unique_ptr<Params> cfr_params = CreateCFRParams();
  cfr_params->ReadFromFile(argv[5]);
  unique_ptr<CFRConfig>
    cfr_config(new CFRConfig(*cfr_params));

  int it;
  if (sscanf(argv[6], "%i", &it) != 1) Usage(argv[0]);
  long long int num_samples;
  if (sscanf(argv[7], "%lli", &num_samples) != 1) Usage(argv[0]);
  string darg = argv[8];
  bool deterministic;
  if (darg == "determ")         deterministic = true;
  else if (darg == "nondeterm") deterministic = false;
  else                          Usage(argv[0]);

  if (deterministic) {
    fprintf(stderr, "WARNING: have seen inaccurate results with determ\n");
    exit(-1);
  }
  
  HandValueTree::Create();

  int responder_p = 0;
  SampledBR sampled_br(*betting_abstraction, *target_card_abstraction, *responder_card_abstraction,
		       *cfr_config, it, responder_p);
  sampled_br.Go(num_samples, deterministic);
}
