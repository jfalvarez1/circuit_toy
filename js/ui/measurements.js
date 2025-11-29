/**
 * Measurements Panel - displays voltmeter, ammeter, and probe readings
 */

class MeasurementsPanel {
    constructor() {
        this.voltmeterEl = document.getElementById('voltmeter-value');
        this.ammeterEl = document.getElementById('ammeter-value');
        this.probeListEl = document.getElementById('probe-list');

        // Measurement points
        this.voltmeterNodes = { positive: null, negative: null };
        this.ammeterComponent = null;

        // Current readings
        this.voltmeterValue = 0;
        this.ammeterValue = 0;
        this.probeReadings = [];
    }

    /**
     * Update voltmeter display
     */
    updateVoltmeter(voltage) {
        this.voltmeterValue = voltage;
        if (this.voltmeterEl) {
            this.voltmeterEl.textContent = this.formatValue(voltage, 'V');
        }
    }

    /**
     * Update ammeter display
     */
    updateAmmeter(current) {
        this.ammeterValue = current;
        if (this.ammeterEl) {
            this.ammeterEl.textContent = this.formatValue(current, 'A');
        }
    }

    /**
     * Update probe readings display
     */
    updateProbes(probes) {
        this.probeReadings = probes;

        if (!this.probeListEl) return;

        if (probes.length === 0) {
            this.probeListEl.innerHTML = '<p class="placeholder" style="font-size: 0.8rem;">No probes placed</p>';
            return;
        }

        let html = '';
        probes.forEach((probe, index) => {
            html += `
                <div class="probe-reading">
                    <span class="probe-name" style="color: ${probe.color}">P${index + 1}</span>
                    <span class="probe-value">${this.formatValue(probe.voltage, 'V')}</span>
                </div>
            `;
        });

        this.probeListEl.innerHTML = html;
    }

    /**
     * Format a value with appropriate units
     */
    formatValue(value, unit) {
        const absVal = Math.abs(value);

        if (absVal === 0) {
            return `0.000 ${unit}`;
        }

        const prefixes = [
            { exp: 9, prefix: 'G' },
            { exp: 6, prefix: 'M' },
            { exp: 3, prefix: 'k' },
            { exp: 0, prefix: '' },
            { exp: -3, prefix: 'm' },
            { exp: -6, prefix: 'u' },
            { exp: -9, prefix: 'n' },
            { exp: -12, prefix: 'p' }
        ];

        for (const { exp, prefix } of prefixes) {
            if (absVal >= Math.pow(10, exp - 3)) {
                const scaled = value / Math.pow(10, exp);
                return `${scaled.toFixed(3)} ${prefix}${unit}`;
            }
        }

        return `${value.toExponential(3)} ${unit}`;
    }

    /**
     * Set voltmeter measurement points
     */
    setVoltmeterNodes(positiveNodeId, negativeNodeId) {
        this.voltmeterNodes.positive = positiveNodeId;
        this.voltmeterNodes.negative = negativeNodeId;
    }

    /**
     * Set ammeter measurement component
     */
    setAmmeterComponent(componentId) {
        this.ammeterComponent = componentId;
    }

    /**
     * Calculate voltmeter reading from circuit
     */
    calculateVoltmeter(circuit) {
        if (!this.voltmeterNodes.positive || !this.voltmeterNodes.negative) {
            return 0;
        }

        const nodePos = circuit.nodeManager.getNode(this.voltmeterNodes.positive);
        const nodeNeg = circuit.nodeManager.getNode(this.voltmeterNodes.negative);

        if (!nodePos || !nodeNeg) {
            return 0;
        }

        return nodePos.voltage - nodeNeg.voltage;
    }

    /**
     * Update all measurements from circuit
     */
    updateFromCircuit(circuit, probes) {
        // Update voltmeter
        const voltage = this.calculateVoltmeter(circuit);
        this.updateVoltmeter(voltage);

        // Update ammeter (would need current tracking)
        // For now, keep at 0 or implement later
        this.updateAmmeter(0);

        // Update probes
        if (probes) {
            const probeReadings = probes.map(probe => {
                const node = circuit.nodeManager.getNode(probe.nodeId);
                return {
                    ...probe,
                    voltage: node ? node.voltage : 0
                };
            });
            this.updateProbes(probeReadings);
        }
    }
}

// Export
window.MeasurementsPanel = MeasurementsPanel;
