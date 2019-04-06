// Maintains a set of sparse numerical values and dense numerical values
// and a mapping between them.

#include <stdio.h>
#include <stdlib.h>

#include "constants.h"
#include "sparse_and_dense.h"

SparseAndDenseInt::SparseAndDenseInt(void) : SparseAndDense()  {
  sparse_to_dense_ = new unordered_map<int, int>;
  int *block = new int[kBlockSize];
  dense_to_sparse_ = new vector<int *>;
  dense_to_sparse_->push_back(block);
}

SparseAndDenseInt::~SparseAndDenseInt(void) {
  delete sparse_to_dense_;
  int num_blocks = dense_to_sparse_->size();
  for (int i = 0; i < num_blocks; ++i) {
    delete [] (*dense_to_sparse_)[i];
  }
  delete dense_to_sparse_;
}

int SparseAndDenseInt::SparseToDense(long long int ll_sparse)  {
  if (ll_sparse > kMaxInt) {
    fprintf(stderr, "SparseAndDenseInt::SparseToDense: sparse too big: %lli\n",
	    ll_sparse);
    exit(-1);
  }
  int sparse = (int)ll_sparse;
  auto it = sparse_to_dense_->find(sparse);
  if (it == sparse_to_dense_->end()) {
    int block = num_ / kBlockSize;
    int dense = num_++;
    if (block >= (int)dense_to_sparse_->size()) {
      dense_to_sparse_->push_back(new int[kBlockSize]);
    }
    int index = dense % kBlockSize;
    (*dense_to_sparse_)[block][index] = sparse;
    (*sparse_to_dense_)[sparse] = dense;
    return dense;
  } else {
    return it->second;
  }
}

long long int SparseAndDenseInt::DenseToSparse(int dense) {
  int block = dense / kBlockSize;
  int index = dense % kBlockSize;
  return (*dense_to_sparse_)[block][index];
}

void SparseAndDenseInt::Clear(void) {
  dense_to_sparse_->clear();
  sparse_to_dense_->clear();
  num_ = 0;
}

SparseAndDenseLong::SparseAndDenseLong(void) : SparseAndDense() {
  sparse_to_dense_ = new unordered_map<long long int, int>;
  long long int *block = new long long int[kBlockSize];
  dense_to_sparse_ = new vector<long long int *>;
  dense_to_sparse_->push_back(block);
}

SparseAndDenseLong::~SparseAndDenseLong(void) {
  delete sparse_to_dense_;
  int num_blocks = dense_to_sparse_->size();
  for (int i = 0; i < num_blocks; ++i) {
    delete [] (*dense_to_sparse_)[i];
  }
  delete dense_to_sparse_;
}

int SparseAndDenseLong::SparseToDense(long long int sparse) {
  unordered_map<long long int, int>::iterator it;
  it = sparse_to_dense_->find(sparse);
  if (it == sparse_to_dense_->end()) {
    int block = num_ / kBlockSize;
    int dense = num_++;
    if (block >= (int)dense_to_sparse_->size()) {
      dense_to_sparse_->push_back(new long long int[kBlockSize]);
    }
    int index = dense % kBlockSize;
    (*dense_to_sparse_)[block][index] = sparse;
    (*sparse_to_dense_)[sparse] = dense;
    return dense;
  } else {
    return it->second;
  }
}

long long int SparseAndDenseLong::DenseToSparse(int dense) {
  int block = dense / kBlockSize;
  int index = dense % kBlockSize;
  return (*dense_to_sparse_)[block][index];
}

void SparseAndDenseLong::Clear(void) {
  dense_to_sparse_->clear();
  sparse_to_dense_->clear();
  num_ = 0;
}
