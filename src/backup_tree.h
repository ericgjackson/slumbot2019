#ifndef _BACKUP_TREE_H_
#define _BACKUP_TREE_H_

#include <memory>
#include <vector>

class Node;
class BackupBuilder {
public:
  BackupBuilder(int stack_size);
  std::shared_ptr<Node> Build(const std::vector< std::vector<double> > &bet_fracs, int st,
			      int last_bet_to);
private:
  std::shared_ptr<Node> Build(const std::vector< std::vector<double> > &bet_fracs, int *num_bets,
			      int st, int pa, int num_street_bets, int last_bet_size,
			      int last_bet_to, bool street_initial);

  int stack_size_;
  int terminal_id_;
};

#endif
