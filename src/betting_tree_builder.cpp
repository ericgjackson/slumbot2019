#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "betting_abstraction.h"
#include "betting_tree.h"
#include "betting_tree_builder.h"
#include "files.h"
#include "game.h"
#include "io.h"

using std::shared_ptr;
using std::unordered_map;
using std::vector;

void BettingTreeBuilder::Build(void) {
  int terminal_id = 0;

#if 0
  if (betting_abstraction_.Limit()) {
    root_ = CreateLimitTree(&terminal_id);
  } else {
    if (betting_abstraction_.NoLimitTreeType() == 0) {
    } else if (betting_abstraction_.NoLimitTreeType() == 1) {
      root_ = CreateNoLimitTree1(target_player_, &terminal_id);
    } else if (betting_abstraction_.NoLimitTreeType() == 2) {
      root_ = CreateNoLimitTree2(target_player_, &terminal_id);
    }
  }
#endif
  // Used to do this for 2-person games
  // root_ = CreateNoLimitTree1(target_player_, &terminal_id);
  // Used to do this only for games with 3 or more players
  root_ = CreateMPTree(target_player_, &terminal_id);
  num_terminals_ = terminal_id;
}

// To handle reentrant trees we keep a boolean array of nodes that have
// already been visited.  Note that even if a node has already been visited,
// we still write out the properties of the node (ID, flags, pot size, etc.).
// But we prevent ourselves from redundantly writing out the subtree below
// the node more than once.
void BettingTreeBuilder::Write(Node *node, vector< vector<int> > *num_nonterminals,
			       Writer *writer) {
  int st = node->Street();
  int id = node->ID();
  int pa = node->PlayerActing();
  bool nt_first_seen = (id == -1);
  // Assign IDs during writing
  if (nt_first_seen) {
    id = (*num_nonterminals)[pa][st]++;
    node->SetNonterminalID(id);
  }
  writer->WriteUnsignedInt(id);
  writer->WriteUnsignedShort(node->LastBetTo());
  writer->WriteUnsignedShort(node->NumSuccs());
  writer->WriteUnsignedShort(node->Flags());
  writer->WriteUnsignedChar(pa);
  writer->WriteUnsignedChar(node->NumRemaining());
  if (node->Terminal()) {
    return;
  }
  if (! nt_first_seen) return;
  int num_succs = node->NumSuccs();
  for (int s = 0; s < num_succs; ++s) {
    Write(node->IthSucc(s), num_nonterminals, writer);
  }
}

void BettingTreeBuilder::Write(void) {
  char buf[500];
  if (asymmetric_) {
    sprintf(buf, "%s/betting_tree.%s.%u.%s.%u", Files::StaticBase(),
	    Game::GameName().c_str(), Game::NumPlayers(),
	    betting_abstraction_.BettingAbstractionName().c_str(),
	    target_player_);
  } else {
    sprintf(buf, "%s/betting_tree.%s.%u.%s", Files::StaticBase(),
	    Game::GameName().c_str(), Game::NumPlayers(),
	    betting_abstraction_.BettingAbstractionName().c_str());
  }
  
  int max_street = Game::MaxStreet();
  int num_players = Game::NumPlayers();
  vector< vector<int> > num_nonterminals(num_players);
  for (int pa = 0; pa < num_players; ++pa) {
    num_nonterminals[pa].resize(max_street + 1);
    for (int st = 0; st <= max_street; ++st) {
      num_nonterminals[pa][st] = 0;
    }
  }
  
  Writer writer(buf);
  Write(root_.get(), &num_nonterminals, &writer);
  for (int st = 0; st <= max_street; ++st) {
    int sum = 0;
    for (int pa = 0; pa < num_players; ++pa) {
      sum += num_nonterminals[pa][st];
    }
    fprintf(stderr, "St %u num nonterminals %u\n", st, sum);
  }
}


void BettingTreeBuilder::Initialize(void) {
  initial_street_ = betting_abstraction_.InitialStreet();
  stack_size_ = betting_abstraction_.StackSize();
  all_in_pot_size_ = 2 * stack_size_;
  min_bet_ = betting_abstraction_.MinBet();

  root_ = NULL;
  num_terminals_ = 0;
}

BettingTreeBuilder::BettingTreeBuilder(const BettingAbstraction &ba) :
  betting_abstraction_(ba) {
  asymmetric_ = false;
  // Parameter should be ignored for symmetric trees.
  target_player_ = -1;
  node_map_.reset(new unordered_map< unsigned long long int, shared_ptr<Node> >);
    
  Initialize();
}

BettingTreeBuilder::BettingTreeBuilder(const BettingAbstraction &ba, int target_player) :
  betting_abstraction_(ba) {
  asymmetric_ = true;
  target_player_ = target_player;
  Initialize();
}
