class BetSizes {
public:
  BetSizes(void);
  ~BetSizes(void) {}
  int NumBets(int st) const {return (int)sizes_[st].size();}
  const std::vector<int> &GetBetSizes(int st, int npb) const {return sizes_[st][npb];}
  void GetBetSizes(int st, int npb, std::vector<int> *bet_sizes) const;
  void AddBetSize(int st, int npb, int sz);
  void Pop(int st) {sizes_[st].pop_back();}
private:
  std::vector< std::vector< std::vector<int> > > sizes_;
};

