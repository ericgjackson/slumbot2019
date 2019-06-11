#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>

#include "betting_tree.h"
#include "betting_trees.h"
#include "board_tree.h"
#include "buckets.h"
#include "cfr_street_values.h"
#include "cfr_value_type.h"
#include "cfr_values.h"
#include "game.h"
#include "io.h"
#include "nonterminal_ids.h"

using std::string;
using std::unique_ptr;

void CFRValues::Initialize(const bool *players, const bool *streets, int root_bd, int root_bd_st,
			   const Buckets &buckets) {
  root_bd_ = root_bd;
  root_bd_st_ = root_bd_st;
  int num_players = Game::NumPlayers();
  players_.reset(new bool[num_players]);
  for (int p = 0; p < num_players; ++p) {
    players_[p] = players == nullptr || players[p];
  }
  int max_street = Game::MaxStreet();
  streets_.reset(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    streets_[st] = streets == nullptr || streets[st];
  }

  num_holdings_.reset(new int[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    if (streets && ! streets[st]) {
      num_holdings_[st] = 0;
      continue;
    }
    if (buckets.None(st)) {
      int num_local_boards = BoardTree::NumLocalBoards(root_bd_st_, root_bd_, st);
      int num_hole_card_pairs = Game::NumHoleCardPairs(st);
      num_holdings_[st] = num_local_boards * num_hole_card_pairs;
    } else {
      num_holdings_[st] = buckets.NumBuckets(st);
    }
  }

  street_values_.reset(new AbstractCFRStreetValues *[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) street_values_[st] = nullptr;
}

CFRValues::CFRValues(const bool *players, const bool *streets, int root_bd, int root_bd_st,
		     const Buckets &buckets, const BettingTree *betting_tree) {
  Initialize(players, streets, root_bd, root_bd_st, buckets);
  
  int num_players = Game::NumPlayers();
  int max_street = Game::MaxStreet();
  int num = num_players * (max_street + 1);
  num_nonterminals_.reset(new int[num]);
  for (int p = 0; p < num_players; ++p) {
    for (int st = 0; st <= max_street; ++st) {
      int index = p * (max_street + 1) + st;
      if (players_[p] && (streets == nullptr || streets[st])) {
	// Assumes symmetric
	num_nonterminals_[index] = betting_tree->NumNonterminals(p, st);
      } else {
	num_nonterminals_[index] = 0;
      }
    }
  }
}

// For asymmetric systems.
CFRValues::CFRValues(const bool *players, const bool *streets, int root_bd, int root_bd_st,
		     const Buckets &buckets, const BettingTrees &betting_trees) {
  Initialize(players, streets, root_bd, root_bd_st, buckets);
  
  int num_players = Game::NumPlayers();
  int max_street = Game::MaxStreet();
  int num = num_players * (max_street + 1);
  num_nonterminals_.reset(new int[num]);
  for (int p = 0; p < num_players; ++p) {
    for (int st = 0; st <= max_street; ++st) {
      int index = p * (max_street + 1) + st;
      if (players_[p] && (streets == nullptr || streets[st])) {
	num_nonterminals_[index] = betting_trees.NumNonterminals(p, p, st);
      } else {
	num_nonterminals_[index] = 0;
      }
    }
  }
}

CFRValues::CFRValues(const CFRValues &p0_values, const CFRValues &p1_values) {
  root_bd_st_ = p0_values.RootSt();
  root_bd_ = p0_values.RootBd();
  int num_players = Game::NumPlayers();
  players_.reset(new bool[num_players]);
  for (int p = 0; p < num_players; ++p) {
    players_[p] = p0_values.Player(p);
  }
  int max_street = Game::MaxStreet();
  streets_.reset(new bool[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    streets_[st] = p0_values.Street(st);
  }
  num_holdings_.reset(new int[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    if (! streets_[st]) {
      num_holdings_[st] = 0;
      continue;
    }
    num_holdings_[st] = p0_values.NumHoldings(st);
  }
  street_values_.reset(new AbstractCFRStreetValues *[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    if (! streets_[st]) {
      street_values_[st] = nullptr;
      continue;
    }
    CFRValueType t = p0_values.StreetValues(st)->MyType();
    if (t == CFRValueType::CFR_INT) {
      street_values_[st] =
	new CFRStreetValues<int>(
			 dynamic_cast<CFRStreetValues<int> *>(p0_values.StreetValues(st)),
			 dynamic_cast<CFRStreetValues<int> *>(p1_values.StreetValues(st)));
    } else if (t == CFRValueType::CFR_DOUBLE) {
      street_values_[st] =
	new CFRStreetValues<double>(
			 dynamic_cast<CFRStreetValues<double> *>(p0_values.StreetValues(st)),
			 dynamic_cast<CFRStreetValues<double> *>(p1_values.StreetValues(st)));
    } else {
      fprintf(stderr, "Unsupported type\n");
      exit(-1);
    }
  }
  // Don't need to fill out num_nonterminals_
}

CFRValues::~CFRValues(void) {
  int max_street = Game::MaxStreet();
  for (int st = 0; st <= max_street; ++st) delete street_values_[st];
}

void CFRValues::AllocateAndClear(const BettingTree *betting_tree, CFRValueType *value_types,
				 bool quantize, int only_p) {
  int max_street = Game::MaxStreet();
  int num_players = Game::NumPlayers();
  for (int st = 0; st <= max_street; ++st) {
    if (streets_[st]) {
      CFRValueType value_type = value_types[st];
      int num_holdings = num_holdings_[st];
      if (value_type == CFRValueType::CFR_INT) {
	if (quantize) {
	  street_values_[st] =
	    new CFRStreetValues<unsigned char>(st, players_.get(), num_holdings,
					       num_nonterminals_.get(), value_type);
	} else {
	  street_values_[st] =
	    new CFRStreetValues<int>(st, players_.get(), num_holdings, num_nonterminals_.get(),
				     value_type);
	}
      } else if (value_type == CFRValueType::CFR_DOUBLE) {
	if (quantize) {
	  street_values_[st] =
	    new CFRStreetValues<unsigned char>(st, players_.get(), num_holdings,
					       num_nonterminals_.get(), value_type);
	} else {
	  street_values_[st] =
	    new CFRStreetValues<double>(st, players_.get(), num_holdings,
					num_nonterminals_.get(), value_type);
	}
      } else if (value_type == CFRValueType::CFR_CHAR) {
	street_values_[st] =
	  new CFRStreetValues<unsigned char>(st, players_.get(), num_holdings,
					     num_nonterminals_.get(), value_type);
      } else if (value_type == CFRValueType::CFR_SHORT) {
	if (quantize) {
	  street_values_[st] =
	    new CFRStreetValues<unsigned char>(st, players_.get(), num_holdings,
					       num_nonterminals_.get(), value_type);
	} else {
	  street_values_[st] =
	    new CFRStreetValues<unsigned short>(st, players_.get(), num_holdings,
						num_nonterminals_.get(), value_type);
	}
      } else {
	fprintf(stderr, "CFRValues::AllocateAndClear() unexpected value type %i\n",
		(int)value_type);
	exit(-1);
      }
      for (int p = 0; p < num_players; ++p) {
	if ((only_p == -1 || p == only_p) && players_[p]) {
	  street_values_[st]->AllocateAndClear(betting_tree->Root(), p);
	}
      }
    }
  }
}

// Shouldn't we respect only_p?
void CFRValues::AllocateAndClear(const BettingTree *betting_tree, CFRValueType value_type,
				 bool quantize, int only_p) {
  int max_street = Game::MaxStreet();
  unique_ptr<CFRValueType []> value_types(new CFRValueType[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    value_types[st] = value_type;
  }
  AllocateAndClear(betting_tree, value_types.get(), quantize, only_p);
}

void CFRValues::CreateStreetValues(int st, CFRValueType value_type, bool quantize) {
  if (street_values_[st] == nullptr) {
    if (value_type == CFRValueType::CFR_CHAR) {
      street_values_[st] =
	new CFRStreetValues<unsigned char>(st, players_.get(), num_holdings_[st],
					   num_nonterminals_.get(), value_type);
    } else if (value_type == CFRValueType::CFR_SHORT) {
      if (quantize) {
	street_values_[st] =
	  new CFRStreetValues<unsigned char>(st, players_.get(), num_holdings_[st],
					     num_nonterminals_.get(), value_type);
      } else {
	street_values_[st] =
	  new CFRStreetValues<unsigned short>(st, players_.get(), num_holdings_[st],
					      num_nonterminals_.get(), value_type);
      }
    } else if (value_type == CFRValueType::CFR_INT) {
      if (quantize) {
	street_values_[st] =
	  new CFRStreetValues<unsigned char>(st, players_.get(), num_holdings_[st],
					     num_nonterminals_.get(), value_type);
      } else {
	street_values_[st] = new CFRStreetValues<int>(st, players_.get(), num_holdings_[st],
						      num_nonterminals_.get(), value_type);
      }
    } else if (value_type == CFRValueType::CFR_DOUBLE) {
      if (quantize) {
	street_values_[st] =
	  new CFRStreetValues<unsigned char>(st, players_.get(), num_holdings_[st],
					     num_nonterminals_.get(), value_type);
      } else {
	street_values_[st] =
	  new CFRStreetValues<double>(st, players_.get(), num_holdings_[st],
				      num_nonterminals_.get(), value_type);
      }
    } else {
      fprintf(stderr, "Unknown value type\n");
      exit(-1);
    }
  }
}

void CFRValues::Read(Node *node, Reader ***readers, void ***decompressors, int p) {
  if (node->Terminal()) return;
  int st = node->Street();
  int num_succs = node->NumSuccs();
  int pa = node->PlayerActing();
  if (street_values_[st] && pa == p) {
    Reader *reader = readers[p][st];
    if (reader == nullptr) {
      fprintf(stderr, "CFRValues::Read(): pa %i st %i missing file?\n", pa, st);
      exit(-1);
    }
    street_values_[st]->ReadNode(node, reader, decompressors ? decompressors[pa][st] : nullptr);
  }
  for (int s = 0; s < num_succs; ++s) {
    Read(node->IthSucc(s), readers, decompressors, p);
  }
}

Reader *CFRValues::InitializeReader(const char *dir, int p, int st, int it,
				    const string &action_sequence, int root_bd_st, int root_bd,
				    bool sumprobs, CFRValueType *value_type) {
  char buf[500];

  int t;
  for (t = 0; t < 4; ++t) {
    unsigned char suffix;
    if (t == 0) {
      suffix = 'd';
      *value_type = CFRValueType::CFR_DOUBLE;
    } else if (t == 1) {
      suffix = 'i';
      *value_type = CFRValueType::CFR_INT;
    } else if (t == 2) {
      suffix = 'c';
      *value_type = CFRValueType::CFR_CHAR;
    } else if (t == 3) {
      suffix = 's';
      *value_type = CFRValueType::CFR_SHORT;
    }
    sprintf(buf, "%s/%s.%s.%u.%u.%u.%u.p%u.%c", dir, sumprobs ? "sumprobs" : "regrets",
	    action_sequence.c_str(), root_bd_st, root_bd, st, it, p, suffix);
    if (FileExists(buf)) break;
  }
  if (t == 4) {
    fprintf(stderr, "Couldn't find file\n");
    fprintf(stderr, "buf: %s\n", buf);
    exit(-1);
  }
  Reader *reader = new Reader(buf);
  return reader;
}

void CFRValues::Read(const char *dir, int it, const BettingTree *betting_tree,
		     const string &action_sequence, int only_p, bool sumprobs, bool quantize) {
  int num_players = Game::NumPlayers();
  Reader ***readers = new Reader **[num_players];
  void ***decompressors = nullptr;
  int max_street = Game::MaxStreet();

  for (int p = 0; p < num_players; ++p) {
    if (only_p != -1 && p != only_p) {
      readers[p] = nullptr;
      continue;
    }
    if (! players_[p]) {
      readers[p] = nullptr;
      continue;
    }
    readers[p] = new Reader *[max_street + 1];
    for (int st = 0; st <= max_street; ++st) {
      if (! streets_[st]) {
	readers[p][st] = nullptr;
	continue;
      }
      CFRValueType value_type;
      readers[p][st] = InitializeReader(dir, p, st, it, action_sequence, root_bd_st_, root_bd_,
					sumprobs, &value_type);
      if (street_values_[st] == nullptr) {
	CreateStreetValues(st, value_type, quantize);
      }
    }
  }

  for (int p = 0; p < num_players; ++p) {
    if ((only_p == -1 || p == only_p) && players_[p]) {
      Read(betting_tree->Root(), readers, decompressors, p);
    }    
  }
  
  for (int p = 0; p < num_players; ++p) {
    if (only_p != -1 && p != only_p) continue;
    if (! players_[p]) continue;
    for (int st = 0; st <= max_street; ++st) {
      if (! streets_[st]) continue;
      if (! readers[p][st]->AtEnd()) {
	fprintf(stderr, "Reader p %u st %u didn't get to end\n", p, st);
	fprintf(stderr, "Pos: %lli\n", readers[p][st]->BytePos());
	fprintf(stderr, "File size: %lli\n", readers[p][st]->FileSize());
	exit(-1);
      }
      delete readers[p][st];
    }
    delete [] readers[p];
  }
  delete [] readers;
  delete [] decompressors;
}

// For asymmetric systems.  For when you want P0's values to be the values trained for a target
// P0 system and P1's values to be the values trained for a target P1 system.
// Be careful to use the right version of Read() for your needs.
void CFRValues::ReadAsymmetric(const char *dir, int it, const BettingTrees &betting_trees,
			       const string &action_sequence, int only_p, bool sumprobs,
			       bool quantize) {
  int num_players = Game::NumPlayers();
  Reader ***readers = new Reader **[num_players];
  void ***decompressors = nullptr;
  int max_street = Game::MaxStreet();
  char asym_dir[500];

  for (int p = 0; p < num_players; ++p) {
    if (only_p != -1 && p != only_p) {
      readers[p] = nullptr;
      continue;
    }
    if (! players_[p]) {
      readers[p] = nullptr;
      continue;
    }
    sprintf(asym_dir, "%s.p%i", dir, p);
    readers[p] = new Reader *[max_street + 1];
    for (int st = 0; st <= max_street; ++st) {
      if (! streets_[st]) {
	readers[p][st] = nullptr;
	continue;
      }
      CFRValueType value_type;
      readers[p][st] = InitializeReader(asym_dir, p, st, it, action_sequence, root_bd_st_, root_bd_,
					sumprobs, &value_type);
      if (street_values_[st] == nullptr) {
	CreateStreetValues(st, value_type, quantize);
      }
    }
  }

  for (int p = 0; p < num_players; ++p) {
    if ((only_p == -1 || p == only_p) && players_[p]) {
      Read(betting_trees.Root(p), readers, decompressors, p);
    }    
  }
  
  for (int p = 0; p < num_players; ++p) {
    if (only_p != -1 && p != only_p) continue;
    if (! players_[p]) continue;
    for (int st = 0; st <= max_street; ++st) {
      if (! streets_[st]) continue;
      if (! readers[p][st]->AtEnd()) {
	fprintf(stderr, "Reader p %u st %u didn't get to end\n", p, st);
	fprintf(stderr, "Pos: %lli\n", readers[p][st]->BytePos());
	fprintf(stderr, "File size: %lli\n", readers[p][st]->FileSize());
	exit(-1);
      }
      delete readers[p][st];
    }
    delete [] readers[p];
  }
  delete [] readers;
  delete [] decompressors;
}

// Prevent redundant writing with reentrant trees
void CFRValues::Write(Node *node, Writer ***writers, void ***compressors, bool ***seen) const {
  if (node->Terminal()) return;
  int num_succs = node->NumSuccs();
  int st = node->Street();
  int pa = node->PlayerActing();
  if (street_values_[st] && players_[pa] && writers[pa]) {
    int nt = node->NonterminalID();
    // If we have seen this node, we have also seen all the descendants of it,
    // so we can just return.
    if (seen) {
      if (seen[st][pa][nt]) return;
      seen[st][pa][nt] = true;
    }
    street_values_[st]->WriteNode(node, writers[pa][st],
				  compressors ? compressors[pa][st] : nullptr);
  }
  for (int s = 0; s < num_succs; ++s) {
    Write(node->IthSucc(s), writers, compressors, seen);
  }
}

static void DeleteWriters(Writer ***writers, void ***compressors) {
  int max_street = Game::MaxStreet();
  int num_players = Game::NumPlayers();
  for (int p = 0; p < num_players; ++p) {
    if (writers[p] == nullptr) continue;
    for (int st = 0; st <= max_street; ++st) {
      if (compressors[p][st]) {
	fprintf(stderr, "Didn't expect compressors\n");
	exit(-1);
      }
      delete writers[p][st];
    }
    delete [] writers[p];
    delete [] compressors[p];
  }
  delete [] writers;
  delete [] compressors;
}

Writer ***CFRValues::InitializeWriters(const char *dir, int it, const string &action_sequence,
				       int only_p, bool sumprobs, void ****compressors) const {
  char buf[500];
  Mkdir(dir);
  int num_players = Game::NumPlayers();
  Writer ***writers = new Writer **[num_players];
  *compressors = new void **[num_players];
  int max_street = Game::MaxStreet();
  for (int p = 0; p < num_players; ++p) {
    if (only_p != -1 && p != only_p) {
      writers[p] = nullptr;
      (*compressors)[p] = nullptr;
      continue;
    }
    if (! players_[p]) {
      writers[p] = nullptr;
      (*compressors)[p] = nullptr;
      continue;
    }
    writers[p] = new Writer *[max_street + 1];
    (*compressors)[p] = new void *[max_street + 1];
    for (int st = 0; st <= max_street; ++st) {
      AbstractCFRStreetValues *street_values = street_values_[st];
      if (street_values == nullptr) {
	writers[p][st] = nullptr;
	(*compressors)[p][st] = nullptr;
	continue;
      }
      CFRValueType value_type = street_values->MyType();
      char suffix;
      if (value_type == CFRValueType::CFR_CHAR)        suffix = 'c';
      else if (value_type == CFRValueType::CFR_SHORT)  suffix = 's';
      else if (value_type == CFRValueType::CFR_INT)    suffix = 'i';
      else if (value_type == CFRValueType::CFR_DOUBLE) suffix = 'd';
      sprintf(buf, "%s/%s.%s.%u.%u.%u.%u.p%u.%c", dir,
	      sumprobs ? "sumprobs" : "regrets", action_sequence.c_str(),
	      root_bd_st_, root_bd_, st, it, p, suffix);
      writers[p][st] = new Writer(buf);
      (*compressors)[p][st] = nullptr;
    }
  }
  return writers;
}

void CFRValues::Write(const char *dir, int it, Node *root, const string &action_sequence,
		      int only_p, bool sumprobs) const {
  int max_street = Game::MaxStreet();
  int num_players = Game::NumPlayers();
  bool ***seen = new bool **[max_street + 1];
  int root_st = root->Street();
  unique_ptr<int []> num_nonterminals(new int[num_players * (max_street + 1)]);
  CountNumNonterminals(root, num_nonterminals.get());
  for (int st = root_st; st <= max_street; ++st) {
    seen[st] = new bool *[num_players];
    for (int p = 0; p < num_players; ++p) {
      int index = p * (max_street + 1) + st;
      int num_nt = num_nonterminals[index];
      seen[st][p] = new bool[num_nt];
      for (int i = 0; i < num_nt; ++i) {
	seen[st][p][i] = false;
      }
    }
  }
  void ***compressors;
  Writer ***writers = InitializeWriters(dir, it, action_sequence, only_p, sumprobs, &compressors);
  Write(root, writers, compressors, seen);
  DeleteWriters(writers, compressors);
  for (int st = root_st; st <= max_street; ++st) {
    for (int p = 0; p < num_players; ++p) {
      delete [] seen[st][p];
    }
    delete [] seen[st];
  }
  delete [] seen;
}

void CFRValues::MergeInto(Node *full_node, Node *subgame_node, int root_bd_st, int root_bd,
			  const CFRValues &subgame_values, const Buckets &buckets,
			  int final_st) {
  if (full_node->Terminal()) return;
  int st = full_node->Street();
  if (st > final_st) return;
  int num_succs = full_node->NumSuccs();
  int p = full_node->PlayerActing();
  if (players_[p] && subgame_values.players_[p] && num_succs > 1 && streets_[st]) {
    street_values_[st]->MergeInto(full_node, subgame_node, root_bd_st, root_bd,
				  subgame_values.StreetValues(st), buckets);
  }
  for (int s = 0; s < num_succs; ++s) {
    MergeInto(full_node->IthSucc(s), subgame_node->IthSucc(s), root_bd_st, root_bd, subgame_values,
	      buckets, final_st);
  }
}

void CFRValues::MergeInto(const CFRValues &subgame_values, int root_bd, Node *full_root,
			  Node *subgame_root, const Buckets &buckets, int final_st) {
  int root_st = full_root->Street();
  MergeInto(full_root, subgame_root, root_st, root_bd, subgame_values, buckets, final_st);
}
