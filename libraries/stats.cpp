#include "../stack_runtime.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <unordered_map>
#include <vector>

static constexpr double epsilon = 1e-12;

// Helper: compute the slice size (product of all dims after the leading axis)
static long slice_size(const Value *val) {
  long sz = 1;
  for (size_t i = 1; i < val->shape.size(); ++i)
    sz *= val->shape[i];
  return sz;
}

// Helper: result shape is val->shape without the leading dimension
static std::vector<long> tail_shape(const Value *val) {
  return std::vector<long>(val->shape.begin() + 1, val->shape.end());
}

static void insertion_sort(std::vector<double> &arr, long low, long high) {
  for (long i = low + 1; i <= high; ++i) {
    double key = arr[i];
    long j = i - 1;
    while (j >= low && arr[j] > key) {
      arr[j + 1] = arr[j];
      --j;
    }
    arr[j + 1] = key;
  }
}

// Sift down the element at index `root` within the sub-array [low, high],
// treating it as an offset max-heap rooted at `low`.
static void sift_down(std::vector<double> &arr, long root, long high,
                      long low) {
  while (true) {
    long left = low + 2 * (root - low) + 1;
    long right = low + 2 * (root - low) + 2;
    long largest = root;

    if (left <= high && arr[left] > arr[largest])
      largest = left;
    if (right <= high && arr[right] > arr[largest])
      largest = right;

    if (largest == root)
      break;

    std::swap(arr[root], arr[largest]);
    root = largest;
  }
}

static void heap_sort(std::vector<double> &arr, long low, long high) {
  long n = high - low + 1;

  // Build max-heap over [low, high]
  for (long i = low + n / 2 - 1; i >= low; --i)
    sift_down(arr, i, high, low);

  // Repeatedly swap the max (at low) to the end and re-heapify
  for (long end = high; end > low; --end) {
    std::swap(arr[low], arr[end]);
    sift_down(arr, low, end - 1, low);
  }
}

static void median_of_three(std::vector<double> &arr, long low, long high) {
  long mid = low + (high - low) / 2;

  if (arr[low] > arr[mid])
    std::swap(arr[low], arr[mid]);
  if (arr[low] > arr[high])
    std::swap(arr[low], arr[high]);
  if (arr[mid] > arr[high])
    std::swap(arr[mid], arr[high]);

  // arr[low] <= arr[mid] <= arr[high]; move pivot to arr[high]
  std::swap(arr[mid], arr[high]);
}

static long quick_sort_partition(std::vector<double> &arr, long low,
                                 long high) {
  median_of_three(arr, low, high);
  double pivot = arr[high];
  long i = low - 1;
  for (long j = low; j < high; ++j) {
    if (arr[j] <= pivot) {
      ++i;
      std::swap(arr[i], arr[j]);
    }
  }
  std::swap(arr[i + 1], arr[high]);
  return i + 1;
}

static constexpr long INSERTION_THRESHOLD = 16;

static void intro_sort_impl(std::vector<double> &arr, long low, long high,
                            int depth_limit) {
  while (high - low + 1 > INSERTION_THRESHOLD) {
    if (depth_limit == 0) {
      // Recursion too deep — guarantee O(n log n) with heap sort
      heap_sort(arr, low, high);
      return;
    }
    --depth_limit;

    long pi = quick_sort_partition(arr, low, high);

    // Recurse into the smaller partition, iterate on the larger one
    if (pi - low < high - pi) {
      intro_sort_impl(arr, low, pi - 1, depth_limit);
      low = pi + 1;
    } else {
      intro_sort_impl(arr, pi + 1, high, depth_limit);
      high = pi - 1;
    }
  }
  // Small partition — insertion sort has the lowest overhead here
  insertion_sort(arr, low, high);
}

static void intro_sort(std::vector<double> &arr) {
  if (arr.size() <= 1)
    return;
  long n = (long)arr.size();
  // Depth limit = 2 * floor(log2(n)), matching typical STL implementations
  int depth_limit = 2 * (int)std::floor(std::log2((double)n));
  intro_sort_impl(arr, 0, n - 1, depth_limit);
}

static void bubble_sort(std::vector<double> &arr) {
  long n = (long)arr.size();
  for (long i = 0; i < n - 1; ++i)
    for (long j = 0; j < n - i - 1; ++j)
      if (arr[j] > arr[j + 1])
        std::swap(arr[j], arr[j + 1]);
}

static void merge_sort_impl(std::vector<double> &arr, long left, long right) {
  if (right - left <= 1)
    return;
  long mid = left + (right - left) / 2;
  merge_sort_impl(arr, left, mid);
  merge_sort_impl(arr, mid, right);
  std::vector<double> tmp;
  tmp.reserve(right - left);
  long i = left, j = mid;
  while (i < mid && j < right)
    tmp.push_back(arr[i] <= arr[j] ? arr[i++] : arr[j++]);
  while (i < mid)
    tmp.push_back(arr[i++]);
  while (j < right)
    tmp.push_back(arr[j++]);
  for (long k = 0; k < (long)tmp.size(); ++k)
    arr[left + k] = tmp[k];
}

static void merge_sort(std::vector<double> &arr) {
  merge_sort_impl(arr, 0, (long)arr.size());
}

extern "C" {

// stats.mean — mean across leading axis
void mean() {
  if (valueStack.empty())
    return;

  Value *val = valueStack.top();
  valueStack.pop();

  if (!val->is_array) {
    // Scalar mean is itself
    valueStack.push(new Value(val->data[0]));
    delete val;
    return;
  }

  long n = val->shape[0];
  long slice = slice_size(val);

  if (val->shape.size() == 1) {
    // 1D: reduce to scalar
    double sum = 0.0;
    for (long i = 0; i < n; ++i)
      sum += val->data[i];
    valueStack.push(new Value(sum / n));
  } else {
    std::vector<long> rshape = tail_shape(val);
    double *result = (double *)malloc(sizeof(double) * slice);

    for (long j = 0; j < slice; ++j) {
      double sum = 0.0;
      for (long i = 0; i < n; ++i)
        sum += val->data[i * slice + j];
      result[j] = sum / n;
    }

    valueStack.push(new Value(rshape, result));
    free(result);
  }

  delete val;
}

// stats.median — median across leading axis
void median() {
  if (valueStack.empty())
    return;

  Value *val = valueStack.top();
  valueStack.pop();

  if (!val->is_array) {
    valueStack.push(new Value(val->data[0]));
    delete val;
    return;
  }

  long n = val->shape[0];
  long slice = slice_size(val);

  if (val->shape.size() == 1) {
    std::vector<double> col(val->data, val->data + n);
    intro_sort(col);
    double med =
        (n % 2 == 0) ? (col[n / 2 - 1] + col[n / 2]) / 2.0 : col[n / 2];
    valueStack.push(new Value(med));
  } else {
    std::vector<long> rshape = tail_shape(val);
    double *result = (double *)malloc(sizeof(double) * slice);

    for (long j = 0; j < slice; ++j) {
      std::vector<double> col(n);
      for (long i = 0; i < n; ++i)
        col[i] = val->data[i * slice + j];
      intro_sort(col);
      result[j] =
          (n % 2 == 0) ? (col[n / 2 - 1] + col[n / 2]) / 2.0 : col[n / 2];
    }

    valueStack.push(new Value(rshape, result));
    free(result);
  }

  delete val;
}

// stats.mode — most frequent value across leading axis
// Ties broken by smallest value. Result matches tail shape (or scalar for 1D).
void mode() {
  if (valueStack.empty())
    return;

  Value *val = valueStack.top();
  valueStack.pop();

  if (!val->is_array) {
    valueStack.push(new Value(val->data[0]));
    delete val;
    return;
  }

  long n = val->shape[0];
  long slice = slice_size(val);

  auto compute_mode = [&](const std::vector<double> &col) -> double {
    std::unordered_map<double, long> counts;
    for (double v : col)
      counts[v]++;
    long best_count = -1;
    double best_val = col[0];
    for (auto &kv : counts) {
      if (kv.second > best_count ||
          (kv.second == best_count && kv.first < best_val)) {
        best_count = kv.second;
        best_val = kv.first;
      }
    }
    return best_val;
  };

  if (val->shape.size() == 1) {
    std::vector<double> col(val->data, val->data + n);
    valueStack.push(new Value(compute_mode(col)));
  } else {
    std::vector<long> rshape = tail_shape(val);
    double *result = (double *)malloc(sizeof(double) * slice);

    for (long j = 0; j < slice; ++j) {
      std::vector<double> col(n);
      for (long i = 0; i < n; ++i)
        col[i] = val->data[i * slice + j];
      result[j] = compute_mode(col);
    }

    valueStack.push(new Value(rshape, result));
    free(result);
  }

  delete val;
}

// stats.variance — population variance across leading axis
void variance() {
  if (valueStack.empty())
    return;

  Value *val = valueStack.top();
  valueStack.pop();

  if (!val->is_array) {
    // Variance of a single value is 0
    valueStack.push(new Value(0.0));
    delete val;
    return;
  }

  long n = val->shape[0];
  long slice = slice_size(val);

  if (val->shape.size() == 1) {
    double sum = 0.0;
    for (long i = 0; i < n; ++i)
      sum += val->data[i];
    double mean = sum / n;
    double var = 0.0;
    for (long i = 0; i < n; ++i) {
      double d = val->data[i] - mean;
      var += d * d;
    }
    valueStack.push(new Value(var / n));
  } else {
    std::vector<long> rshape = tail_shape(val);
    double *result = (double *)malloc(sizeof(double) * slice);

    for (long j = 0; j < slice; ++j) {
      double sum = 0.0;
      for (long i = 0; i < n; ++i)
        sum += val->data[i * slice + j];
      double mean = sum / n;
      double var = 0.0;
      for (long i = 0; i < n; ++i) {
        double d = val->data[i * slice + j] - mean;
        var += d * d;
      }
      result[j] = var / n;
    }

    valueStack.push(new Value(rshape, result));
    free(result);
  }

  delete val;
}

// stats.sdev — population standard deviation across leading axis
void sdev() {
  if (valueStack.empty())
    return;

  Value *val = valueStack.top();
  valueStack.pop();

  if (!val->is_array) {
    // Variance of a single value is 0
    valueStack.push(new Value(0.0));
    delete val;
    return;
  }

  long n = val->shape[0];
  long slice = slice_size(val);

  if (val->shape.size() == 1) {
    double sum = 0.0;
    for (long i = 0; i < n; ++i)
      sum += val->data[i];
    double mean = sum / n;
    double var = 0.0;
    for (long i = 0; i < n; ++i) {
      double d = val->data[i] - mean;
      var += d * d;
    }
    valueStack.push(new Value(pow(var / n, 0.5)));
  } else {
    std::vector<long> rshape = tail_shape(val);
    double *result = (double *)malloc(sizeof(double) * slice);

    for (long j = 0; j < slice; ++j) {
      double sum = 0.0;
      for (long i = 0; i < n; ++i)
        sum += val->data[i * slice + j];
      double mean = sum / n;
      double var = 0.0;
      for (long i = 0; i < n; ++i) {
        double d = val->data[i * slice + j] - mean;
        var += d * d;
      }
      result[j] = pow(var / n, 0.5);
    }

    valueStack.push(new Value(rshape, result));
    free(result);
  }

  delete val;
}

// stats.predict_linear — fits y = mx + b per column across the leading axis
// treating row index as x (0, 1, ..., n-1), then predicts row n.
// Result has the same shape as a single slice (tail shape, or scalar for 1D).
void predict_linear() {
  if (valueStack.empty())
    return;

  Value *val = valueStack.top();
  valueStack.pop();

  if (!val->is_array) {
    // Single point: best prediction is the point itself
    valueStack.push(new Value(val->data[0]));
    delete val;
    return;
  }

  long n = val->shape[0];
  long slice = slice_size(val);

  // Precompute x statistics (same for every column)
  // x = 0, 1, ..., n-1
  double x_sum = (double)n * (n - 1) / 2.0;
  double x_mean = x_sum / n;
  double x_var = 0.0;
  for (long i = 0; i < n; ++i) {
    double d = i - x_mean;
    x_var += d * d;
  }
  // x_var is sum of (xi - x_mean)^2; we divide by it for slope

  auto predict_col = [&](const std::vector<double> &col) -> double {
    double y_mean = 0.0;
    for (double v : col)
      y_mean += v;
    y_mean /= n;

    double cov = 0.0;
    for (long i = 0; i < n; ++i)
      cov += (i - x_mean) * (col[i] - y_mean);

    double m = (x_var > 0.0) ? cov / x_var : 0.0;
    double b = y_mean - m * x_mean;
    return m * n + b; // predict at x = n (the next row)
  };

  if (val->shape.size() == 1) {
    std::vector<double> col(val->data, val->data + n);
    valueStack.push(new Value(predict_col(col)));
  } else {
    std::vector<long> rshape = tail_shape(val);
    double *result = (double *)malloc(sizeof(double) * slice);

    for (long j = 0; j < slice; ++j) {
      std::vector<double> col(n);
      for (long i = 0; i < n; ++i)
        col[i] = val->data[i * slice + j];
      result[j] = predict_col(col);
    }

    valueStack.push(new Value(rshape, result));
    free(result);
  }

  delete val;
}

void is_stochastic() {
  if (valueStack.empty()) {
    valueStack.push(new Value(0.0));
    return;
  }

  Value *val = valueStack.top();
  valueStack.pop();

  // Must be a 2D matrix
  if (!val->is_array || val->shape.size() != 2) {
    valueStack.push(new Value(0.0));
    delete val;
    return;
  }

  long rows = val->shape[0];
  long cols = val->shape[1];
  bool valid = true;

  for (long i = 0; i < rows && valid; ++i) {
    double row_sum = 0.0;
    for (long j = 0; j < cols; ++j) {
      double v = val->data[i * cols + j];
      if (v < 0.0) {
        valid = false;
        break;
      }
      row_sum += v;
    }
    if (valid && std::abs(row_sum - 1.0) > epsilon)
      valid = false;
  }

  valueStack.push(new Value(valid ? 1.0 : 0.0));
  delete val;
}

} // extern "C"