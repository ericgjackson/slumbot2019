// Compute a pseudo-best-response using Monte-Carlo Tree Search.
//
// Based on Python code from http://mcts.ai/code/python.html
//
// Need to distinguish our-choice and opp-choice nodes.
//
// A MCTSNode should correspond to a specific information set, I guess, not just a betting
// node.  Do I create a MCTSNode for every information set visited in the rollout?  I don't
// think so.  Wouldn't be tractable.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <vector>

#include "betting_abstraction.h"
#include "betting_trees.h"
#include "board_tree.h"
#include "buckets.h"
#include "canonical_cards.h"
#include "card_abstraction.h"
#include "cards.h"
#include "cfr_config.h"
#include "cfr_utils.h"
#include "cfr_values.h"
#include "constants.h"
#include "files.h"
#include "game.h"
#include "hand_tree.h"
#include "hand_value_tree.h"
#include "io.h"
#include "rand.h"
#include "split.h"

using std::shared_ptr;
using std::unique_ptr;
using std::vector;

class MCTSNode {
public:
  MCTSNode(MCTSNode *parent, Node *node);
  MCTSNode *GetParent(void) const {return parent_;}
  Node *GetNode(void) const {return node_;}
  void Update(int value);
  // TODO
  bool FullyExpanded(void) const {return false;}
  MCTSNode *UCTSelectChild(void) const;
  // Do I need to add them in the same order as the underlying betting tree?
  void AddChild(MCTSNode *child) {children_.push_back(child);}
private:
  static constexpr double kMCTS = 1.0;
  double UCTScore(double log_parent_visits) const {
    return mean_outcome_ + kMCTS * sqrt(2 * log_parent_visits / visits_);
  }

  MCTSNode *parent_;
  Node *node_;
  int visits_;
  double sum_outcomes_;
  double mean_outcome_;
  vector<MCTSNode *> children_;
};

MCTSNode::MCTSNode(MCTSNode *parent, Node *node) : parent_(parent), node_(node) {
  visits_ = 0;
  sum_outcomes_ = 0;
  mean_outcome_ = 0;
};

void MCTSNode::Update(int value) {
  ++visits_;
  sum_outcomes_ += value;
  mean_outcome_ = sum_outcomes_ / visits_;
}

// Assumes there is at least one child
// Assumes visits_ is > 0
MCTSNode *MCTSNode::UCTSelectChild(void) const {
  double log_parent_visits = log(visits_);
  MCTSNode *best_child = children_[0];
  double best_score = best_child->UCTScore(log_parent_visits);
  int num_children = children_.size();
  for (int i = 1; i < num_children; ++i) {
    MCTSNode *child = children_[i];
    double score = child->UCTScore(log_parent_visits);
    if (score > best_score) {
      best_score = score;
      best_child = child;
    }
  }
  return best_child;
}

class MCTS {
public:
  MCTS(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
       const Buckets &buckets, const BettingTrees &betting_trees, bool current, int num_threads,
       int it, int responder);
  virtual ~MCTS(void) {}
  void Go(int num_iterations);
private:
  MCTSNode *Select(MCTSNode *mcts_node);
  MCTSNode *Expand(MCTSNode *mcts_node);
  Node *Rollout(MCTSNode *mcts_node);
  void BackPropagate(MCTSNode *mcts_node, int value);
  int Value(Node *node);
  
  const CardAbstraction &card_abstraction_;
  const BettingAbstraction &betting_abstraction_;
  const CFRConfig &cfr_config_;
  const Buckets &buckets_;
  const BettingTrees &betting_trees_;
  int it_;
  int responder_;
  std::unique_ptr<CFRValues> sumprobs_;
  unique_ptr<MCTSNode> mcts_root_;
};

MCTS::MCTS(const CardAbstraction &ca, const BettingAbstraction &ba, const CFRConfig &cc,
	   const Buckets &buckets, const BettingTrees &betting_trees, bool current, int num_threads,
	   int it, int responder) :
  card_abstraction_(ca), betting_abstraction_(ba), cfr_config_(cc), buckets_(buckets),
  betting_trees_(betting_trees), it_(it), responder_(responder) {
  BoardTree::Create();
  int num_players = Game::NumPlayers();
  unique_ptr<bool []> players(new bool[num_players]);
  for (int p = 0; p < num_players; ++p) {
    players[p] = p != responder_;
  }
  sumprobs_.reset(new CFRValues(players.get(), nullptr, 0, 0, buckets_,
				betting_trees_.GetBettingTree()));

  char dir[500];
  sprintf(dir, "%s/%s.%u.%s.%i.%i.%i.%s.%s", Files::OldCFRBase(), Game::GameName().c_str(),
	  Game::NumPlayers(), card_abstraction_.CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(),
	  betting_abstraction_.BettingAbstractionName().c_str(),
	  cfr_config_.CFRConfigName().c_str());
  if (betting_abstraction_.Asymmetric()) {
    char buf[100];
    sprintf(buf, ".p%u", responder_);
    strcat(dir, buf);
  }

  sumprobs_->Read(dir, it_, betting_trees_.GetBettingTree(), "x", -1, true, false);
  mcts_root_.reset(new MCTSNode(nullptr, betting_trees_.Root()));
}

MCTSNode *MCTS::Select(MCTSNode *mcts_node) {
  while (mcts_node->FullyExpanded()) {
    mcts_node = mcts_node->UCTSelectChild();
  }
  return mcts_node;
}

MCTSNode *MCTS::Expand(MCTSNode *mcts_node) {
  Node *node = mcts_node->GetNode();
  // Return node?
  if (node->Terminal()) return nullptr;
  int num_succs = node->NumSuccs();
  int s = RandBetween(0, num_succs - 1);
  Node *succ = node->IthSucc(s);
  MCTSNode *new_node = new MCTSNode(mcts_node, succ);
  mcts_node->AddChild(new_node);
  return new_node;					
}

Node *MCTS::Rollout(MCTSNode *mcts_node) {
  Node *node = mcts_node->GetNode();
  while (! node->Terminal()) {
    int dsi = node->DefaultSuccIndex();
    node = node->IthSucc(dsi);
  }
  return node;
}

void MCTS::BackPropagate(MCTSNode *mcts_node, int value) {
  do {
    mcts_node->Update(value);
    mcts_node = mcts_node->GetParent();
  } while (mcts_node);
  // Backpropagate
  // while node != None: # backpropagate from the expanded node and work back to the root node
  // node.Update(state.GetResult(node.playerJustMoved)) # state is terminal. Update node with result from POV of node.playerJustMoved
  // node = node.parentNode
}

int MCTS::Value(Node *node) {
  return 0;
}

void MCTS::Go(int num_iterations) {
  InitRand();
  for (int i = 0; i < num_iterations; ++i) {
    MCTSNode *mcts_node = Select(mcts_root_.get());
    mcts_node = Expand(mcts_node);
    Node *node = Rollout(mcts_node);
    int value = Value(node);
    BackPropagate(mcts_node, value);
  }
}
