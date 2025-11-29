/**
 * Voltage and Current Source components
 */

/**
 * DC Voltage Source
 */
class DCVoltageSource extends Component {
    constructor(x, y) {
        super('dc_voltage', x, y);

        this.width = 40;
        this.height = 40;
        this.label = 'V1';

        // Terminals: positive at top, negative at bottom
        this.terminals = [
            { x: 0, y: -25, name: '+' },
            { x: 0, y: 25, name: '-' }
        ];

        this.needsVoltageVariable = true;

        this.properties = {
            voltage: 5 // Volts
        };
    }

    static getDisplayName() {
        return 'DC Voltage Source';
    }

    static getDefaultProperties() {
        return { voltage: 5 };
    }

    static getPropertyDefinitions() {
        return [
            { name: 'voltage', label: 'Voltage', type: 'number', unit: 'V', min: -1000, max: 1000 }
        ];
    }

    stamp(A, b, mna, time, prevSolution, dt) {
        const nPlus = mna.getNodeIndex(this.nodes[0]);
        const nMinus = mna.getNodeIndex(this.nodes[1]);
        const voltIdx = mna.nodeCount + this.voltageVarIndex;

        const V = this.properties.voltage;

        // Voltage source equations:
        // V+ - V- = V
        // Adds current variable to the system

        // Row for voltage constraint: V+ - V- = V
        if (nPlus > 0) {
            A.add(voltIdx, nPlus - 1, 1);
        }
        if (nMinus > 0) {
            A.add(voltIdx, nMinus - 1, -1);
        }
        b.add(voltIdx, V);

        // Current contribution to node equations
        if (nPlus > 0) {
            A.add(nPlus - 1, voltIdx, 1);
        }
        if (nMinus > 0) {
            A.add(nMinus - 1, voltIdx, -1);
        }
    }

    drawShape(ctx) {
        // Draw circle
        ctx.beginPath();
        ctx.arc(0, 0, 15, 0, Math.PI * 2);
        ctx.stroke();

        // Draw + and -
        ctx.fillStyle = ctx.strokeStyle;
        ctx.font = 'bold 12px sans-serif';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText('+', 0, -6);
        ctx.fillText('-', 0, 8);

        // Terminal lines
        ctx.beginPath();
        ctx.moveTo(0, -15);
        ctx.lineTo(0, -25);
        ctx.moveTo(0, 15);
        ctx.lineTo(0, 25);
        ctx.stroke();
    }

    getValueString() {
        return this.formatEngineering(this.properties.voltage, 'V');
    }
}

/**
 * AC Voltage Source
 */
class ACVoltageSource extends Component {
    constructor(x, y) {
        super('ac_voltage', x, y);

        this.width = 40;
        this.height = 40;
        this.label = 'V1';

        this.terminals = [
            { x: 0, y: -25, name: '+' },
            { x: 0, y: 25, name: '-' }
        ];

        this.needsVoltageVariable = true;

        this.properties = {
            amplitude: 5,    // Peak voltage
            frequency: 60,   // Hz
            phase: 0,        // Degrees
            offset: 0        // DC offset
        };
    }

    static getDisplayName() {
        return 'AC Voltage Source';
    }

    static getDefaultProperties() {
        return { amplitude: 5, frequency: 60, phase: 0, offset: 0 };
    }

    static getPropertyDefinitions() {
        return [
            { name: 'amplitude', label: 'Amplitude', type: 'number', unit: 'V', min: 0, max: 1000 },
            { name: 'frequency', label: 'Frequency', type: 'number', unit: 'Hz', min: 0, max: 1e9 },
            { name: 'phase', label: 'Phase', type: 'number', unit: 'deg', min: -180, max: 180 },
            { name: 'offset', label: 'DC Offset', type: 'number', unit: 'V', min: -1000, max: 1000 }
        ];
    }

    getVoltageAtTime(time) {
        const { amplitude, frequency, phase, offset } = this.properties;
        const omega = 2 * Math.PI * frequency;
        const phaseRad = phase * Math.PI / 180;
        return amplitude * Math.sin(omega * time + phaseRad) + offset;
    }

    stamp(A, b, mna, time, prevSolution, dt) {
        const nPlus = mna.getNodeIndex(this.nodes[0]);
        const nMinus = mna.getNodeIndex(this.nodes[1]);
        const voltIdx = mna.nodeCount + this.voltageVarIndex;

        const V = this.getVoltageAtTime(time);

        if (nPlus > 0) {
            A.add(voltIdx, nPlus - 1, 1);
        }
        if (nMinus > 0) {
            A.add(voltIdx, nMinus - 1, -1);
        }
        b.add(voltIdx, V);

        if (nPlus > 0) {
            A.add(nPlus - 1, voltIdx, 1);
        }
        if (nMinus > 0) {
            A.add(nMinus - 1, voltIdx, -1);
        }
    }

    drawShape(ctx) {
        // Draw circle
        ctx.beginPath();
        ctx.arc(0, 0, 15, 0, Math.PI * 2);
        ctx.stroke();

        // Draw sine wave
        ctx.beginPath();
        ctx.moveTo(-8, 0);
        ctx.bezierCurveTo(-5, -8, 0, -8, 0, 0);
        ctx.bezierCurveTo(0, 8, 5, 8, 8, 0);
        ctx.stroke();

        // Terminal lines
        ctx.beginPath();
        ctx.moveTo(0, -15);
        ctx.lineTo(0, -25);
        ctx.moveTo(0, 15);
        ctx.lineTo(0, 25);
        ctx.stroke();
    }

    getValueString() {
        const { amplitude, frequency } = this.properties;
        return `${this.formatEngineering(amplitude, 'V')} @ ${this.formatEngineering(frequency, 'Hz')}`;
    }
}

/**
 * DC Current Source
 */
class DCCurrentSource extends Component {
    constructor(x, y) {
        super('dc_current', x, y);

        this.width = 40;
        this.height = 40;
        this.label = 'I1';

        // Current flows from - to + (conventional current)
        this.terminals = [
            { x: 0, y: -25, name: '+' },
            { x: 0, y: 25, name: '-' }
        ];

        this.properties = {
            current: 0.001 // Amps (1mA default)
        };
    }

    static getDisplayName() {
        return 'DC Current Source';
    }

    static getDefaultProperties() {
        return { current: 0.001 };
    }

    static getPropertyDefinitions() {
        return [
            { name: 'current', label: 'Current', type: 'number', unit: 'A', min: -100, max: 100 }
        ];
    }

    stamp(A, b, mna, time, prevSolution, dt) {
        const nPlus = mna.getNodeIndex(this.nodes[0]);
        const nMinus = mna.getNodeIndex(this.nodes[1]);

        const I = this.properties.current;

        // Current source: adds/subtracts current from nodes
        // Current flows into + node, out of - node
        if (nPlus > 0) {
            b.add(nPlus - 1, -I); // Current leaving node (KCL convention)
        }
        if (nMinus > 0) {
            b.add(nMinus - 1, I);  // Current entering node
        }
    }

    drawShape(ctx) {
        // Draw circle
        ctx.beginPath();
        ctx.arc(0, 0, 15, 0, Math.PI * 2);
        ctx.stroke();

        // Draw arrow pointing up (current direction)
        ctx.beginPath();
        ctx.moveTo(0, 8);
        ctx.lineTo(0, -8);
        ctx.moveTo(-4, -4);
        ctx.lineTo(0, -8);
        ctx.lineTo(4, -4);
        ctx.stroke();

        // Terminal lines
        ctx.beginPath();
        ctx.moveTo(0, -15);
        ctx.lineTo(0, -25);
        ctx.moveTo(0, 15);
        ctx.lineTo(0, 25);
        ctx.stroke();
    }

    getValueString() {
        return this.formatEngineering(this.properties.current, 'A');
    }
}

// Register components
ComponentRegistry.register('dc_voltage', DCVoltageSource);
ComponentRegistry.register('ac_voltage', ACVoltageSource);
ComponentRegistry.register('dc_current', DCCurrentSource);

// Export
window.DCVoltageSource = DCVoltageSource;
window.ACVoltageSource = ACVoltageSource;
window.DCCurrentSource = DCCurrentSource;
