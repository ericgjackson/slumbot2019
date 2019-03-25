#ifndef _CFR_VALUES_H_
#define _CFR_VALUES_H_

#include <memory>
#include <string>

#include "cfr_street_values.h"
#include "cfr_value_type.h"

class BettingTree;
class Buckets;
class Node;

class CFRValues {
 public:
  CFRValues(const bool *players, const bool *streets, int root_bd, int root_bd_st,
	    const Buckets &buckets, const BettingTree *betting_tree);
  virtual ~CFRValues(void);
  AbstractCFRStreetValues *StreetValues(int st) const {
    return street_values_[st];
  }
  void AllocateAndClearInts(Node *node, int only_p);
  void AllocateAndClearDoubles(Node *node, int only_p);
  void CreateStreetValues(int st, CFRValueType value_type);
  Reader *InitializeReader(const char *dir, int p, int st, int it,
			   const std::string &action_sequence, int root_bd_st, int root_bd,
			   bool sumprobs, CFRValueType *value_type);
  void Read(const char *dir, int it, Node *root, const std::string &action_sequence, int only_p,
	    bool sumprobs);
  Writer ***InitializeWriters(const char *dir, int it, const std::string &action_sequence,
			      int only_p, bool sumprobs, void ****compressors) const;
  void Write(const char *dir, int it, Node *root, const std::string &action_sequence, int only_p,
	     bool sumprobs) const;
  void RMProbs(int st, int p, int nt, int offset, int num_succs, int dsi,
	       double *probs) const {
    street_values_[st]->RMProbs(p, nt, offset, num_succs, dsi, probs);
  }
  void PureProbs(int st, int p, int nt, int offset, int num_succs, double *probs) const {
    street_values_[st]->PureProbs(p, nt, offset, num_succs, probs);
  }
  void ReadNode(Node *node, Reader *reader, void *decompressor) {
    street_values_[node->Street()]->ReadNode(node, reader, decompressor);
  }
  void WriteNode(Node *node, Writer *writer, void *compressor) const {
    street_values_[node->Street()]->WriteNode(node, writer, compressor);
  }
  void MergeInto(const CFRValues &subgame_values, int root_bd, Node *full_root, Node *subgame_root,
		 const Buckets &buckets, int final_st);
  
 protected:
  void Read(Node *node, Reader ***readers, void ***decompressors, int only_p);
  void Write(Node *node, Writer ***writers, void ***compressors, bool ***seen) const;
  void MergeInto(Node *full_node, Node *subgame_node, int root_bd_st, int root_bd,
		 const CFRValues &subgame_values, const Buckets &buckets, int final_st);
  
  AbstractCFRStreetValues **street_values_;
  std::unique_ptr<bool []> players_;
  std::unique_ptr<bool []> streets_;
  int root_bd_;
  int root_bd_st_;
  std::unique_ptr<int []> num_holdings_;
  int **num_nonterminals_;
};

#endif
