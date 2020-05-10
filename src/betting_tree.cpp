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
using std::unique_ptr;
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
  if (num_succs > 0) {
    succs_.reset(new shared_ptr<Node>[num_succs]);
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
  if (num_succs > 0) {
    succs_.reset(new shared_ptr<Node>[num_succs]);
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
  if (num_succs > 0) {
    succs_.reset(new shared_ptr<Node>[num_succs]);
  }
  for (int s = 0; s < num_succs; ++s) succs_[s] = nullptr;
  flags_ = flags;
  player_acting_ = player_acting;
  num_remaining_ = num_remaining;
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

void Node::PrintTree(int depth, const string &name, vector< vector< vector<bool> > > *seen,
		     int last_st) const {
  bool recurse = true;
  int st = Street();
  int pa = PlayerActing();
  if (! Terminal()) {
    int nt = NonterminalID();
    if ((*seen)[st][pa][nt]) recurse = false;
    (*seen)[st][pa][nt] = true;
  }
  Indent(2 * depth);
  int num_succs = NumSuccs();
  printf("\"%s\" (id %u lbt %u ns %u st %u", name.c_str(), id_, LastBetTo(), num_succs, st);
  // if (! Terminal()) {
  // Showdown nodes have pa 255
  if (pa != 255) printf(" p%uc", pa);
  printf(")");
  if (! recurse) {
    printf(" *");
  }
  printf("\n");
  if (recurse) {
    for (int s = 0; s < num_succs; ++s) {
      char c;
      if (s == CallSuccIndex())      c = 'C';
      else if (s == FoldSuccIndex()) c = 'F';
      else                           c = 'B';
      string new_name = name;
      if (st > last_st) new_name += " ";
      new_name += c;
      succs_[s]->PrintTree(depth + 1, new_name, seen, st);
    }
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

// Typically this will be the call succ.  In the unusual case where there is no call succ, we will
// use the first succ, whatever it is.  In trees where open-limping is prohibited, the fold succ
// will be the default succ.  Note that we are dependent on the ordering of the succs
int Node::DefaultSuccIndex(void) const {
  return 0;
}

// Only works for heads-up
bool Node::StreetInitial(void) const {
  if (Terminal()) return false;
  int csi = CallSuccIndex();
  // Not sure this can happen
  if (csi == -1) return false;
  Node *c = IthSucc(csi);
  return (! c->Terminal() && c->Street() == Street());
}

int BettingTree::NumNonterminals(int p, int st) const {
  return num_nonterminals_[p * (Game::MaxStreet() + 1) + st];
}

void BettingTree::Display(void) const {
  int num_players = Game::NumPlayers();
  int max_street = Game::MaxStreet();
  vector< vector< vector<bool> > > seen(max_street + 1);
  for (int st = 0; st <= max_street; ++st) {
    seen[st].resize(num_players);
    for (int p = 0; p < num_players; ++p) {
      int num_nt = NumNonterminals(p, st);
      seen[st][p].resize(num_nt);
      for (int i = 0; i < num_nt; ++i) {
	seen[st][p][i] = false;
      }
    }
  }
  root_->PrintTree(0, "", &seen, initial_street_);
}

void BettingTree::Display(Node *node) const {
  int num_players = Game::NumPlayers();
  int max_street = Game::MaxStreet();
  vector< vector< vector<bool> > > seen(max_street + 1);
  for (int st = 0; st <= max_street; ++st) {
    seen[st].resize(num_players);
    for (int p = 0; p < num_players; ++p) {
      int num_nt = NumNonterminals(p, st);
      seen[st][p].resize(num_nt);
      for (int i = 0; i < num_nt; ++i) {
	seen[st][p][i] = false;
      }
    }
  }
  node->PrintTree(0, "", &seen, initial_street_);
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
  terminals_.reset(new Node *[num_terminals_]);
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
// It appears we inherit the nonterminal IDs of the source tree, although they may get
// changed by the caller.
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

shared_ptr<Node> BettingTree::Read(Reader *reader, unordered_map< int, shared_ptr<Node> > *maps) {
  int id = reader->ReadUnsignedIntOrDie();
  unsigned short last_bet_to = reader->ReadUnsignedShortOrDie();
  unsigned short num_succs = reader->ReadUnsignedShortOrDie();
  unsigned short flags = reader->ReadUnsignedShortOrDie();
  unsigned char pa = reader->ReadUnsignedCharOrDie();
  unsigned char num_remaining = reader->ReadUnsignedCharOrDie();
  int st = (int)((flags & Node::kStreetMask) >> Node::kStreetShift);
  int map_index = st * Game::NumPlayers() + pa;
  unordered_map< int, shared_ptr<Node> > *m = &maps[map_index];
  if (num_succs > 0) {
    // Check if node already seen.  For now assume reentrancy only at nonterminal nodes.
    unordered_map< int, shared_ptr<Node> >::iterator it;
    it = m->find(id);
    if (it != m->end()) return it->second;
  }
  shared_ptr<Node> node(new Node(id, last_bet_to, num_succs, flags, pa, num_remaining));
  if (num_succs == 0) {
    ++num_terminals_;
    return node;
  }
  (*m)[id] = node;
  for (int s = 0; s < num_succs; ++s) {
    shared_ptr<Node> succ(Read(reader, maps));
    node->SetIthSucc(s, succ);
  }
  return node;
}

// Maintain a map from ids to shared pointers to nodes.
void BettingTree::Initialize(int target_player, const BettingAbstraction &ba) {
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
  root_ = nullptr;
  num_terminals_ = 0;
  int max_street = Game::MaxStreet();
  int num_players = Game::NumPlayers();
  int num_maps = (max_street + 1) * num_players;
  unique_ptr<unordered_map< int, shared_ptr<Node> > []>
    maps(new unordered_map< int, shared_ptr<Node> > [num_maps]);
  root_ = Read(&reader, maps.get());
  FillTerminalArray();
  num_nonterminals_.reset(new int[num_players * (max_street + 1)]);
  CountNumNonterminals(this, num_nonterminals_.get());
}

BettingTree::BettingTree(const BettingAbstraction &ba) {
  Initialize(0, ba);
}

BettingTree::BettingTree(const BettingAbstraction &ba, int target_player) {
  Initialize(target_player, ba);
}

// A subtree constructor
// This doesn't preserve the reentrancy of the source tree
BettingTree::BettingTree(Node *subtree_root) {
  int subtree_street = subtree_root->Street();
  initial_street_ = subtree_street;
  num_terminals_ = 0;
  root_ = Clone(subtree_root, &num_terminals_);
  FillTerminalArray();
  int num_players = Game::NumPlayers();
  int max_street = Game::MaxStreet();
  num_nonterminals_.reset(new int[num_players * (max_street + 1)]);
  // AssignNonterminalIDs() doesn't handle reentrancy yet.  We currently do not preserve the
  // reentrancy of the source tree inside of Clone().  But if we did, then we would need to
  // modify AssignNonterminalIDs() accordingly.
  AssignNonterminalIDs(this, num_nonterminals_.get());
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


// Map from acting node succs onto opp node succs
unique_ptr<int []> GetSuccMapping(Node *acting_node, Node *opp_node) {
  int acting_num_succs = acting_node->NumSuccs();
  int opp_num_succs = opp_node->NumSuccs();
  unique_ptr<int []> succ_mapping(new int[acting_num_succs]);
  for (int as = 0; as < acting_num_succs; ++as) {
    int os = -1;
    if (as == acting_node->CallSuccIndex()) {
      for (int s = 0; s < opp_num_succs; ++s) {
	if (s == opp_node->CallSuccIndex()) {
	  os = s;
	  break;
	}
      }
    } else if (as == acting_node->FoldSuccIndex()) {
      for (int s = 0; s < opp_num_succs; ++s) {
	if (s == opp_node->FoldSuccIndex()) {
	  os = s;
	  break;
	}
      }
    } else {
      int bet_to = acting_node->IthSucc(as)->LastBetTo();
      for (int s = 0; s < opp_num_succs; ++s) {
	if (opp_node->IthSucc(s)->LastBetTo() == bet_to) {
	  os = s;
	  break;
	}
      }
    }
    if (os == -1) {
      fprintf(stderr, "GetSuccMapping: no matching succ; ans %i ons %i as %i\n",
	      acting_num_succs, opp_num_succs, as);
      exit(-1);
    }
    succ_mapping[as] = os;
  }
  return succ_mapping;
}
