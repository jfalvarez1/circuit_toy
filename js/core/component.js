/**
 * Base Component class for all circuit elements
 */

let componentIdCounter = 0;

class Component {
    constructor(type, x, y) {
        this.id = ++componentIdCounter;
        this.type = type;
        this.x = x;
        this.y = y;
        this.rotation = 0; // 0, 90, 180, 270 degrees
        this.selected = false;
        this.highlighted = false;

        // Terminal positions relative to component center
        this.terminals = [];

        // Node connections (node IDs that this component connects to)
        this.nodes = [];

        // Whether this component adds a current variable (voltage sources, etc.)
        this.needsVoltageVariable = false;
        this.voltageVarIndex = -1;

        // Component properties (editable)
        this.properties = {};

        // Display settings
        this.width = 60;
        this.height = 20;
        this.label = '';
        this.showLabel = true;
        this.showValue = true;
    }

    /**
     * Get display name for this component type
     */
    static getDisplayName() {
        return 'Component';
    }

    /**
     * Get the default properties for this component type
     */
    static getDefaultProperties() {
        return {};
    }

    /**
     * Get property definitions for the UI
     * @returns {Array} Array of { name, label, type, unit, min, max, options }
     */
    static getPropertyDefinitions() {
        return [];
    }

    /**
     * Get node identifiers that this component connects to
     * @returns {Array} Array of node IDs
     */
    getNodes() {
        return this.nodes;
    }

    /**
     * Set the nodes this component is connected to
     * @param {Array} nodes - Array of node IDs
     */
    setNodes(nodes) {
        this.nodes = nodes;
    }

    /**
     * Stamp the component into the MNA matrix
     * @param {Matrix} A - Conductance matrix
     * @param {Vector} b - Current vector
     * @param {MNABuilder} mna - MNA builder for node mapping
     * @param {number} time - Current simulation time
     * @param {Vector} prevSolution - Previous solution
     * @param {number} dt - Time step
     */
    stamp(A, b, mna, time, prevSolution, dt) {
        // Override in subclasses
    }

    /**
     * Get terminal positions in world coordinates
     * @returns {Array} Array of {x, y} positions
     */
    getTerminalPositions() {
        const cos = Math.cos(this.rotation * Math.PI / 180);
        const sin = Math.sin(this.rotation * Math.PI / 180);

        return this.terminals.map(t => ({
            x: this.x + t.x * cos - t.y * sin,
            y: this.y + t.x * sin + t.y * cos,
            name: t.name
        }));
    }

    /**
     * Check if a point is near a terminal
     * @param {number} px - Point x
     * @param {number} py - Point y
     * @param {number} threshold - Distance threshold
     * @returns {Object|null} Terminal info or null
     */
    getTerminalAt(px, py, threshold = 10) {
        const terminals = this.getTerminalPositions();
        for (let i = 0; i < terminals.length; i++) {
            const t = terminals[i];
            const dx = px - t.x;
            const dy = py - t.y;
            if (Math.sqrt(dx * dx + dy * dy) <= threshold) {
                return { index: i, ...t };
            }
        }
        return null;
    }

    /**
     * Check if a point is within the component bounds
     * @param {number} px - Point x
     * @param {number} py - Point y
     * @returns {boolean}
     */
    containsPoint(px, py) {
        // Transform point to component local coordinates
        const cos = Math.cos(-this.rotation * Math.PI / 180);
        const sin = Math.sin(-this.rotation * Math.PI / 180);

        const dx = px - this.x;
        const dy = py - this.y;

        const localX = dx * cos - dy * sin;
        const localY = dx * sin + dy * cos;

        return Math.abs(localX) <= this.width / 2 + 5 &&
               Math.abs(localY) <= this.height / 2 + 5;
    }

    /**
     * Rotate the component by 90 degrees
     */
    rotate() {
        this.rotation = (this.rotation + 90) % 360;
    }

    /**
     * Draw the component on the canvas
     * @param {CanvasRenderingContext2D} ctx - Canvas context
     */
    draw(ctx) {
        ctx.save();
        ctx.translate(this.x, this.y);
        ctx.rotate(this.rotation * Math.PI / 180);

        // Selection/highlight styling
        if (this.selected) {
            ctx.strokeStyle = '#e94560';
            ctx.lineWidth = 2;
        } else if (this.highlighted) {
            ctx.strokeStyle = '#00d9ff';
            ctx.lineWidth = 2;
        } else {
            ctx.strokeStyle = '#ffffff';
            ctx.lineWidth = 1.5;
        }

        ctx.fillStyle = '#ffffff';
        ctx.font = '10px sans-serif';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';

        // Draw component shape (override in subclasses)
        this.drawShape(ctx);

        // Draw terminals
        this.drawTerminals(ctx);

        ctx.restore();

        // Draw label and value (not rotated)
        if (this.showLabel || this.showValue) {
            this.drawLabels(ctx);
        }
    }

    /**
     * Draw the component shape (override in subclasses)
     */
    drawShape(ctx) {
        // Default: simple rectangle
        ctx.strokeRect(-this.width / 2, -this.height / 2, this.width, this.height);
    }

    /**
     * Draw terminal connection points
     */
    drawTerminals(ctx) {
        ctx.fillStyle = '#00d9ff';
        for (const t of this.terminals) {
            ctx.beginPath();
            ctx.arc(t.x, t.y, 3, 0, Math.PI * 2);
            ctx.fill();
        }
    }

    /**
     * Draw component label and value
     */
    drawLabels(ctx) {
        ctx.save();
        ctx.fillStyle = '#b0b0b0';
        ctx.font = '10px sans-serif';
        ctx.textAlign = 'center';

        let labelY = this.y - this.height / 2 - 12;

        if (this.showLabel && this.label) {
            ctx.fillText(this.label, this.x, labelY);
            labelY -= 12;
        }

        if (this.showValue) {
            const valueStr = this.getValueString();
            if (valueStr) {
                ctx.fillStyle = '#ffffff';
                ctx.fillText(valueStr, this.x, this.y + this.height / 2 + 12);
            }
        }

        ctx.restore();
    }

    /**
     * Get a formatted string of the component's main value
     */
    getValueString() {
        return '';
    }

    /**
     * Format a value with engineering notation
     */
    formatEngineering(value, unit) {
        const prefixes = [
            { exp: 12, prefix: 'T' },
            { exp: 9, prefix: 'G' },
            { exp: 6, prefix: 'M' },
            { exp: 3, prefix: 'k' },
            { exp: 0, prefix: '' },
            { exp: -3, prefix: 'm' },
            { exp: -6, prefix: 'u' },
            { exp: -9, prefix: 'n' },
            { exp: -12, prefix: 'p' }
        ];

        if (value === 0) return `0 ${unit}`;

        const absVal = Math.abs(value);
        const sign = value < 0 ? '-' : '';

        for (const { exp, prefix } of prefixes) {
            if (absVal >= Math.pow(10, exp)) {
                const scaled = absVal / Math.pow(10, exp);
                const formatted = scaled < 10 ? scaled.toFixed(2) :
                                  scaled < 100 ? scaled.toFixed(1) :
                                  scaled.toFixed(0);
                return `${sign}${formatted} ${prefix}${unit}`;
            }
        }

        return `${value.toExponential(2)} ${unit}`;
    }

    /**
     * Serialize component to JSON
     */
    toJSON() {
        return {
            id: this.id,
            type: this.type,
            x: this.x,
            y: this.y,
            rotation: this.rotation,
            properties: { ...this.properties },
            nodes: [...this.nodes],
            label: this.label
        };
    }

    /**
     * Deserialize component from JSON
     */
    static fromJSON(json, ComponentClass) {
        const comp = new ComponentClass(json.x, json.y);
        comp.id = json.id;
        comp.rotation = json.rotation || 0;
        comp.properties = { ...json.properties };
        comp.nodes = [...(json.nodes || [])];
        comp.label = json.label || '';
        return comp;
    }
}

// Component registry
const ComponentRegistry = {
    types: {},

    register(type, ComponentClass) {
        this.types[type] = ComponentClass;
    },

    create(type, x, y) {
        const ComponentClass = this.types[type];
        if (!ComponentClass) {
            console.error(`Unknown component type: ${type}`);
            return null;
        }
        return new ComponentClass(x, y);
    },

    getClass(type) {
        return this.types[type];
    },

    getTypes() {
        return Object.keys(this.types);
    }
};

// Export
window.Component = Component;
window.ComponentRegistry = ComponentRegistry;
