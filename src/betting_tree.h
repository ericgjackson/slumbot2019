#ifndef _BETTING_TREE_H_
#define _BETTING_TREE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "constants.h"

class BettingAbstraction;
class Reader;
class Writer;

class Node {
public:
  Node(int id, int street, int player_acting, const std::shared_ptr<Node> &call_succ,
       const std::shared_ptr<Node> &fold_succ, std::vector< std::shared_ptr<Node> > *bet_succs,
       int num_remaining, int bet_to);
  Node(Node *node);
  Node(int id, int last_bet_to, int num_succs, unsigned short flags, unsigned char player_acting,
       unsigned char num_remaining);
  ~Node(void) {}

  int PlayerActing(void) const {return player_acting_;}
  bool Terminal(void) const {return NumSuccs() == 0;}
  int TerminalID(void) const {return Terminal() ? id_ : -1;}
  int NonterminalID(void) const {return Terminal() ? -1 : id_;}
  int Street(void) const {
    return (int)((flags_ & kStreetMask) >> kStreetShift);
  }
  int NumSuccs(void) const {return num_succs_;}
  Node *IthSucc(int i) const {return succs_[i].get();}
  int NumRemaining(void) const {return num_remaining_;}
  bool Showdown(void) const {return Terminal() && num_remaining_ > 1;}
  int LastBetTo(void) const {return last_bet_to_;}
  int CallSuccIndex(void) const;
  int FoldSuccIndex(void) const;
  int DefaultSuccIndex(void) const;
  bool StreetInitial(void) const;
  std::string ActionName(int s);
  void PrintTree(int depth, const std::string &name,
		 std::vector< std::vector< std::vector<bool> > > *seen, int last_street) const;
  bool HasCallSucc(void) const {return (bool)(flags_ & kHasCallSuccFlag);}
  bool HasFoldSucc(void) const {return (bool)(flags_ & kHasFoldSuccFlag);}
  int ID(void) const {return id_;}
  unsigned short Flags(void) const {return flags_;}
  void SetTerminalID(int id) {id_ = id;}
  void SetNonterminalID(int id) {id_ = id;}
  void SetNumSuccs(int n) {num_succs_ = n;}
  void SetIthSucc(int s, std::shared_ptr<Node> succ) {succs_[s] = succ;}
  void SetHasCallSuccFlag(void) {flags_ |= kHasCallSuccFlag;}
  void SetHasFoldSuccFlag(void) {flags_ |= kHasFoldSuccFlag;}
  void ClearHasCallSuccFlag(void) {flags_ &= ~kHasCallSuccFlag;}
  void ClearHasFoldSuccFlag(void) {flags_ &= ~kHasFoldSuccFlag;}

  // Bit 0: has-call-succ
  // Bit 1: has-fold-succ
  // Bit 2: special (not currently used)
  // Bits 3,4: street
  static const unsigned short kHasCallSuccFlag = 1;
  static const unsigned short kHasFoldSuccFlag = 2;
  static const unsigned short kSpecialFlag = 4;
  static const unsigned short kStreetMask = 24;
  static const int kStreetShift = 3;

 private:
  std::unique_ptr<std::shared_ptr<Node> []> succs_;
  int id_;
  short last_bet_to_;
  short num_succs_;
  unsigned short flags_;
  unsigned char player_acting_;
  unsigned char num_remaining_;
};

class BettingTree {
 public:
  // Normal constructor
  BettingTree(const BettingAbstraction &ba);
  // For asymmetric betting abstractions
  BettingTree(const BettingAbstraction &ba, int target_player);
  // For cloning a (sub)tree
  BettingTree(Node *subtree_root);
  virtual ~BettingTree(void) {}
  void Display(void) const;
  void Display(Node *node) const;
  Node *Root(void) const {return root_.get();}
  int NumTerminals(void) const {return num_terminals_;}
  Node *Terminal(int i) const {return terminals_[i];}
  int NumNonterminals(int p, int st) const;
  int InitialStreet(void) const {return initial_street_;}

 private:
  void FillTerminalArray(void);
  void FillTerminalArray(Node *node);
  std::shared_ptr<Node> Clone(Node *old_n, int *num_terminals);
  void Initialize(int target_player, const BettingAbstraction &ba);
  std::shared_ptr<Node> Read(Reader *reader,
			     std::unordered_map< int, std::shared_ptr<Node> > *maps);

  std::shared_ptr<Node> root_;
  int initial_street_;
  int num_terminals_;
  std::unique_ptr<Node * []> terminals_;
  std::unique_ptr<int []> num_nonterminals_;
};

bool TwoSuccsCorrespond(Node *node1, int s1, Node *node2, int s2);
std::unique_ptr<int []> GetSuccMapping(Node *acting_node, Node *opp_node);

#endif
