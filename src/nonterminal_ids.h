#ifndef _NONTERMINAL_IDS_H_
#define _NONTERMINAL_IDS_H_

class BettingTree;
class Node;

void AssignNonterminalIDs(Node *root, int ***ret_num_nonterminals);
void AssignNonterminalIDs(BettingTree *betting_tree, int ***ret_num_nonterminals);
int **CountNumNonterminals(Node *root);
int **CountNumNonterminals(BettingTree *betting_tree);

#endif
