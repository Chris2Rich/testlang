#ifndef STACK_RUNTIME_H
#define STACK_RUNTIME_H

#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <math.h>
#include <numeric>
#include <stack>
#include <vector>

struct Value {
  bool is_array;
  std::vector<long> shape;
  double *data;
  long total_size;
  Value(double val) : is_array(false), shape(), total_size(1) {
    data = (double *)malloc(sizeof(double));
    data[0] = val;
  }

  Value(const std::vector<long> &sh, double *arr_data)
      : is_array(true), shape(sh) {
    total_size =
        std::accumulate(sh.begin(), sh.end(), 1, std::multiplies<long>());
    data = (double *)malloc(sizeof(double) * total_size);
    memcpy(data, arr_data, sizeof(double) * total_size);
  }

  Value(const Value &other)
      : is_array(other.is_array), shape(other.shape),
        total_size(other.total_size) {
    data = (double *)malloc(sizeof(double) * total_size);
    memcpy(data, other.data, sizeof(double) * total_size);
  }

  ~Value() {
    if (data)
      free(data);
  }

  long to_flat_index(const std::vector<long> &indices) const {
    if (indices.size() != shape.size())
      return 0;

    long flat_idx = 0;
    long stride = 1;
    for (long i = shape.size() - 1; i >= 0; --i) {
      flat_idx += indices[i] * stride;
      stride *= shape[i];
    }
    return flat_idx;
  }

  double get_element(long flat_index) const {
    if (is_array) {
      return data[flat_index % total_size];
    } else {
      return data[0];
    }
  }

  static bool are_broadcastable(const std::vector<long> &shape1,
                                const std::vector<long> &shape2) {
    long max_dims = std::max(shape1.size(), shape2.size());

    for (long i = 0; i < max_dims; ++i) {
      long dim1 = (i < shape1.size()) ? shape1[shape1.size() - 1 - i] : 1;
      long dim2 = (i < shape2.size()) ? shape2[shape2.size() - 1 - i] : 1;

      if (dim1 != dim2 && dim1 != 1 && dim2 != 1) {
        return false;
      }
    }
    return true;
  }

  static std::vector<long> broadcast_shape(const std::vector<long> &shape1,
                                           const std::vector<long> &shape2) {
    long max_dims = std::max(shape1.size(), shape2.size());
    std::vector<long> result(max_dims);

    for (long i = 0; i < max_dims; ++i) {
      long dim1 = (i < shape1.size()) ? shape1[shape1.size() - 1 - i] : 1;
      long dim2 = (i < shape2.size()) ? shape2[shape2.size() - 1 - i] : 1;
      result[max_dims - 1 - i] = std::max(dim1, dim2);
    }
    return result;
  }

  void print(std::ostream &os) const {
    if (!is_array) {
      os << data[0];
      return;
    }

    if (shape.size() == 1) {
      os << "[";
      for (long i = 0; i < shape[0]; ++i) {
        if (i > 0)
          os << ", ";
        os << data[i];
      }
      os << "]";
    } else if (shape.size() == 2) {
      os << "[" << std::endl;
      for (long i = 0; i < shape[0]; ++i) {
        os << "  [";
        for (long j = 0; j < shape[1]; ++j) {
          if (j > 0)
            os << ", ";
          os << data[i * shape[1] + j];
        }
        os << "]";
        if (i < shape[0] - 1)
          os << ",";
        os << std::endl;
      }
      os << "]";
    } else {
      os << "Array(shape=[";
      for (size_t i = 0; i < shape.size(); ++i) {
        if (i > 0)
          os << ", ";
        os << shape[i];
      }
      os << "], data=[";
      for (long i = 0; i < std::min(long(10), total_size); ++i) {
        if (i > 0)
          os << ", ";
        os << data[i];
      }
      if (total_size > 10)
        os << ", ...";
      os << "])";
    }
  }
};

extern std::stack<Value *> valueStack;

#endif // STACK_RUNTIME_H
