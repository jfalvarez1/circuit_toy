/**
 * Linear system solver using LU decomposition with partial pivoting
 * Solves Ax = b for circuit analysis
 */

class LinearSolver {
    /**
     * Solve the linear system Ax = b using LU decomposition
     * @param {Matrix} A - Coefficient matrix
     * @param {Vector} b - Right-hand side vector
     * @returns {Vector} Solution vector x
     */
    static solve(A, b) {
        const n = A.rows;

        // Create augmented matrix
        const aug = new Matrix(n, n + 1);
        for (let i = 0; i < n; i++) {
            for (let j = 0; j < n; j++) {
                aug.set(i, j, A.get(i, j));
            }
            aug.set(i, n, b.get(i));
        }

        // Forward elimination with partial pivoting
        for (let col = 0; col < n; col++) {
            // Find pivot
            let maxRow = col;
            let maxVal = Math.abs(aug.get(col, col));

            for (let row = col + 1; row < n; row++) {
                const val = Math.abs(aug.get(row, col));
                if (val > maxVal) {
                    maxVal = val;
                    maxRow = row;
                }
            }

            // Swap rows if needed
            if (maxRow !== col) {
                for (let j = 0; j <= n; j++) {
                    const temp = aug.get(col, j);
                    aug.set(col, j, aug.get(maxRow, j));
                    aug.set(maxRow, j, temp);
                }
            }

            // Check for singular matrix
            const pivot = aug.get(col, col);
            if (Math.abs(pivot) < 1e-15) {
                // Matrix is singular or nearly singular
                // Try to continue with a small value
                aug.set(col, col, 1e-15);
            }

            // Eliminate column
            for (let row = col + 1; row < n; row++) {
                const factor = aug.get(row, col) / aug.get(col, col);
                for (let j = col; j <= n; j++) {
                    aug.set(row, j, aug.get(row, j) - factor * aug.get(col, j));
                }
            }
        }

        // Back substitution
        const x = new Vector(n);
        for (let i = n - 1; i >= 0; i--) {
            let sum = aug.get(i, n);
            for (let j = i + 1; j < n; j++) {
                sum -= aug.get(i, j) * x.get(j);
            }
            const diag = aug.get(i, i);
            x.set(i, Math.abs(diag) > 1e-15 ? sum / diag : 0);
        }

        return x;
    }

    /**
     * Solve using Newton-Raphson iteration for nonlinear circuits
     * @param {Function} f - Function that computes residual f(x) = 0
     * @param {Function} J - Function that computes Jacobian matrix
     * @param {Vector} x0 - Initial guess
     * @param {number} maxIter - Maximum iterations
     * @param {number} tol - Convergence tolerance
     * @returns {Object} { solution, converged, iterations }
     */
    static newtonRaphson(f, J, x0, maxIter = 50, tol = 1e-9) {
        let x = x0.clone();

        for (let iter = 0; iter < maxIter; iter++) {
            const fx = f(x);
            const normFx = fx.norm();

            if (normFx < tol) {
                return { solution: x, converged: true, iterations: iter };
            }

            const Jx = J(x);
            const dx = LinearSolver.solve(Jx, fx.scale(-1));

            // Line search for better convergence
            let alpha = 1.0;
            let newX = x.addVector(dx.scale(alpha));
            let newNorm = f(newX).norm();

            // Simple backtracking line search
            let attempts = 0;
            while (newNorm > normFx && attempts < 10) {
                alpha *= 0.5;
                newX = x.addVector(dx.scale(alpha));
                newNorm = f(newX).norm();
                attempts++;
            }

            x = newX;

            // Check for convergence
            if (dx.norm() < tol) {
                return { solution: x, converged: true, iterations: iter + 1 };
            }
        }

        return { solution: x, converged: false, iterations: maxIter };
    }
}

/**
 * Modified Nodal Analysis (MNA) builder
 * Constructs the system of equations for circuit analysis
 */
class MNABuilder {
    constructor() {
        this.nodeCount = 0;
        this.voltageSourceCount = 0;
        this.components = [];
        this.nodeMap = new Map(); // Maps node names to indices
    }

    /**
     * Get or create a node index
     * @param {string} nodeName - Node identifier
     * @returns {number} Node index (0 is always ground)
     */
    getNodeIndex(nodeName) {
        if (nodeName === 'gnd' || nodeName === '0' || nodeName === 0) {
            return 0;
        }

        if (!this.nodeMap.has(nodeName)) {
            this.nodeCount++;
            this.nodeMap.set(nodeName, this.nodeCount);
        }
        return this.nodeMap.get(nodeName);
    }

    /**
     * Add a voltage source to the MNA system
     * Returns the index of the current variable for this source
     */
    addVoltageSource() {
        const idx = this.voltageSourceCount;
        this.voltageSourceCount++;
        return idx;
    }

    /**
     * Build the MNA matrix system
     * @param {Array} components - List of circuit components
     * @param {number} time - Current simulation time
     * @param {Vector} prevSolution - Previous solution for transient
     * @param {number} dt - Time step
     * @returns {Object} { A, b, size }
     */
    buildSystem(components, time = 0, prevSolution = null, dt = 1e-6) {
        // Reset counters
        this.nodeCount = 0;
        this.voltageSourceCount = 0;
        this.nodeMap = new Map();
        this.nodeMap.set('gnd', 0);
        this.nodeMap.set('0', 0);
        this.nodeMap.set(0, 0);

        // First pass: collect all nodes
        for (const comp of components) {
            for (const node of comp.getNodes()) {
                this.getNodeIndex(node);
            }
        }

        // Count voltage sources
        for (const comp of components) {
            if (comp.needsVoltageVariable) {
                comp.voltageVarIndex = this.addVoltageSource();
            }
        }

        // Matrix size: (nodeCount) + (voltage source count)
        // Node 0 (ground) is not included in equations
        const n = this.nodeCount + this.voltageSourceCount;

        if (n === 0) {
            return { A: null, b: null, size: 0 };
        }

        const A = new Matrix(n, n);
        const b = new Vector(n);

        // Second pass: stamp components
        for (const comp of components) {
            comp.stamp(A, b, this, time, prevSolution, dt);
        }

        return { A, b, size: n, nodeMap: this.nodeMap };
    }
}

// Export
window.LinearSolver = LinearSolver;
window.MNABuilder = MNABuilder;
