#ifndef _BACKUP_TREE_H_
#define _BACKUP_TREE_H_

#include <memory>
#include <vector>

class BettingTrees;
class Node;

struct ObservedBet {
  int st;
  int p;
  // "npbs" = "num previous bets on street"
  int npbs;
  // "npb" = "num previous bets"
  int npb;
  int sz;
};

class ObservedBets {
public:
  ObservedBets(void) {}
  ~ObservedBets(void) {}
  ObservedBets(const ObservedBets &src);
  void AddObservedBet(int st, int p, int npbs, int npb, int sz);
  void GetObservedBetSizes(int st, int p, int npbs, int npb, std::vector<int> *sizes) const;
  bool ObservedACall(int st, int p, int npbs, int npb) const;
  void Remove(int st, int p, int npbs, int npb);
private:
  std::vector<ObservedBet> observed_bets_;
};

class BackupBuilder {
public:
  BackupBuilder(int stack_size);
  BettingTrees *BuildTrees(const ObservedBets &observed_bets, const int *min_bets,
			   const int *max_bets, int st, int last_bet_to);
private:
  std::shared_ptr<Node> Build(const ObservedBets &observed_bets, const int *min_bets,
			      const int *max_bets, int st, int pa, int npbs, int npb,
			      int last_bet_size, int last_bet_to, bool street_initial,
			      bool on_path);
  std::shared_ptr<Node> Build(const ObservedBets &observed_bets, const int *min_bets,
			      const int *max_bets, int st, int last_bet_to);

  int stack_size_;
  int terminal_id_;
};

#endif
