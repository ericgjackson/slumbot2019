#include <stdio.h>
#include <stdlib.h>

#include "board_tree.h"
#include "buckets.h"
#include "card_abstraction.h"
#include "files.h"
#include "game.h"
#include "io.h"

Buckets::Buckets(const CardAbstraction &ca, bool numb_only) {
  BoardTree::Create();
  int max_street = Game::MaxStreet();
  none_.reset(new bool[max_street + 1]);
  short_buckets_ = new unsigned short *[max_street + 1];
  int_buckets_ = new int *[max_street + 1];
  for (int st = 0; st <= max_street; ++st) {
    short_buckets_[st] = nullptr;
    int_buckets_[st] = nullptr;
  }
  char buf[500];
  num_buckets_.reset(new int[max_street + 1]);
  for (int st = 0; st <= max_street; ++st) {
    if (ca.Bucketing(st) == "none") {
      none_[st] = true;
      num_buckets_[st] = 0;
      continue;
    }
    none_[st] = false;
    sprintf(buf, "%s/num_buckets.%s.%i.%i.%i.%s.%i", Files::StaticBase(), Game::GameName().c_str(),
	    Game::NumRanks(), Game::NumSuits(), max_street, ca.Bucketing(st).c_str(), st);
    Reader reader(buf);
    num_buckets_[st] = reader.ReadIntOrDie();
  }

  if (! numb_only) {
    for (int st = 0; st <= max_street; ++st) {
      if (none_[st]) continue;
      int num_boards = BoardTree::NumBoards(st);
      int num_hole_card_pairs = Game::NumHoleCardPairs(st);
      // Need to use an unsigned int for this
      unsigned int num_hands = (unsigned int)num_boards * (unsigned int)num_hole_card_pairs;
      long long int lli_num_hands = num_hands;
  
      sprintf(buf, "%s/buckets.%s.%i.%i.%i.%s.%i", Files::StaticBase(), Game::GameName().c_str(),
	      Game::NumRanks(), Game::NumSuits(), max_street, ca.Bucketing(st).c_str(), st);
      Reader reader(buf);
      long long int file_size = reader.FileSize();
      if (file_size == lli_num_hands * 2) {
	short_buckets_[st] = new unsigned short[num_hands];
	for (unsigned int h = 0; h < num_hands; ++h) {
	  short_buckets_[st][h] = reader.ReadUnsignedShortOrDie();
	}
      } else if (file_size == lli_num_hands * 4) {
	int_buckets_[st] = new int[num_hands];
	for (unsigned int h = 0; h < num_hands; ++h) {
	  int_buckets_[st][h] = reader.ReadIntOrDie();
	}
      } else {
	fprintf(stderr, "BucketsInstance::Initialize: Unexpected file size %lli\n", file_size);
	exit(-1);
      }
    }
  }
}

// Allow a dummy empty buckets object to be created
Buckets::Buckets(void) {
  short_buckets_ = nullptr;
  int_buckets_ = nullptr;
}

Buckets::~Buckets(void) {
  int max_street = Game::MaxStreet();
  if (short_buckets_) {
    for (int st = 0; st <= max_street; ++st) {
      if (short_buckets_[st]) delete [] short_buckets_[st];
    }
    delete [] short_buckets_;
  }
  if (int_buckets_) {
    for (int st = 0; st <= max_street; ++st) {
      if (int_buckets_[st]) delete [] int_buckets_[st];
    }
    delete [] int_buckets_;
  }
}
