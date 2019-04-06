#ifndef _KMEANS_H_
#define _KMEANS_H_

#include <vector>

using namespace std;

class KMeansThread;

class KMeans {
public:
  KMeans(int num_clusters, int dim, int num_objects, float **objects, double neighbor_thresh,
	 int num_threads);
  ~KMeans(void);
  void Cluster(int num_its);
  int Assignment(int o) const {return assignments_[o];}
  int NumClusters(void) const {return num_clusters_;}
  int ClusterSize(int c) const {return cluster_sizes_[c];}
  void SingleObjectClusters(int num_clusters, int dim, int num_objects, float **objects);

  static const int kMaxNeighbors = 10000;


 protected:
  void ComputeIntraCentroidDistances(void);
  int Nearest(int o, float *obj) const;
  int Assign(double *avg_dist);
  void Update(void);
  void EliminateEmpty(void);
  int BinarySearch(double r, int begin, int end, double *cum_sq_distance_to_nearest, bool *used);
  void SeedPlusPlus(void);
  void Seed1();
  void Seed2();

  int num_objects_;
  int num_clusters_;
  float **objects_;
  int dim_;
  double neighbor_thresh_;
  int *cluster_sizes_;
  float **means_;
  int *assignments_;
  vector< pair<float, int> > *neighbor_vectors_;
  double intra_time_;
  double assign_time_;
  int num_threads_;
  KMeansThread **threads_;
};

#endif
