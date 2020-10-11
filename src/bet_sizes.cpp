BetSizes::BetSizes(void) {
  int max_street = Game::MaxStreet();
  sizes_.resize(max_street + 1);
}

// "npb" = num prior bets
void BetSizes::AddBetSize(int st, int npb, int sz) {
  if (npb > (int)sizes_[st].size()) {
    fprintf(stderr, "Cannot add %ith bet on street without adding previous bet\n", npb);
    exit(-1);
  } else if (npb == (int)sizes_[st].size()) {
    sizes_[st].resize(npb + 1);
  }
  sizes_[st][npb].push_back(sz);
}

void BetSizes::GetBetSizes(int st, int npb, vector<int> *bet_sizes) const {
  const vector<int> &v = GetBetSizes(st, npb);
  int sz = v.size();
  bet_sizes->resize(sz);
  for (int i = 0; i < sz; ++i) (*bet_sizes)[i] = v[i];
}

