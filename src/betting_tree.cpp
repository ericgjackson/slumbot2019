#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "betting_abstraction.h"
#include "betting_tree.h"
#include "constants.h"
#include "files.h"
#include "game.h"
#include "io.h"
#include "nonterminal_ids.h"

using std::shared_ptr;
using std::string;
using std::unordered_map;
using std::vector;

Node::Node(int id, int street, int player_acting, const shared_ptr<Node> &call_succ,
	   const shared_ptr<Node> &fold_succ, vector< shared_ptr<Node> > *bet_succs,
	   int num_remaining, int bet_to) {
  int num_succs = 0;
  if (call_succ) {
    ++num_succs;
  }
  if (fold_succ) {
    ++num_succs;
  }
  int num_bet_succs = 0;
  if (bet_succs)  {
    num_bet_succs = bet_succs->size();
    num_succs += num_bet_succs;
  }
  if (num_succs == 0) {
    succs_ = NULL;
  } else {
    succs_ = new shared_ptr<Node>[num_succs];
    int i = 0;
    if (call_succ) succs_[i++] = call_succ;
    if (fold_succ) succs_[i++] = fold_succ;
    for (int j = 0; j < num_bet_succs; ++j) {
      succs_[i++] = (*bet_succs)[j];
    }
  }
  id_ = id;
  last_bet_to_ = bet_to;
  num_succs_ = num_succs;
  flags_ = 0;
  if (call_succ)   flags_ |= kHasCallSuccFlag;
  if (fold_succ)   flags_ |= kHasFoldSuccFlag;
  flags_ |= (((unsigned short)street) << kStreetShift);
  if (player_acting > 255) {
    fprintf(stderr, "player_acting OOB: %u\n", player_acting);
    exit(-1);
  }
  if (num_remaining > 255) {
    fprintf(stderr, "num_remaining OOB: %u\n", num_remaining);
    exit(-1);
  }
  player_acting_ = player_acting;
  num_remaining_ = num_remaining;
}

Node::Node(Node *src) {
  int num_succs = src->NumSuccs();
  if (num_succs == 0) {
    succs_ = NULL;
  } else {
    succs_ = new shared_ptr<Node>[num_succs];
  }
  for (int s = 0; s < num_succs; ++s) succs_[s] = NULL;
  id_ = src->id_;
  last_bet_to_ = src->last_bet_to_;
  num_succs_ = src->num_succs_;
  flags_ = src->flags_;
  player_acting_ = src->player_acting_;
  num_remaining_ = src->num_remaining_;
}

Node::Node(int id, int last_bet_to, int num_succs, unsigned short flags,
	   unsigned char player_acting, unsigned char num_remaining) {
  id_ = id;
  last_bet_to_ = last_bet_to;
  num_succs_ = num_succs;
  if (num_succs == 0) {
    succs_ = nullptr;
  } else {
    succs_ = new shared_ptr<Node>[num_succs];
  }
  for (int s = 0; s < num_succs; ++s) succs_[s] = nullptr;
  flags_ = flags;
  player_acting_ = player_acting;
  num_remaining_ = num_remaining;
}

Node::~Node(void) {
  delete [] succs_;
}

string Node::ActionName(int s) {
  if (s == CallSuccIndex()) {
    return "c";
  } else if (s == FoldSuccIndex()) {
    return "f";
  } else {
    Node *b = IthSucc(s);
    int bet_size;
    if (Game::NumPlayers() > 2) {
      bet_size = b->LastBetTo() - LastBetTo();
    } else {
      if (! b->HasCallSucc()) {
	fprintf(stderr, "Expected node to have call succ\n");
	exit(-1);
      }
      bet_size = b->LastBetTo() - LastBetTo();
    }
    char buf[100];
    sprintf(buf, "b%u", bet_size);
    return buf;
  }
}

static void Indent(int num) {
  for (int i = 0; i < num; ++i) printf(" ");
}

void Node::PrintTree(int depth, string name,
		     int last_street) {
  Indent(2 * depth);
  int street = Street();
  if (street > last_street) name += " ";
  printf("\"%s\" (id %u lbt %u ns %u s %u", name.c_str(), id_, LastBetTo(),
	 NumSuccs(), street);
  if (NumSuccs() > 0) {
    printf(" p%uc", player_acting_);
  }
  printf(")\n");
  int num_succs = NumSuccs();
  for (int s = 0; s < num_succs; ++s) {
    char c;
    if (s == CallSuccIndex())      c = 'C';
    else if (s == FoldSuccIndex()) c = 'F';
    else                           c = 'B';
    string new_name = name + c;
    succs_[s]->PrintTree(depth + 1, new_name, street);
  }
}

// Note that we are dependent on the ordering of the succs
int Node::CallSuccIndex(void) const {
  if (HasCallSucc()) return 0;
  else               return -1;
}

int Node::FoldSuccIndex(void) const {
  if (HasFoldSucc()) {
    // Normally if you have a fold succ you must have a call succ too, but
    // I've done experiments where I disallow an open-call.
    if (HasCallSucc()) return 1;
    else               return 0;
  } else {
    return -1;
  }
}

// Typically this will be the call succ.  In the unusual case where there is
// no call succ, we will use the first succ, whatever it is.  In trees
// where open-limping is prohibited, the fold succ will be the default succ.
// Note that we are dependent on the ordering of the succs
int Node::DefaultSuccIndex(void) const {
  return 0;
}

void BettingTree::Display(void) {
  root_->PrintTree(0, "", initial_street_);
}

void BettingTree::FillTerminalArray(Node *node) {
  if (node->Terminal()) {
    int terminal_id = node->TerminalID();
    if (terminal_id >= num_terminals_) {
      fprintf(stderr, "Out of bounds terminal ID: %i (num terminals %i)\n",
	      terminal_id, num_terminals_);
      exit(-1);
    }
    terminals_[terminal_id] = node;
    return;
  }
  for (int i = 0; i < node->NumSuccs(); ++i) {
    FillTerminalArray(node->IthSucc(i));
  }
}

void BettingTree::FillTerminalArray(void) {
  terminals_ = new Node *[num_terminals_];
  if (root_.get()) FillTerminalArray(root_.get());
}

#if 0
bool BettingTree::GetPathToNamedNode(const char *str, Node *node,
				     vector<Node *> *path) {
  char c = *str;
  // Allow an unconsumed space at the end of the name.  So we can find a node
  // either by the strictly proper name "CC " or by "CC".
  if (c == 0 || c == '\n' || (c == ' ' && (str[1] == 0 || str[1] == '\n'))) {
    return true;
  }
  if (c == ' ') return GetPathToNamedNode(str + 1, node, path);
  Node *succ;
  const char *next_str;
  if (c == 'F') {
    int s = node->FoldSuccIndex();
    succ = node->IthSucc(s);
    next_str = str + 1;
  } else if (c == 'C') {
    int s = node->CallSuccIndex();
    succ = node->IthSucc(s);
    next_str = str + 1;
  } else if (c == 'B') {
    int i = 1;
    while (str[i] >= '0' && str[i] <= '9') ++i;
    if (i == 1) {
      // Must be limit tree
      succ = node->IthSucc(node->NumSuccs() - 1);
    } else {
      char buf[20];
      if (i > 10) {
	fprintf(stderr, "Too big a bet size: %s\n", str);
	exit(-1);
      }
      // B43 - i will be 3
      memcpy(buf, str + 1, i - 1);
      buf[i-1] = 0;
      int bet_size;
      if (sscanf(buf, "%i", &bet_size) != 1) {
	fprintf(stderr, "Couldn't parse bet size: %s\n", str);
	exit(-1);
      }
      int s = node->CallSuccIndex();
      if (s == -1) {
	// This doesn't work for graft trees
	fprintf(stderr, "GetPathToNamedNode: bet node has no call succ\n");
	exit(-1);
      }
      Node *before_call_succ = node->IthSucc(s);
      int before_pot_size = before_call_succ->PotSize();
      int num_succs = node->NumSuccs();
      int j;
      for (j = 0; j < num_succs; ++j) {
	Node *jth_succ = node->IthSucc(j);
	int s2 = jth_succ->CallSuccIndex();
	if (s2 == -1) continue;
	Node *call_succ = jth_succ->IthSucc(s2);
	int after_pot_size = call_succ->PotSize();
	int this_bet_size = (after_pot_size - before_pot_size) / 2;
	if (this_bet_size == bet_size) break;
      }
      if (j == num_succs) {
	fprintf(stderr, "Couldn't find node with bet size %i\n", bet_size);
	exit(-1);
      }
      succ = node->IthSucc(j);
    }
    next_str = str + i;
  } else {
    fprintf(stderr, "Couldn't parse node name from %s\n", str);
    exit(-1);
  }
  if (succ == NULL) {
    return false;
  }
  path->push_back(succ);
  return GetPathToNamedNode(next_str, succ, path);
}

bool BettingTree::GetPathToNamedNode(const char *str, vector<Node *> *path) {
  path->push_back(root_.get());
  return GetPathToNamedNode(str, root_.get(), path);
}

// Works for no-limit now?
// Takes a string like "BC CB" or "B100C B50" and returns the node named by
// that string
Node *BettingTree::GetNodeFromName(const char *str, Node *node) {
  char c = *str;
  // Allow an unconsumed space at the end of the name.  So we can find a node
  // either by the strictly proper name "CC " or by "CC".
  if (c == 0 || c == '\n' || (c == ' ' && (str[1] == 0 || str[1] == '\n'))) {
    return node;
  }
  if (c == ' ') return GetNodeFromName(str + 1, node);
  Node *succ;
  const char *next_str;
  if (c == 'F') {
    int s = node->FoldSuccIndex();
    succ = node->IthSucc(s);
    next_str = str + 1;
  } else if (c == 'C') {
    int s = node->CallSuccIndex();
    succ = node->IthSucc(s);
    next_str = str + 1;
  } else if (c == 'B') {
    int i = 1;
    while (str[i] >= '0' && str[i] <= '9') ++i;
    if (i == 1) {
      // Must be limit tree
      succ = node->IthSucc(node->NumSuccs() - 1);
    } else {
      char buf[20];
      if (i > 10) {
	fprintf(stderr, "Too big a bet size: %s\n", str);
	exit(-1);
      }
      // B43 - i will be 3
      memcpy(buf, str + 1, i - 1);
      buf[i-1] = 0;
      int bet_size;
      if (sscanf(buf, "%i", &bet_size) != 1) {
	fprintf(stderr, "Couldn't parse bet size: %s\n", str);
	exit(-1);
      }
      int s = node->CallSuccIndex();
      Node *before_call_succ = node->IthSucc(s);
      int before_pot_size = before_call_succ->PotSize();
      int num_succs = node->NumSuccs();
      int j;
      for (j = 0; j < num_succs; ++j) {
	Node *jth_succ = node->IthSucc(j);
	int s2 = jth_succ->CallSuccIndex();
	if (s2 == -1) continue;
	Node *call_succ = jth_succ->IthSucc(s2);
	int after_pot_size = call_succ->PotSize();
	int this_bet_size = (after_pot_size - before_pot_size) / 2;
	if (this_bet_size == bet_size) break;
      }
      if (j == num_succs) {
	fprintf(stderr, "Couldn't find node with bet size %i\n", bet_size);
	exit(-1);
      }
      succ = node->IthSucc(j);
    }
    next_str = str + i;
  } else {
    fprintf(stderr, "Couldn't parse node name from %s\n", str);
    exit(-1);
  }
  if (succ == NULL) {
    return NULL;
  }
  return GetNodeFromName(next_str, succ);
}

Node *BettingTree::GetNodeFromName(const char *str) {
  Node *node = GetNodeFromName(str, root_.get());
  if (node == NULL) {
    fprintf(stderr, "Couldn't find node with name \"%s\"\n", str);
  }
  return node;
}
#endif

// Used by the subtree constructor
// This doesn't preserve the reentrancy of the source tree
shared_ptr<Node> BettingTree::Clone(Node *old_n, int *num_terminals) {
  shared_ptr<Node> new_n(new Node(old_n));
  if (new_n->Terminal()) {
    // Need to reindex the terminal nodes
    new_n->SetTerminalID(*num_terminals);
    ++*num_terminals;
  }
  int num_succs = old_n->NumSuccs();
  for (int s = 0; s < num_succs; ++s) {
    shared_ptr<Node> new_succ(Clone(old_n->IthSucc(s), num_terminals));
    new_n->SetIthSucc(s, new_succ);
  }
  return new_n;
}

shared_ptr<Node>
BettingTree::Read(Reader *reader,
		  unordered_map< int, shared_ptr<Node> > ***maps) {
  int id = reader->ReadUnsignedIntOrDie();
  unsigned short last_bet_to = reader->ReadUnsignedShortOrDie();
  unsigned short num_succs = reader->ReadUnsignedShortOrDie();
  unsigned short flags = reader->ReadUnsignedShortOrDie();
  unsigned char pa = reader->ReadUnsignedCharOrDie();
  unsigned char num_remaining = reader->ReadUnsignedCharOrDie();
  if (num_succs > 0) {
    // Check if node already seen.  For now assume reentrancy only at
    // nonterminal nodes.
    unordered_map< int, shared_ptr<Node> >::iterator it;
    int st = (int)((flags & Node::kStreetMask) >> Node::kStreetShift);
    it = maps[st][pa]->find(id);
    if (it != maps[st][pa]->end()) {
      return it->second;
    }
  }
  shared_ptr<Node>
    node(new Node(id, last_bet_to, num_succs, flags, pa, num_remaining));
  if (num_succs == 0) {
    ++num_terminals_;
    return node;
  }
  int st = node->Street();
  (*maps[st][pa])[id] = node;
  for (int s = 0; s < num_succs; ++s) {
    shared_ptr<Node> succ(Read(reader, maps));
    node->SetIthSucc(s, succ);
  }
  return node;
}

// Maintain a map from ids to shared pointers to nodes.
void BettingTree::Initialize(int target_player,
			     const BettingAbstraction &ba) {
  char buf[500];
  if (ba.Asymmetric()) {
    sprintf(buf, "%s/betting_tree.%s.%u.%s.%u", Files::StaticBase(),
	    Game::GameName().c_str(), Game::NumPlayers(),
	    ba.BettingAbstractionName().c_str(), target_player);
  } else {
    sprintf(buf, "%s/betting_tree.%s.%u.%s", Files::StaticBase(),
	    Game::GameName().c_str(), Game::NumPlayers(),
	    ba.BettingAbstractionName().c_str());
  }
  Reader reader(buf);
  initial_street_ = ba.InitialStreet();
  root_ = NULL;
  terminals_ = NULL;
  num_terminals_ = 0;
  int max_street = Game::MaxStreet();
  int num_players = Game::NumPlayers();
  unordered_map< int, shared_ptr<Node> > ***maps =
    new unordered_map< int, shared_ptr<Node> > **[max_street + 1];
  for (int st = 0; st <= max_street; ++st) {
    maps[st] =
      new unordered_map< int, shared_ptr<Node> > *[num_players];
    for (int pa = 0; pa < num_players; ++pa) {
      maps[st][pa] = new unordered_map< int, shared_ptr<Node> >;
    }
  }
  root_ = Read(&reader, maps);
  for (int st = 0; st <= max_street; ++st) {
    for (int pa = 0; pa < num_players; ++pa) {
      delete maps[st][pa];
    }
    delete [] maps[st];
  }
  delete [] maps;
  FillTerminalArray();
  num_nonterminals_ = CountNumNonterminals(this);
}

BettingTree::BettingTree(void) : root_(nullptr) {
}

BettingTree *BettingTree::BuildTree(const BettingAbstraction &ba) {
  BettingTree *tree = new BettingTree();
  tree->Initialize(true, ba);
  return tree;
}

BettingTree *BettingTree::BuildAsymmetricTree(const BettingAbstraction &ba,
					      int target_player) {
  BettingTree *tree = new BettingTree();
  tree->Initialize(target_player, ba);
  return tree;
}

// A subtree constructor
BettingTree *BettingTree::BuildSubtree(Node *subtree_root) {
  BettingTree *tree = new BettingTree();
  int subtree_street = subtree_root->Street();
  tree->initial_street_ = subtree_street;
  tree->num_terminals_ = 0;
  tree->root_ = tree->Clone(subtree_root, &tree->num_terminals_);
  tree->FillTerminalArray();
  AssignNonterminalIDs(tree, &tree->num_nonterminals_);
  return tree;
}

BettingTree::~BettingTree(void) {
  delete [] terminals_;
  int num_players = Game::NumPlayers();
  for (int p = 0; p < num_players; ++p) {
    delete [] num_nonterminals_[p];
  }
  delete [] num_nonterminals_;
}

void BettingTree::GetStreetInitialNodes(Node *node, int street,
					vector<Node *> *nodes) {
  if (node->Street() == street) {
    nodes->push_back(node);
    return;
  }
  int num_succs = node->NumSuccs();
  for (int s = 0; s < num_succs; ++s) {
    GetStreetInitialNodes(node->IthSucc(s), street, nodes);
  }
}

void BettingTree::GetStreetInitialNodes(int street,
					vector<Node *> *nodes) {
  nodes->clear();
  if (root_) {
    GetStreetInitialNodes(root_.get(), street, nodes);
  }
}

// Two succs correspond if they are both call succs
// Two succs correspond if they are both fold succs
// Two succs correspond if they are both bet succs and the bet size is the
// same.
// Problem: in graft trees bet succs may not have a call succ.  So how do we
// compare if two bet succs are the same?
bool TwoSuccsCorrespond(Node *node1, int s1, Node *node2,
			int s2) {
  bool is_call_succ1 = (s1 == node1->CallSuccIndex());
  bool is_call_succ2 = (s2 == node2->CallSuccIndex());
  if (is_call_succ1 && is_call_succ2) return true;
  if (is_call_succ1 || is_call_succ2) return false;
  bool is_fold_succ1 = (s1 == node1->FoldSuccIndex());
  bool is_fold_succ2 = (s2 == node2->FoldSuccIndex());
  if (is_fold_succ1 && is_fold_succ2) return true;
  if (is_fold_succ1 || is_fold_succ2) return false;
  Node *b1 = node1->IthSucc(s1);
  Node *bc1 = b1->IthSucc(b1->CallSuccIndex());
  Node *b2 = node2->IthSucc(s2);
  Node *bc2 = b2->IthSucc(b2->CallSuccIndex());
  return (bc1->LastBetTo() == bc2->LastBetTo());
}
