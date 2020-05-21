#include <stdio.h>
#include <stdlib.h>

#include <memory>
#include <string>
#include <vector>

#include "betting_abstraction.h"
#include "betting_abstraction_params.h"
#include "betting_tree.h"
#include "betting_trees.h"
#include "buckets.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "cfr_config.h"
#include "cfr_params.h"
#include "cfr_value_type.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "io.h"
#include "params.h"

using std::string;
using std::unique_ptr;
using std::vector;

static Reader *InitializeReader(const char *dir, int p, int st, int it,
				const string &action_sequence, int root_bd_st, int root_bd,
				bool sumprobs, CFRValueType *value_type) {
  char buf[500];

  int t;
  for (t = 0; t < 4; ++t) {
    unsigned char suffix;
    if (t == 0) {
      suffix = 'd';
      *value_type = CFRValueType::CFR_DOUBLE;
    } else if (t == 1) {
      suffix = 'i';
      *value_type = CFRValueType::CFR_INT;
    } else if (t == 2) {
      suffix = 'c';
      *value_type = CFRValueType::CFR_CHAR;
    } else if (t == 3) {
      suffix = 's';
      *value_type = CFRValueType::CFR_SHORT;
    }
    sprintf(buf, "%s/%s.%s.%u.%u.%u.%u.p%u.%c", dir, sumprobs ? "sumprobs" : "regrets",
	    action_sequence.c_str(), root_bd_st, root_bd, st, it, p, suffix);
    if (FileExists(buf)) break;
  }
  if (t == 4) {
    fprintf(stderr, "Couldn't find file\n");
    fprintf(stderr, "buf: %s\n", buf);
    exit(-1);
  }
  Reader *reader = new Reader(buf);
  return reader;
}

static void Walk(Node *node, int p, const Buckets &buckets, Reader **readers,
		 Writer **writers, CFRValueType *value_types) {
  if (node->Terminal()) return;
  int num_succs = node->NumSuccs();
  if (node->PlayerActing() == p) {
    if (num_succs > 1) {
      int st = node->Street();
      int num_buckets = buckets.NumBuckets(st);
      unique_ptr<int []> i_probs;
      Reader *reader = readers[st];
      Writer *writer = writers[st];
      CFRValueType value_type = value_types[st];
      if (value_type == CFRValueType::CFR_INT) {
	i_probs.reset(new int[num_succs]);
      }
      for (int b = 0; b < num_buckets; ++b) {
	int i_sum = 0;
	if (value_type == CFRValueType::CFR_INT) {
	  for (int s = 0; s < num_succs; ++s) {
	    i_probs[s] = reader->ReadIntOrDie();
	    i_sum += i_probs[s];
	  }
	}
	if (i_sum == 0) {
	  for (int s = 0; s < num_succs; ++s) {
	    writer->WriteUnsignedChar(0);
	  }
	} else {
	  double d_sum = i_sum;
	  for (int s = 0; s < num_succs; ++s) {
	    if (i_probs[s] == i_sum) {
	      // Make sure we don't try to write 256
	      writer->WriteUnsignedChar(255);
	    } else {
	      double frac = i_probs[s] / d_sum;
	      unsigned char c = (unsigned char)(frac * 256.0);
	      writer->WriteUnsignedChar(c);
	    }
	  }
	}
      }
    }
  }
  for (int s = 0; s < num_succs; ++s) {
    Walk(node->IthSucc(s), p, buckets, readers, writers, value_types);
  }
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <card params> <betting abstraction params> "
	  "<old CFR params> <new CFR params> <it>\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 7) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unique_ptr<Params> card_params = CreateCardAbstractionParams();
  card_params->ReadFromFile(argv[2]);
  unique_ptr<CardAbstraction>
    card_abstraction(new CardAbstraction(*card_params));
  unique_ptr<Params> betting_params = CreateBettingAbstractionParams();
  betting_params->ReadFromFile(argv[3]);
  unique_ptr<BettingAbstraction>
    betting_abstraction(new BettingAbstraction(*betting_params));
  unique_ptr<Params> old_cfr_params = CreateCFRParams();
  old_cfr_params->ReadFromFile(argv[4]);
  unique_ptr<CFRConfig> old_cfr_config(new CFRConfig(*old_cfr_params));
  unique_ptr<Params> new_cfr_params = CreateCFRParams();
  new_cfr_params->ReadFromFile(argv[5]);
  unique_ptr<CFRConfig> new_cfr_config(new CFRConfig(*new_cfr_params));
  int it;
  if (sscanf(argv[6], "%i", &it) != 1)          Usage(argv[0]);

  Buckets buckets(*card_abstraction, true);

  unique_ptr<BettingTrees> betting_trees(new BettingTrees(*betting_abstraction));

  char old_dir[500], new_dir[500];
  sprintf(old_dir, "%s/%s.%u.%s.%i.%i.%i.%s.%s", Files::OldCFRBase(),
	  Game::GameName().c_str(), Game::NumPlayers(),
	  card_abstraction->CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(),
	  betting_abstraction->BettingAbstractionName().c_str(),
	  old_cfr_config->CFRConfigName().c_str());
  sprintf(new_dir, "%s/%s.%u.%s.%i.%i.%i.%s.%s", Files::OldCFRBase(),
	  Game::GameName().c_str(), Game::NumPlayers(),
	  card_abstraction->CardAbstractionName().c_str(), Game::NumRanks(),
	  Game::NumSuits(), Game::MaxStreet(),
	  betting_abstraction->BettingAbstractionName().c_str(),
	  new_cfr_config->CFRConfigName().c_str());
  Mkdir(new_dir);
  int max_street = Game::MaxStreet();

  for (int p = 0; p <= 1; ++p) {
    unique_ptr<Reader * []> readers(new Reader *[max_street + 1]);
    unique_ptr<Writer * []> writers(new Writer *[max_street + 1]);
    unique_ptr<CFRValueType []> value_types(new CFRValueType[max_street + 1]);
    for (int st = 0; st <= max_street; ++st) {
      readers[st] = InitializeReader(old_dir, p, st, it, "x", 0, 0, true, &value_types[st]);
      char buf[500];
      sprintf(buf, "%s/sumprobs.x.0.0.%i.%i.p%i.c", new_dir, st, it, p);
      writers[st] = new Writer(buf);
    }
    Walk(betting_trees->Root(), p, buckets, readers.get(), writers.get(), value_types.get());
    for (int st = 0; st <= max_street; ++st) {
      delete readers[st];
      delete writers[st];
    }
  }
  
}
