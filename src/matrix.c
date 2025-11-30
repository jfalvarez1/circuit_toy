/**
 * Circuit Playground - Matrix Operations Implementation
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "matrix.h"

Matrix *matrix_create(int rows, int cols) {
    Matrix *m = malloc(sizeof(Matrix));
    if (!m) return NULL;

    m->rows = rows;
    m->cols = cols;
    m->data = calloc(rows * cols, sizeof(double));

    if (!m->data) {
        free(m);
        return NULL;
    }

    return m;
}

void matrix_free(Matrix *m) {
    if (m) {
        free(m->data);
        free(m);
    }
}

void matrix_zero(Matrix *m) {
    if (m && m->data) {
        memset(m->data, 0, m->rows * m->cols * sizeof(double));
    }
}

double matrix_get(Matrix *m, int row, int col) {
    if (!m || row < 0 || row >= m->rows || col < 0 || col >= m->cols) {
        return 0.0;
    }
    return m->data[row * m->cols + col];
}

void matrix_set(Matrix *m, int row, int col, double val) {
    if (m && row >= 0 && row < m->rows && col >= 0 && col < m->cols) {
        m->data[row * m->cols + col] = val;
    }
}

void matrix_add(Matrix *m, int row, int col, double val) {
    if (m && row >= 0 && row < m->rows && col >= 0 && col < m->cols) {
        m->data[row * m->cols + col] += val;
    }
}

Matrix *matrix_clone(Matrix *m) {
    if (!m) return NULL;

    Matrix *clone = matrix_create(m->rows, m->cols);
    if (clone) {
        memcpy(clone->data, m->data, m->rows * m->cols * sizeof(double));
    }
    return clone;
}

Vector *vector_create(int size) {
    Vector *v = malloc(sizeof(Vector));
    if (!v) return NULL;

    v->size = size;
    v->data = calloc(size, sizeof(double));

    if (!v->data) {
        free(v);
        return NULL;
    }

    return v;
}

void vector_free(Vector *v) {
    if (v) {
        free(v->data);
        free(v);
    }
}

void vector_zero(Vector *v) {
    if (v && v->data) {
        memset(v->data, 0, v->size * sizeof(double));
    }
}

double vector_get(Vector *v, int idx) {
    if (!v || idx < 0 || idx >= v->size) {
        return 0.0;
    }
    return v->data[idx];
}

void vector_set(Vector *v, int idx, double val) {
    if (v && idx >= 0 && idx < v->size) {
        v->data[idx] = val;
    }
}

void vector_add(Vector *v, int idx, double val) {
    if (v && idx >= 0 && idx < v->size) {
        v->data[idx] += val;
    }
}

Vector *vector_clone(Vector *v) {
    if (!v) return NULL;

    Vector *clone = vector_create(v->size);
    if (clone) {
        memcpy(clone->data, v->data, v->size * sizeof(double));
    }
    return clone;
}

double vector_norm(Vector *v) {
    if (!v) return 0.0;

    double sum = 0.0;
    for (int i = 0; i < v->size; i++) {
        sum += v->data[i] * v->data[i];
    }
    return sqrt(sum);
}

/**
 * Solve linear system Ax = b using Gaussian elimination with partial pivoting
 */
Vector *linear_solve(Matrix *A, Vector *b) {
    if (!A || !b || A->rows != A->cols || A->rows != b->size) {
        return NULL;
    }

    int n = A->rows;

    // Create augmented matrix [A|b]
    double *aug = malloc(n * (n + 1) * sizeof(double));
    if (!aug) return NULL;

    // Copy A and b into augmented matrix
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            aug[i * (n + 1) + j] = A->data[i * n + j];
        }
        aug[i * (n + 1) + n] = b->data[i];
    }

    // Forward elimination with partial pivoting
    for (int col = 0; col < n; col++) {
        // Find pivot
        int max_row = col;
        double max_val = fabs(aug[col * (n + 1) + col]);

        for (int row = col + 1; row < n; row++) {
            double val = fabs(aug[row * (n + 1) + col]);
            if (val > max_val) {
                max_val = val;
                max_row = row;
            }
        }

        // Swap rows if needed
        if (max_row != col) {
            for (int j = 0; j <= n; j++) {
                double temp = aug[col * (n + 1) + j];
                aug[col * (n + 1) + j] = aug[max_row * (n + 1) + j];
                aug[max_row * (n + 1) + j] = temp;
            }
        }

        // Check for singular matrix
        double pivot = aug[col * (n + 1) + col];
        if (fabs(pivot) < 1e-15) {
            // Matrix is singular, set small value
            pivot = 1e-15;
            aug[col * (n + 1) + col] = pivot;
        }

        // Eliminate column
        for (int row = col + 1; row < n; row++) {
            double factor = aug[row * (n + 1) + col] / pivot;
            for (int j = col; j <= n; j++) {
                aug[row * (n + 1) + j] -= factor * aug[col * (n + 1) + j];
            }
        }
    }

    // Back substitution
    Vector *x = vector_create(n);
    if (!x) {
        free(aug);
        return NULL;
    }

    for (int i = n - 1; i >= 0; i--) {
        double sum = aug[i * (n + 1) + n];
        for (int j = i + 1; j < n; j++) {
            sum -= aug[i * (n + 1) + j] * x->data[j];
        }
        double diag = aug[i * (n + 1) + i];
        x->data[i] = (fabs(diag) > 1e-15) ? sum / diag : 0.0;
    }

    free(aug);
    return x;
}
