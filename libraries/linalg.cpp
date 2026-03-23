#include "../stack_runtime.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <vector>

// ============================================================================
//  Internal matrix type — row-major, heap-allocated, RAII
//  All linalg algorithms work on Mat internally and convert to/from Value
//  at the boundary. This keeps the algorithm code clean and prevents any
//  accidental stack corruption mid-computation.
// ============================================================================

struct Mat {
    long rows, cols;
    std::vector<double> d; // row-major: d[i*cols + j]

    Mat(long r, long c) : rows(r), cols(c), d(r * c, 0.0) {}

    double &at(long i, long j)       { return d[i * cols + j]; }
    double  at(long i, long j) const { return d[i * cols + j]; }

    // Construct from a Value (must be 2D)
    static Mat from_value(const Value *v) {
        Mat m(v->shape[0], v->shape[1]);
        std::copy(v->data, v->data + m.rows * m.cols, m.d.begin());
        return m;
    }

    // Push this matrix onto the value stack
    void push() const {
        std::vector<long> shape = {rows, cols};
        // Value constructor copies the data, so passing d.data() is safe
        valueStack.push(new Value(shape, const_cast<double *>(d.data())));
    }

    // Push as a 1-D vector (used for eigenvalue/singular-value arrays)
    void push_flat() const {
        std::vector<long> shape = {rows * cols};
        valueStack.push(new Value(shape, const_cast<double *>(d.data())));
    }

    Mat col(long j) const {
        Mat c(rows, 1);
        for (long i = 0; i < rows; ++i) c.at(i, 0) = at(i, j);
        return c;
    }

    double dot_col(long j, const Mat &other, long oj) const {
        double s = 0.0;
        for (long i = 0; i < rows; ++i) s += at(i, j) * other.at(i, oj);
        return s;
    }

    double norm_col(long j) const {
        return std::sqrt(dot_col(j, *this, j));
    }

    Mat transpose() const {
        Mat t(cols, rows);
        for (long i = 0; i < rows; ++i)
            for (long jj = 0; jj < cols; ++jj)
                t.at(jj, i) = at(i, jj);
        return t;
    }

    // In-place identity
    void set_identity() {
        std::fill(d.begin(), d.end(), 0.0);
        long mn = std::min(rows, cols);
        for (long i = 0; i < mn; ++i) at(i, i) = 1.0;
    }

    static Mat identity(long n) {
        Mat m(n, n);
        m.set_identity();
        return m;
    }

    static Mat multiply(const Mat &A, const Mat &B) {
        Mat C(A.rows, B.cols);
        for (long i = 0; i < A.rows; ++i)
            for (long k = 0; k < A.cols; ++k)
                for (long j = 0; j < B.cols; ++j)
                    C.at(i, j) += A.at(i, k) * B.at(k, j);
        return C;
    }
};

static constexpr double LINALG_EPS   = 1e-9; // near-zero threshold
static constexpr int    MAX_ITER     = 1000;   // iteration cap for QR/SVD

// ============================================================================
//  Validation helpers
// ============================================================================

static bool require_2d(const Value *v, const char *fn) {
    if (!v->is_array || v->shape.size() != 2) {
        std::cerr << "Runtime Error: " << fn
                  << " requires a 2-D matrix" << std::endl;
        return false;
    }
    return true;
}

static bool require_square(const Value *v, const char *fn) {
    if (!require_2d(v, fn)) return false;
    if (v->shape[0] != v->shape[1]) {
        std::cerr << "Runtime Error: " << fn
                  << " requires a square matrix" << std::endl;
        return false;
    }
    return true;
}

// ============================================================================
//  QR decomposition via modified Gram-Schmidt
//
//  Decomposes A (m×n, m >= n) into Q (m×n orthonormal) and R (n×n upper-tri).
//  Modified Gram-Schmidt is numerically superior to classical GS because it
//  re-orthogonalises against already-computed columns incrementally, reducing
//  floating-point error accumulation.
//
//  Pushes TWO values: Q on bottom, R on top  (so caller pops R first).
// ============================================================================

static void qr_decompose(const Mat &A, Mat &Q, Mat &R) {
    long m = A.rows, n = A.cols;
    Q = Mat(m, n);
    R = Mat(n, n);

    // Copy columns of A into Q to begin orthogonalisation
    for (long j = 0; j < n; ++j)
        for (long i = 0; i < m; ++i)
            Q.at(i, j) = A.at(i, j);

    for (long j = 0; j < n; ++j) {
        // Compute norm of current column
        double norm = Q.norm_col(j);
        R.at(j, j) = norm;

        if (norm < LINALG_EPS) continue; // numerically zero column, skip

        // Normalise column j
        for (long i = 0; i < m; ++i)
            Q.at(i, j) /= norm;

        // Subtract projection onto column j from all subsequent columns
        for (long k = j + 1; k < n; ++k) {
            double proj = Q.dot_col(j, Q, k); // <q_j, q_k>
            R.at(j, k) = proj;
            for (long i = 0; i < m; ++i)
                Q.at(i, k) -= proj * Q.at(i, j);
        }
    }
}

// ============================================================================
//  linalg.qr  — QR decomposition
//
//  Pops:  A  (m×n matrix, m >= n)
//  Pushes: Q (m×n), then R (n×n) — R is on top
// ============================================================================

extern "C" void linalg_qr() {
    if (valueStack.empty()) return;
    Value *v = valueStack.top(); valueStack.pop();

    if (!require_2d(v, "linalg.qr")) { delete v; return; }
    if (v->shape[0] < v->shape[1]) {
        std::cerr << "Runtime Error: linalg.qr requires m >= n" << std::endl;
        delete v; return;
    }

    Mat A = Mat::from_value(v);
    delete v;

    Mat Q(0,0), R(0,0);
    qr_decompose(A, Q, R);

    Q.push(); // bottom
    R.push(); // top
}

// ============================================================================
//  Gaussian elimination with partial pivoting → row-echelon form
//
//  Returns the row-echelon form of A and the number of row swaps performed
//  (needed for determinant sign). Works on a copy — non-destructive.
// ============================================================================

static Mat gaussian_echelon(const Mat &A, int &swap_count) {
    Mat M = A; // working copy
    long m = M.rows, n = M.cols;
    long pivot_row = 0;
    swap_count = 0;

    for (long col = 0; col < n && pivot_row < m; ++col) {
        // Find row with largest absolute value in this column (partial pivot)
        long best = pivot_row;
        double best_val = std::abs(M.at(pivot_row, col));
        for (long row = pivot_row + 1; row < m; ++row) {
            double v = std::abs(M.at(row, col));
            if (v > best_val) { best_val = v; best = row; }
        }

        if (best_val < LINALG_EPS) continue; // entire column is zero, skip

        // Swap rows if needed
        if (best != pivot_row) {
            for (long jj = 0; jj < n; ++jj)
                std::swap(M.at(pivot_row, jj), M.at(best, jj));
            ++swap_count;
        }

        // Eliminate rows below pivot
        double pivot = M.at(pivot_row, col);
        for (long row = pivot_row + 1; row < m; ++row) {
            double factor = M.at(row, col) / pivot;
            for (long jj = col; jj < n; ++jj)
                M.at(row, jj) -= factor * M.at(pivot_row, jj);
            M.at(row, col) = 0.0; // force exact zero
        }

        ++pivot_row;
    }

    return M;
}

// ============================================================================
//  linalg.gaussian  — Gaussian elimination (partial pivot, row-echelon form)
//
//  Pops:  A  (m×n matrix)
//  Pushes: row-echelon form of A
// ============================================================================

extern "C" void linalg_gaussian() {
    if (valueStack.empty()) return;
    Value *v = valueStack.top(); valueStack.pop();

    if (!require_2d(v, "linalg.gaussian")) { delete v; return; }

    Mat A = Mat::from_value(v);
    delete v;

    int swaps;
    Mat E = gaussian_echelon(A, swaps);
    E.push();
}

// ============================================================================
//  linalg.inverse  — matrix inverse via Gauss-Jordan elimination
//
//  Augments [A | I] and reduces to [I | A^-1].
//  Partial pivoting is applied throughout for numerical stability.
//
//  Pops:  A  (n×n matrix)
//  Pushes: A^-1, or prints an error and pushes nothing if A is singular
// ============================================================================

extern "C" void linalg_inverse() {
    if (valueStack.empty()) return;
    Value *v = valueStack.top(); valueStack.pop();

    if (!require_square(v, "linalg.inverse")) { delete v; return; }

    long n = v->shape[0];
    Mat A = Mat::from_value(v);
    delete v;

    // Build augmented matrix [A | I]
    Mat aug(n, 2 * n);
    for (long i = 0; i < n; ++i) {
        for (long j = 0; j < n; ++j)
            aug.at(i, j) = A.at(i, j);
        aug.at(i, i + n) = 1.0; // identity on the right
    }

    // Forward elimination with partial pivoting
    for (long col = 0; col < n; ++col) {
        // Partial pivot
        long best = col;
        double best_val = std::abs(aug.at(col, col));
        for (long row = col + 1; row < n; ++row) {
            double val = std::abs(aug.at(row, col));
            if (val > best_val) { best_val = val; best = row; }
        }

        if (best_val < LINALG_EPS) {
            std::cerr << "Runtime Error: linalg.inverse — matrix is singular "
                         "or numerically rank-deficient" << std::endl;
            return;
        }

        if (best != col)
            for (long j = 0; j < 2 * n; ++j)
                std::swap(aug.at(col, j), aug.at(best, j));

        // Scale pivot row so the pivot becomes 1
        double pivot = aug.at(col, col);
        for (long j = 0; j < 2 * n; ++j)
            aug.at(col, j) /= pivot;

        // Eliminate the entire column — above AND below (Gauss-Jordan)
        for (long row = 0; row < n; ++row) {
            if (row == col) continue;
            double factor = aug.at(row, col);
            for (long j = 0; j < 2 * n; ++j)
                aug.at(row, j) -= factor * aug.at(col, j);
        }
    }

    // Extract right half — that's A^-1
    Mat inv(n, n);
    for (long i = 0; i < n; ++i)
        for (long j = 0; j < n; ++j)
            inv.at(i, j) = aug.at(i, j + n);

    inv.push();
}

// ============================================================================
//  Eigenvalues + eigenvectors via QR iteration (Francis shift)
//
//  The QR algorithm iteratively applies QR decompositions: A <- R*Q, while
//  accumulating Q matrices to converge eigenvectors. Francis shift improves
//  convergence by shifting A by an approximation of the eigenvalue before
//  each step.
//
//  Converges for symmetric matrices and most general real matrices.
//  Complex eigenvalue pairs (from non-symmetric matrices) will converge to
//  2×2 diagonal blocks; we extract the real eigenvalues from those blocks.
// ============================================================================

extern "C" void linalg_eigen() {
    if (valueStack.empty()) return;
    Value *v = valueStack.top(); valueStack.pop();

    if (!require_square(v, "linalg.eigen")) { delete v; return; }

    long n = v->shape[0];
    Mat A = Mat::from_value(v);
    delete v;

    // V accumulates the product of all Q matrices → columns become eigenvectors
    Mat V = Mat::identity(n);

    for (int iter = 0; iter < MAX_ITER; ++iter) {
        // Francis (Wilkinson) shift: use bottom-right element as shift
        double shift = A.at(n - 1, n - 1);

        // Shift A
        for (long i = 0; i < n; ++i) A.at(i, i) -= shift;

        Mat Q(0,0), R(0,0);
        qr_decompose(A, Q, R);

        // A <- R * Q + shift*I  (unshift after recombination)
        A = Mat::multiply(R, Q);
        for (long i = 0; i < n; ++i) A.at(i, i) += shift;

        // Accumulate eigenvectors
        V = Mat::multiply(V, Q);

        // Convergence check: off-diagonal elements of the lower triangle
        double off = 0.0;
        for (long i = 1; i < n; ++i)
            for (long j = 0; j < i; ++j)
                off += std::abs(A.at(i, j));

        if (off < LINALG_EPS) break;
    }

    // Build result: n × (n+1) matrix
    //   column 0       = eigenvalues  (diagonal of A after convergence)
    //   columns 1..n   = eigenvectors (columns of V)
    Mat result(n, n + 1);
    for (long i = 0; i < n; ++i) {
        result.at(i, 0) = A.at(i, i); // eigenvalue
        for (long j = 0; j < n; ++j)
            result.at(j, i + 1) = V.at(j, i); // eigenvector in column i+1
    }

    result.push();
}

// ============================================================================
//  Singular Value Decomposition via Golub-Reinsch (bidiagonalisation + QR)
//
//  For an m×n matrix A (m >= n):
//    A = U * Σ * V^T
//  where U is m×n (left singular vectors), Σ is n×1 (singular values,
//  descending), V is n×n (right singular vectors).
//
//  Algorithm:
//    1. Bidiagonalise A using Householder reflections → B = U_h^T * A * V_h
//    2. Apply QR iterations on B to diagonalise → singular values on diagonal
//    3. Accumulate U and V throughout
//
//  Pops:  A  (m×n, must be rank-2, m >= n)
//  Pushes: U (m×n), then Σ as 1-D array of length n, then V (n×n)
//          Stack order top→bottom: V, Σ, U
// ============================================================================

// Compute Householder vector for a column segment starting at index 'start'
static std::vector<double> householder_vec(const Mat &M, long col, long start) {
    long m = M.rows;
    std::vector<double> v(m - start, 0.0);
    for (long i = start; i < m; ++i) v[i - start] = M.at(i, col);

    double norm = 0.0;
    for (double x : v) norm += x * x;
    norm = std::sqrt(norm);

    v[0] += (v[0] >= 0 ? 1.0 : -1.0) * norm;

    double v_norm = 0.0;
    for (double x : v) v_norm += x * x;
    v_norm = std::sqrt(v_norm);

    if (v_norm > LINALG_EPS)
        for (double &x : v) x /= v_norm;

    return v;
}

// Apply Householder reflector H = I - 2*v*v^T from the left to M[start:, col:]
static void apply_householder_left(Mat &M, const std::vector<double> &v,
                                   long row_start, long col_start) {
    long m = M.rows, n = M.cols;
    for (long j = col_start; j < n; ++j) {
        double dot = 0.0;
        for (long i = row_start; i < m; ++i)
            dot += v[i - row_start] * M.at(i, j);
        for (long i = row_start; i < m; ++i)
            M.at(i, j) -= 2.0 * v[i - row_start] * dot;
    }
}

// Apply Householder reflector from the right to M[:, col_start:]
static void apply_householder_right(Mat &M, const std::vector<double> &v,
                                    long row_start, long col_start) {
    long m = M.rows, n = M.cols;
    (void)n;
    for (long i = row_start; i < m; ++i) {
        double dot = 0.0;
        for (long j = col_start; j < (long)v.size() + col_start; ++j)
            dot += M.at(i, j) * v[j - col_start];
        for (long j = col_start; j < (long)v.size() + col_start; ++j)
            M.at(i, j) -= 2.0 * v[j - col_start] * dot;
    }
}

extern "C" void linalg_svd() {
    if (valueStack.empty()) return;
    Value *val = valueStack.top(); valueStack.pop();

    if (!require_2d(val, "linalg.svd")) { delete val; return; }
    if (val->shape[0] < val->shape[1]) {
        std::cerr << "Runtime Error: linalg.svd requires m >= n "
                     "(transpose first if m < n)" << std::endl;
        delete val; return;
    }

    long m = val->shape[0], n = val->shape[1];
    Mat B  = Mat::from_value(val);
    delete val;

    Mat U = Mat::identity(m);
    Mat V = Mat::identity(n);

    // ---- Step 1: Bidiagonalise via Householder ----
    for (long k = 0; k < n; ++k) {
        // Left Householder: zero out below-diagonal in column k
        {
            auto v = householder_vec(B, k, k);
            if (!v.empty()) {
                apply_householder_left(B, v, k, k);
                apply_householder_right(U, v, k, 0); // accumulate into U^T
            }
        }

        // Right Householder: zero out right of superdiagonal in row k
        if (k < n - 2) {
            // Work on the transposed view for row operations
            Mat Bt = B.transpose();
            auto v = householder_vec(Bt, k, k + 1);
            if (!v.empty()) {
                apply_householder_left(Bt, v, k + 1, k);
                B = Bt.transpose();
                apply_householder_right(V, v, 0, k + 1);
            }
        }
    }

    // U was accumulated as U^T, so transpose it back
    U = U.transpose();

    // ---- Step 2: QR iteration on bidiagonal B to extract singular values ----
    for (int iter = 0; iter < MAX_ITER; ++iter) {
        // Zero out negligible superdiagonal entries
        for (long i = 0; i < n - 1; ++i) {
            if (std::abs(B.at(i, i + 1)) <
                LINALG_EPS * (std::abs(B.at(i, i)) + std::abs(B.at(i + 1, i + 1)))) {
                B.at(i, i + 1) = 0.0;
            }
        }

        // Check convergence: all superdiagonal elements near zero
        double off = 0.0;
        for (long i = 0; i < n - 1; ++i) off += std::abs(B.at(i, i + 1));
        if (off < LINALG_EPS) break;

        // Implicit QR step: apply Givens rotations
        for (long i = 0; i < n - 1; ++i) {
            double a = B.at(i, i), b = B.at(i, i + 1);
            double r = std::sqrt(a * a + b * b);
            if (r < LINALG_EPS) continue;

            double c = a / r, s = b / r;

            // Apply Givens rotation from right (columns i, i+1)
            for (long row = 0; row < m; ++row) {
                double x = B.at(row, i), y = (i + 1 < n) ? B.at(row, i + 1) : 0.0;
                B.at(row, i)     =  c * x + s * y;
                if (i + 1 < n) B.at(row, i + 1) = -s * x + c * y;
            }
            // Accumulate into V
            for (long row = 0; row < n; ++row) {
                double x = V.at(row, i), y = V.at(row, i + 1);
                V.at(row, i)     =  c * x + s * y;
                V.at(row, i + 1) = -s * x + c * y;
            }

            // Apply Givens rotation from left (rows i, i+1)
            a = B.at(i, i); b = B.at(i + 1, i);
            r = std::sqrt(a * a + b * b);
            if (r < LINALG_EPS) continue;
            c = a / r; s = b / r;

            for (long col = 0; col < n; ++col) {
                double x = B.at(i, col), y = B.at(i + 1, col);
                B.at(i, col)     =  c * x + s * y;
                B.at(i + 1, col) = -s * x + c * y;
            }
            // Accumulate into U
            for (long col = 0; col < m; ++col) {
                double x = U.at(col, i), y = U.at(col, i + 1);
                U.at(col, i)     =  c * x + s * y;
                U.at(col, i + 1) = -s * x + c * y;
            }
        }
    }

    // ---- Step 3: Extract singular values and enforce positivity ----
    // Singular values are the diagonal of B; make them non-negative
    std::vector<double> sigma(n);
    for (long i = 0; i < n; ++i) {
        sigma[i] = B.at(i, i);
        if (sigma[i] < 0.0) {
            sigma[i] = -sigma[i];
            for (long j = 0; j < m; ++j) U.at(j, i) = -U.at(j, i);
        }
    }

    // Sort singular values descending, permute U and V columns accordingly
    std::vector<long> idx(n);
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(),
              [&](long a, long b) { return sigma[a] > sigma[b]; });

    std::vector<double> sigma_sorted(n);
    Mat U_sorted(m, n), V_sorted(n, n);
    for (long k = 0; k < n; ++k) {
        sigma_sorted[k] = sigma[idx[k]];
        for (long i = 0; i < m; ++i) U_sorted.at(i, k) = U.at(i, idx[k]);
        for (long i = 0; i < n; ++i) V_sorted.at(i, k) = V.at(i, idx[k]);
    }

    // Push: U on bottom, Σ in middle, V on top
    U_sorted.push();

    {
        std::vector<long> shape = {n};
        valueStack.push(new Value(shape, sigma_sorted.data()));
    }

    V_sorted.push();
}
