// Old code that was used for multithreading inside of VCFR.  Including the old implementation of
// VCFR::Split().

class VCFRThread {
public:
  VCFRThread(VCFR *vcfr, int thread_index, int num_threads, Node *p0_node, Node *p1_node,
	     int pgbd, const VCFRState &pred_state, int *prev_canons);
  ~VCFRThread(void) {}
  void Run(void);
  void Join(void);
  void Go(void);
  shared_ptr<double []> RetVals(void) const {return ret_vals_;}
private:
  VCFR *vcfr_;
  int thread_index_;
  int num_threads_;
  Node *p0_node_;
  Node *p1_node_;
  int pgbd_;
  const VCFRState &pred_state_;
  int *prev_canons_;
  shared_ptr<double []> ret_vals_;
  pthread_t pthread_id_;
};

VCFRThread::VCFRThread(VCFR *vcfr, int thread_index, int num_threads, Node *p0_node,
		       Node *p1_node, int pgbd, const VCFRState &pred_state, int *prev_canons) :
  vcfr_(vcfr), thread_index_(thread_index), num_threads_(num_threads), p0_node_(p0_node),
  p1_node_(p1_node), pgbd_(pgbd), pred_state_(pred_state), prev_canons_(prev_canons) {
}

static void *vcfr_thread_run(void *v_t) {
  VCFRThread *t = (VCFRThread *)v_t;
  t->Go();
  return NULL;
}

void VCFRThread::Run(void) {
  pthread_create(&pthread_id_, NULL, vcfr_thread_run, this);
}

void VCFRThread::Join(void) {
  pthread_join(pthread_id_, NULL); 
}

#if 0
void VCFRThread::Go(void) {
  int st = p0_node_->Street();
  int pst = st - 1;
  int num_boards = BoardTree::NumBoards(st);
  int num_prev_hole_card_pairs = Game::NumHoleCardPairs(pst);
  Card max_card1 = Game::MaxCard() + 1;
  ret_vals_.reset(new double[num_prev_hole_card_pairs]);
  for (int i = 0; i < num_prev_hole_card_pairs; ++i) ret_vals_[i] = 0;
  for (int bd = thread_index_; bd < num_boards; bd += num_threads_) {
    shared_ptr<double []> bd_vals =
      vcfr_->ProcessInternal(p0_node_, p1_node_, bd, pred_state_, 0, 0);
    const CanonicalCards *hands = pred_state_.GetHandTree()->Hands(st, bd);
    int board_variants = BoardTree::NumVariants(st, bd);
    int num_hands = hands->NumRaw();
    for (int h = 0; h < num_hands; ++h) {
      const Card *cards = hands->Cards(h);
      Card hi = cards[0];
      Card lo = cards[1];
      int enc = hi * max_card1 + lo;
      int prev_canon = prev_canons_[enc];
      ret_vals_[prev_canon] += board_variants * bd_vals[h];
    }
  }
}
#endif

void VCFRThread::Go(void) {
  int nst = p0_node_->Street();
  int pst = nst - 1;
  int num_prev_hole_card_pairs = Game::NumHoleCardPairs(pst);
  Card max_card1 = Game::MaxCard() + 1;
  ret_vals_.reset(new double[num_prev_hole_card_pairs]);
  for (int i = 0; i < num_prev_hole_card_pairs; ++i) ret_vals_[i] = 0;
  int ngbd_begin = BoardTree::SuccBoardBegin(pst, pgbd_, nst);
  int ngbd_end = BoardTree::SuccBoardEnd(pst, pgbd_, nst);
  for (int ngbd = ngbd_begin; ngbd < ngbd_end; ++ngbd) {
    // Only do work that belongs to this thread
    if (ngbd % num_threads_ != thread_index_) continue;
    int root_st = pred_state_.GetHandTree()->RootSt();
    int root_bd = pred_state_.GetHandTree()->RootBd();
    shared_ptr<double []> bd_vals =
      vcfr_->ProcessInternal(p0_node_, p1_node_, ngbd, pred_state_, root_bd, root_st);
    int nlbd = BoardTree::LocalIndex(pred_state_.RootBdSt(), pred_state_.RootBd(), nst, ngbd);
    const CanonicalCards *hands = pred_state_.GetHandTree()->Hands(nst, nlbd);
    int board_variants = BoardTree::NumVariants(nst, ngbd);
    int num_hands = hands->NumRaw();
    for (int nhcp = 0; nhcp < num_hands; ++nhcp) {
      const Card *cards = hands->Cards(nhcp);
      Card hi = cards[0];
      Card lo = cards[1];
      int enc = hi * max_card1 + lo;
      int prev_canon = prev_canons_[enc];
      ret_vals_[prev_canon] += board_variants * bd_vals[nhcp];
    }
  }
}

// Divide work at a street-initial node between multiple threads.  Spawns
// the threads, joins them, aggregates the resulting CVs.
// Ugly that we pass prev_canons in.
void VCFR::Split(Node *p0_node, Node *p1_node, int pgbd, const VCFRState &state, int *prev_canons,
		 double *vals) {
  int nst = p0_node->Street();
  int pst = nst - 1;
  int prev_num_hole_card_pairs = Game::NumHoleCardPairs(pst);
  for (int i = 0; i < prev_num_hole_card_pairs; ++i) vals[i] = 0;
  unique_ptr<unique_ptr<VCFRThread> []> threads(new unique_ptr<VCFRThread>[num_threads_]);
  for (int t = 0; t < num_threads_; ++t) {
    threads[t].reset(new VCFRThread(this, t, num_threads_, p0_node, p1_node, pgbd, state,
				    prev_canons));
  }
  for (int t = 1; t < num_threads_; ++t) {
    threads[t]->Run();
  }
  // Do first thread in main thread
  threads[0]->Go();
  for (int t = 1; t < num_threads_; ++t) {
    threads[t]->Join();
  }
  for (int t = 0; t < num_threads_; ++t) {
    shared_ptr<double []> t_vals = threads[t]->RetVals();
    for (int i = 0; i < prev_num_hole_card_pairs; ++i) {
      vals[i] += t_vals[i];
    }
  }
}

