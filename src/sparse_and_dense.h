#ifndef _SPARSE_AND_DENSE_H_
#define _SPARSE_AND_DENSE_H_

#include <unordered_map>
#include <vector>

using namespace std;

class SparseAndDense {
 public:
  SparseAndDense(void) {num_ = 0;}
  virtual ~SparseAndDense(void) {}
  // Adds a sparse value, returns the corresponding dense value
  virtual int SparseToDense(long long int sparse) = 0;
  virtual long long int DenseToSparse(int dense) = 0;
  virtual void Clear(void) = 0;
  int Num(void) const {return num_;}

 protected:
  int num_;
};

class SparseAndDenseInt : public SparseAndDense {
public:
  SparseAndDenseInt(void);
  ~SparseAndDenseInt(void);
  int SparseToDense(long long int sparse);
  // Return a long long int even though sparse values can be represented as ints.  Caller can cast
  // as needed.
  long long int DenseToSparse(int dense);
  void Clear(void);
private:
  static const int kBlockSize = 1000000;

  unordered_map<int, int> *sparse_to_dense_;
  vector<int *> *dense_to_sparse_;
};

class SparseAndDenseLong : public SparseAndDense {
public:
  SparseAndDenseLong(void);
  ~SparseAndDenseLong(void);
  // Adds a sparse value, returns the corresponding dense value
  int SparseToDense(long long int sparse);
  long long int DenseToSparse(int dense);
  void Clear(void);
private:
  static const int kBlockSize = 1000000;

  unordered_map<long long int, int> *sparse_to_dense_;
  vector<long long int *> *dense_to_sparse_;
};

#endif
