#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "betting_tree.h"
#include "board_tree.h"
#include "buckets.h"
#include "cfr_street_values.h"
#include "cfr_value_type.h"
#include "game.h"
#include "io.h"

using std::unique_ptr;

void AbstractCFRStreetValues::MergeInto(Node *full_node, Node *subgame_node,
					int root_bd_st, int root_bd,
					const AbstractCFRStreetValues *subgame_values,
					const Buckets &buckets) {
  CFRValueType vtype = MyType();
  if (vtype == CFR_CHAR) {
    CFRStreetValues<unsigned char> *street_values =
      dynamic_cast<CFRStreetValues<unsigned char> *>(this);
    const CFRStreetValues<unsigned char> *subgame_street_values =
      dynamic_cast<const CFRStreetValues<unsigned char> *>(subgame_values);
    street_values->MergeInto(full_node, subgame_node, root_bd_st, root_bd, subgame_street_values,
			     buckets);
  } else if (vtype == CFR_SHORT) {
    CFRStreetValues<unsigned short> *street_values =
      dynamic_cast<CFRStreetValues<unsigned short> *>(this);
    const CFRStreetValues<unsigned short> *subgame_street_values =
      dynamic_cast<const CFRStreetValues<unsigned short> *>(subgame_values);
    street_values->MergeInto(full_node, subgame_node, root_bd_st, root_bd, subgame_street_values,
			     buckets);
  } else if (vtype == CFR_INT) {
    CFRStreetValues<int> *street_values = dynamic_cast<CFRStreetValues<int> *>(this);
    const CFRStreetValues<int> *subgame_street_values =
      dynamic_cast<const CFRStreetValues<int> *>(subgame_values);
    street_values->MergeInto(full_node, subgame_node, root_bd_st, root_bd, subgame_street_values,
			     buckets);
  } else if (vtype == CFR_DOUBLE) {
    CFRStreetValues<double> *street_values = dynamic_cast<CFRStreetValues<double> *>(this);
    const CFRStreetValues<double> *subgame_street_values =
      dynamic_cast<const CFRStreetValues<double> *>(subgame_values);
    street_values->MergeInto(full_node, subgame_node, root_bd_st, root_bd, subgame_street_values,
			     buckets);
  } else {
    fprintf(stderr, "Unknown CFRValueType\n");
    exit(-1);
  }
}

template <typename T>
CFRStreetValues<T>::CFRStreetValues(int st, const bool *players, int num_holdings,
				    int **num_nonterminals) {
  st_ = st;
  int num_players = Game::NumPlayers();
  players_.reset(new bool[num_players]);
  for (int p = 0; p < num_players; ++p) {
    players_[p] = players == nullptr || players[p];
  }
  num_holdings_ = num_holdings;
  num_nonterminals_.reset(new int[num_players]);
  for (int p = 0; p < num_players; ++p) {
    if (players_[p]) {
      num_nonterminals_[p] = num_nonterminals[p][st_];
    } else {
      num_nonterminals_[p] = 0;
    }
  }
  data_ = nullptr;
}

template <typename T>
CFRStreetValues<T>::~CFRStreetValues(void) {
  int num_players = Game::NumPlayers();
  for (int p = 0; p < num_players; ++p) {
    int num_nt = num_nonterminals_[p];
    for (int i = 0; i < num_nt; ++i) {
      delete [] data_[p][i];
    }
    delete [] data_[p];
  }
  delete [] data_;
}

template <>
CFRValueType CFRStreetValues<unsigned char>::MyType(void) const {
  return CFR_CHAR;
}

template <>
CFRValueType CFRStreetValues<unsigned short>::MyType(void) const {
  return CFR_SHORT;
}

template <>
CFRValueType CFRStreetValues<int>::MyType(void) const {
  return CFR_INT;
}

template <>
CFRValueType CFRStreetValues<unsigned int>::MyType(void) const {
  return CFR_INT;
}

template <>
CFRValueType CFRStreetValues<double>::MyType(void) const {
  return CFR_DOUBLE;
}

template <typename T>
void CFRStreetValues<T>::AllocateAndClear2(Node *node) {
  if (node->Terminal()) return;
  int st = node->Street();
  if (st > st_) return;
  int num_succs = node->NumSuccs();
  if (st == st_) {
    int pa = node->PlayerActing();
    if (players_[pa]) {
      int nt = node->NonterminalID();
      // Check for reentrant nodes
      if (data_[pa][nt] == nullptr)  {
	int num_actions = num_holdings_ * num_succs;
	data_[pa][nt] = new T[num_actions];
	for (int a = 0; a < num_actions; ++a) {
	  data_[pa][nt][a] = 0;
	}
      }
    }
  }
  for (int s = 0; s < num_succs; ++s) {
    AllocateAndClear2(node->IthSucc(s));
  }
}

template <typename T>
void CFRStreetValues<T>::AllocateAndClear(Node *node) {
  int num_players = Game::NumPlayers();
  data_ = new T **[num_players];
  for (int p = 0; p < num_players; ++p) {
    if (players_[p]) {
      int num_nt = num_nonterminals_[p];
      data_[p] = new T *[num_nt];
      for (int i = 0; i < num_nt; ++i) data_[p][i] = nullptr;
    } else {
      data_[p] = nullptr;
    }
  }
  AllocateAndClear2(node);
}

// Compute probs from the current hand values using regret matching.
// Normally the values we do this to are regrets, but they may also be
// sumprobs.
// May eventually want to take parameters for nonneg, explore, uniform,
// nonterminal succs, num nonterminal succs.
template <typename T>
void CFRStreetValues<T>::RMProbs(int p, int nt, int offset, int num_succs, int dsi,
				 double *probs) const {
  T *my_vals = &data_[p][nt][offset];
  double sum = 0;
  for (int s = 0; s < num_succs; ++s) {
    T v = my_vals[s];
    if (v > 0) sum += v;
  }
  if (sum == 0) {
    for (int s = 0; s < num_succs; ++s) {
      probs[s] = s == dsi ? 1.0 : 0;
    }
  } else {
    for (int s = 0; s < num_succs; ++s) {
      T v = my_vals[s];
      if (v > 0) probs[s] = v / sum;
      else       probs[s] = 0;
    }
  }
}

template <class T>
void CFRStreetValues<T>::PureProbs(int p, int nt, int offset, int num_succs, double *probs) const {
  T *my_vals = &data_[p][nt][offset];
  T max_v = my_vals[0];
  int best_s = 0;
  for (int s = 1; s < num_succs; ++s) {
    T val = my_vals[s];
    if (val > max_v) {
      max_v = val;
      best_s = s;
    }
  }
  for (int s = 0; s < num_succs; ++s) {
    probs[s] = s == best_s ? 1.0 : 0.0;
  }
}

template <typename T>
void CFRStreetValues<T>::Floor(int p, int nt, int num_succs, int floor) {
  int num = num_holdings_ * num_succs;
  for (int i = 0; i < num; ++i) {
    if (data_[p][nt][i] < floor) data_[p][nt][i] = floor;
  }
}

template <typename T>
void CFRStreetValues<T>::Set(int p, int nt, int h, int num_succs, T *vals) {
  int offset = h * num_succs;
  for (int s = 0; s < num_succs; ++s) {
    data_[p][nt][offset + s] = vals[s];
  }
}

template <typename T>
void CFRStreetValues<T>::WriteNode(Node *node, Writer *writer,
				   void *compressor) const {
  int p = node->PlayerActing();
  int nt = node->NonterminalID();
  int num_succs = node->NumSuccs();
  if (num_succs <= 1) return;
  // if (compressed_streets_[st]) {}
  if (compressor) {
    fprintf(stderr, "Compression not supported yet\n");
    exit(-1);
  } else {
    int num_actions = num_holdings_ * num_succs;
    for (int a = 0; a < num_actions; ++a) {
      writer->Write(data_[p][nt][a]);
    }
  }
}

template <typename T>
void CFRStreetValues<T>::InitializeValuesForReading(int p, int nt, int num_succs) {
  if (data_ == nullptr) {
    int num_players = Game::NumPlayers();
    data_ = new T **[num_players];
    for (int p = 0; p < num_players; ++p) data_[p] = nullptr;
  }
  if (data_[p] == nullptr) {
    int num_nt = num_nonterminals_[p];
    data_[p] = new T *[num_nt];
    for (int i = 0; i < num_nt; ++i) data_[p][i] = nullptr;
  }
  int num_actions = num_holdings_ * num_succs;
  data_[p][nt] = new T[num_actions];
  // Don't need to zero
}

template <typename T>
void CFRStreetValues<T>::ReadNode(Node *node, Reader *reader, void *decompressor) {
  int num_succs = node->NumSuccs();
  if (num_succs <= 1) return;
  int p = node->PlayerActing();
  int nt = node->NonterminalID();
  // Assume this is because this node is reentrant.
  if (data_ && data_[p] && data_[p][nt]) return;
  InitializeValuesForReading(p, nt, num_succs);
  if (decompressor) {
    fprintf(stderr, "Decompression not supported yet\n");
    exit(-1);
  }
  int num_actions = num_holdings_ * num_succs;
  for (int a = 0; a < num_actions; ++a) {
    reader->ReadOrDie(&data_[p][nt][a]);
  }
}

template <typename T>
void CFRStreetValues<T>::MergeInto(Node *full_node, Node *subgame_node, int root_bd_st,
				   int root_bd, const CFRStreetValues<T> *subgame_values,
				   const Buckets &buckets) {
  int num_succs = full_node->NumSuccs();
  int p = full_node->PlayerActing();
  if (players_[p] && subgame_values->players_[p] && num_succs > 1) {
    // Lazily allocate (and clear)
    if (data_ == nullptr) {
      int num_players = Game::NumPlayers();
      data_ = new T **[num_players];
      for (int p = 0; p < num_players; ++p) data_[p] = nullptr;
    }
    if (data_[p] == nullptr) {
      int num_nt = num_nonterminals_[p];
      data_[p] = new T *[num_nt];
      for (int i = 0; i < num_nt; ++i) data_[p][i] = nullptr;
    }

    int num_actions = num_holdings_ * num_succs;
    int full_nt = full_node->NonterminalID();
    int subgame_nt = subgame_node->NonterminalID();
    if (data_[p][full_nt] == nullptr) {
      data_[p][full_nt] = new T[num_actions];
      // Zeroing out is needed for bucketed case
      for (int a = 0; a < num_actions; ++a) {
	data_[p][full_nt][a] = 0;
      }
    }
    
    int subgame_num_holdings = subgame_values->num_holdings_;
    if (buckets.None(st_)) {
      int num_hole_card_pairs = Game::NumHoleCardPairs(st_);
      int num_local_boards =
	BoardTree::NumLocalBoards(root_bd_st, root_bd, st_);
      if (subgame_num_holdings != num_hole_card_pairs * num_local_boards) {
	fprintf(stderr, "Num holdings didn't match what was expected\n");
	fprintf(stderr, "subgame_num_holdings %u\n", subgame_num_holdings);
	fprintf(stderr, "nhcp %u\n", num_hole_card_pairs);
	fprintf(stderr, "nlb %u\n", num_local_boards);
	fprintf(stderr, "root_bd_st %u st %u\n", root_bd_st, st_);
	exit(-1);
      }
      int a = 0;
      T *vals = subgame_values->data_[p][subgame_nt];
      for (int lbd = 0; lbd < num_local_boards; ++lbd) {
	int gbd =
	  BoardTree::GlobalIndex(root_bd_st, root_bd, st_, lbd);
	for (int hcp = 0; hcp < num_hole_card_pairs; ++hcp) {
	  for (int s = 0; s < num_succs; ++s) {
	    T v = vals[a++];
	    int new_a = gbd * num_hole_card_pairs * num_succs +
	      hcp * num_succs + s;
	    data_[p][full_nt][new_a] = v;
	  }
	}
      }
    } else {
      if (num_holdings_ != subgame_num_holdings) {
	fprintf(stderr,
		"CFRStreetValues::MergeInto(): Mismatched num. of buckets\n");
	exit(-1);
      }
      int num_actions = num_holdings_ * num_succs;
      T *vals = subgame_values->data_[p][subgame_nt];
      for (int a = 0; a < num_actions; ++a) {
	data_[p][full_nt][a] += vals[a];
      }
    }
  }
}


template class CFRStreetValues<int>;
template class CFRStreetValues<double>;
template class CFRStreetValues<unsigned char>;
template class CFRStreetValues<unsigned short>;
