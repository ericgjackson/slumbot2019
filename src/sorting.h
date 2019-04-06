#ifndef _SORTING_H_
#define _SORTING_H_

#include <string>
#include <vector>

#include "cards.h"

void SortCards(Card *cards, unsigned int n);

struct PDILowerCompare {
  bool operator()(const std::pair<double, int> &p1, const std::pair<double, int> &p2) {
    if (p1.first < p2.first) {
      return true;
    } else {
      return false;
    }
  }
};

extern PDILowerCompare g_pdi_lower_compare;

struct PDUILowerCompare {
  bool operator()(const std::pair<double, unsigned int> &p1,
		  const std::pair<double, unsigned int> &p2) {
    if (p1.first < p2.first) {
      return true;
    } else {
      return false;
    }
  }
};

extern PDUILowerCompare g_pdui_lower_compare;

struct PIILowerCompare {
  bool operator()(const std::pair<int, int> &p1, const std::pair<int, int> &p2) {
    if (p1.first < p2.first) {
      return true;
    } else {
      return false;
    }
  }
};

extern PIILowerCompare g_pii_lower_compare;

struct PDSHigherCompare {
  bool operator()(const std::pair<double, std::string> &p1,
		  const std::pair<double, std::string> &p2) {
    if (p1.first > p2.first) {
      return true;
    } else {
      return false;
    }
  }
};

extern PDSHigherCompare g_pds_higher_compare;

struct PFUILowerCompare {
  bool operator()(const std::pair<float, unsigned int> &p1,
		  const std::pair<float, unsigned int> &p2) {
    if (p1.first < p2.first) {
      return true;
    } else {
      return false;
    }
  }
};

extern PFUILowerCompare g_pfui_lower_compare;

#endif
