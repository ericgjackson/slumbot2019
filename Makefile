# Note that if any header files are missing when you try to build, things fail
# in mysterious ways.  You get told there is "No rule to make target obj/foo.o".
HEADS =	src/fast_hash.h src/rand.h src/constants.h src/files.h src/cards.h src/io.h src/split.h \
	src/params.h src/game_params.h src/game.h src/card_abstraction_params.h \
	src/card_abstraction.h src/betting_abstraction_params.h src/betting_abstraction.h \
	src/cfr_params.h src/cfr_config.h src/nonterminal_ids.h src/betting_tree.h \
	src/betting_trees.h src/betting_tree_builder.h src/hand_evaluator.h src/hand_value_tree.h \
	src/sorting.h src/canonical.h src/canonical_cards.h src/board_tree.h src/buckets.h \
	src/cfr_value_type.h src/cfr_street_values.h src/cfr_values.h src/prob_method.h \
	src/hand_tree.h src/vcfr_state.h src/vcfr.h src/cfr_utils.h src/cfrp.h \
	src/rgbr.h src/resolving_method.h src/subgame_utils.h src/dynamic_cbr.h src/eg_cfr.h \
	src/unsafe_eg_cfr.h src/cfrd_eg_cfr.h src/combined_eg_cfr.h src/regret_compression.h \
	src/tcfr.h src/rollout.h src/sparse_and_dense.h src/kmeans.h src/reach_probs.h \
	src/backup_tree.h src/ecfr.h

# -Wl,--no-as-needed fixes my problem of undefined reference to
# pthread_create (and pthread_join).  Comments I found on the web indicate
# that these flags are a workaround to a gcc bug.
LIBRARIES = -pthread -Wl,--no-as-needed

# For profiling:
# LDFLAGS = -pg
LDFLAGS = 

# Causes problems
#  -fipa-pta
# -ffast-math may need to be turned off for tests like std::isnan() to work
# -ffast-math makes small changes to results of floating point calculations!
# For profiling:
# CFLAGS = -std=c++17 -Wall -O3 -march=native -ffast-math -g -pg
CFLAGS = -std=c++17 -Wall -O3 -march=native -ffast-math -flto

obj/%.o:	src/%.cpp $(HEADS)
		gcc $(CFLAGS) -c -o $@ $<

OBJS =	obj/fast_hash.o obj/rand.o obj/files.o obj/cards.o obj/io.o obj/split.o obj/params.o \
	obj/game_params.o obj/game.o obj/card_abstraction_params.o obj/card_abstraction.o \
	obj/betting_abstraction_params.o obj/betting_abstraction.o obj/cfr_params.o \
	obj/cfr_config.o obj/nonterminal_ids.o obj/betting_tree.o obj/betting_trees.o \
	obj/betting_tree_builder.o obj/no_limit_tree.o obj/mp_betting_tree.o obj/hand_evaluator.o \
	obj/hand_value_tree.o obj/sorting.o obj/canonical.o obj/canonical_cards.o obj/board_tree.o \
	obj/buckets.o obj/cfr_street_values.o obj/cfr_values.o obj/hand_tree.o obj/vcfr_state.o \
	obj/cfr_utils.o obj/vcfr.o obj/cfrp.o obj/rgbr.o obj/resolving_method.o \
	obj/subgame_utils.o obj/dynamic_cbr.o obj/eg_cfr.o obj/unsafe_eg_cfr.o obj/cfrd_eg_cfr.o \
	obj/combined_eg_cfr.o obj/regret_compression.o obj/tcfr.o obj/rollout.o \
	obj/sparse_and_dense.o obj/kmeans.o obj/mcts.o obj/reach_probs.o obj/backup_tree.o \
	obj/ecfr.o

all:	bin/show_num_boards bin/show_boards bin/build_hand_value_tree bin/build_null_buckets \
	bin/build_rollout_features bin/combine_features bin/build_unique_buckets \
	bin/build_kmeans_buckets bin/crossproduct bin/prify bin/show_num_buckets \
	bin/build_betting_tree bin/show_betting_tree bin/run_cfrp bin/run_tcfr bin/run_ecfr \
	bin/run_rgbr bin/solve_all_subgames bin/solve_all_backup_subgames \
	bin/solve_one_subgame_safe bin/solve_one_subgame_unsafe bin/progressively_solve_subgames \
	bin/assemble_subgames bin/dump_file bin/show_preflop_strategy bin/show_preflop_reach_probs \
	bin/show_probs_at_node bin/play bin/head_to_head bin/mc_node bin/eval_node bin/sampled_br \
	bin/run_approx_rgbr bin/test_backup_tree bin/estimate_ram bin/find_gaps bin/keep_backups \
	bin/quantize_sumprobs

bin/show_num_boards:	obj/show_num_boards.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/show_num_boards obj/show_num_boards.o $(OBJS) $(LIBRARIES)

bin/show_boards:	obj/show_boards.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/show_boards obj/show_boards.o $(OBJS) $(LIBRARIES)

bin/build_hand_value_tree:	obj/build_hand_value_tree.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/build_hand_value_tree obj/build_hand_value_tree.o $(OBJS) \
	$(LIBRARIES)

bin/build_null_buckets:	obj/build_null_buckets.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/build_null_buckets obj/build_null_buckets.o $(OBJS) \
	$(LIBRARIES)

bin/build_rollout_features:	obj/build_rollout_features.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/build_rollout_features obj/build_rollout_features.o \
	$(OBJS) $(LIBRARIES)

bin/combine_features:	obj/combine_features.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/combine_features obj/combine_features.o $(OBJS) $(LIBRARIES)

bin/build_unique_buckets:	obj/build_unique_buckets.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/build_unique_buckets obj/build_unique_buckets.o $(OBJS) \
	$(LIBRARIES)

bin/build_kmeans_buckets:	obj/build_kmeans_buckets.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/build_kmeans_buckets obj/build_kmeans_buckets.o $(OBJS) \
	$(LIBRARIES)

bin/crossproduct:	obj/crossproduct.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/crossproduct obj/crossproduct.o $(OBJS) $(LIBRARIES)

bin/prify:	obj/prify.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/prify obj/prify.o $(OBJS) $(LIBRARIES)

bin/show_num_buckets:	obj/show_num_buckets.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/show_num_buckets obj/show_num_buckets.o $(OBJS) \
	$(LIBRARIES)

bin/build_betting_tree:	obj/build_betting_tree.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/build_betting_tree obj/build_betting_tree.o $(OBJS) \
	$(LIBRARIES)

bin/show_betting_tree:	obj/show_betting_tree.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/show_betting_tree obj/show_betting_tree.o $(OBJS) \
	$(LIBRARIES)

bin/run_cfrp:	obj/run_cfrp.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/run_cfrp obj/run_cfrp.o $(OBJS) $(LIBRARIES)

bin/run_tcfr:	obj/run_tcfr.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/run_tcfr obj/run_tcfr.o $(OBJS) $(LIBRARIES)

bin/run_ecfr:	obj/run_ecfr.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/run_ecfr obj/run_ecfr.o $(OBJS) $(LIBRARIES)

bin/run_rgbr:	obj/run_rgbr.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/run_rgbr obj/run_rgbr.o $(OBJS) $(LIBRARIES)

bin/solve_all_subgames:	obj/solve_all_subgames.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/solve_all_subgames obj/solve_all_subgames.o $(OBJS) \
	$(LIBRARIES)

bin/solve_all_backup_subgames:	obj/solve_all_backup_subgames.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/solve_all_backup_subgames obj/solve_all_backup_subgames.o \
	$(OBJS) $(LIBRARIES)

bin/solve_one_subgame_safe:	obj/solve_one_subgame_safe.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/solve_one_subgame_safe obj/solve_one_subgame_safe.o $(OBJS) \
	$(LIBRARIES)

bin/solve_one_subgame_unsafe:	obj/solve_one_subgame_unsafe.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/solve_one_subgame_unsafe obj/solve_one_subgame_unsafe.o \
	$(OBJS) $(LIBRARIES)

bin/progressively_solve_subgames:	obj/progressively_solve_subgames.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/progressively_solve_subgames \
	obj/progressively_solve_subgames.o $(OBJS) $(LIBRARIES)

bin/assemble_subgames:	obj/assemble_subgames.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/assemble_subgames obj/assemble_subgames.o $(OBJS) \
	$(LIBRARIES)

bin/dump_file:	obj/dump_file.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/dump_file obj/dump_file.o $(OBJS) $(LIBRARIES)

bin/show_preflop_strategy:	obj/show_preflop_strategy.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/show_preflop_strategy obj/show_preflop_strategy.o $(OBJS) \
	$(LIBRARIES)

bin/show_preflop_reach_probs:	obj/show_preflop_reach_probs.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/show_preflop_reach_probs obj/show_preflop_reach_probs.o \
	$(OBJS) $(LIBRARIES)

bin/show_probs_at_node:	obj/show_probs_at_node.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/show_probs_at_node obj/show_probs_at_node.o $(OBJS) \
	$(LIBRARIES)

bin/play:	obj/play.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/play obj/play.o $(OBJS) $(LIBRARIES)

bin/head_to_head:	obj/head_to_head.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/head_to_head obj/head_to_head.o $(OBJS) $(LIBRARIES)

bin/mc_node:	obj/mc_node.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/mc_node obj/mc_node.o $(OBJS) $(LIBRARIES)

bin/eval_node:	obj/eval_node.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/eval_node obj/eval_node.o $(OBJS) $(LIBRARIES)

bin/sampled_br:	obj/sampled_br.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/sampled_br obj/sampled_br.o $(OBJS) $(LIBRARIES)

bin/run_approx_rgbr:	obj/run_approx_rgbr.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/run_approx_rgbr obj/run_approx_rgbr.o $(OBJS) $(LIBRARIES)

bin/test_backup_tree:	obj/test_backup_tree.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/test_backup_tree obj/test_backup_tree.o $(OBJS) $(LIBRARIES)

bin/estimate_ram:	obj/estimate_ram.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/estimate_ram obj/estimate_ram.o $(OBJS) $(LIBRARIES)

bin/find_gaps:	obj/find_gaps.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/find_gaps obj/find_gaps.o $(OBJS) $(LIBRARIES)

bin/keep_backups:	obj/keep_backups.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/keep_backups obj/keep_backups.o $(OBJS) $(LIBRARIES)

bin/quantize_sumprobs:	obj/quantize_sumprobs.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/quantize_sumprobs obj/quantize_sumprobs.o $(OBJS) \
	$(LIBRARIES)

bin/x:	obj/x.o $(OBJS) $(HEADS)
	g++ $(LDFLAGS) $(CFLAGS) -o bin/x obj/x.o $(OBJS) $(LIBRARIES)

