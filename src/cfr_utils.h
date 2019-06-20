#ifndef _CFR_UTILS_H_
#define _CFR_UTILS_H_

#include <memory>
#include <string>

template<typename T> class CFRStreetValues;
class CanonicalCards;
class CardAbstraction;
class CFRConfig;
class Node;

template <typename T> void RMProbs(const T *vals, int num_succs, int dsi, double *probs);
template <typename T> void ComputeOurValsBucketed(const T *all_cs_vals, int num_hole_card_pairs,
						  int num_succs, int dsi,
						  std::shared_ptr<double []> *succ_vals,
						  int *street_buckets,
						  std::shared_ptr<double []> vals);
template <typename T> void ComputeOurVals(const T *all_cs_vals, int num_hole_card_pairs,
					  int num_succs, int dsi,
					  std::shared_ptr<double []> *succ_vals, int lbd,
					  std::shared_ptr<double []> vals);
template <typename T> void SetCurrentAbstractedStrategy(const T *all_regrets, int num_buckets,
							int num_succs, int dsi,
							double *all_cs_probs);
std::shared_ptr<double []> Showdown(Node *node, const CanonicalCards *hands, double *opp_probs,
				    double sum_opp_probs, double *total_card_probs);
std::shared_ptr<double []> Fold(Node *node, int p, const CanonicalCards *hands, double *opp_probs,
				double sum_opp_probs, double *total_card_probs);
void CommonBetResponseCalcs(int st, const CanonicalCards *hands, double *opp_probs,
			    double *sum_opp_probs, double *total_card_probs);
template <typename T>
void ProcessOppProbs(Node *node, const CanonicalCards *hands, int *street_buckets,
		     double *opp_probs, std::shared_ptr<double []> *succ_opp_probs,
		     double *current_probs, int it, int soft_warmup, int hard_warmup,
		     double sumprob_scaling, CFRStreetValues<T> *sumprobs);
template <typename T1, typename T2>
void ProcessOppProbs(Node *node, int lbd, const CanonicalCards *hands, bool bucketed,
		     int *street_buckets, double *opp_probs,
		     std::shared_ptr<double []> *succ_opp_probs,
		     const CFRStreetValues<T1> &cs_vals, int dsi, int it, int soft_warmup,
		     int hard_warmup, double sumprob_scaling,
		     CFRStreetValues<T2> *sumprobs);
void DeleteOldFiles(const CardAbstraction &ca, const std::string &betting_abstraction_name,
		    const CFRConfig &cc, int it);

#endif
