/**
 * Semiconductor components: Diode, BJT, MOSFET
 */

/**
 * Ideal Diode (with Shockley equation for more realistic behavior)
 */
class Diode extends Component {
    constructor(x, y) {
        super('diode', x, y);

        this.width = 40;
        this.height = 20;
        this.label = 'D1';

        // Anode (+) to Cathode (-), current flows from anode to cathode
        this.terminals = [
            { x: -25, y: 0, name: 'A' },  // Anode
            { x: 25, y: 0, name: 'K' }    // Cathode
        ];

        this.properties = {
            saturationCurrent: 1e-12,  // Is (A)
            thermalVoltage: 0.026,     // Vt = kT/q at room temp (~26mV)
            idealityFactor: 1          // n
        };
    }

    static getDisplayName() {
        return 'Diode';
    }

    static getDefaultProperties() {
        return { saturationCurrent: 1e-12, thermalVoltage: 0.026, idealityFactor: 1 };
    }

    static getPropertyDefinitions() {
        return [
            { name: 'saturationCurrent', label: 'Is', type: 'number', unit: 'A', min: 1e-18, max: 1e-6 },
            { name: 'thermalVoltage', label: 'Vt', type: 'number', unit: 'V', min: 0.01, max: 0.1 },
            { name: 'idealityFactor', label: 'n', type: 'number', unit: '', min: 1, max: 2 }
        ];
    }

    stamp(A, b, mna, time, prevSolution, dt) {
        const nA = mna.getNodeIndex(this.nodes[0]); // Anode
        const nK = mna.getNodeIndex(this.nodes[1]); // Cathode

        const { saturationCurrent: Is, thermalVoltage: Vt, idealityFactor: n } = this.properties;
        const nVt = n * Vt;

        // Get previous voltage across diode
        let Vd = 0.6; // Initial guess
        if (prevSolution) {
            const vA = nA > 0 ? prevSolution.get(nA - 1) : 0;
            const vK = nK > 0 ? prevSolution.get(nK - 1) : 0;
            Vd = vA - vK;
        }

        // Limit voltage for numerical stability
        Vd = Math.max(-5 * nVt, Math.min(40 * nVt, Vd));

        // Diode current: Id = Is * (exp(Vd / nVt) - 1)
        // Linearized: Id = Id0 + Gd * (Vd - Vd0)
        // where Gd = dId/dVd = Is/nVt * exp(Vd0/nVt)

        const expTerm = Math.exp(Vd / nVt);
        const Id = Is * (expTerm - 1);
        const Gd = (Is / nVt) * expTerm;

        // Equivalent current source: Ieq = Id - Gd * Vd
        const Ieq = Id - Gd * Vd;

        // Stamp conductance
        if (nA > 0) {
            A.add(nA - 1, nA - 1, Gd);
        }
        if (nK > 0) {
            A.add(nK - 1, nK - 1, Gd);
        }
        if (nA > 0 && nK > 0) {
            A.add(nA - 1, nK - 1, -Gd);
            A.add(nK - 1, nA - 1, -Gd);
        }

        // Stamp current source
        if (nA > 0) {
            b.add(nA - 1, -Ieq);
        }
        if (nK > 0) {
            b.add(nK - 1, Ieq);
        }
    }

    drawShape(ctx) {
        ctx.beginPath();

        // Left lead
        ctx.moveTo(-25, 0);
        ctx.lineTo(-8, 0);

        // Triangle (anode side)
        ctx.moveTo(-8, -10);
        ctx.lineTo(-8, 10);
        ctx.lineTo(8, 0);
        ctx.closePath();
        ctx.stroke();

        // Bar (cathode side)
        ctx.beginPath();
        ctx.moveTo(8, -10);
        ctx.lineTo(8, 10);
        ctx.stroke();

        // Right lead
        ctx.beginPath();
        ctx.moveTo(8, 0);
        ctx.lineTo(25, 0);
        ctx.stroke();
    }

    getValueString() {
        return 'Diode';
    }
}

/**
 * NPN BJT Transistor (simplified Ebers-Moll model)
 */
class NPNTransistor extends Component {
    constructor(x, y) {
        super('npn_bjt', x, y);

        this.width = 40;
        this.height = 50;
        this.label = 'Q1';

        // B: Base, C: Collector, E: Emitter
        this.terminals = [
            { x: -25, y: 0, name: 'B' },   // Base
            { x: 15, y: -20, name: 'C' },  // Collector
            { x: 15, y: 20, name: 'E' }    // Emitter
        ];

        this.properties = {
            beta: 100,           // Current gain (hFE)
            saturationCurrent: 1e-14,
            earlyVoltage: 100    // VA for Early effect
        };
    }

    static getDisplayName() {
        return 'NPN BJT';
    }

    static getDefaultProperties() {
        return { beta: 100, saturationCurrent: 1e-14, earlyVoltage: 100 };
    }

    static getPropertyDefinitions() {
        return [
            { name: 'beta', label: 'Beta (hFE)', type: 'number', unit: '', min: 10, max: 1000 },
            { name: 'saturationCurrent', label: 'Is', type: 'number', unit: 'A', min: 1e-18, max: 1e-9 },
            { name: 'earlyVoltage', label: 'VA', type: 'number', unit: 'V', min: 10, max: 500 }
        ];
    }

    stamp(A, b, mna, time, prevSolution, dt) {
        const nB = mna.getNodeIndex(this.nodes[0]); // Base
        const nC = mna.getNodeIndex(this.nodes[1]); // Collector
        const nE = mna.getNodeIndex(this.nodes[2]); // Emitter

        const { beta, saturationCurrent: Is } = this.properties;
        const Vt = 0.026; // Thermal voltage

        // Get operating point voltages
        let Vbe = 0.6, Vbc = 0;
        if (prevSolution) {
            const vB = nB > 0 ? prevSolution.get(nB - 1) : 0;
            const vC = nC > 0 ? prevSolution.get(nC - 1) : 0;
            const vE = nE > 0 ? prevSolution.get(nE - 1) : 0;
            Vbe = vB - vE;
            Vbc = vB - vC;
        }

        // Limit voltages
        Vbe = Math.max(-5 * Vt, Math.min(40 * Vt, Vbe));
        Vbc = Math.max(-5 * Vt, Math.min(40 * Vt, Vbc));

        // Forward and reverse currents
        const expBE = Math.exp(Vbe / Vt);
        const expBC = Math.exp(Vbc / Vt);

        // Base-Emitter diode
        const Ibe = Is * (expBE - 1) / beta;
        const Gbe = (Is / (beta * Vt)) * expBE;

        // Collector current (controlled source)
        const Ic = Is * (expBE - expBC);
        const GcBE = (Is / Vt) * expBE;
        const GcBC = (Is / Vt) * expBC;

        // Stamp B-E diode
        const IeqBE = Ibe - Gbe * Vbe;

        if (nB > 0) A.add(nB - 1, nB - 1, Gbe);
        if (nE > 0) A.add(nE - 1, nE - 1, Gbe);
        if (nB > 0 && nE > 0) {
            A.add(nB - 1, nE - 1, -Gbe);
            A.add(nE - 1, nB - 1, -Gbe);
        }

        if (nB > 0) b.add(nB - 1, -IeqBE);
        if (nE > 0) b.add(nE - 1, IeqBE);

        // Stamp collector current (VCCS: controlled by Vbe)
        const IeqC = Ic - GcBE * Vbe + GcBC * Vbc;

        // Collector current source controlled by Vbe
        if (nC > 0) A.add(nC - 1, nC - 1, GcBC);
        if (nE > 0) A.add(nE - 1, nE - 1, GcBE);

        if (nC > 0 && nB > 0) A.add(nC - 1, nB - 1, GcBE - GcBC);
        if (nC > 0 && nE > 0) A.add(nC - 1, nE - 1, -GcBE);
        if (nE > 0 && nB > 0) A.add(nE - 1, nB - 1, -GcBE);
        if (nE > 0 && nC > 0) A.add(nE - 1, nC - 1, GcBC);

        // Stamp equivalent currents
        if (nC > 0) b.add(nC - 1, -IeqC);
        if (nE > 0) b.add(nE - 1, IeqC);
    }

    drawShape(ctx) {
        // Draw NPN transistor symbol
        ctx.beginPath();

        // Base lead
        ctx.moveTo(-25, 0);
        ctx.lineTo(-5, 0);

        // Vertical bar
        ctx.moveTo(-5, -12);
        ctx.lineTo(-5, 12);

        // Collector line (with arrow)
        ctx.moveTo(-5, -6);
        ctx.lineTo(12, -16);
        ctx.lineTo(15, -20);

        // Emitter line (with arrow pointing out for NPN)
        ctx.moveTo(-5, 6);
        ctx.lineTo(12, 16);
        ctx.lineTo(15, 20);

        ctx.stroke();

        // Arrow on emitter
        ctx.beginPath();
        ctx.moveTo(8, 12);
        ctx.lineTo(12, 16);
        ctx.lineTo(6, 16);
        ctx.closePath();
        ctx.fill();

        // Circle outline
        ctx.beginPath();
        ctx.arc(0, 0, 20, 0, Math.PI * 2);
        ctx.stroke();
    }

    getValueString() {
        return `NPN beta=${this.properties.beta}`;
    }
}

/**
 * PNP BJT Transistor
 */
class PNPTransistor extends Component {
    constructor(x, y) {
        super('pnp_bjt', x, y);

        this.width = 40;
        this.height = 50;
        this.label = 'Q1';

        this.terminals = [
            { x: -25, y: 0, name: 'B' },
            { x: 15, y: -20, name: 'C' },
            { x: 15, y: 20, name: 'E' }
        ];

        this.properties = {
            beta: 100,
            saturationCurrent: 1e-14,
            earlyVoltage: 100
        };
    }

    static getDisplayName() {
        return 'PNP BJT';
    }

    static getDefaultProperties() {
        return { beta: 100, saturationCurrent: 1e-14, earlyVoltage: 100 };
    }

    static getPropertyDefinitions() {
        return NPNTransistor.getPropertyDefinitions();
    }

    stamp(A, b, mna, time, prevSolution, dt) {
        // PNP is similar to NPN but with reversed polarities
        const nB = mna.getNodeIndex(this.nodes[0]);
        const nC = mna.getNodeIndex(this.nodes[1]);
        const nE = mna.getNodeIndex(this.nodes[2]);

        const { beta, saturationCurrent: Is } = this.properties;
        const Vt = 0.026;

        let Veb = 0.6, Vcb = 0;
        if (prevSolution) {
            const vB = nB > 0 ? prevSolution.get(nB - 1) : 0;
            const vC = nC > 0 ? prevSolution.get(nC - 1) : 0;
            const vE = nE > 0 ? prevSolution.get(nE - 1) : 0;
            Veb = vE - vB;
            Vcb = vC - vB;
        }

        Veb = Math.max(-5 * Vt, Math.min(40 * Vt, Veb));
        Vcb = Math.max(-5 * Vt, Math.min(40 * Vt, Vcb));

        const expEB = Math.exp(Veb / Vt);
        const expCB = Math.exp(Vcb / Vt);

        const Ieb = Is * (expEB - 1) / beta;
        const Geb = (Is / (beta * Vt)) * expEB;

        const Ic = Is * (expEB - expCB);
        const GcEB = (Is / Vt) * expEB;
        const GcCB = (Is / Vt) * expCB;

        // Stamp E-B diode (reversed from NPN)
        const IeqEB = Ieb - Geb * Veb;

        if (nE > 0) A.add(nE - 1, nE - 1, Geb);
        if (nB > 0) A.add(nB - 1, nB - 1, Geb);
        if (nE > 0 && nB > 0) {
            A.add(nE - 1, nB - 1, -Geb);
            A.add(nB - 1, nE - 1, -Geb);
        }

        if (nE > 0) b.add(nE - 1, -IeqEB);
        if (nB > 0) b.add(nB - 1, IeqEB);

        // Collector current
        const IeqC = Ic - GcEB * Veb + GcCB * Vcb;

        if (nC > 0) A.add(nC - 1, nC - 1, GcCB);
        if (nB > 0) A.add(nB - 1, nB - 1, GcEB);

        if (nC > 0 && nE > 0) A.add(nC - 1, nE - 1, GcEB);
        if (nC > 0 && nB > 0) A.add(nC - 1, nB - 1, -GcEB - GcCB);
        if (nB > 0 && nE > 0) A.add(nB - 1, nE - 1, -GcEB);
        if (nB > 0 && nC > 0) A.add(nB - 1, nC - 1, GcCB);

        if (nC > 0) b.add(nC - 1, -IeqC);
        if (nB > 0) b.add(nB - 1, IeqC);
    }

    drawShape(ctx) {
        ctx.beginPath();

        // Base lead
        ctx.moveTo(-25, 0);
        ctx.lineTo(-5, 0);

        // Vertical bar
        ctx.moveTo(-5, -12);
        ctx.lineTo(-5, 12);

        // Collector line
        ctx.moveTo(-5, -6);
        ctx.lineTo(12, -16);
        ctx.lineTo(15, -20);

        // Emitter line (arrow pointing IN for PNP)
        ctx.moveTo(-5, 6);
        ctx.lineTo(12, 16);
        ctx.lineTo(15, 20);

        ctx.stroke();

        // Arrow on emitter pointing in
        ctx.beginPath();
        ctx.moveTo(-2, 8);
        ctx.lineTo(2, 10);
        ctx.lineTo(0, 4);
        ctx.closePath();
        ctx.fill();

        // Circle outline
        ctx.beginPath();
        ctx.arc(0, 0, 20, 0, Math.PI * 2);
        ctx.stroke();
    }

    getValueString() {
        return `PNP beta=${this.properties.beta}`;
    }
}

/**
 * N-Channel MOSFET (simplified model)
 */
class NMOS extends Component {
    constructor(x, y) {
        super('nmos', x, y);

        this.width = 40;
        this.height = 50;
        this.label = 'M1';

        // G: Gate, D: Drain, S: Source
        this.terminals = [
            { x: -25, y: 0, name: 'G' },
            { x: 15, y: -20, name: 'D' },
            { x: 15, y: 20, name: 'S' }
        ];

        this.properties = {
            kn: 0.001,        // Transconductance parameter (A/V^2)
            vth: 1.0,         // Threshold voltage
            lambda: 0.01      // Channel length modulation
        };
    }

    static getDisplayName() {
        return 'NMOS';
    }

    static getDefaultProperties() {
        return { kn: 0.001, vth: 1.0, lambda: 0.01 };
    }

    static getPropertyDefinitions() {
        return [
            { name: 'kn', label: 'Kn', type: 'number', unit: 'A/V2', min: 1e-6, max: 1 },
            { name: 'vth', label: 'Vth', type: 'number', unit: 'V', min: 0, max: 10 },
            { name: 'lambda', label: 'Lambda', type: 'number', unit: '1/V', min: 0, max: 0.1 }
        ];
    }

    stamp(A, b, mna, time, prevSolution, dt) {
        const nG = mna.getNodeIndex(this.nodes[0]);
        const nD = mna.getNodeIndex(this.nodes[1]);
        const nS = mna.getNodeIndex(this.nodes[2]);

        const { kn, vth, lambda } = this.properties;

        // Get operating point
        let Vgs = 0, Vds = 0;
        if (prevSolution) {
            const vG = nG > 0 ? prevSolution.get(nG - 1) : 0;
            const vD = nD > 0 ? prevSolution.get(nD - 1) : 0;
            const vS = nS > 0 ? prevSolution.get(nS - 1) : 0;
            Vgs = vG - vS;
            Vds = vD - vS;
        }

        // MOSFET regions
        let Id = 0, Gm = 0, Gds = 0;

        if (Vgs <= vth) {
            // Cutoff: Id = 0
            Id = 0;
            Gm = 0;
            Gds = 1e-12; // Small conductance for convergence
        } else if (Vds < Vgs - vth) {
            // Triode/Linear region
            Id = kn * ((Vgs - vth) * Vds - 0.5 * Vds * Vds) * (1 + lambda * Vds);
            Gm = kn * Vds * (1 + lambda * Vds);
            Gds = kn * ((Vgs - vth) - Vds) * (1 + lambda * Vds) + kn * ((Vgs - vth) * Vds - 0.5 * Vds * Vds) * lambda;
        } else {
            // Saturation region
            const Vov = Vgs - vth;
            Id = 0.5 * kn * Vov * Vov * (1 + lambda * Vds);
            Gm = kn * Vov * (1 + lambda * Vds);
            Gds = 0.5 * kn * Vov * Vov * lambda;
        }

        // Ensure minimum conductance
        Gds = Math.max(Gds, 1e-12);

        // Linearized model: Id = Id0 + Gm*(Vgs - Vgs0) + Gds*(Vds - Vds0)
        const Ieq = Id - Gm * Vgs - Gds * Vds;

        // Stamp drain-source conductance
        if (nD > 0) A.add(nD - 1, nD - 1, Gds);
        if (nS > 0) A.add(nS - 1, nS - 1, Gds + Gm);
        if (nD > 0 && nS > 0) {
            A.add(nD - 1, nS - 1, -Gds);
            A.add(nS - 1, nD - 1, -Gds);
        }

        // Stamp transconductance (Gm controlled by Vgs)
        if (nD > 0 && nG > 0) A.add(nD - 1, nG - 1, Gm);
        if (nD > 0 && nS > 0) A.add(nD - 1, nS - 1, -Gm);
        if (nS > 0 && nG > 0) A.add(nS - 1, nG - 1, -Gm);

        // Stamp equivalent current
        if (nD > 0) b.add(nD - 1, -Ieq);
        if (nS > 0) b.add(nS - 1, Ieq);
    }

    drawShape(ctx) {
        ctx.beginPath();

        // Gate lead
        ctx.moveTo(-25, 0);
        ctx.lineTo(-10, 0);

        // Gate vertical line
        ctx.moveTo(-10, -10);
        ctx.lineTo(-10, 10);

        // Channel (vertical bar with gap)
        ctx.moveTo(-5, -12);
        ctx.lineTo(-5, -4);
        ctx.moveTo(-5, 4);
        ctx.lineTo(-5, 12);

        // Drain connection
        ctx.moveTo(-5, -8);
        ctx.lineTo(10, -8);
        ctx.lineTo(10, -20);
        ctx.lineTo(15, -20);

        // Source connection
        ctx.moveTo(-5, 8);
        ctx.lineTo(10, 8);
        ctx.lineTo(10, 20);
        ctx.lineTo(15, 20);

        // Body connection and arrow
        ctx.moveTo(10, 0);
        ctx.lineTo(-5, 0);

        ctx.stroke();

        // Arrow pointing from source to body (N-channel)
        ctx.beginPath();
        ctx.moveTo(-5, 0);
        ctx.lineTo(2, 3);
        ctx.lineTo(2, -3);
        ctx.closePath();
        ctx.fill();
    }

    getValueString() {
        return `NMOS Vth=${this.properties.vth}V`;
    }
}

/**
 * P-Channel MOSFET
 */
class PMOS extends Component {
    constructor(x, y) {
        super('pmos', x, y);

        this.width = 40;
        this.height = 50;
        this.label = 'M1';

        this.terminals = [
            { x: -25, y: 0, name: 'G' },
            { x: 15, y: -20, name: 'D' },
            { x: 15, y: 20, name: 'S' }
        ];

        this.properties = {
            kp: 0.0005,
            vth: -1.0,
            lambda: 0.01
        };
    }

    static getDisplayName() {
        return 'PMOS';
    }

    static getDefaultProperties() {
        return { kp: 0.0005, vth: -1.0, lambda: 0.01 };
    }

    static getPropertyDefinitions() {
        return [
            { name: 'kp', label: 'Kp', type: 'number', unit: 'A/V2', min: 1e-6, max: 1 },
            { name: 'vth', label: 'Vth', type: 'number', unit: 'V', min: -10, max: 0 },
            { name: 'lambda', label: 'Lambda', type: 'number', unit: '1/V', min: 0, max: 0.1 }
        ];
    }

    stamp(A, b, mna, time, prevSolution, dt) {
        const nG = mna.getNodeIndex(this.nodes[0]);
        const nD = mna.getNodeIndex(this.nodes[1]);
        const nS = mna.getNodeIndex(this.nodes[2]);

        const { kp, vth, lambda } = this.properties;

        let Vsg = 0, Vsd = 0;
        if (prevSolution) {
            const vG = nG > 0 ? prevSolution.get(nG - 1) : 0;
            const vD = nD > 0 ? prevSolution.get(nD - 1) : 0;
            const vS = nS > 0 ? prevSolution.get(nS - 1) : 0;
            Vsg = vS - vG;
            Vsd = vS - vD;
        }

        const Vtp = -vth; // Positive threshold for PMOS

        let Id = 0, Gm = 0, Gds = 0;

        if (Vsg <= Vtp) {
            Id = 0;
            Gm = 0;
            Gds = 1e-12;
        } else if (Vsd < Vsg - Vtp) {
            Id = kp * ((Vsg - Vtp) * Vsd - 0.5 * Vsd * Vsd) * (1 + lambda * Vsd);
            Gm = kp * Vsd * (1 + lambda * Vsd);
            Gds = kp * ((Vsg - Vtp) - Vsd) * (1 + lambda * Vsd);
        } else {
            const Vov = Vsg - Vtp;
            Id = 0.5 * kp * Vov * Vov * (1 + lambda * Vsd);
            Gm = kp * Vov * (1 + lambda * Vsd);
            Gds = 0.5 * kp * Vov * Vov * lambda;
        }

        Gds = Math.max(Gds, 1e-12);

        const Ieq = Id - Gm * Vsg - Gds * Vsd;

        // Stamp (note: current flows from S to D in PMOS)
        if (nS > 0) A.add(nS - 1, nS - 1, Gds + Gm);
        if (nD > 0) A.add(nD - 1, nD - 1, Gds);
        if (nS > 0 && nD > 0) {
            A.add(nS - 1, nD - 1, -Gds);
            A.add(nD - 1, nS - 1, -Gds);
        }

        if (nS > 0 && nG > 0) A.add(nS - 1, nG - 1, -Gm);
        if (nD > 0 && nG > 0) A.add(nD - 1, nG - 1, Gm);
        if (nD > 0 && nS > 0) A.add(nD - 1, nS - 1, -Gm);

        if (nS > 0) b.add(nS - 1, -Ieq);
        if (nD > 0) b.add(nD - 1, Ieq);
    }

    drawShape(ctx) {
        ctx.beginPath();

        // Gate lead
        ctx.moveTo(-25, 0);
        ctx.lineTo(-12, 0);

        // Gate bubble (for PMOS)
        ctx.arc(-11, 0, 2, 0, Math.PI * 2);

        // Gate vertical line
        ctx.moveTo(-8, -10);
        ctx.lineTo(-8, 10);

        // Channel
        ctx.moveTo(-3, -12);
        ctx.lineTo(-3, -4);
        ctx.moveTo(-3, 4);
        ctx.lineTo(-3, 12);

        // Drain connection
        ctx.moveTo(-3, -8);
        ctx.lineTo(10, -8);
        ctx.lineTo(10, -20);
        ctx.lineTo(15, -20);

        // Source connection
        ctx.moveTo(-3, 8);
        ctx.lineTo(10, 8);
        ctx.lineTo(10, 20);
        ctx.lineTo(15, 20);

        // Body connection
        ctx.moveTo(10, 0);
        ctx.lineTo(-3, 0);

        ctx.stroke();

        // Arrow pointing from body to source (P-channel)
        ctx.beginPath();
        ctx.moveTo(2, 0);
        ctx.lineTo(-3, 3);
        ctx.lineTo(-3, -3);
        ctx.closePath();
        ctx.fill();
    }

    getValueString() {
        return `PMOS Vth=${this.properties.vth}V`;
    }
}

// Register components
ComponentRegistry.register('diode', Diode);
ComponentRegistry.register('npn_bjt', NPNTransistor);
ComponentRegistry.register('pnp_bjt', PNPTransistor);
ComponentRegistry.register('nmos', NMOS);
ComponentRegistry.register('pmos', PMOS);

// Export
window.Diode = Diode;
window.NPNTransistor = NPNTransistor;
window.PNPTransistor = PNPTransistor;
window.NMOS = NMOS;
window.PMOS = PMOS;
