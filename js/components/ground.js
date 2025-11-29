/**
 * Ground component - reference point for circuit
 */

class Ground extends Component {
    constructor(x, y) {
        super('ground', x, y);

        this.width = 30;
        this.height = 30;
        this.label = 'GND';

        // Single terminal at top
        this.terminals = [
            { x: 0, y: -15, name: 'gnd' }
        ];

        this.properties = {};
    }

    static getDisplayName() {
        return 'Ground';
    }

    static getPropertyDefinitions() {
        return [];
    }

    getNodes() {
        return this.nodes;
    }

    stamp(A, b, mna, time, prevSolution, dt) {
        // Ground node is handled specially by the MNA builder
        // The node connected to ground is set to voltage = 0
        const nodeIdx = mna.getNodeIndex(this.nodes[0]);

        if (nodeIdx === 0) {
            // This is the ground node, nothing to stamp
            return;
        }

        // If ground is connected to a non-ground node, we need to
        // force that node to 0V. This is done by adding a large conductance
        // to ground (or treating it as a 0V voltage source)

        // Add a very large conductance to force node to 0
        const gLarge = 1e10;
        const n = nodeIdx - 1;
        if (n >= 0 && n < A.rows) {
            A.add(n, n, gLarge);
            // b[n] += gLarge * 0 = 0
        }
    }

    drawShape(ctx) {
        // Draw ground symbol
        ctx.beginPath();

        // Vertical line
        ctx.moveTo(0, -15);
        ctx.lineTo(0, 0);

        // Horizontal lines (ground symbol)
        ctx.moveTo(-12, 0);
        ctx.lineTo(12, 0);

        ctx.moveTo(-8, 5);
        ctx.lineTo(8, 5);

        ctx.moveTo(-4, 10);
        ctx.lineTo(4, 10);

        ctx.stroke();
    }

    getValueString() {
        return '0V';
    }
}

// Register component
ComponentRegistry.register('ground', Ground);

// Export
window.Ground = Ground;
