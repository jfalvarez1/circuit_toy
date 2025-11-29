/**
 * Wire class for connecting components
 */

let wireIdCounter = 0;

class Wire {
    constructor(startNodeId, endNodeId) {
        this.id = ++wireIdCounter;
        this.startNodeId = startNodeId;
        this.endNodeId = endNodeId;

        // Wire path (list of points for routing)
        this.points = [];

        // Display properties
        this.selected = false;
        this.highlighted = false;
        this.color = '#00d9ff';
        this.width = 2;

        // Current flow (for visualization)
        this.current = 0;
    }

    /**
     * Set wire path points
     */
    setPath(points) {
        this.points = points.map(p => ({ x: p.x, y: p.y }));
    }

    /**
     * Get all points including start and end
     */
    getAllPoints(nodeManager) {
        const allPoints = [];

        const startNode = nodeManager.getNode(this.startNodeId);
        if (startNode) {
            allPoints.push({ x: startNode.x, y: startNode.y });
        }

        allPoints.push(...this.points);

        const endNode = nodeManager.getNode(this.endNodeId);
        if (endNode) {
            allPoints.push({ x: endNode.x, y: endNode.y });
        }

        return allPoints;
    }

    /**
     * Check if a point is near the wire
     */
    containsPoint(px, py, nodeManager, threshold = 5) {
        const points = this.getAllPoints(nodeManager);

        for (let i = 0; i < points.length - 1; i++) {
            const p1 = points[i];
            const p2 = points[i + 1];

            const dist = this.pointToLineDistance(px, py, p1.x, p1.y, p2.x, p2.y);
            if (dist <= threshold) {
                return true;
            }
        }
        return false;
    }

    /**
     * Calculate distance from point to line segment
     */
    pointToLineDistance(px, py, x1, y1, x2, y2) {
        const dx = x2 - x1;
        const dy = y2 - y1;
        const lengthSq = dx * dx + dy * dy;

        if (lengthSq === 0) {
            // Point-like segment
            return Math.sqrt((px - x1) * (px - x1) + (py - y1) * (py - y1));
        }

        // Project point onto line
        let t = ((px - x1) * dx + (py - y1) * dy) / lengthSq;
        t = Math.max(0, Math.min(1, t));

        const projX = x1 + t * dx;
        const projY = y1 + t * dy;

        return Math.sqrt((px - projX) * (px - projX) + (py - projY) * (py - projY));
    }

    /**
     * Draw the wire
     */
    draw(ctx, nodeManager, showCurrent = false) {
        const points = this.getAllPoints(nodeManager);
        if (points.length < 2) return;

        ctx.save();

        // Wire style
        if (this.selected) {
            ctx.strokeStyle = '#e94560';
            ctx.lineWidth = this.width + 1;
        } else if (this.highlighted) {
            ctx.strokeStyle = '#ffffff';
            ctx.lineWidth = this.width + 1;
        } else {
            ctx.strokeStyle = this.color;
            ctx.lineWidth = this.width;
        }

        ctx.lineCap = 'round';
        ctx.lineJoin = 'round';

        // Draw wire path
        ctx.beginPath();
        ctx.moveTo(points[0].x, points[0].y);

        for (let i = 1; i < points.length; i++) {
            ctx.lineTo(points[i].x, points[i].y);
        }

        ctx.stroke();

        // Draw current flow arrows if enabled
        if (showCurrent && Math.abs(this.current) > 0.001) {
            this.drawCurrentFlow(ctx, points);
        }

        ctx.restore();
    }

    /**
     * Draw current flow indicators
     */
    drawCurrentFlow(ctx, points) {
        const arrowSize = 6;
        const spacing = 30;

        ctx.fillStyle = this.current > 0 ? '#00ff88' : '#ff8800';

        for (let i = 0; i < points.length - 1; i++) {
            const p1 = points[i];
            const p2 = points[i + 1];

            const dx = p2.x - p1.x;
            const dy = p2.y - p1.y;
            const length = Math.sqrt(dx * dx + dy * dy);

            if (length < spacing) continue;

            const ux = dx / length;
            const uy = dy / length;

            // Draw arrows along segment
            const numArrows = Math.floor(length / spacing);
            for (let j = 1; j <= numArrows; j++) {
                const t = j / (numArrows + 1);
                const ax = p1.x + dx * t;
                const ay = p1.y + dy * t;

                // Arrow direction based on current
                const dir = this.current > 0 ? 1 : -1;

                ctx.beginPath();
                ctx.moveTo(ax + ux * arrowSize * dir, ay + uy * arrowSize * dir);
                ctx.lineTo(ax - uy * arrowSize * 0.5, ay + ux * arrowSize * 0.5);
                ctx.lineTo(ax + uy * arrowSize * 0.5, ay - ux * arrowSize * 0.5);
                ctx.closePath();
                ctx.fill();
            }
        }
    }

    /**
     * Serialize to JSON
     */
    toJSON() {
        return {
            id: this.id,
            startNodeId: this.startNodeId,
            endNodeId: this.endNodeId,
            points: this.points.map(p => ({ x: p.x, y: p.y }))
        };
    }

    /**
     * Deserialize from JSON
     */
    static fromJSON(json) {
        const wire = new Wire(json.startNodeId, json.endNodeId);
        wire.id = json.id;
        wire.points = json.points || [];

        if (wire.id >= wireIdCounter) {
            wireIdCounter = wire.id;
        }

        return wire;
    }
}

/**
 * Wire Manager handles wire creation and routing
 */
class WireManager {
    constructor(nodeManager) {
        this.nodeManager = nodeManager;
        this.wires = new Map(); // id -> Wire
        this.tempWire = null; // Wire being drawn
    }

    /**
     * Create a wire between two nodes
     */
    createWire(startNodeId, endNodeId) {
        const wire = new Wire(startNodeId, endNodeId);
        this.wires.set(wire.id, wire);
        return wire;
    }

    /**
     * Get wire by ID
     */
    getWire(id) {
        return this.wires.get(id);
    }

    /**
     * Delete a wire
     */
    deleteWire(wireId) {
        this.wires.delete(wireId);
    }

    /**
     * Find wire at position
     */
    findWireAt(x, y, threshold = 5) {
        for (const wire of this.wires.values()) {
            if (wire.containsPoint(x, y, this.nodeManager, threshold)) {
                return wire;
            }
        }
        return null;
    }

    /**
     * Get all wires connected to a node
     */
    getWiresForNode(nodeId) {
        const result = [];
        for (const wire of this.wires.values()) {
            if (wire.startNodeId === nodeId || wire.endNodeId === nodeId) {
                result.push(wire);
            }
        }
        return result;
    }

    /**
     * Delete all wires connected to a node
     */
    deleteWiresForNode(nodeId) {
        for (const [id, wire] of this.wires.entries()) {
            if (wire.startNodeId === nodeId || wire.endNodeId === nodeId) {
                this.wires.delete(id);
            }
        }
    }

    /**
     * Start drawing a new wire
     */
    startTempWire(startNodeId) {
        this.tempWire = {
            startNodeId,
            points: [],
            endPoint: null
        };
    }

    /**
     * Update temporary wire end point
     */
    updateTempWire(x, y) {
        if (this.tempWire) {
            this.tempWire.endPoint = { x, y };
        }
    }

    /**
     * Add a bend point to temporary wire
     */
    addTempWirePoint(x, y) {
        if (this.tempWire) {
            this.tempWire.points.push({ x, y });
        }
    }

    /**
     * Finish temporary wire
     */
    finishTempWire(endNodeId) {
        if (!this.tempWire) return null;

        // Don't create wire to same node
        if (this.tempWire.startNodeId === endNodeId) {
            this.tempWire = null;
            return null;
        }

        const wire = this.createWire(this.tempWire.startNodeId, endNodeId);
        wire.setPath(this.tempWire.points);

        this.tempWire = null;
        return wire;
    }

    /**
     * Cancel temporary wire
     */
    cancelTempWire() {
        this.tempWire = null;
    }

    /**
     * Get all wires
     */
    getAllWires() {
        return Array.from(this.wires.values());
    }

    /**
     * Clear all wires
     */
    clear() {
        this.wires.clear();
        this.tempWire = null;
        wireIdCounter = 0;
    }

    /**
     * Draw all wires
     */
    draw(ctx, showCurrent = false) {
        // Draw completed wires
        for (const wire of this.wires.values()) {
            wire.draw(ctx, this.nodeManager, showCurrent);
        }

        // Draw temporary wire
        if (this.tempWire && this.tempWire.endPoint) {
            const startNode = this.nodeManager.getNode(this.tempWire.startNodeId);
            if (startNode) {
                ctx.save();
                ctx.strokeStyle = '#ffaa00';
                ctx.lineWidth = 2;
                ctx.setLineDash([5, 5]);
                ctx.lineCap = 'round';

                ctx.beginPath();
                ctx.moveTo(startNode.x, startNode.y);

                for (const p of this.tempWire.points) {
                    ctx.lineTo(p.x, p.y);
                }

                ctx.lineTo(this.tempWire.endPoint.x, this.tempWire.endPoint.y);
                ctx.stroke();

                ctx.restore();
            }
        }
    }

    /**
     * Serialize to JSON
     */
    toJSON() {
        return {
            wires: Array.from(this.wires.values()).map(w => w.toJSON())
        };
    }

    /**
     * Deserialize from JSON
     */
    static fromJSON(json, nodeManager) {
        const manager = new WireManager(nodeManager);
        for (const wireJson of json.wires) {
            const wire = Wire.fromJSON(wireJson);
            manager.wires.set(wire.id, wire);
        }
        return manager;
    }
}

// Export
window.Wire = Wire;
window.WireManager = WireManager;
