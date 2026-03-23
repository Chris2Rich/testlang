#include "stack_runtime.h"
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <math.h>
#include <numeric>
#include <stack>
#include <vector>

std::stack<Value *> valueStack;

extern "C" {
void push_double(double val) { valueStack.push(new Value(val)); }

void push_array_data(long size, double *data) {
  std::vector<long> shape = {size};
  valueStack.push(new Value(shape, data));
}

void push_multidim_array(long ndim, long *shape_data, double *data) {
  std::vector<long> shape(shape_data, shape_data + ndim);
  valueStack.push(new Value(shape, data));
}

void push_shape() {
  if (valueStack.empty()) {
    return;
  }
  Value *a = valueStack.top();
  valueStack.pop();

  if (!a->is_array) {
    // Scalar has empty shape - represented as an empty array
    // If we want to return the shape as an array, for a scalar it's an empty
    // array.
    std::vector<long> sh = {0};
    valueStack.push(new Value(sh, nullptr));
  } else {
    // Return an array containing the dimensions of 'a'
    std::vector<double> dim_data(a->shape.size());
    for (size_t i = 0; i < a->shape.size(); ++i) {
      dim_data[i] = static_cast<double>(a->shape[i]);
    }
    std::vector<long> result_shape = {static_cast<long>(a->shape.size())};
    valueStack.push(new Value(result_shape, dim_data.data()));
  }
  delete a;
}

void unary_op(void (*operation)(double, double *)) {
  if (valueStack.size() < 1) {
    std::cerr << "Runtime Error: Not enough values for unary operation"
              << std::endl;
    return;
  }

  Value *a = valueStack.top();
  valueStack.pop();

  if (!a->is_array) {
    double result;
    operation(a->data[0], &result);
    valueStack.push(new Value(result));
  } else {
    std::vector<long> result_shape = a->shape;

    long result_size = std::accumulate(result_shape.begin(), result_shape.end(),
                                       1, std::multiplies<long>());
    double *result_data = (double *)malloc(sizeof(double) * result_size);

    for (long i = 0; i < result_size; ++i) {
      operation(a->get_element(i), &result_data[i]);
    }

    if (result_size == 1) {
      valueStack.push(new Value(result_data[0]));
      free(result_data);
    } else {
      valueStack.push(new Value(result_shape, result_data));
      free(result_data);
    }
  }

  delete a;
}

void binary_op(void (*operation)(double, double, double *)) {
  if (valueStack.size() < 2) {
    std::cerr << "Runtime Error: Not enough values for binary operation"
              << std::endl;
    return;
  }

  Value *b = valueStack.top();
  valueStack.pop();
  Value *a = valueStack.top();
  valueStack.pop();

  if (!a->is_array && !b->is_array) {
    double result;
    operation(a->data[0], b->data[0], &result);
    valueStack.push(new Value(result));
  } else {
    std::vector<long> a_shape = a->is_array ? a->shape : std::vector<long>{};
    std::vector<long> b_shape = b->is_array ? b->shape : std::vector<long>{};

    if (!Value::are_broadcastable(a_shape, b_shape)) {
      std::cerr << "Runtime Error: Shapes not broadcastable" << std::endl;
      delete a;
      delete b;
      return;
    }

    std::vector<long> result_shape = Value::broadcast_shape(a_shape, b_shape);
    long result_size = std::accumulate(result_shape.begin(), result_shape.end(),
                                       1, std::multiplies<long>());
    double *result_data = (double *)malloc(sizeof(double) * result_size);

    for (long i = 0; i < result_size; ++i) {
      // Convert flat index to multi-dimensional indices for result shape
      std::vector<long> indices(result_shape.size());
      long temp = i;
      for (long j = result_shape.size() - 1; j >= 0; --j) {
        indices[j] = temp % result_shape[j];
        temp /= result_shape[j];
      }

      // Calculate index for a
      long a_idx = 0;
      if (a->is_array) {
        std::vector<long> a_indices(a_shape.size());
        long a_offset = result_shape.size() - a_shape.size();
        for (long j = 0; j < a_shape.size(); ++j) {
          long idx = j + a_offset;
          a_indices[j] = indices[idx] % a_shape[j];
        }
        a_idx = a->to_flat_index(a_indices);
      }

      // Calculate index for b
      long b_idx = 0;
      if (b->is_array) {
        std::vector<long> b_indices(b_shape.size());
        long b_offset = result_shape.size() - b_shape.size();
        for (long j = 0; j < b_shape.size(); ++j) {
          long idx = j + b_offset;
          b_indices[j] = indices[idx] % b_shape[j];
        }
        b_idx = b->to_flat_index(b_indices);
      }

      operation(a->get_element(a_idx), b->get_element(b_idx), &result_data[i]);
    }

    if (result_size == 1) {
      valueStack.push(new Value(result_data[0]));
      free(result_data);
    } else {
      valueStack.push(new Value(result_shape, result_data));
      free(result_data);
    }
  }

  delete a;
  delete b;
}

void push_pi() { push_double(2 * asin(1)); }

void push_e() { push_double(exp(1)); }

void matrix_multiply() {
  if (valueStack.size() < 2) {
    std::cerr << "Runtime Error: Not enough values for matrix multiplication"
              << std::endl;
    return;
  }

  Value *a = valueStack.top();
  valueStack.pop();
  Value *b = valueStack.top();
  valueStack.pop();

  if (!a->is_array || !b->is_array || a->shape.size() != 2 ||
      b->shape.size() != 2 || a->shape[1] != b->shape[0]) {
    std::cerr << "Runtime Error: Invalid shapes for matrix multiplication"
              << std::endl;
    delete a;
    delete b;
    return;
  }

  long m = a->shape[0];
  long n = a->shape[1];
  long p = b->shape[1];

  std::vector<long> result_shape = {m, p};
  double *result_data = (double *)malloc(sizeof(double) * m * p);

  for (long i = 0; i < m; ++i) {
    for (long j = 0; j < p; ++j) {
      double sum = 0.0;
      for (long k = 0; k < n; ++k) {
        sum += a->data[i * n + k] * b->data[k * p + j];
      }
      result_data[i * p + j] = sum;
    }
  }

  valueStack.push(new Value(result_shape, result_data));
  free(result_data);
  delete a;
  delete b;
}

void expand_top() {
  if (valueStack.size() < 2)
    return;

  Value *a = valueStack.top();
  valueStack.pop();
  Value *b = valueStack.top();
  valueStack.pop();

  if (!a->is_array || !b->is_array) {
    valueStack.push(b);
    valueStack.push(a);
    return;
  }

  const std::vector<long> &a_shape = a->shape;
  const std::vector<long> &b_shape = b->shape;

  std::vector<long> expanded_shape = a_shape;
  expanded_shape.insert(expanded_shape.end(), b_shape.begin(), b_shape.end());

  long a_size = a->total_size;
  long b_size = b->total_size;
  long final_size = a_size * b_size;

  double *a_expanded_data = (double *)malloc(sizeof(double) * final_size);
  for (long i = 0; i < a_size; ++i) {
    for (long j = 0; j < b_size; ++j) {
      a_expanded_data[i * b_size + j] = a->data[i];
    }
  }

  double *b_expanded_data = (double *)malloc(sizeof(double) * final_size);
  for (long i = 0; i < a_size; ++i) {
    for (long j = 0; j < b_size; ++j) {
      b_expanded_data[i * b_size + j] = b->data[j];
    }
  }

  valueStack.push(new Value(expanded_shape, b_expanded_data));
  valueStack.push(new Value(expanded_shape, a_expanded_data));

  delete a;
  delete b;
  free(a_expanded_data);
  free(b_expanded_data);
}

void reshape_top(long ndim, long *new_shape) {
  if (valueStack.empty())
    return;

  Value *val = valueStack.top();
  valueStack.pop();

  std::vector<long> shape(new_shape, new_shape + ndim);
  long new_size =
      std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<long>());

  if (new_size != val->total_size) {
    std::cerr << "Runtime Error: Cannot reshape array with different total size"
              << std::endl;
    return;
  }

  Value *reshaped = new Value(shape, val->data);
  delete val;
  valueStack.push(reshaped);
}

void pop_and_print() {
  if (valueStack.empty()) {
    std::cout << "Stack empty" << std::endl;
    return;
  }

  Value *val = valueStack.top();
  valueStack.pop();

  val->print(std::cout);
  std::cout << std::endl;

  delete val;
}

void pop() {
  if (valueStack.empty()) {
    std::cout << "Stack empty" << std::endl;
    return;
  }

  valueStack.pop();
}

void band_op(double a, double b, double *result) {
  *result = double(long(a) & long(b));
}
void bor_op(double a, double b, double *result) {
  *result = double(long(a) | long(b));
}
void bxor_op(double a, double b, double *result) {
  *result = double(long(a) ^ long(b));
}
void bnot_op(double a, double *result) { *result = double(~long(a)); }

void lshift_op(double a, double b, double *result) {
  *result = double(long(a) << long(b));
}
void rshift_op(double a, double b, double *result) {
  *result = double(long(a) >> long(b));
}

void add_op(double a, double b, double *result) { *result = a + b; }
void sub_op(double a, double b, double *result) { *result = a - b; }
void mul_op(double a, double b, double *result) { *result = a * b; }
void div_op(double a, double b, double *result) { *result = a / b; }
void mod_op(double a, double b, double *result) { *result = fmod(a, b); }

void pow_op(double a, double b, double *result) { *result = pow(a, b); }
void log_op(double a, double b, double *result) { *result = log(a) / log(b); }
void exp_op(double a, double *result) { *result = exp(a); }
void ln_op(double a, double *result) { *result = log(a); }

void sin_op(double a, double *result) { *result = sin(a); }
void cos_op(double a, double *result) { *result = cos(a); }
void tan_op(double a, double *result) { *result = tan(a); }

void asin_op(double a, double *result) { *result = asin(a); }
void acos_op(double a, double *result) { *result = acos(a); }
void atan_op(double a, double *result) { *result = atan(a); }

void do_band() { binary_op(band_op); }
void do_bor() { binary_op(bor_op); }
void do_bxor() { binary_op(bxor_op); }
void do_bnot() { unary_op(bnot_op); }

void do_lshift() { binary_op(lshift_op); }
void do_rshift() { binary_op(rshift_op); }

void do_add() { binary_op(add_op); }
void do_sub() { binary_op(sub_op); }
void do_mul() { binary_op(mul_op); }
void do_div() { binary_op(div_op); }
void do_mod() { binary_op(mod_op); }
void do_pow() { binary_op(pow_op); }
void do_log() { binary_op(log_op); }
void do_exp() { unary_op(exp_op); }
void do_ln() { unary_op(ln_op); }

void do_sin() { unary_op(sin_op); }
void do_cos() { unary_op(cos_op); }
void do_tan() { unary_op(tan_op); }

void do_asin() { unary_op(asin_op); }
void do_acos() { unary_op(acos_op); }
void do_atan() { unary_op(atan_op); }

void do_iota() {
  if (valueStack.size() < 2) {
    std::cerr << "Runtime Error: iota requires 2 arguments (shape and filler)"
              << std::endl;
    return;
  }

  // In RTL language, if user writes `; iota shape filler`
  // Stack has shape then filler. filler is top.
  Value *shapeArray = valueStack.top();
  valueStack.pop();
  Value *filler = valueStack.top();
  valueStack.pop();

  if (!shapeArray->is_array) {
    std::cerr << "Runtime Error: iota requires an array for the target shape"
              << std::endl;
    delete filler;
    delete shapeArray;
    return;
  }

  std::vector<long> target_shape;
  for (long i = 0; i < shapeArray->total_size; ++i) {
    target_shape.push_back(static_cast<long>(shapeArray->data[i]));
  }

  std::vector<long> filler_shape =
      filler->is_array ? filler->shape : std::vector<long>{};

  if (!Value::are_broadcastable(filler_shape, target_shape)) {
    std::cerr << "Runtime Error: Filler not broadcastable to target shape"
              << std::endl;
    delete filler;
    delete shapeArray;
    return;
  }

  long result_size = std::accumulate(target_shape.begin(), target_shape.end(),
                                     1, std::multiplies<long>());
  double *result_data = (double *)malloc(sizeof(double) * result_size);

  for (long i = 0; i < result_size; ++i) {
    // Convert flat index to multi-dimensional indices for target shape
    std::vector<long> indices(target_shape.size());
    long temp = i;
    for (long j = target_shape.size() - 1; j >= 0; --j) {
      indices[j] = temp % target_shape[j];
      temp /= target_shape[j];
    }

    // Calculate index for filler
    long filler_idx = 0;
    if (filler->is_array) {
      std::vector<long> filler_indices(filler_shape.size());
      long filler_offset = target_shape.size() - filler_shape.size();
      for (long j = 0; j < filler_shape.size(); ++j) {
        long idx = j + filler_offset;
        // Use modulo for broadcasting
        filler_indices[j] = indices[idx] % filler_shape[j];
      }
      filler_idx = filler->to_flat_index(filler_indices);
    }
    result_data[i] = filler->get_element(filler_idx);
  }

  valueStack.push(new Value(target_shape, result_data));
  free(result_data);
  delete filler;
  delete shapeArray;
}

void do_iota_n() {
  if (valueStack.size() < 1) {
    std::cerr << "Runtime Error: iota_n requires 1 arguments (shape)"
              << std::endl;
    return;
  }

  Value *shapeArray = valueStack.top();
  valueStack.pop();

  if (!shapeArray->is_array) {
    std::cerr << "Runtime Error: iota_n requires an array for the target shape"
              << std::endl;
    delete shapeArray;
    return;
  }

  std::vector<long> target_shape;
  for (long i = 0; i < shapeArray->total_size; ++i) {
    target_shape.push_back(static_cast<long>(shapeArray->data[i]));
  }

  long result_size = std::accumulate(target_shape.begin(), target_shape.end(),
                                     1, std::multiplies<long>());
  double *result_data = (double *)malloc(sizeof(double) * result_size);

  for (long i = 0; i < result_size; ++i) {
    result_data[i] = i;
  }

  valueStack.push(new Value(target_shape, result_data));
  free(result_data);
  delete shapeArray;
}

void do_reshape() {
  if (valueStack.size() < 2) {
    std::cerr << "Runtime Error: reshape requires 2 arguments (shape and data)"
              << std::endl;
    return;
  }

  Value *shapeArray = valueStack.top();
  valueStack.pop();
  Value *dataArray = valueStack.top();
  valueStack.pop();

  if (!shapeArray->is_array) {
    std::cerr << "Runtime Error: reshape requires an array for the target shape"
              << std::endl;
    return;
  }

  std::vector<long> target_shape;
  for (long i = 0; i < shapeArray->total_size; ++i) {
    target_shape.push_back(static_cast<long>(shapeArray->data[i]));
  }

  long new_size = std::accumulate(target_shape.begin(), target_shape.end(), 1,
                                  std::multiplies<long>());

  if (new_size != dataArray->total_size) {
    std::cerr << "Runtime Error: Cannot reshape array with different total size"
              << std::endl;
    return;
  }

  valueStack.push(new Value(target_shape, dataArray->data));
  delete dataArray;
  delete shapeArray;
}

void duplicate_top() {
  if (valueStack.empty())
    return;
  Value *val = valueStack.top();
  valueStack.push(new Value(*val));
}

void swap_top() {
  if (valueStack.size() < 2)
    return;
  Value *a = valueStack.top();
  valueStack.pop();
  Value *b = valueStack.top();
  valueStack.pop();
  valueStack.push(a);
  valueStack.push(b);
}

void transpose_top() {
  if (valueStack.empty())
    return;

  Value *val = valueStack.top();
  valueStack.pop();

  if (!val->is_array) {
    valueStack.push(val);
    return;
  }

  std::vector<long> transposed_shape(val->shape.rbegin(), val->shape.rend());
  long total_size = val->total_size;
  double *transposed_data = (double *)malloc(sizeof(double) * total_size);

  for (long i = 0; i < total_size; ++i) {
    std::vector<long> indices(val->shape.size());
    long temp = i;
    for (long j = val->shape.size() - 1; j >= 0; --j) {
      indices[j] = temp % val->shape[j];
      temp /= val->shape[j];
    }

    std::reverse(indices.begin(), indices.end());
    long transposed_idx = 0;
    long stride = 1;
    for (long j = transposed_shape.size() - 1; j >= 0; --j) {
      transposed_idx += indices[j] * stride;
      stride *= transposed_shape[j];
    }

    transposed_data[transposed_idx] = val->data[i];
  }

  valueStack.push(new Value(transposed_shape, transposed_data));
  free(transposed_data);
  delete val;
}

void *runtime_malloc(size_t size) { return std::malloc(size); }

void runtime_free(void *ptr) { std::free(ptr); }

void pop_value() {
  if (valueStack.empty())
    return;
  Value *v = valueStack.top();
  valueStack.pop();
  delete v;
}

double check_zero_pop() {
  if (valueStack.empty()) {
    return 1.0;
  }
  Value *v = valueStack.top();
  valueStack.pop();

  double result = 0.0;
  if (!v->is_array) {
    result = (v->data[0] == 0.0) ? 1.0 : 0.0;
  } else {
    bool flag = false;
    for (int i = 0; i < v->total_size; i++) {
      flag = (flag != false) || (v->data[i] != 0);
    }

    result = (flag == false) ? 1.0 : 0.0;
  }

  delete v;
  return result;
}

double check_less_zero_pop() {
  if (valueStack.empty()) {
    return 1.0;
  }
  Value *v = valueStack.top();
  valueStack.pop();

  double result = 0.0;
  if (!v->is_array) {
    result = (v->data[0] <= 0.0) ? 1.0 : 0.0;
  } else {
    bool flag = false;
    for (int i = 0; i < v->total_size; i++) {
      flag = (flag != false) || (v->data[i] > 0);
    }

    result = (flag == false) ? 1.0 : 0.0;
  }

  delete v;
  return result;
}

void do_where() {
  if (valueStack.size() < 3) {
    std::cerr << "Runtime Error: where requires 3 inputs" << std::endl;
    return;
  }

  // Pop in reverse order of stack (top first)
  Value *truth_values = valueStack.top();
  valueStack.pop();
  Value *true_case = valueStack.top();
  valueStack.pop();
  Value *false_case = valueStack.top();
  valueStack.pop();

  // Get shapes (empty for scalars)
  std::vector<long> tv_shape =
      truth_values->is_array ? truth_values->shape : std::vector<long>{};
  std::vector<long> tc_shape =
      true_case->is_array ? true_case->shape : std::vector<long>{};
  std::vector<long> fc_shape =
      false_case->is_array ? false_case->shape : std::vector<long>{};

  // Check if all pairs are broadcastable
  if (!Value::are_broadcastable(tv_shape, tc_shape) ||
      !Value::are_broadcastable(tv_shape, fc_shape) ||
      !Value::are_broadcastable(tc_shape, fc_shape)) {
    std::cerr << "Runtime Error: Shapes not broadcastable for where"
              << std::endl;
    delete truth_values;
    delete true_case;
    delete false_case;
    return;
  }

  // Compute common broadcast shape for all three
  std::vector<long> result_shape = tv_shape;
  result_shape = Value::broadcast_shape(result_shape, tc_shape);
  result_shape = Value::broadcast_shape(result_shape, fc_shape);

  long result_size = std::accumulate(result_shape.begin(), result_shape.end(),
                                     1, std::multiplies<long>());
  double *result_data = (double *)malloc(sizeof(double) * result_size);

  for (long i = 0; i < result_size; ++i) {
    // Convert flat index to multi-dimensional indices
    std::vector<long> indices(result_shape.size());
    long temp = i;
    for (long j = result_shape.size() - 1; j >= 0; --j) {
      indices[j] = temp % result_shape[j];
      temp /= result_shape[j];
    }

    // Calculate indices for each operand
    long tv_idx = 0;
    if (truth_values->is_array) {
      std::vector<long> tv_indices(tv_shape.size());
      long offset = result_shape.size() - tv_shape.size();
      for (long j = 0; j < tv_shape.size(); ++j) {
        long idx = j + offset;
        tv_indices[j] = indices[idx] % tv_shape[j];
      }
      tv_idx = truth_values->to_flat_index(tv_indices);
    }

    long tc_idx = 0;
    if (true_case->is_array) {
      std::vector<long> tc_indices(tc_shape.size());
      long offset = result_shape.size() - tc_shape.size();
      for (long j = 0; j < tc_shape.size(); ++j) {
        long idx = j + offset;
        tc_indices[j] = indices[idx] % tc_shape[j];
      }
      tc_idx = true_case->to_flat_index(tc_indices);
    }

    long fc_idx = 0;
    if (false_case->is_array) {
      std::vector<long> fc_indices(fc_shape.size());
      long offset = result_shape.size() - fc_shape.size();
      for (long j = 0; j < fc_shape.size(); ++j) {
        long idx = j + offset;
        fc_indices[j] = indices[idx] % fc_shape[j];
      }
      fc_idx = false_case->to_flat_index(fc_indices);
    }

    // Apply where condition
    double tv = truth_values->get_element(tv_idx);
    if (tv != 0.0) {
      result_data[i] = true_case->get_element(tc_idx);
    } else {
      result_data[i] = false_case->get_element(fc_idx);
    }
  }

  if (result_size == 1) {
    valueStack.push(new Value(result_data[0]));
  } else {
    valueStack.push(new Value(result_shape, result_data));
  }

  free(result_data);
  delete truth_values;
  delete true_case;
  delete false_case;
}

void do_index() {
  if (valueStack.size() < 2) {
    std::cerr << "Runtime Error: index requires 2 inputs" << std::endl;
    return;
  }

  Value *index_val = valueStack.top();
  valueStack.pop();
  Value *target_array = valueStack.top();
  valueStack.pop();

  // Handle the case where target_array is a scalar (not an array)
  if (!target_array->is_array) {
    if (!index_val->is_array) {
      if (index_val->data[0] == 0.0) {
        valueStack.push(new Value(target_array->data[0]));
      } else {
        std::cerr << "Runtime Error: Invalid index for scalar value"
                  << std::endl;
      }
    } else {
      std::cerr << "Runtime Error: Cannot index scalar with array" << std::endl;
    }
    delete index_val;
    delete target_array; // Fixed memory leak
    return;
  }

  // Extract provided indices into a vector
  std::vector<long> indices;
  if (!index_val->is_array) {
    indices.push_back(static_cast<long>(index_val->data[0]));
  } else {
    if (index_val->shape.size() != 1) {
      std::cerr << "Runtime Error: Index must be a 1D array" << std::endl;
      delete index_val;
      delete target_array;
      return;
    }
    for (long i = 0; i < index_val->shape[0]; ++i) {
      indices.push_back(static_cast<long>(index_val->data[i]));
    }
  }

  // Check if we provided too many indices
  if (indices.size() > target_array->shape.size()) {
    std::cerr << "Runtime Error: Number of indices exceeds array dimensions"
              << std::endl;
    delete index_val;
    delete target_array;
    return;
  }

  // Validate that all provided indices are within array bounds
  bool valid = true;
  for (size_t i = 0; i < indices.size(); ++i) {
    if (indices[i] < 0 || indices[i] >= target_array->shape[i]) {
      valid = false;
      break;
    }
  }

  if (!valid) {
    std::cerr << "Runtime Error: Index out of bounds" << std::endl;
    delete index_val;
    delete target_array;
    return;
  }

  // Calculate the shape and total size of the resulting slice
  std::vector<long> result_shape(target_array->shape.begin() + indices.size(),
                                 target_array->shape.end());

  long result_size = 1;
  for (long dim : result_shape) {
    result_size *= dim;
  }

  // Calculate flat offset inside the target_array
  long flat_offset = 0;
  if (!target_array->shape.empty()) {
    // Compute strides for each dimension
    std::vector<long> strides(target_array->shape.size(), 1);
    for (long i = static_cast<long>(target_array->shape.size()) - 2; i >= 0;
         --i) {
      strides[i] = strides[i + 1] * target_array->shape[i + 1];
    }

    for (size_t i = 0; i < indices.size(); ++i) {
      flat_offset += indices[i] * strides[i];
    }
  }

  if (result_shape.empty()) {
    // Fully indexed: return a single scalar element
    valueStack.push(new Value(target_array->data[flat_offset]));
  } else {
    // Partially indexed: return a multi-dimensional array slice
    double *result_data = (double *)malloc(sizeof(double) * result_size);
    std::memcpy(result_data, target_array->data + flat_offset,
                sizeof(double) * result_size);

    valueStack.push(new Value(result_shape, result_data));
    free(result_data);
  }

  delete index_val;
  delete target_array;
}

void do_rotate() {
  if (valueStack.size() < 2) {
    std::cerr << "Runtime Error: rotate requires 2 inputs" << std::endl;
    return;
  }

  Value *n_val = valueStack.top();
  valueStack.pop();
  Value *m_val = valueStack.top();
  valueStack.pop();

  if (!m_val->is_array && !n_val->is_array) {
    long n = static_cast<long>(n_val->data[0]);
    long m = static_cast<long>(m_val->data[0]);

    if (n <= 0) {
      std::cerr
          << "Runtime Error: Number of elements to rotate must be positive"
          << std::endl;
    } else if (n > static_cast<long>(valueStack.size())) {
      std::cerr << "Runtime Error: Cannot rotate more elements than available "
                   "in stack"
                << std::endl;
    } else {
      // Normalize m to be strictly positive and within[0, n)
      m %= n;
      if (m < 0) {
        m += n;
      }

      if (m > 0) {
        // Extract top n elements
        std::vector<Value *> elements;
        elements.reserve(n);

        for (long i = 0; i < n; ++i) {
          elements.push_back(valueStack.top());
          valueStack.pop();
        }

        // Push back to stack in reverse order to maintain correct stack
        // semantics By correctly offsetting with m, we execute a perfect cyclic
        // stack rotation
        for (long i = n - 1; i >= 0; --i) {
          valueStack.push(elements[(i + m) % n]);
        }
      }
    }
  } else {
    std::cerr << "Runtime Error: rotate requires scalar arguments" << std::endl;
  }

  delete m_val;
  delete n_val;
}
}