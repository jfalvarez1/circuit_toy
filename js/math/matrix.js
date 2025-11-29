/**
 * Matrix operations for circuit simulation
 * Used by the Modified Nodal Analysis (MNA) solver
 */

class Matrix {
    constructor(rows, cols, fill = 0) {
        this.rows = rows;
        this.cols = cols;
        this.data = [];
        for (let i = 0; i < rows; i++) {
            this.data[i] = new Array(cols).fill(fill);
        }
    }

    static identity(n) {
        const m = new Matrix(n, n);
        for (let i = 0; i < n; i++) {
            m.data[i][i] = 1;
        }
        return m;
    }

    static fromArray(arr) {
        const m = new Matrix(arr.length, arr[0].length);
        for (let i = 0; i < arr.length; i++) {
            for (let j = 0; j < arr[0].length; j++) {
                m.data[i][j] = arr[i][j];
            }
        }
        return m;
    }

    get(row, col) {
        return this.data[row][col];
    }

    set(row, col, value) {
        this.data[row][col] = value;
    }

    add(row, col, value) {
        this.data[row][col] += value;
    }

    clone() {
        const m = new Matrix(this.rows, this.cols);
        for (let i = 0; i < this.rows; i++) {
            for (let j = 0; j < this.cols; j++) {
                m.data[i][j] = this.data[i][j];
            }
        }
        return m;
    }

    addMatrix(other) {
        if (this.rows !== other.rows || this.cols !== other.cols) {
            throw new Error('Matrix dimensions must match for addition');
        }
        const result = new Matrix(this.rows, this.cols);
        for (let i = 0; i < this.rows; i++) {
            for (let j = 0; j < this.cols; j++) {
                result.data[i][j] = this.data[i][j] + other.data[i][j];
            }
        }
        return result;
    }

    multiply(other) {
        if (this.cols !== other.rows) {
            throw new Error('Matrix dimensions incompatible for multiplication');
        }
        const result = new Matrix(this.rows, other.cols);
        for (let i = 0; i < this.rows; i++) {
            for (let j = 0; j < other.cols; j++) {
                let sum = 0;
                for (let k = 0; k < this.cols; k++) {
                    sum += this.data[i][k] * other.data[k][j];
                }
                result.data[i][j] = sum;
            }
        }
        return result;
    }

    scale(scalar) {
        const result = new Matrix(this.rows, this.cols);
        for (let i = 0; i < this.rows; i++) {
            for (let j = 0; j < this.cols; j++) {
                result.data[i][j] = this.data[i][j] * scalar;
            }
        }
        return result;
    }

    transpose() {
        const result = new Matrix(this.cols, this.rows);
        for (let i = 0; i < this.rows; i++) {
            for (let j = 0; j < this.cols; j++) {
                result.data[j][i] = this.data[i][j];
            }
        }
        return result;
    }

    toString() {
        return this.data.map(row =>
            row.map(val => val.toFixed(6).padStart(12)).join(' ')
        ).join('\n');
    }
}

/**
 * Vector class for circuit analysis
 */
class Vector {
    constructor(size, fill = 0) {
        this.size = size;
        this.data = new Array(size).fill(fill);
    }

    static fromArray(arr) {
        const v = new Vector(arr.length);
        for (let i = 0; i < arr.length; i++) {
            v.data[i] = arr[i];
        }
        return v;
    }

    get(index) {
        return this.data[index];
    }

    set(index, value) {
        this.data[index] = value;
    }

    add(index, value) {
        this.data[index] += value;
    }

    clone() {
        const v = new Vector(this.size);
        for (let i = 0; i < this.size; i++) {
            v.data[i] = this.data[i];
        }
        return v;
    }

    addVector(other) {
        if (this.size !== other.size) {
            throw new Error('Vector sizes must match');
        }
        const result = new Vector(this.size);
        for (let i = 0; i < this.size; i++) {
            result.data[i] = this.data[i] + other.data[i];
        }
        return result;
    }

    subtract(other) {
        if (this.size !== other.size) {
            throw new Error('Vector sizes must match');
        }
        const result = new Vector(this.size);
        for (let i = 0; i < this.size; i++) {
            result.data[i] = this.data[i] - other.data[i];
        }
        return result;
    }

    scale(scalar) {
        const result = new Vector(this.size);
        for (let i = 0; i < this.size; i++) {
            result.data[i] = this.data[i] * scalar;
        }
        return result;
    }

    norm() {
        let sum = 0;
        for (let i = 0; i < this.size; i++) {
            sum += this.data[i] * this.data[i];
        }
        return Math.sqrt(sum);
    }

    toArray() {
        return [...this.data];
    }

    toString() {
        return '[' + this.data.map(v => v.toFixed(6)).join(', ') + ']';
    }
}

// Export for use in other modules
window.Matrix = Matrix;
window.Vector = Vector;
