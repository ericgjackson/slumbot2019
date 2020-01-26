#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "betting_tree.h"
#include "board_tree.h"
#include "buckets.h"
#include "cfr_street_values.h"
#include "cfr_utils.h"
#include "cfr_value_type.h"
#include "game.h"
#include "io.h"

using std::shared_ptr;
using std::unique_ptr;

void AbstractCFRStreetValues::MergeInto(Node *full_node, Node *subgame_node, int root_bd_st,
					int root_bd, const AbstractCFRStreetValues *subgame_values,
					const Buckets &buckets) {
  CFRValueType vtype = MyType();
  if (vtype == CFRValueType::CFR_CHAR) {
    CFRStreetValues<unsigned char> *street_values =
      dynamic_cast<CFRStreetValues<unsigned char> *>(this);
    const CFRStreetValues<unsigned char> *subgame_street_values =
      dynamic_cast<const CFRStreetValues<unsigned char> *>(subgame_values);
    street_values->MergeInto(full_node, subgame_node, root_bd_st, root_bd, subgame_street_values,
			     buckets);
  } else if (vtype == CFRValueType::CFR_SHORT) {
    CFRStreetValues<unsigned short> *street_values =
      dynamic_cast<CFRStreetValues<unsigned short> *>(this);
    const CFRStreetValues<unsigned short> *subgame_street_values =
      dynamic_cast<const CFRStreetValues<unsigned short> *>(subgame_values);
    street_values->MergeInto(full_node, subgame_node, root_bd_st, root_bd, subgame_street_values,
			     buckets);
  } else if (vtype == CFRValueType::CFR_INT) {
    CFRStreetValues<int> *street_values = dynamic_cast<CFRStreetValues<int> *>(this);
    const CFRStreetValues<int> *subgame_street_values =
      dynamic_cast<const CFRStreetValues<int> *>(subgame_values);
    street_values->MergeInto(full_node, subgame_node, root_bd_st, root_bd, subgame_street_values,
			     buckets);
  } else if (vtype == CFRValueType::CFR_DOUBLE) {
    CFRStreetValues<double> *street_values =
      dynamic_cast<CFRStreetValues<double> *>(this);
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
				    int *num_nonterminals, CFRValueType file_value_type) {
  file_value_type_ = file_value_type;
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
      num_nonterminals_[p] = num_nonterminals[p * (Game::MaxStreet() + 1) + st_];
    } else {
      num_nonterminals_[p] = 0;
    }
  }
  data_ = nullptr;
}

template <typename T>
CFRStreetValues<T>::CFRStreetValues(CFRStreetValues<T> *p0_values, CFRStreetValues<T> *p1_values) {
  st_ = p0_values->Street();
  players_.reset(new bool[2]);
  players_[0] = true;
  players_[1] = true;
  num_holdings_ = p0_values->NumHoldings();
  num_nonterminals_.reset(new int[2]);
  num_nonterminals_[0] = p0_values->NumNonterminals(0);
  num_nonterminals_[1] = p1_values->NumNonterminals(1);
  data_ = new T **[2];
  data_[0] = p0_values->data_[0];
  p0_values->data_[0] = nullptr;
  data_[1] = p1_values->data_[1];
  p1_values->data_[1] = nullptr;
}

template <typename T>
CFRStreetValues<T>::~CFRStreetValues(void) {
  // This can happen for values for an all-in subtree.
  if (data_ == nullptr) return;
  int num_players = Game::NumPlayers();
  for (int p = 0; p < num_players; ++p) {
    if (data_[p] == nullptr) continue;
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
  return CFRValueType::CFR_CHAR;
}

template <>
CFRValueType CFRStreetValues<unsigned short>::MyType(void) const {
  return CFRValueType::CFR_SHORT;
}

template <>
CFRValueType CFRStreetValues<int>::MyType(void) const {
  return CFRValueType::CFR_INT;
}

template <>
CFRValueType CFRStreetValues<unsigned int>::MyType(void) const {
  return CFRValueType::CFR_INT;
}

template <>
CFRValueType CFRStreetValues<double>::MyType(void) const {
  return CFRValueType::CFR_DOUBLE;
}

template <typename T>
void CFRStreetValues<T>::AllocateAndClear2(Node *node, int p) {
  if (node->Terminal()) return;
  int st = node->Street();
  if (st > st_) return;
  int num_succs = node->NumSuccs();
  if (st == st_) {
    int pa = node->PlayerActing();
    if (pa == p) {
      int nt = node->NonterminalID();
      // Check for reentrant nodes
      if (data_[p][nt] == nullptr)  {
	int num_actions = num_holdings_ * num_succs;
	data_[p][nt] = new T[num_actions];
	for (int a = 0; a < num_actions; ++a) {
	  data_[p][nt][a] = 0;
	}
      }
    }
  }
  for (int s = 0; s < num_succs; ++s) {
    AllocateAndClear2(node->IthSucc(s), p);
  }
}

template <typename T>
void CFRStreetValues<T>::AllocateAndClear(Node *node, int p) {
  if (! players_[p]) return;
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
  AllocateAndClear2(node, p);
}

// Compute probs from the current hand values using regret matching.  Normally the values we do this
// to are regrets, but they may also be sumprobs.
// May eventually want to take parameters for nonneg, explore, uniform, nonterminal succs,
// num nonterminal succs.
// Note: doesn't handle nodes with one succ
// Since this is invoked in an inner loop, might be nice to pull the array lookups out
// (data_[p][nt][offset]).
template <typename T>
void CFRStreetValues<T>::RMProbs(int p, int nt, int offset, int num_succs, int dsi,
				 double *probs) const {
#if 0
  fprintf(stderr, "RMProbs p %i nt %i offset %i\n", p, nt, offset);
  fprintf(stderr, "data_ %p\n", data_);
  fprintf(stderr, "data_[p] %p\n", data_[p]);
  fprintf(stderr, "data_[p][nt] %p\n", data_[p][nt]);
#endif
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

// Note: doesn't handle nodes with one succ
template <typename T>
void CFRStreetValues<T>::PureProbs(int p, int nt, int offset, int num_succs,
				   double *probs) const {
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

// Uses the current strategy (from regrets or sumprobs) to compute the weighted average of
// the successor values.  This version for systems employing card abstraction.
template <typename T>
void CFRStreetValues<T>::ComputeOurValsBucketed(int pa, int nt, int num_hole_card_pairs,
						int num_succs, int dsi,
						shared_ptr<double []> *succ_vals,
						int *street_buckets, shared_ptr<double []> vals)
  const {
  const T *all_cs_vals = data_[pa][nt];
  ::ComputeOurValsBucketed(all_cs_vals, num_hole_card_pairs, num_succs, dsi, succ_vals,
			   street_buckets, vals);
}

// Uses the current strategy (from regrets or sumprobs) to compute the weighted average of
// the successor values.  This version for systems employing no card abstraction.
template <typename T>
void CFRStreetValues<T>::ComputeOurVals(int pa, int nt, int num_hole_card_pairs, int num_succs,
					int dsi, shared_ptr<double []> *succ_vals, int lbd,
					shared_ptr<double []> vals)
  const {
  const T *all_cs_vals = data_[pa][nt];
  ::ComputeOurVals(all_cs_vals, num_hole_card_pairs, num_succs, dsi, succ_vals, lbd, vals);
}

// Set the current strategy probs from the regrets.  Used for abstracted systems in CFR+.
template <typename T>
void CFRStreetValues<T>::SetCurrentAbstractedStrategy(int pa, int nt, int num_buckets,
						      int num_succs, int dsi,
						      double *all_cs_probs) const {
  const T *all_regrets = data_[pa][nt];
  ::SetCurrentAbstractedStrategy(all_regrets, num_buckets, num_succs, dsi, all_cs_probs);
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
void CFRStreetValues<T>::WriteNode(Node *node, Writer *writer, void *compressor) const {
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

// Doesn't support abstraction
template <typename T>
void CFRStreetValues<T>::WriteBoardValuesForNode(Node *node, Writer *writer, void *compressor,
						 int lbd, int num_hole_card_pairs) const {
  int p = node->PlayerActing();
  int nt = node->NonterminalID();
  int num_succs = node->NumSuccs();
  if (num_succs <= 1) return;
  // if (compressed_streets_[st]) {}
  if (compressor) {
    fprintf(stderr, "Compression not supported yet\n");
    exit(-1);
  } else {
    int offset = lbd * num_hole_card_pairs * num_succs;
    int num_actions = num_hole_card_pairs * num_succs;
    for (int a = 0; a < num_actions; ++a) {
      writer->Write(data_[p][nt][offset + a]);
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
  if (data_[p][nt] == nullptr) {
    int num_actions = num_holdings_ * num_succs;
    data_[p][nt] = new T[num_actions];
  }
  // Don't need to zero
}

// raw_probs need not sum to 1.0
// raw_probs can contain negative values (e.g., from regrets).  Negative values are treated like
// zero.
// num_succs must be at least 1
// We take care to make sure quantized probabilities always sum to exactly 255.
static void Quantize(double *raw_probs, int num_succs, unsigned char *quantized_probs) {
  double sum = 0;
  for (int s = 0; s < num_succs; ++s) {
    sum += raw_probs[s];
  }
  int q_sum = 0;
  for (int s = 0; s < num_succs - 1; ++s) {
    double raw = raw_probs[s];
    if (raw < 0) {
      quantized_probs[s] = 0;
      continue;
    }
    // Note that we multiply by 256.0, not 255.0.  I want 256 equally sized buckets.
    // But I want the maximum value to be 255.
    // Corner case is that raw_probs[s] == sum which could lead to a quantized prob value of 256.
    int q = (raw_probs[s] / sum) * 256.0;
    if (q_sum + q > 255) q = 255 - q_sum;
    quantized_probs[s] = q;
    q_sum += q;
  }
  if (q_sum > 255) {
    fprintf(stderr, "q_sum > 255?!?  %i\n", q_sum);
    for (int s = 0; s < num_succs; ++s) {
      fprintf(stderr, "raw %f q %i\n", raw_probs[s], (int)quantized_probs[s]);
    }
    exit(-1);
  }
  quantized_probs[num_succs - 1] = 255 - q_sum;
}

template <>
unsigned char ***CFRStreetValues<unsigned char>::GetUnsignedCharData(void) {
  return data_;
}

template <>
unsigned char ***CFRStreetValues<unsigned short>::GetUnsignedCharData(void) {
  fprintf(stderr, "Cannot call GetUnsignedCharData()\n");
  exit(-1);
}

template <>
unsigned char ***CFRStreetValues<int>::GetUnsignedCharData(void) {
  fprintf(stderr, "Cannot call GetUnsignedCharData()\n");
  exit(-1);
}

template <>
unsigned char ***CFRStreetValues<unsigned int>::GetUnsignedCharData(void) {
  fprintf(stderr, "Cannot call GetUnsignedCharData()\n");
  exit(-1);
}

template <>
unsigned char ***CFRStreetValues<double>::GetUnsignedCharData(void) {
  fprintf(stderr, "Cannot call GetUnsignedCharData()\n");
  exit(-1);
}

template <typename T>
void CFRStreetValues<T>::ReadNode(Node *node, Reader *reader, void *decompressor) {
  int num_succs = node->NumSuccs();
  if (num_succs <= 1) return;
  int p = node->PlayerActing();
  int nt = node->NonterminalID();
  // Assume this is because this node is reentrant.
  if (data_ && data_[p] && data_[p][nt]) {
    return;
  }
  InitializeValuesForReading(p, nt, num_succs);
  if (decompressor) {
    fprintf(stderr, "Decompression not supported yet\n");
    exit(-1);
  }
  int num_actions = num_holdings_ * num_succs;
  if (file_value_type_ == CFRValueType::CFR_CHAR) {
    for (int a = 0; a < num_actions; ++a) {
      reader->ReadOrDie(&data_[p][nt][a]);
    }
  } else if (file_value_type_ == CFRValueType::CFR_SHORT) {
    if (sizeof(T) == 1) {
      // Quantizing
      unique_ptr<double []> succ_probs(new double[num_succs]);
      unsigned char ***data = GetUnsignedCharData();
      for (int h = 0; h < num_holdings_; ++h) {
	for (int s = 0; s < num_succs; ++s) {
	  unsigned short us;
	  reader->ReadOrDie(&us);
	  succ_probs[s] = us;
	}
	Quantize(succ_probs.get(), num_succs, &data[p][nt][h * num_succs]);
      }
    } else {
      for (int a = 0; a < num_actions; ++a) {
	reader->ReadOrDie(&data_[p][nt][a]);
      }
    }
  } else if (file_value_type_ == CFRValueType::CFR_INT) {
    if (sizeof(T) == 1) {
      unique_ptr<double []> succ_probs(new double[num_succs]);
      unsigned char ***data = GetUnsignedCharData();
      for (int h = 0; h < num_holdings_; ++h) {
	for (int s = 0; s < num_succs; ++s) {
	  int i;
	  reader->ReadOrDie(&i);
	  succ_probs[s] = i;
	}
	Quantize(succ_probs.get(), num_succs, &data[p][nt][h * num_succs]);
      }
    } else {
      for (int a = 0; a < num_actions; ++a) {
	reader->ReadOrDie(&data_[p][nt][a]);
      }
    }
  } else if (file_value_type_ == CFRValueType::CFR_DOUBLE) {
    if (sizeof(T) == 1) {
      unique_ptr<double []> succ_probs(new double[num_succs]);
      unsigned char ***data = GetUnsignedCharData();
      for (int h = 0; h < num_holdings_; ++h) {
	for (int s = 0; s < num_succs; ++s) {
	  reader->ReadOrDie(&succ_probs[s]);
	}
	Quantize(succ_probs.get(), num_succs, &data[p][nt][h * num_succs]);
      }
    } else {
      for (int a = 0; a < num_actions; ++a) {
	reader->ReadOrDie(&data_[p][nt][a]);
#if 0
	if (node->Street() == 3 && p == 0) {
	  printf("nt %i h %i s %i d %f\n", nt, a / num_succs, a % num_succs,
		 (double)data_[p][nt][a]);
	}
#endif
      }
    }
  }
}

// Doesn't support abstraction.
// Doesn't support reentrancy.
// Normally used for resolved subgames.  Read just one board's data from disk.
template <typename T>
void CFRStreetValues<T>::ReadBoardValuesForNode(Node *node, Reader *reader, void *decompressor,
						int lbd, int num_hole_card_pairs) {
  int num_succs = node->NumSuccs();
  if (num_succs <= 1) return;
  int p = node->PlayerActing();
  int nt = node->NonterminalID();
  InitializeValuesForReading(p, nt, num_succs);
  if (decompressor) {
    fprintf(stderr, "Decompression not supported yet\n");
    exit(-1);
  }
  int offset = lbd * num_hole_card_pairs * num_succs;
  int num_actions = num_hole_card_pairs * num_succs;
  for (int a = 0; a < num_actions; ++a) {
    reader->ReadOrDie(&data_[p][nt][a + offset]);
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
	int gbd = BoardTree::GlobalIndex(root_bd_st, root_bd, st_, lbd);
	for (int hcp = 0; hcp < num_hole_card_pairs; ++hcp) {
	  for (int s = 0; s < num_succs; ++s) {
	    T v = vals[a++];
	    int new_a = gbd * num_hole_card_pairs * num_succs + hcp * num_succs + s;
	    data_[p][full_nt][new_a] = v;
	  }
	}
      }
    } else {
      if (num_holdings_ != subgame_num_holdings) {
	fprintf(stderr, "CFRStreetValues::MergeInto(): Mismatched num. of buckets\n");
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

template <typename T> void CopyNValues(T *from_values, T *to_values, int num) {
  for (int i = 0; i < num; ++i) to_values[i] = from_values[i];
}

template void CopyNValues<double>(double *from_values, double *to_values, int num);
template void CopyNValues<int>(int *from_values, int *to_values, int num);

template <typename T> void CopyUnabstractedValues(T *from_values, T *to_values, int st,
						  int num_succs, int from_bd, int to_bd) {
  int num_hole_card_pairs = Game::NumHoleCardPairs(st);
  int num_values = num_hole_card_pairs * num_succs;
  int from_offset = from_bd * num_values;
  int to_offset = to_bd * num_values;
  CopyNValues(from_values + from_offset, to_values + to_offset, num_values);
}

template void CopyUnabstractedValues<double>(double *from_values, double *to_values, int st,
					     int num_succs, int from_bd, int to_bd);
template void CopyUnabstractedValues<int>(int *from_values, int *to_values, int st,
					  int num_succs, int from_bd, int to_bd);

#if 0
template <typename T>
void CFRStreetValues<T>::CopyUnabstractedValues(Node *from_node, CFRStreetValues<T> *to_csv,
						Node *to_node, int from_bd, int to_bd) {
  int pa = from_node->PlayerActing(0);
  int from_num_succs = from_node->NumSuccs();
  int to_num_succs = to_node->NumSuccs();
  if (from_num_succs != to_num_succs) {
    fprintf(stderr, "Mismatched num_succs\n");
    exit(-1);
  }
  T *from_values = AllValues(pa, from_node->NonterminalID());
  T *to_values = to_csv->AllValues(pa, to_node->NonterminalID());
  CopyUnabstractedValues(from_values, to_values, st_, from_num_succs, from_bd, to_bd);
}
#endif

template class CFRStreetValues<int>;
template class CFRStreetValues<double>;
template class CFRStreetValues<unsigned char>;
template class CFRStreetValues<unsigned short>;
