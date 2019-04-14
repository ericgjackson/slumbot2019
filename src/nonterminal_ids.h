#ifndef _NONTERMINAL_IDS_H_
#define _NONTERMINAL_IDS_H_

class BettingTree;
class Node;

void AssignNonterminalIDs(Node *root, int * num_nonterminals);
void AssignNonterminalIDs(BettingTree *betting_tree, int *num_nonterminals);
void CountNumNonterminals(Node *root, int *num_nonterminals);
void CountNumNonterminals(BettingTree *betting_tree, int *num_nonterminals);

#endif
