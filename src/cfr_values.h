#ifndef _CFR_VALUES_H_
#define _CFR_VALUES_H_

#include <memory>
#include <string>

#include "betting_tree.h"
#include "cfr_street_values.h"
#include "cfr_value_type.h"

class BettingTree;
class BettingTrees;
class Buckets;
class Node;

class CFRValues {
 public:
  CFRValues(const bool *players, const bool *streets, int root_bd, int root_bd_st,
	    const Buckets &buckets, const BettingTree *betting_tree);
  CFRValues(const bool *players, const bool *streets, int root_bd, int root_bd_st,
	    const Buckets &buckets, const BettingTrees &betting_trees);
  CFRValues(const CFRValues &p0_values, const CFRValues &p1_values);
  virtual ~CFRValues(void);
  AbstractCFRStreetValues *StreetValues(int st) const {return street_values_[st];}
  void AllocateAndClear(const BettingTree *betting_tree, CFRValueType *value_types,
			bool quantize, int only_p);
  void AllocateAndClear(const BettingTree *betting_tree, CFRValueType value_type, bool quantize,
			int only_p);
  void CreateStreetValues(int st, CFRValueType value_type, bool quantize);
  void Read(const char *dir, int it, const BettingTree *betting_tree,
	    const std::string &action_sequence, int only_p, bool sumprobs, bool quantize);
  void ReadAsymmetric(const char *dir, int it, const BettingTrees &betting_trees,
		      const std::string &action_sequence, int only_p, bool sumprobs,
		      bool quantize);
  void Write(const char *dir, int it, Node *root, const std::string &action_sequence, int only_p,
	     bool sumprobs) const;
  // Note: doesn't handle nodes with one succ
  void RMProbs(int st, int p, int nt, int offset, int num_succs, int dsi,
	       double *probs) const {
    street_values_[st]->RMProbs(p, nt, offset, num_succs, dsi, probs);
  }
  // Note: doesn't handle nodes with one succ
  void PureProbs(int st, int p, int nt, int offset, int num_succs, double *probs) const {
    street_values_[st]->PureProbs(p, nt, offset, num_succs, probs);
  }
  void ReadNode(Node *node, Reader *reader, void *decompressor) {
    street_values_[node->Street()]->ReadNode(node, reader, decompressor);
  }
  void ReadBoardValuesForNode(Node *node, Reader *reader, void *decompressor, int lbd,
			      int num_hole_card_pairs) {
    street_values_[node->Street()]->ReadBoardValuesForNode(node, reader, decompressor, lbd,
							   num_hole_card_pairs);
  }
  void WriteBoardValuesForNode(Node *node, Writer *writer, void *compressor, int lbd,
			       int num_hole_card_pairs) const {
    street_values_[node->Street()]->WriteBoardValuesForNode(node, writer, compressor, lbd,
							    num_hole_card_pairs);
  }
  void MergeInto(const CFRValues &subgame_values, int root_bd, Node *full_root, Node *subgame_root,
		 const Buckets &buckets, int final_st);
  bool Player(int p) const {return players_[p];}
  bool Street(int st) const {return streets_[st];}
  int NumHoldings(int st) const {return num_holdings_[st];}
  int RootSt(void) const {return root_bd_st_;}
  int RootBd(void) const {return root_bd_;}
 protected:
  void Read(Node *node, Reader ***readers, void ***decompressors, int p);
  Reader *InitializeReader(const char *dir, int p, int st, int it,
			   const std::string &action_sequence, int root_bd_st, int root_bd,
			   bool sumprobs, CFRValueType *value_type);
  void Write(Node *node, Writer ***writers, void ***compressors, bool ***seen) const;
  Writer ***InitializeWriters(const char *dir, int it, const std::string &action_sequence,
			      int only_p, bool sumprobs, void ****compressors) const;
  void MergeInto(Node *full_node, Node *subgame_node, int root_bd_st, int root_bd,
		 const CFRValues &subgame_values, const Buckets &buckets, int final_st);
  void Initialize(const bool *players, const bool *streets, int root_bd, int root_bd_st,
		  const Buckets &buckets);
  
  std::unique_ptr<AbstractCFRStreetValues * []> street_values_;
  std::unique_ptr<bool []> players_;
  std::unique_ptr<bool []> streets_;
  int root_bd_;
  int root_bd_st_;
  std::unique_ptr<int []> num_holdings_;
  std::unique_ptr<int []> num_nonterminals_;
};

#endif
