/**
 * Passive components: Resistor, Capacitor, Inductor
 */

/**
 * Resistor
 */
class Resistor extends Component {
    constructor(x, y) {
        super('resistor', x, y);

        this.width = 60;
        this.height = 20;
        this.label = 'R1';

        this.terminals = [
            { x: -35, y: 0, name: '1' },
            { x: 35, y: 0, name: '2' }
        ];

        this.properties = {
            resistance: 1000 // Ohms
        };
    }

    static getDisplayName() {
        return 'Resistor';
    }

    static getDefaultProperties() {
        return { resistance: 1000 };
    }

    static getPropertyDefinitions() {
        return [
            { name: 'resistance', label: 'Resistance', type: 'number', unit: 'Ohm', min: 0.001, max: 1e12 }
        ];
    }

    stamp(A, b, mna, time, prevSolution, dt) {
        const n1 = mna.getNodeIndex(this.nodes[0]);
        const n2 = mna.getNodeIndex(this.nodes[1]);

        const G = 1 / this.properties.resistance; // Conductance

        // Resistor stamp:
        // G  -G   V1     I1
        // -G  G   V2  =  I2

        if (n1 > 0) {
            A.add(n1 - 1, n1 - 1, G);
        }
        if (n2 > 0) {
            A.add(n2 - 1, n2 - 1, G);
        }
        if (n1 > 0 && n2 > 0) {
            A.add(n1 - 1, n2 - 1, -G);
            A.add(n2 - 1, n1 - 1, -G);
        }
    }

    drawShape(ctx) {
        // Draw zigzag resistor symbol
        ctx.beginPath();

        // Left lead
        ctx.moveTo(-35, 0);
        ctx.lineTo(-25, 0);

        // Zigzag
        const zigWidth = 50;
        const zigHeight = 8;
        const numZigs = 6;
        const segWidth = zigWidth / numZigs;

        ctx.lineTo(-25 + segWidth / 2, -zigHeight);

        for (let i = 1; i < numZigs; i++) {
            const x = -25 + segWidth / 2 + i * segWidth;
            const y = (i % 2 === 0) ? -zigHeight : zigHeight;
            ctx.lineTo(x, y);
        }

        ctx.lineTo(25, 0);

        // Right lead
        ctx.lineTo(35, 0);

        ctx.stroke();
    }

    getValueString() {
        return this.formatEngineering(this.properties.resistance, 'Ohm');
    }
}

/**
 * Capacitor
 */
class Capacitor extends Component {
    constructor(x, y) {
        super('capacitor', x, y);

        this.width = 40;
        this.height = 30;
        this.label = 'C1';

        this.terminals = [
            { x: -25, y: 0, name: '1' },
            { x: 25, y: 0, name: '2' }
        ];

        this.properties = {
            capacitance: 1e-6 // Farads (1uF default)
        };

        // State for transient analysis
        this.voltage = 0;
        this.current = 0;
    }

    static getDisplayName() {
        return 'Capacitor';
    }

    static getDefaultProperties() {
        return { capacitance: 1e-6 };
    }

    static getPropertyDefinitions() {
        return [
            { name: 'capacitance', label: 'Capacitance', type: 'number', unit: 'F', min: 1e-15, max: 1 }
        ];
    }

    stamp(A, b, mna, time, prevSolution, dt) {
        const n1 = mna.getNodeIndex(this.nodes[0]);
        const n2 = mna.getNodeIndex(this.nodes[1]);

        const C = this.properties.capacitance;

        // Trapezoidal integration (companion model):
        // I = C * dV/dt
        // Using backward Euler: I = C * (V - Vprev) / dt
        // This gives: G_eq = C / dt, I_eq = C * Vprev / dt

        const Geq = C / dt;
        let Ieq = 0;

        if (prevSolution) {
            const v1Prev = n1 > 0 ? prevSolution.get(n1 - 1) : 0;
            const v2Prev = n2 > 0 ? prevSolution.get(n2 - 1) : 0;
            const Vprev = v1Prev - v2Prev;
            Ieq = C * Vprev / dt;
        }

        // Stamp equivalent conductance
        if (n1 > 0) {
            A.add(n1 - 1, n1 - 1, Geq);
        }
        if (n2 > 0) {
            A.add(n2 - 1, n2 - 1, Geq);
        }
        if (n1 > 0 && n2 > 0) {
            A.add(n1 - 1, n2 - 1, -Geq);
            A.add(n2 - 1, n1 - 1, -Geq);
        }

        // Stamp equivalent current source
        if (n1 > 0) {
            b.add(n1 - 1, -Ieq);
        }
        if (n2 > 0) {
            b.add(n2 - 1, Ieq);
        }
    }

    drawShape(ctx) {
        // Draw capacitor symbol (two parallel lines)
        ctx.beginPath();

        // Left lead
        ctx.moveTo(-25, 0);
        ctx.lineTo(-5, 0);

        // Left plate
        ctx.moveTo(-5, -12);
        ctx.lineTo(-5, 12);

        // Right plate
        ctx.moveTo(5, -12);
        ctx.lineTo(5, 12);

        // Right lead
        ctx.moveTo(5, 0);
        ctx.lineTo(25, 0);

        ctx.stroke();
    }

    getValueString() {
        return this.formatEngineering(this.properties.capacitance, 'F');
    }
}

/**
 * Inductor
 */
class Inductor extends Component {
    constructor(x, y) {
        super('inductor', x, y);

        this.width = 60;
        this.height = 20;
        this.label = 'L1';

        this.terminals = [
            { x: -35, y: 0, name: '1' },
            { x: 35, y: 0, name: '2' }
        ];

        this.needsVoltageVariable = true;

        this.properties = {
            inductance: 1e-3 // Henrys (1mH default)
        };

        // State for transient analysis
        this.current = 0;
    }

    static getDisplayName() {
        return 'Inductor';
    }

    static getDefaultProperties() {
        return { inductance: 1e-3 };
    }

    static getPropertyDefinitions() {
        return [
            { name: 'inductance', label: 'Inductance', type: 'number', unit: 'H', min: 1e-12, max: 1000 }
        ];
    }

    stamp(A, b, mna, time, prevSolution, dt) {
        const n1 = mna.getNodeIndex(this.nodes[0]);
        const n2 = mna.getNodeIndex(this.nodes[1]);
        const currIdx = mna.nodeCount + this.voltageVarIndex;

        const L = this.properties.inductance;

        // Inductor companion model (backward Euler):
        // V = L * dI/dt
        // V = L * (I - Iprev) / dt
        // Req = L / dt, Veq = L * Iprev / dt

        const Req = L / dt;
        let Veq = 0;

        if (prevSolution && currIdx < prevSolution.size) {
            const Iprev = prevSolution.get(currIdx);
            Veq = L * Iprev / dt;
        }

        // Stamp as a voltage source with equivalent resistance
        // V1 - V2 = Req * I + Veq

        if (n1 > 0) {
            A.add(currIdx, n1 - 1, 1);
            A.add(n1 - 1, currIdx, 1);
        }
        if (n2 > 0) {
            A.add(currIdx, n2 - 1, -1);
            A.add(n2 - 1, currIdx, -1);
        }

        // V1 - V2 - Req * I = Veq
        A.add(currIdx, currIdx, -Req);
        b.add(currIdx, Veq);
    }

    drawShape(ctx) {
        // Draw inductor symbol (coils)
        ctx.beginPath();

        // Left lead
        ctx.moveTo(-35, 0);
        ctx.lineTo(-25, 0);

        // Draw coils
        const numCoils = 4;
        const coilWidth = 50 / numCoils;
        const coilRadius = coilWidth / 2;

        for (let i = 0; i < numCoils; i++) {
            const cx = -25 + coilRadius + i * coilWidth;
            ctx.arc(cx, 0, coilRadius, Math.PI, 0, false);
        }

        // Right lead
        ctx.lineTo(35, 0);

        ctx.stroke();
    }

    getValueString() {
        return this.formatEngineering(this.properties.inductance, 'H');
    }
}

// Register components
ComponentRegistry.register('resistor', Resistor);
ComponentRegistry.register('capacitor', Capacitor);
ComponentRegistry.register('inductor', Inductor);

// Export
window.Resistor = Resistor;
window.Capacitor = Capacitor;
window.Inductor = Inductor;
