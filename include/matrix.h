/**
 * Circuit Playground - Matrix Operations for MNA Solver
 */

#ifndef MATRIX_H
#define MATRIX_H

#include "types.h"

// Matrix structure
typedef struct {
    int rows;
    int cols;
    double *data;
} Matrix;

// Vector structure
typedef struct {
    int size;
    double *data;
} Vector;

// Matrix functions
Matrix *matrix_create(int rows, int cols);
void matrix_free(Matrix *m);
void matrix_zero(Matrix *m);
double matrix_get(Matrix *m, int row, int col);
void matrix_set(Matrix *m, int row, int col, double val);
void matrix_add(Matrix *m, int row, int col, double val);
Matrix *matrix_clone(Matrix *m);

// Vector functions
Vector *vector_create(int size);
void vector_free(Vector *v);
void vector_zero(Vector *v);
double vector_get(Vector *v, int idx);
void vector_set(Vector *v, int idx, double val);
void vector_add(Vector *v, int idx, double val);
Vector *vector_clone(Vector *v);
double vector_norm(Vector *v);

// Linear solver - solves Ax = b, returns x
Vector *linear_solve(Matrix *A, Vector *b);

#endif // MATRIX_H
