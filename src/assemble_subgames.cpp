// We assume no nested subgame solving, and subgames are always solved at
// street-initial nodes.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <string>
#include <vector>

#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_trees.h"
#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "cfr_config.h"
#include "cfr_params.h"
#include "cfr_utils.h"
#include "cfr_values.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "io.h"
#include "params.h"
#include "resolving_method.h"
#include "split.h"
#include "subgame_utils.h" // ReadSubgame()

using std::string;
using std::unique_ptr;
using std::vector;

class Assembler {
public:
  Assembler(const BettingTrees &base_betting_trees, const BettingTrees &subgame_betting_trees,
	    int solve_street, int base_it, int subgame_it, const CardAbstraction &base_ca,
	    const CardAbstraction &subgame_ca, const CardAbstraction &merged_ca,
	    const BettingAbstraction &base_ba, const BettingAbstraction &subgame_ba,
	    const CFRConfig &base_cc, const CFRConfig &subgame_cc, const CFRConfig &merged_cc,
	    ResolvingMethod method);
  Assembler(void);
  ~Assembler(void);
  void Go(void);
private:
  void WalkTrunk(Node *base_node, Node *subgame_node, const string &action_sequence, int last_st);

  bool asymmetric_;
  const BettingTrees &base_betting_trees_;
  const BettingTrees &subgame_betting_trees_;
  int solve_street_;
  int base_it_;
  int subgame_it_;
  const CardAbstraction &base_ca_;
  const CardAbstraction &subgame_ca_;
  const CardAbstraction &merged_ca_;
  const BettingAbstraction &base_ba_;
  const BettingAbstraction &subgame_ba_;
  const CFRConfig &base_cc_;
  const CFRConfig &subgame_cc_;
  const CFRConfig &merged_cc_;
  ResolvingMethod method_;
  unique_ptr<CFRValues> base_sumprobs_;
  unique_ptr<CFRValues> merged_sumprobs_;
  unique_ptr<Buckets> subgame_buckets_;
};

Assembler::Assembler(const BettingTrees &base_betting_trees,
		     const BettingTrees &subgame_betting_trees, int solve_street, int base_it,
		     int subgame_it, const CardAbstraction &base_ca,
		     const CardAbstraction &subgame_ca, const CardAbstraction &merged_ca,
		     const BettingAbstraction &base_ba, const BettingAbstraction &subgame_ba,
		     const CFRConfig &base_cc, const CFRConfig &subgame_cc,
		     const CFRConfig &merged_cc, ResolvingMethod method) :
  base_betting_trees_(base_betting_trees), subgame_betting_trees_(subgame_betting_trees),
  base_ca_(base_ca), subgame_ca_(subgame_ca), merged_ca_(merged_ca), base_ba_(base_ba),
  subgame_ba_(subgame_ba), base_cc_(base_cc), subgame_cc_(subgame_cc), merged_cc_(merged_cc) {
  asymmetric_ = false;
  solve_street_ = solve_street;
  base_it_ = base_it;
  subgame_it_ = subgame_it;
  method_ = method;

  DeleteOldFiles(merged_ca_, subgame_ba_.BettingAbstractionName(), merged_cc_, subgame_it_);

  subgame_buckets_.reset(new Buckets(subgame_ca_, true));
  
  int max_street = Game::MaxStreet();
  unique_ptr<bool []> base_streets(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) base_streets[st] = st < solve_street_;
  Buckets base_buckets(base_ca_, true);

  unique_ptr<bool []> base_compressed_streets(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    base_compressed_streets[st] = false;
  }
  const vector<int> &bcsv = base_cc_.CompressedStreets();
  int num_bcsv = bcsv.size();
  for (int i = 0; i < num_bcsv; ++i) {
    int st = bcsv[i];
    base_compressed_streets[st] = true;
  }

  CFRValues base_sumprobs(nullptr, base_streets.get(), 0, 0, base_buckets, base_betting_trees_);
  
  char read_dir[500], write_dir[500];
  sprintf(read_dir, "%s/%s.%i.%s.%i.%i.%i.%s.%s", Files::OldCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), base_ca.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(), base_ba_.BettingAbstractionName().c_str(),
	  base_cc_.CFRConfigName().c_str());
  base_sumprobs.Read(read_dir, base_it, base_betting_trees_.GetBettingTree(), "x", -1, true, false);
  sprintf(write_dir, "%s/%s.%i.%s.%i.%i.%i.%s.%s", Files::NewCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), merged_ca_.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(), subgame_ba_.BettingAbstractionName().c_str(),
	  merged_cc_.CFRConfigName().c_str());
  base_sumprobs.Write(write_dir, subgame_it_, base_betting_trees_.Root(), "x", -1, true);

  unique_ptr<bool []> subgame_streets(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    subgame_streets[st] = st >= solve_street_;
  }
  Buckets merged_buckets(merged_ca_, true);

  unique_ptr<bool []> merged_compressed_streets(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    merged_compressed_streets[st] = false;
  }
  const vector<int> &mcsv = merged_cc_.CompressedStreets();
  int num_mcsv = mcsv.size();
  for (int i = 0; i < num_mcsv; ++i) {
    int st = mcsv[i];
    merged_compressed_streets[st] = true;
  }

  merged_sumprobs_.reset(new CFRValues(nullptr, subgame_streets.get(), 0, 0, merged_buckets,
				       subgame_betting_trees_));
  for (int st = 0; st <= max_street; ++st) {
    if (subgame_streets[st]) merged_sumprobs_->CreateStreetValues(st, CFRValueType::CFR_DOUBLE,
								  false);
  }
}

Assembler::~Assembler(void) {
}

// When we get to the target street, read the entire base strategy for
// this subtree into merged sumprobs.  Then go through and override parts
// with the subgame strategy.
void Assembler::WalkTrunk(Node *base_node, Node *subgame_node, const string &action_sequence,
			  int last_st) {
  if (base_node->Terminal()) return;
  int st = base_node->Street();
  if (st == solve_street_) {
    int num_boards = BoardTree::NumBoards(st);
    int base_subtree_nt = base_node->NonterminalID();
    fprintf(stderr, "Base subtree NT: %i lbt %i\n", base_subtree_nt, base_node->LastBetTo());
    unique_ptr<BettingTrees> subtrees(new BettingTrees(subgame_node));
#if 0
    int max_street = Game::MaxStreet();
    unique_ptr<bool []> subgame_streets(new bool[max_street + 1]);
    for (int st = 0; st <= max_street; ++st) subgame_streets[st] = st >= solve_street_;
    unique_ptr<bool []> subgame_compressed_streets(new bool[max_street + 1]);
    for (int st = 0; st <= max_street; ++st) subgame_compressed_streets[st] = false;
    const vector<int> &ecsv = subgame_cc_.CompressedStreets();
    int num_ecsv = ecsv.size();
    for (int i = 0; i < num_ecsv; ++i) {
      int st = ecsv[i];
      subgame_compressed_streets[st] = true;
    }
#endif
    for (int gbd = 0; gbd < num_boards; ++gbd) {
      unique_ptr<CFRValues> subgame_sumprobs =
	ReadSubgame(action_sequence, subtrees.get(), gbd, base_ca_, subgame_ca_, base_ba_,
		    subgame_ba_, base_cc_, subgame_cc_, *subgame_buckets_.get(), method_, st, gbd,
		    -1);
      merged_sumprobs_->MergeInto(*subgame_sumprobs.get(), gbd, subgame_node, subtrees->Root(),
				  *subgame_buckets_, Game::MaxStreet());
    }
    return;
  }
  int num_succs = base_node->NumSuccs();
  for (int s = 0; s < num_succs; ++s) {
    string action = base_node->ActionName(s);
    WalkTrunk(base_node->IthSucc(s), subgame_node->IthSucc(s), action_sequence + action, st);
  }
}

void Assembler::Go(void) {
  WalkTrunk(base_betting_trees_.Root(), subgame_betting_trees_.Root(), "x",
	    base_betting_trees_.Root()->Street());
  char dir[500];
  sprintf(dir, "%s/%s.%i.%s.%i.%i.%i.%s.%s", Files::NewCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), merged_ca_.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(), subgame_ba_.BettingAbstractionName().c_str(),
	  merged_cc_.CFRConfigName().c_str());
  merged_sumprobs_->Write(dir, subgame_it_, subgame_betting_trees_.Root(), "x", -1, true);
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <base card params> "
	  "<subgame card params> <merged card params> <base betting params> "
	  "<subgame betting params> <base CFR params> <subgame CFR params> "
	  "<merged CFR params> <solve street> <base it> <subgame it> "
	  "<method>\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 14) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unique_ptr<Params> base_card_params = CreateCardAbstractionParams();
  base_card_params->ReadFromFile(argv[2]);
  unique_ptr<CardAbstraction>
    base_card_abstraction(new CardAbstraction(*base_card_params));
  unique_ptr<Params> subgame_card_params = CreateCardAbstractionParams();
  subgame_card_params->ReadFromFile(argv[3]);
  unique_ptr<CardAbstraction>
    subgame_card_abstraction(new CardAbstraction(*subgame_card_params));
  unique_ptr<Params> merged_card_params = CreateCardAbstractionParams();
  merged_card_params->ReadFromFile(argv[4]);
  unique_ptr<CardAbstraction>
    merged_card_abstraction(new CardAbstraction(*merged_card_params));
  unique_ptr<Params> base_betting_params = CreateBettingAbstractionParams();
  base_betting_params->ReadFromFile(argv[5]);
  unique_ptr<BettingAbstraction>
    base_betting_abstraction(new BettingAbstraction(*base_betting_params));
  unique_ptr<Params> subgame_betting_params = CreateBettingAbstractionParams();
  subgame_betting_params->ReadFromFile(argv[6]);
  unique_ptr<BettingAbstraction>
    subgame_betting_abstraction(
		    new BettingAbstraction(*subgame_betting_params));
  unique_ptr<Params> base_cfr_params = CreateCFRParams();
  base_cfr_params->ReadFromFile(argv[7]);
  unique_ptr<CFRConfig>
    base_cfr_config(new CFRConfig(*base_cfr_params));
  unique_ptr<Params> subgame_cfr_params = CreateCFRParams();
  subgame_cfr_params->ReadFromFile(argv[8]);
  unique_ptr<CFRConfig>
    subgame_cfr_config(new CFRConfig(*subgame_cfr_params));
  unique_ptr<Params> merged_cfr_params = CreateCFRParams();
  merged_cfr_params->ReadFromFile(argv[9]);
  unique_ptr<CFRConfig>
    merged_cfr_config(new CFRConfig(*merged_cfr_params));

  int solve_street, base_it, subgame_it;
  if (sscanf(argv[10], "%i", &solve_street) != 1)  Usage(argv[0]);
  if (sscanf(argv[11], "%i", &base_it) != 1)       Usage(argv[0]);
  if (sscanf(argv[12], "%i", &subgame_it) != 1)    Usage(argv[0]);
  string m = argv[13];
  ResolvingMethod method;
  if (m == "unsafe")         method = ResolvingMethod::UNSAFE;
  else if (m == "cfrd")      method = ResolvingMethod::CFRD;
  else if (m == "maxmargin") method = ResolvingMethod::MAXMARGIN;
  else if (m == "combined")  method = ResolvingMethod::COMBINED;
  else                       Usage(argv[0]);

  BettingTrees base_betting_trees(*base_betting_abstraction);
  BettingTrees subgame_betting_trees(*subgame_betting_abstraction);
  BoardTree::Create();

  Assembler assembler(base_betting_trees, subgame_betting_trees, solve_street, base_it, subgame_it,
		      *base_card_abstraction, *subgame_card_abstraction, *merged_card_abstraction,
		      *base_betting_abstraction, *subgame_betting_abstraction, *base_cfr_config,
		      *subgame_cfr_config, *merged_cfr_config, method);
  assembler.Go();
}
