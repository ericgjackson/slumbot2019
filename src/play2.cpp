// fracs are bet sizes expressed as fractions of the pot.
static double BelowProb(double actual_frac, double below_frac,
			double above_frac) {
  double below_prob =
      ((above_frac - actual_frac) * (1.0 + below_frac)) /
      ((above_frac - below_frac) * (1.0 + actual_frac));
  return below_prob;
}
