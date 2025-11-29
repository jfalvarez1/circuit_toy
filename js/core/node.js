/**
 * Node class represents a connection point in the circuit
 * Multiple component terminals can connect to a single node
 */

let nodeIdCounter = 0;

class CircuitNode {
    constructor(x, y) {
        this.id = ++nodeIdCounter;
        this.x = x;
        this.y = y;

        // List of connected terminals: { componentId, terminalIndex }
        this.connections = [];

        // Computed voltage (updated after simulation)
        this.voltage = 0;

        // Is this a ground node?
        this.isGround = false;

        // Display properties
        this.radius = 4;
        this.highlighted = false;
        this.selected = false;
    }

    /**
     * Add a connection to this node
     * @param {number} componentId - Component ID
     * @param {number} terminalIndex - Terminal index on the component
     */
    addConnection(componentId, terminalIndex) {
        // Avoid duplicates
        const exists = this.connections.some(
            c => c.componentId === componentId && c.terminalIndex === terminalIndex
        );
        if (!exists) {
            this.connections.push({ componentId, terminalIndex });
        }
    }

    /**
     * Remove a connection from this node
     */
    removeConnection(componentId, terminalIndex) {
        this.connections = this.connections.filter(
            c => !(c.componentId === componentId && c.terminalIndex === terminalIndex)
        );
    }

    /**
     * Remove all connections for a component
     */
    removeComponentConnections(componentId) {
        this.connections = this.connections.filter(c => c.componentId !== componentId);
    }

    /**
     * Check if this node has any connections
     */
    hasConnections() {
        return this.connections.length > 0;
    }

    /**
     * Get the number of connections
     */
    getConnectionCount() {
        return this.connections.length;
    }

    /**
     * Check if a point is within the node
     */
    containsPoint(px, py, threshold = 8) {
        const dx = px - this.x;
        const dy = py - this.y;
        return Math.sqrt(dx * dx + dy * dy) <= threshold;
    }

    /**
     * Update node position
     */
    setPosition(x, y) {
        this.x = x;
        this.y = y;
    }

    /**
     * Draw the node
     */
    draw(ctx, showVoltage = false) {
        ctx.save();

        // Node style
        if (this.isGround) {
            ctx.fillStyle = '#888888';
        } else if (this.selected) {
            ctx.fillStyle = '#e94560';
        } else if (this.highlighted) {
            ctx.fillStyle = '#00d9ff';
        } else if (this.connections.length > 2) {
            // Junction node (more than 2 connections)
            ctx.fillStyle = '#ffaa00';
        } else {
            ctx.fillStyle = '#00d9ff';
        }

        // Draw node circle
        ctx.beginPath();
        ctx.arc(this.x, this.y, this.radius, 0, Math.PI * 2);
        ctx.fill();

        // Draw voltage label if enabled
        if (showVoltage && !this.isGround) {
            ctx.fillStyle = '#00ff88';
            ctx.font = '9px monospace';
            ctx.textAlign = 'left';
            ctx.fillText(this.voltage.toFixed(2) + 'V', this.x + 8, this.y - 4);
        }

        ctx.restore();
    }

    /**
     * Serialize to JSON
     */
    toJSON() {
        return {
            id: this.id,
            x: this.x,
            y: this.y,
            connections: this.connections.map(c => ({ ...c })),
            isGround: this.isGround
        };
    }

    /**
     * Deserialize from JSON
     */
    static fromJSON(json) {
        const node = new CircuitNode(json.x, json.y);
        node.id = json.id;
        node.connections = json.connections.map(c => ({ ...c }));
        node.isGround = json.isGround || false;
        return node;
    }
}

/**
 * Node Manager handles creation, merging, and querying of nodes
 */
class NodeManager {
    constructor() {
        this.nodes = new Map(); // id -> CircuitNode
        this.groundNode = null;
    }

    /**
     * Create a new node at position
     */
    createNode(x, y) {
        const node = new CircuitNode(x, y);
        this.nodes.set(node.id, node);
        return node;
    }

    /**
     * Get node by ID
     */
    getNode(id) {
        return this.nodes.get(id);
    }

    /**
     * Find node at position
     */
    findNodeAt(x, y, threshold = 10) {
        for (const node of this.nodes.values()) {
            if (node.containsPoint(x, y, threshold)) {
                return node;
            }
        }
        return null;
    }

    /**
     * Find or create node at position
     */
    findOrCreateNode(x, y, threshold = 10) {
        let node = this.findNodeAt(x, y, threshold);
        if (!node) {
            node = this.createNode(x, y);
        }
        return node;
    }

    /**
     * Delete a node
     */
    deleteNode(nodeId) {
        const node = this.nodes.get(nodeId);
        if (node && node === this.groundNode) {
            this.groundNode = null;
        }
        this.nodes.delete(nodeId);
    }

    /**
     * Merge two nodes (combine their connections)
     */
    mergeNodes(keepNodeId, removeNodeId) {
        const keepNode = this.nodes.get(keepNodeId);
        const removeNode = this.nodes.get(removeNodeId);

        if (!keepNode || !removeNode) return;

        // Transfer connections
        for (const conn of removeNode.connections) {
            keepNode.addConnection(conn.componentId, conn.terminalIndex);
        }

        // If removed node was ground, transfer that
        if (removeNode.isGround) {
            keepNode.isGround = true;
            this.groundNode = keepNode;
        }

        this.deleteNode(removeNodeId);
    }

    /**
     * Set a node as ground
     */
    setGroundNode(nodeId) {
        // Clear previous ground
        if (this.groundNode) {
            this.groundNode.isGround = false;
        }

        const node = this.nodes.get(nodeId);
        if (node) {
            node.isGround = true;
            this.groundNode = node;
        }
    }

    /**
     * Get all nodes
     */
    getAllNodes() {
        return Array.from(this.nodes.values());
    }

    /**
     * Clear all nodes
     */
    clear() {
        this.nodes.clear();
        this.groundNode = null;
        nodeIdCounter = 0;
    }

    /**
     * Remove connections for a deleted component
     */
    removeComponentConnections(componentId) {
        for (const node of this.nodes.values()) {
            node.removeComponentConnections(componentId);
        }

        // Clean up orphan nodes (nodes with no connections)
        for (const [id, node] of this.nodes.entries()) {
            if (!node.hasConnections() && !node.isGround) {
                this.nodes.delete(id);
            }
        }
    }

    /**
     * Build node-to-index mapping for MNA
     * Ground node gets index 0, others get sequential indices
     */
    buildNodeIndexMap() {
        const map = new Map();
        let index = 0;

        // Ground is always index 0
        if (this.groundNode) {
            map.set(this.groundNode.id, 0);
        }

        // Assign indices to other nodes
        for (const node of this.nodes.values()) {
            if (!node.isGround && !map.has(node.id)) {
                index++;
                map.set(node.id, index);
            }
        }

        return map;
    }

    /**
     * Draw all nodes
     */
    draw(ctx, showVoltages = false) {
        for (const node of this.nodes.values()) {
            node.draw(ctx, showVoltages);
        }
    }

    /**
     * Serialize to JSON
     */
    toJSON() {
        return {
            nodes: Array.from(this.nodes.values()).map(n => n.toJSON()),
            groundNodeId: this.groundNode ? this.groundNode.id : null
        };
    }

    /**
     * Deserialize from JSON
     */
    static fromJSON(json) {
        const manager = new NodeManager();
        for (const nodeJson of json.nodes) {
            const node = CircuitNode.fromJSON(nodeJson);
            manager.nodes.set(node.id, node);

            if (json.groundNodeId && node.id === json.groundNodeId) {
                manager.groundNode = node;
                node.isGround = true;
            }

            // Update counter to avoid ID collisions
            if (node.id >= nodeIdCounter) {
                nodeIdCounter = node.id;
            }
        }
        return manager;
    }
}

// Export
window.CircuitNode = CircuitNode;
window.NodeManager = NodeManager;
