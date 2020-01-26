#ifndef _CFR_STREET_VALUES_H_
#define _CFR_STREET_VALUES_H_

#include <memory>

#include "cfr_value_type.h"

class Buckets;
class Node;
class Reader;
class Writer;

class AbstractCFRStreetValues {
 public:
  virtual ~AbstractCFRStreetValues(void) {}
  virtual void AllocateAndClear(Node *node, int p) = 0;
  virtual void RMProbs(int p, int nt, int offset,  int num_succs, int dsi, double *probs) const = 0;
  virtual void PureProbs(int p, int nt, int offset, int num_succs, double *probs) const = 0;
  virtual void ComputeOurValsBucketed(int pa, int nt, int num_hole_card_pairs, int num_succs,
				      int dsi, std::shared_ptr<double []> *succ_vals,
				      int *street_buckets,
				      std::shared_ptr<double []> vals) const = 0;
  virtual void ComputeOurVals(int pa, int nt, int num_hole_card_pairs, int num_succs, int dsi,
			      std::shared_ptr<double []> *succ_vals, int lbd,
			      std::shared_ptr<double []> vals) const = 0;
  virtual void SetCurrentAbstractedStrategy(int pa, int nt, int num_buckets, int num_succs, int dsi,
					    double *all_cs_probs) const = 0;
  virtual void Floor(int p, int nt, int num_succs, int floor) = 0;
  virtual bool Players(int p) const = 0;
  virtual void ReadNode(Node *node, Reader *reader, void *decompressor) = 0;
  virtual void ReadBoardValuesForNode(Node *node, Reader *reader, void *decompressor, int lbd,
				      int num_hole_card_pairs) = 0;
  virtual void WriteNode(Node *node, Writer *writer, void *compressor) const = 0;
  virtual void WriteBoardValuesForNode(Node *node, Writer *writer, void *compressor, int lbd,
				       int num_hole_card_pairs) const = 0;
  virtual CFRValueType MyType(void) const = 0;
  virtual void MergeInto(Node *full_node, Node *subgame_node, int root_bd_st, int root_bd,
			 const AbstractCFRStreetValues *subgame_values, const Buckets &buckets);
};

template <typename T>
class CFRStreetValues : public AbstractCFRStreetValues {
public:
  CFRStreetValues(int st, const bool *players, int num_holdings, int *num_nonterminals,
		  CFRValueType file_value_type);
  CFRStreetValues(CFRStreetValues *p0_values, CFRStreetValues *p1_values);
  virtual ~CFRStreetValues(void);
  CFRValueType MyType(void) const;
  int Street(void) const {return st_;}
  bool Players(int p) const {return players_[p];}
  int NumHoldings(void) const {return num_holdings_;}
  int NumNonterminals(int p) const {return num_nonterminals_[p];}
  T *AllValues(int p, int nt) const {return data_[p] ? data_[p][nt] : nullptr;}
  void AllocateAndClear(Node *node, int p);
  // Note: doesn't handle nodes with one succ
  void RMProbs(int p, int nt, int offset, int num_succs, int dsi, double *probs) const;
  // Note: doesn't handle nodes with one succ
  void PureProbs(int p, int nt, int offset, int num_succs, double *probs) const;
  void ComputeOurValsBucketed(int pa, int nt, int num_hole_card_pairs, int num_succs, int dsi,
			      std::shared_ptr<double []> *succ_vals, int *street_buckets,
			      std::shared_ptr<double []> vals) const;
  void ComputeOurVals(int pa, int nt, int num_hole_card_pairs, int num_succs, int dsi,
		      std::shared_ptr<double []> *succ_vals, int lbd,
		      std::shared_ptr<double []> vals) const;
  void SetCurrentAbstractedStrategy(int pa, int nt, int num_buckets, int num_succs, int dsi,
				    double *all_cs_probs) const;
  void Floor(int p, int nt, int num_succs, int floor);
  void Set(int p, int nt, int h, int num_succs, T *vals);
  void InitializeValuesForReading(int p, int nt, int num_succs);
  void ReadNode(Node *node, Reader *reader, void *decompressor);
  void ReadBoardValuesForNode(Node *node, Reader *reader, void *decompressor, int lbd,
			      int num_hole_card_pairs);
  void WriteNode(Node *node, Writer *writer, void *compressor) const;
  void WriteBoardValuesForNode(Node *node, Writer *writer, void *compressor, int lbd,
			       int num_hole_card_pairs) const;
  void MergeInto(Node *full_node, Node *subgame_node, int root_bd_st, int root_bd,
		 const CFRStreetValues<T> *subgame_values, const Buckets &buckets);
protected:
  void AllocateAndClear2(Node *node, int p);
  unsigned char ***GetUnsignedCharData(void);
  
  int st_;
  std::unique_ptr<bool []> players_;
  int num_holdings_;
  std::unique_ptr<int []> num_nonterminals_;
  T ***data_;
  CFRValueType file_value_type_;
};

template <typename T> void CopyUnabstractedValues(T *from_values, T *to_values, int st,
						  int num_succs, int from_bd, int to_bd);

#endif
