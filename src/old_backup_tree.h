#ifndef _BACKUP_TREE_H_
#define _BACKUP_TREE_H_

#include <memory>
#include <vector>

class Node;

class BackupBuilder {
public:
  BackupBuilder(int stack_size);
  std::shared_ptr<Node> Build(const std::vector<Node *> &path, int st);
private:
  std::shared_ptr<Node> OffPathSubtree(int st, int player_acting, bool street_initial,
				       int last_bet_size, int last_bet_to, int num_street_bets);
  std::shared_ptr<Node> Build(const std::vector<Node *> &path, int index, int num_street_bets,
			      int last_bet_size);

  int stack_size_;
  int terminal_id_;
};

#endif
