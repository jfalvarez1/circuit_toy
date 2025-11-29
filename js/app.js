/**
 * Circuit Playground Simulator - Main Application
 */

class CircuitPlaygroundApp {
    constructor() {
        // Create circuit
        this.circuit = new Circuit();

        // Create canvas manager
        this.canvasManager = new CanvasManager('circuit-canvas');
        this.canvasManager.setCircuit(this.circuit);
        this.canvasManager.setApp(this);

        // Create simulation engine
        this.simulationEngine = new SimulationEngine(this.circuit);
        this.simulationEngine.onUpdate = (time, solution) => this.onSimulationUpdate(time, solution);
        this.simulationEngine.onError = (error) => this.onSimulationError(error);

        // Create UI managers
        this.palette = new ComponentPalette(this.canvasManager);
        this.propertiesPanel = new PropertiesPanel();
        this.propertiesPanel.onPropertyChange = (comp) => this.onPropertyChange(comp);
        this.measurementsPanel = new MeasurementsPanel();
        this.oscilloscope = new Oscilloscope('oscilloscope-canvas');
        this.oscilloscope.setSimulationEngine(this.simulationEngine);

        // Display options
        this.showVoltages = false;
        this.showCurrent = false;

        // Setup controls
        this.setupControls();

        // Center view
        this.canvasManager.centerView();

        // Select default tool
        this.palette.selectDefaultTool();

        // Initial status
        this.updateStatus('Ready - Select a component or tool to begin');
        this.updateCounts();
    }

    /**
     * Setup toolbar and control event listeners
     */
    setupControls() {
        // Run button
        document.getElementById('btn-run')?.addEventListener('click', () => {
            this.runSimulation();
        });

        // Pause button
        document.getElementById('btn-pause')?.addEventListener('click', () => {
            this.pauseSimulation();
        });

        // Step button
        document.getElementById('btn-step')?.addEventListener('click', () => {
            this.stepSimulation();
        });

        // Reset button
        document.getElementById('btn-reset')?.addEventListener('click', () => {
            this.resetSimulation();
        });

        // Clear button
        document.getElementById('btn-clear')?.addEventListener('click', () => {
            this.clearCircuit();
        });

        // Save button
        document.getElementById('btn-save')?.addEventListener('click', () => {
            this.saveCircuit();
        });

        // Load button
        document.getElementById('btn-load')?.addEventListener('click', () => {
            this.loadCircuit();
        });

        // Speed slider
        const speedSlider = document.getElementById('sim-speed');
        const speedValue = document.getElementById('speed-value');
        speedSlider?.addEventListener('input', (e) => {
            const speed = parseFloat(e.target.value);
            this.simulationEngine.setSpeed(speed);
            speedValue.textContent = `${speed}x`;
        });
    }

    /**
     * Run simulation
     */
    runSimulation() {
        // First run DC analysis
        const result = this.simulationEngine.runDCAnalysis();

        if (!result.success) {
            this.updateStatus(`Error: ${result.error}`);
            return;
        }

        // Start transient simulation
        this.simulationEngine.start();

        document.getElementById('btn-run').disabled = true;
        document.getElementById('btn-pause').disabled = false;
        document.body.classList.add('running');

        this.updateStatus('Simulation running');
        this.canvasManager.render();
    }

    /**
     * Pause simulation
     */
    pauseSimulation() {
        this.simulationEngine.pause();

        document.getElementById('btn-run').disabled = false;
        document.getElementById('btn-pause').disabled = true;
        document.body.classList.remove('running');

        this.updateStatus('Simulation paused');
    }

    /**
     * Step simulation
     */
    stepSimulation() {
        // Run DC analysis if no solution yet
        if (!this.simulationEngine.solution) {
            const result = this.simulationEngine.runDCAnalysis();
            if (!result.success) {
                this.updateStatus(`Error: ${result.error}`);
                return;
            }
        }

        // Run single step
        this.simulationEngine.step();

        // Update display
        this.onSimulationUpdate(
            this.simulationEngine.time,
            this.simulationEngine.solution
        );
    }

    /**
     * Reset simulation
     */
    resetSimulation() {
        this.simulationEngine.reset();

        document.getElementById('btn-run').disabled = false;
        document.getElementById('btn-pause').disabled = true;
        document.body.classList.remove('running');

        this.oscilloscope.clear();
        this.updateStatus('Simulation reset');
        this.updateSimTime(0);
        this.canvasManager.render();
    }

    /**
     * Clear the circuit
     */
    clearCircuit() {
        if (!confirm('Clear the entire circuit?')) return;

        this.resetSimulation();
        this.circuit.clear();
        this.canvasManager.probes = [];
        this.propertiesPanel.clear();
        this.measurementsPanel.updateProbes([]);

        this.updateStatus('Circuit cleared');
        this.updateCounts();
        this.canvasManager.render();
    }

    /**
     * Save circuit to local storage / file
     */
    saveCircuit() {
        try {
            const data = this.circuit.export();

            // Create download
            const blob = new Blob([data], { type: 'application/json' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = 'circuit.json';
            a.click();
            URL.revokeObjectURL(url);

            this.updateStatus('Circuit saved');
        } catch (e) {
            this.updateStatus(`Save error: ${e.message}`);
        }
    }

    /**
     * Load circuit from file
     */
    loadCircuit() {
        const input = document.createElement('input');
        input.type = 'file';
        input.accept = '.json';

        input.addEventListener('change', (e) => {
            const file = e.target.files[0];
            if (!file) return;

            const reader = new FileReader();
            reader.onload = (e) => {
                try {
                    const newCircuit = Circuit.import(e.target.result);
                    if (newCircuit) {
                        this.circuit = newCircuit;
                        this.canvasManager.setCircuit(this.circuit);
                        this.simulationEngine.circuit = this.circuit;
                        this.resetSimulation();

                        this.updateStatus('Circuit loaded');
                        this.updateCounts();
                        this.canvasManager.render();
                    } else {
                        this.updateStatus('Failed to load circuit');
                    }
                } catch (err) {
                    this.updateStatus(`Load error: ${err.message}`);
                }
            };
            reader.readAsText(file);
        });

        input.click();
    }

    /**
     * Simulation update callback
     */
    onSimulationUpdate(time, solution) {
        // Update time display
        this.updateSimTime(time);

        // Update measurements
        const probeReadings = this.canvasManager.getProbeReadings();
        this.measurementsPanel.updateFromCircuit(this.circuit, probeReadings);

        // Update oscilloscope
        this.oscilloscope.update(probeReadings);

        // Render canvas
        this.canvasManager.render();
    }

    /**
     * Simulation error callback
     */
    onSimulationError(error) {
        this.updateStatus(`Error: ${error}`);
        this.pauseSimulation();
    }

    /**
     * Component selected callback
     */
    onComponentSelected(component) {
        this.propertiesPanel.showComponent(component);
    }

    /**
     * Component deselected callback
     */
    onComponentDeselected() {
        this.propertiesPanel.clear();
    }

    /**
     * Property changed callback
     */
    onPropertyChange(component) {
        this.circuit.updateComponentNodes(component);
        this.canvasManager.render();
    }

    /**
     * Update probe readings
     */
    updateProbes(probes) {
        const readings = probes.map(probe => {
            const node = this.circuit.nodeManager.getNode(probe.nodeId);
            return {
                ...probe,
                voltage: node ? node.voltage : 0
            };
        });
        this.measurementsPanel.updateProbes(readings);
    }

    /**
     * Update status message
     */
    updateStatus(message) {
        const statusEl = document.getElementById('status-message');
        if (statusEl) {
            statusEl.textContent = message;
        }
    }

    /**
     * Update simulation time display
     */
    updateSimTime(time) {
        const timeEl = document.getElementById('sim-time');
        if (timeEl) {
            let display;
            if (time < 0.001) {
                display = `${(time * 1e6).toFixed(1)} us`;
            } else if (time < 1) {
                display = `${(time * 1000).toFixed(2)} ms`;
            } else {
                display = `${time.toFixed(3)} s`;
            }
            timeEl.textContent = `Time: ${display}`;
        }
    }

    /**
     * Update component and node counts
     */
    updateCounts() {
        const nodeEl = document.getElementById('node-count');
        const compEl = document.getElementById('component-count');

        if (nodeEl) {
            nodeEl.textContent = `Nodes: ${this.circuit.getNodeCount()}`;
        }
        if (compEl) {
            compEl.textContent = `Components: ${this.circuit.getComponentCount()}`;
        }
    }
}

// Initialize app when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    window.app = new CircuitPlaygroundApp();
});
