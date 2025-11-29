/**
 * Ideal Operational Amplifier
 */

class OpAmp extends Component {
    constructor(x, y) {
        super('opamp', x, y);

        this.width = 60;
        this.height = 50;
        this.label = 'U1';

        // Terminals: inverting input (-), non-inverting input (+), output
        // Power supply terminals optional for ideal model
        this.terminals = [
            { x: -35, y: -12, name: 'IN-' },  // Inverting input
            { x: -35, y: 12, name: 'IN+' },   // Non-inverting input
            { x: 35, y: 0, name: 'OUT' }      // Output
        ];

        this.needsVoltageVariable = true;

        this.properties = {
            gain: 100000,      // Open-loop gain (ideal: infinity, practical: ~100k)
            vOffset: 0,        // Input offset voltage
            outputMax: 15,     // Positive rail voltage
            outputMin: -15     // Negative rail voltage
        };
    }

    static getDisplayName() {
        return 'Op-Amp';
    }

    static getDefaultProperties() {
        return { gain: 100000, vOffset: 0, outputMax: 15, outputMin: -15 };
    }

    static getPropertyDefinitions() {
        return [
            { name: 'gain', label: 'Open-loop Gain', type: 'number', unit: '', min: 1, max: 1e9 },
            { name: 'vOffset', label: 'Input Offset', type: 'number', unit: 'mV', min: -100, max: 100 },
            { name: 'outputMax', label: 'V+ Rail', type: 'number', unit: 'V', min: 0, max: 100 },
            { name: 'outputMin', label: 'V- Rail', type: 'number', unit: 'V', min: -100, max: 0 }
        ];
    }

    stamp(A, b, mna, time, prevSolution, dt) {
        const nInv = mna.getNodeIndex(this.nodes[0]);   // Inverting input (-)
        const nNonInv = mna.getNodeIndex(this.nodes[1]); // Non-inverting input (+)
        const nOut = mna.getNodeIndex(this.nodes[2]);   // Output
        const currIdx = mna.nodeCount + this.voltageVarIndex;

        const { gain, vOffset, outputMax, outputMin } = this.properties;

        // Get current output voltage to check for saturation
        let Vout = 0;
        if (prevSolution && nOut > 0) {
            Vout = prevSolution.get(nOut - 1);
        }

        // Check saturation
        const saturatedHigh = Vout >= outputMax;
        const saturatedLow = Vout <= outputMin;

        if (saturatedHigh || saturatedLow) {
            // In saturation: output is a fixed voltage source
            const Vsat = saturatedHigh ? outputMax : outputMin;

            // Voltage source at output
            if (nOut > 0) {
                A.add(currIdx, nOut - 1, 1);
                A.add(nOut - 1, currIdx, 1);
            }
            b.add(currIdx, Vsat);

            // Still need high input impedance (infinite for ideal)
            // No current into input terminals - handled implicitly
        } else {
            // Normal operation: Vout = A * (V+ - V-)
            // For MNA: We model as a voltage source where
            // Vout = A * (Vninv - Vinv) + Voffset
            // Or rearranged: Vout - A*Vninv + A*Vinv = Voffset

            // Use large but finite gain to avoid numerical issues
            const A_gain = Math.min(gain, 1e9);

            // Model as VCVS (Voltage Controlled Voltage Source)
            // Output voltage equation:
            // Vout = A * (V+ - V-) + Voffset
            // In matrix form: Vout - A*V+ + A*V- = Voffset

            if (nOut > 0) {
                A.add(currIdx, nOut - 1, 1);
                A.add(nOut - 1, currIdx, 1);
            }

            if (nNonInv > 0) {
                A.add(currIdx, nNonInv - 1, -A_gain);
            }

            if (nInv > 0) {
                A.add(currIdx, nInv - 1, A_gain);
            }

            b.add(currIdx, vOffset / 1000); // Convert mV to V

            // Note: Ideal op-amp has infinite input impedance
            // This is modeled by not adding any conductance to input nodes
        }
    }

    drawShape(ctx) {
        ctx.beginPath();

        // Triangle
        ctx.moveTo(-25, -25);
        ctx.lineTo(-25, 25);
        ctx.lineTo(25, 0);
        ctx.closePath();
        ctx.stroke();

        // Input leads
        ctx.beginPath();
        ctx.moveTo(-35, -12);
        ctx.lineTo(-25, -12);
        ctx.moveTo(-35, 12);
        ctx.lineTo(-25, 12);
        ctx.stroke();

        // Output lead
        ctx.beginPath();
        ctx.moveTo(25, 0);
        ctx.lineTo(35, 0);
        ctx.stroke();

        // Draw + and - symbols
        ctx.fillStyle = ctx.strokeStyle;
        ctx.font = 'bold 10px sans-serif';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';

        // Minus at inverting input
        ctx.fillText('-', -18, -12);

        // Plus at non-inverting input
        ctx.fillText('+', -18, 12);
    }

    getValueString() {
        return `Gain: ${this.formatEngineering(this.properties.gain, '')}`;
    }
}

// Register component
ComponentRegistry.register('opamp', OpAmp);

// Export
window.OpAmp = OpAmp;
