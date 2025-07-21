// Enhanced runtime with proper multidimensional array support
#include <iostream>
#include <stack>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <numeric>

struct Value {
    bool is_array;
    std::vector<int> shape;  // Shape dimensions (empty for scalars)
    double* data;           // Flattened data
    int total_size;         // Total number of elements
    
    // Constructor for scalar
    Value(double val) : is_array(false), shape(), total_size(1) {
        data = (double*)malloc(sizeof(double));
        data[0] = val;
    }
    
    // Constructor for multidimensional array
    Value(const std::vector<int>& sh, double* arr_data) 
        : is_array(true), shape(sh) {
        total_size = std::accumulate(sh.begin(), sh.end(), 1, std::multiplies<int>());
        data = (double*)malloc(sizeof(double) * total_size);
        memcpy(data, arr_data, sizeof(double) * total_size);
    }
    
    // Copy constructor
    Value(const Value& other) 
        : is_array(other.is_array), shape(other.shape), total_size(other.total_size) {
        data = (double*)malloc(sizeof(double) * total_size);
        memcpy(data, other.data, sizeof(double) * total_size);
    }
    
    ~Value() {
        if (data) free(data);
    }
    
    // Convert multi-dimensional indices to flat index
    int to_flat_index(const std::vector<int>& indices) const {
        if (indices.size() != shape.size()) return 0;
        
        int flat_idx = 0;
        int stride = 1;
        for (int i = shape.size() - 1; i >= 0; --i) {
            flat_idx += indices[i] * stride;
            stride *= shape[i];
        }
        return flat_idx;
    }
    
    // Get element with broadcasting
    double get_element(int flat_index) const {
        if (is_array) {
            return data[flat_index % total_size];  // Wrap around for broadcasting
        } else {
            return data[0]; // broadcast scalar
        }
    }
    
    // Check if two shapes are broadcastable
    static bool are_broadcastable(const std::vector<int>& shape1, const std::vector<int>& shape2) {
        int max_dims = std::max(shape1.size(), shape2.size());
        
        for (int i = 0; i < max_dims; ++i) {
            int dim1 = (i < shape1.size()) ? shape1[shape1.size() - 1 - i] : 1;
            int dim2 = (i < shape2.size()) ? shape2[shape2.size() - 1 - i] : 1;
            
            if (dim1 != dim2 && dim1 != 1 && dim2 != 1) {
                return false;
            }
        }
        return true;
    }
    
    // Compute broadcast result shape
    static std::vector<int> broadcast_shape(const std::vector<int>& shape1, const std::vector<int>& shape2) {
        int max_dims = std::max(shape1.size(), shape2.size());
        std::vector<int> result(max_dims);
        
        for (int i = 0; i < max_dims; ++i) {
            int dim1 = (i < shape1.size()) ? shape1[shape1.size() - 1 - i] : 1;
            int dim2 = (i < shape2.size()) ? shape2[shape2.size() - 1 - i] : 1;
            result[max_dims - 1 - i] = std::max(dim1, dim2);
        }
        return result;
    }
    
    // Print array with proper shape formatting
    void print(std::ostream& os) const {
        if (!is_array) {
            os << data[0];
            return;
        }
        
        if (shape.size() == 1) {
            // 1D array
            os << "[";
            for (int i = 0; i < shape[0]; ++i) {
                if (i > 0) os << ", ";
                os << data[i];
            }
            os << "]";
        } else if (shape.size() == 2) {
            // 2D array (matrix)
            os << "[" << std::endl;
            for (int i = 0; i < shape[0]; ++i) {
                os << "  [";
                for (int j = 0; j < shape[1]; ++j) {
                    if (j > 0) os << ", ";
                    os << data[i * shape[1] + j];
                }
                os << "]";
                if (i < shape[0] - 1) os << ",";
                os << std::endl;
            }
            os << "]";
        } else {
            // Higher dimensions - just print flat with shape info
            os << "Array(shape=[";
            for (size_t i = 0; i < shape.size(); ++i) {
                if (i > 0) os << ", ";
                os << shape[i];
            }
            os << "], data=[";
            for (int i = 0; i < std::min(10, total_size); ++i) {
                if (i > 0) os << ", ";
                os << data[i];
            }
            if (total_size > 10) os << ", ...";
            os << "])";
        }
    }
};

static std::stack<Value*> valueStack;

extern "C" {
    void push_double(double val) {
        valueStack.push(new Value(val));
    }
    
    void push_array_data(int size, double* data) {
        std::vector<int> shape = {size};  // Default to 1D
        valueStack.push(new Value(shape, data));
    }
    
    // Enhanced: push multidimensional array
    void push_multidim_array(int ndim, int* shape_data, double* data) {
        std::vector<int> shape(shape_data, shape_data + ndim);
        valueStack.push(new Value(shape, data));
    }
    
    // Binary operation with proper broadcasting
    void binary_op(void (*operation)(double, double, double*)) {
        if (valueStack.size() < 2) {
            std::cerr << "Runtime Error: Not enough values for binary operation" << std::endl;
            return;
        }
        
        Value* b = valueStack.top(); valueStack.pop();
        Value* a = valueStack.top(); valueStack.pop();
        
        // Handle scalar cases
        if (!a->is_array && !b->is_array) {
            // Both scalars
            double result;
            operation(a->data[0], b->data[0], &result);
            valueStack.push(new Value(result));
        } else {
            // At least one array - use broadcasting
            std::vector<int> a_shape = a->is_array ? a->shape : std::vector<int>{1};
            std::vector<int> b_shape = b->is_array ? b->shape : std::vector<int>{1};
            
            if (!Value::are_broadcastable(a_shape, b_shape)) {
                std::cerr << "Runtime Error: Shapes not broadcastable" << std::endl;
                delete a; delete b;
                return;
            }
            
            std::vector<int> result_shape = Value::broadcast_shape(a_shape, b_shape);
            int result_size = std::accumulate(result_shape.begin(), result_shape.end(), 1, std::multiplies<int>());
            double* result_data = (double*)malloc(sizeof(double) * result_size);
            
            // Perform broadcasted operation
            for (int i = 0; i < result_size; ++i) {
                // Map flat index to multidimensional indices and back to original arrays
                int a_idx = i % a->total_size;
                int b_idx = i % b->total_size;
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
        
        delete a; delete b;
    }
    
    // Matrix multiplication (proper 2D operation)
    void matrix_multiply() {
        if (valueStack.size() < 2) {
            std::cerr << "Runtime Error: Not enough values for matrix multiplication" << std::endl;
            return;
        }
        
        Value* b = valueStack.top(); valueStack.pop();
        Value* a = valueStack.top(); valueStack.pop();
        
        // Check dimensions for matrix multiplication
        if (!a->is_array || !b->is_array || 
            a->shape.size() != 2 || b->shape.size() != 2 ||
            a->shape[1] != b->shape[0]) {
            std::cerr << "Runtime Error: Invalid shapes for matrix multiplication" << std::endl;
            delete a; delete b;
            return;
        }
        
        int m = a->shape[0];
        int n = a->shape[1];
        int p = b->shape[1];
        
        std::vector<int> result_shape = {m, p};
        double* result_data = (double*)malloc(sizeof(double) * m * p);
        
        // Perform matrix multiplication
        for (int i = 0; i < m; ++i) {
            for (int j = 0; j < p; ++j) {
                double sum = 0.0;
                for (int k = 0; k < n; ++k) {
                    sum += a->data[i * n + k] * b->data[k * p + j];
                }
                result_data[i * p + j] = sum;
            }
        }
        
        valueStack.push(new Value(result_shape, result_data));
        free(result_data);
        delete a; delete b;
    }
    
    // Reshape operation
    void reshape_top(int ndim, int* new_shape) {
        if (valueStack.empty()) return;
        
        Value* val = valueStack.top();
        valueStack.pop();
        
        std::vector<int> shape(new_shape, new_shape + ndim);
        int new_size = std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<int>());
        
        if (new_size != val->total_size) {
            std::cerr << "Runtime Error: Cannot reshape array with different total size" << std::endl;
            valueStack.push(val);
            return;
        }
        
        Value* reshaped = new Value(shape, val->data);
        delete val;
        valueStack.push(reshaped);
    }
    
    void pop_and_print() {
        if (valueStack.empty()) {
            std::cout << "Stack empty" << std::endl;
            return;
        }
        
        Value* val = valueStack.top();
        valueStack.pop();
        
        val->print(std::cout);
        std::cout << std::endl;
        
        delete val;
    }
    
    // Standard operations (unchanged)
    void add_op(double a, double b, double* result) { *result = a + b; }
    void sub_op(double a, double b, double* result) { *result = a - b; }
    void mul_op(double a, double b, double* result) { *result = a * b; }
    void div_op(double a, double b, double* result) { *result = a / b; }
    
    void do_add() { binary_op(add_op); }
    void do_sub() { binary_op(sub_op); }
    void do_mul() { binary_op(mul_op); }
    void do_div() { binary_op(div_op); }
    
    // Other utility functions remain the same...
    void duplicate_top() {
        if (valueStack.empty()) return;
        Value* val = valueStack.top();
        valueStack.push(new Value(*val));
    }
    
    void swap_top() {
        if (valueStack.size() < 2) return;
        Value* a = valueStack.top(); valueStack.pop();
        Value* b = valueStack.top(); valueStack.pop();
        valueStack.push(a);
        valueStack.push(b);
    }

    void* runtime_malloc(size_t size) {
        return std::malloc(size);
    }

    void runtime_free(void* ptr) {
        std::free(ptr);
    }
}