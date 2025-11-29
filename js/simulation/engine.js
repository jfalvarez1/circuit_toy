/**
 * Simulation Engine
 * Manages circuit simulation using Modified Nodal Analysis
 */

class SimulationEngine {
    constructor(circuit) {
        this.circuit = circuit;
        this.mnaBuilder = new MNABuilder();

        // Simulation state
        this.running = false;
        this.paused = false;
        this.time = 0;
        this.timeStep = 1e-6;      // 1 microsecond default
        this.maxTimeStep = 0.01;   // 10 milliseconds max
        this.minTimeStep = 1e-9;   // 1 nanosecond min
        this.speed = 1;            // Speed multiplier

        // Solution
        this.solution = null;
        this.prevSolution = null;

        // Convergence settings
        this.maxIterations = 50;
        this.tolerance = 1e-9;

        // Animation
        this.animationId = null;
        this.lastFrameTime = 0;

        // Callbacks
        this.onUpdate = null;
        this.onError = null;

        // History for oscilloscope
        this.historyLength = 10000;
        this.history = {
            time: [],
            probes: {}
        };
    }

    /**
     * Set simulation speed
     */
    setSpeed(speed) {
        this.speed = Math.max(0.1, Math.min(100, speed));
    }

    /**
     * Set time step
     */
    setTimeStep(dt) {
        this.timeStep = Math.max(this.minTimeStep, Math.min(this.maxTimeStep, dt));
    }

    /**
     * Reset simulation
     */
    reset() {
        this.stop();
        this.time = 0;
        this.solution = null;
        this.prevSolution = null;
        this.history = {
            time: [],
            probes: {}
        };

        // Reset component states
        for (const comp of this.circuit.getAllComponents()) {
            if (comp.voltage !== undefined) comp.voltage = 0;
            if (comp.current !== undefined) comp.current = 0;
        }

        // Reset node voltages
        for (const node of this.circuit.nodeManager.getAllNodes()) {
            node.voltage = 0;
        }
    }

    /**
     * Run DC analysis (find operating point)
     */
    runDCAnalysis() {
        // Build node map
        this.circuit.buildNodeMap();

        // Get components
        const components = this.circuit.getAllComponents();
        if (components.length === 0) {
            return { success: false, error: 'No components in circuit' };
        }

        // Check for ground
        if (!this.circuit.nodeManager.groundNode) {
            return { success: false, error: 'No ground reference in circuit' };
        }

        // Build MNA system
        const { A, b, size, nodeMap } = this.mnaBuilder.buildSystem(
            components,
            0, // time = 0 for DC
            null, // no previous solution
            1e-3  // use reasonable time step
        );

        if (size === 0) {
            return { success: false, error: 'No nodes to solve' };
        }

        // Solve iteratively for nonlinear components
        let solution = new Vector(size);
        let converged = false;

        for (let iter = 0; iter < this.maxIterations; iter++) {
            // Rebuild system with current estimate
            const system = this.mnaBuilder.buildSystem(
                components,
                0,
                solution,
                1e-3
            );

            try {
                const newSolution = LinearSolver.solve(system.A, system.b);

                // Check convergence
                let maxDiff = 0;
                for (let i = 0; i < size; i++) {
                    maxDiff = Math.max(maxDiff, Math.abs(newSolution.get(i) - solution.get(i)));
                }

                solution = newSolution;

                if (maxDiff < this.tolerance) {
                    converged = true;
                    break;
                }
            } catch (e) {
                return { success: false, error: `Solver error: ${e.message}` };
            }
        }

        this.solution = solution;
        this.prevSolution = solution.clone();

        // Update node voltages
        this.circuit.updateNodeVoltages(solution);

        return {
            success: true,
            converged,
            solution: solution.toArray()
        };
    }

    /**
     * Run single simulation step
     */
    step() {
        // Build node map if needed
        if (!this.circuit.nodeIndexMap) {
            this.circuit.buildNodeMap();
        }

        const components = this.circuit.getAllComponents();
        if (components.length === 0) return false;

        if (!this.circuit.nodeManager.groundNode) {
            if (this.onError) {
                this.onError('No ground reference in circuit');
            }
            return false;
        }

        // Build and solve system
        const system = this.mnaBuilder.buildSystem(
            components,
            this.time,
            this.prevSolution,
            this.timeStep
        );

        if (system.size === 0) return false;

        try {
            // Initialize solution if needed
            if (!this.solution || this.solution.size !== system.size) {
                this.solution = new Vector(system.size);
            }

            // Iterative solve for nonlinear components
            for (let iter = 0; iter < this.maxIterations; iter++) {
                const newSystem = this.mnaBuilder.buildSystem(
                    components,
                    this.time,
                    this.solution,
                    this.timeStep
                );

                const newSolution = LinearSolver.solve(newSystem.A, newSystem.b);

                // Check convergence
                let maxDiff = 0;
                for (let i = 0; i < system.size; i++) {
                    maxDiff = Math.max(maxDiff, Math.abs(newSolution.get(i) - this.solution.get(i)));
                }

                this.solution = newSolution;

                if (maxDiff < this.tolerance) {
                    break;
                }
            }

            // Update for next step
            this.prevSolution = this.solution.clone();
            this.time += this.timeStep;

            // Update circuit
            this.circuit.updateNodeVoltages(this.solution);

            // Record history for probes
            this.recordHistory();

            return true;

        } catch (e) {
            if (this.onError) {
                this.onError(`Simulation error: ${e.message}`);
            }
            return false;
        }
    }

    /**
     * Record voltage history for oscilloscope
     */
    recordHistory() {
        // Limit history size
        if (this.history.time.length >= this.historyLength) {
            this.history.time.shift();
            for (const probeId in this.history.probes) {
                this.history.probes[probeId].shift();
            }
        }

        this.history.time.push(this.time);

        // Record all node voltages as potential probe points
        for (const node of this.circuit.nodeManager.getAllNodes()) {
            const probeId = `node_${node.id}`;
            if (!this.history.probes[probeId]) {
                this.history.probes[probeId] = [];
            }
            this.history.probes[probeId].push(node.voltage);
        }
    }

    /**
     * Get voltage history for a probe
     */
    getProbeHistory(probeId) {
        return {
            time: this.history.time,
            values: this.history.probes[probeId] || []
        };
    }

    /**
     * Start continuous simulation
     */
    start() {
        if (this.running && !this.paused) return;

        // Run initial DC analysis if no solution
        if (!this.solution) {
            const result = this.runDCAnalysis();
            if (!result.success) {
                if (this.onError) {
                    this.onError(result.error);
                }
                return;
            }
        }

        this.running = true;
        this.paused = false;
        this.lastFrameTime = performance.now();

        const animate = (timestamp) => {
            if (!this.running || this.paused) return;

            const elapsed = timestamp - this.lastFrameTime;
            this.lastFrameTime = timestamp;

            // Calculate how many steps to take based on speed
            const realTimeStep = elapsed / 1000; // Convert to seconds
            const simTimeTarget = realTimeStep * this.speed;
            const stepsNeeded = Math.ceil(simTimeTarget / this.timeStep);

            // Run simulation steps (limit to prevent freeze)
            const maxSteps = 1000;
            const steps = Math.min(stepsNeeded, maxSteps);

            for (let i = 0; i < steps; i++) {
                if (!this.step()) {
                    this.stop();
                    return;
                }
            }

            // Callback for UI update
            if (this.onUpdate) {
                this.onUpdate(this.time, this.solution);
            }

            this.animationId = requestAnimationFrame(animate);
        };

        this.animationId = requestAnimationFrame(animate);
    }

    /**
     * Pause simulation
     */
    pause() {
        this.paused = true;
        if (this.animationId) {
            cancelAnimationFrame(this.animationId);
            this.animationId = null;
        }
    }

    /**
     * Resume simulation
     */
    resume() {
        if (this.running && this.paused) {
            this.paused = false;
            this.lastFrameTime = performance.now();
            this.start();
        }
    }

    /**
     * Stop simulation
     */
    stop() {
        this.running = false;
        this.paused = false;
        if (this.animationId) {
            cancelAnimationFrame(this.animationId);
            this.animationId = null;
        }
    }

    /**
     * Get current at a node (sum of currents)
     */
    getNodeCurrent(nodeId) {
        // This would require tracking branch currents
        // For now, return 0
        return 0;
    }

    /**
     * Get voltage at a node
     */
    getNodeVoltage(nodeId) {
        const node = this.circuit.nodeManager.getNode(nodeId);
        return node ? node.voltage : 0;
    }

    /**
     * Get voltage between two nodes
     */
    getVoltageDifference(nodeId1, nodeId2) {
        return this.getNodeVoltage(nodeId1) - this.getNodeVoltage(nodeId2);
    }

    /**
     * Check if simulation is running
     */
    isRunning() {
        return this.running && !this.paused;
    }

    /**
     * Check if simulation is paused
     */
    isPaused() {
        return this.running && this.paused;
    }
}

// Export
window.SimulationEngine = SimulationEngine;
