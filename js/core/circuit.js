/**
 * Circuit class manages all components, nodes, and wires
 */

class Circuit {
    constructor() {
        this.components = new Map(); // id -> Component
        this.nodeManager = new NodeManager();
        this.wireManager = new WireManager(this.nodeManager);

        // Simulation state
        this.solution = null;
        this.nodeIndexMap = null;
    }

    /**
     * Add a component to the circuit
     */
    addComponent(component) {
        this.components.set(component.id, component);

        // Create nodes for component terminals
        const terminals = component.getTerminalPositions();
        const nodeIds = [];

        for (let i = 0; i < terminals.length; i++) {
            const t = terminals[i];
            // Check if there's an existing node nearby
            let node = this.nodeManager.findNodeAt(t.x, t.y, 10);
            if (!node) {
                node = this.nodeManager.createNode(t.x, t.y);
            }
            node.addConnection(component.id, i);
            nodeIds.push(node.id);
        }

        component.setNodes(nodeIds);
        return component;
    }

    /**
     * Remove a component from the circuit
     */
    removeComponent(componentId) {
        const component = this.components.get(componentId);
        if (!component) return;

        // Remove node connections
        this.nodeManager.removeComponentConnections(componentId);

        // Remove wires connected to component's nodes
        for (const nodeId of component.nodes) {
            this.wireManager.deleteWiresForNode(nodeId);
        }

        this.components.delete(componentId);
    }

    /**
     * Get component by ID
     */
    getComponent(id) {
        return this.components.get(id);
    }

    /**
     * Find component at position
     */
    findComponentAt(x, y) {
        for (const comp of this.components.values()) {
            if (comp.containsPoint(x, y)) {
                return comp;
            }
        }
        return null;
    }

    /**
     * Find component terminal at position
     */
    findTerminalAt(x, y, threshold = 10) {
        for (const comp of this.components.values()) {
            const terminal = comp.getTerminalAt(x, y, threshold);
            if (terminal) {
                return {
                    component: comp,
                    terminalIndex: terminal.index,
                    x: terminal.x,
                    y: terminal.y,
                    name: terminal.name
                };
            }
        }
        return null;
    }

    /**
     * Get all components
     */
    getAllComponents() {
        return Array.from(this.components.values());
    }

    /**
     * Get component count
     */
    getComponentCount() {
        return this.components.size;
    }

    /**
     * Get node count
     */
    getNodeCount() {
        return this.nodeManager.getAllNodes().length;
    }

    /**
     * Connect two nodes (create wire and merge nodes)
     */
    connectNodes(nodeId1, nodeId2) {
        if (nodeId1 === nodeId2) return null;

        // Create wire
        const wire = this.wireManager.createWire(nodeId1, nodeId2);

        // Merge nodes (they become the same electrical node)
        // For simplicity, we keep both nodes but track they're connected via wire
        return wire;
    }

    /**
     * Build the node index map for simulation
     * Connected nodes (via wires) should have the same index
     */
    buildNodeMap() {
        const nodes = this.nodeManager.getAllNodes();
        const wires = this.wireManager.getAllWires();

        // Union-Find for grouping connected nodes
        const parent = new Map();

        function find(x) {
            if (!parent.has(x)) parent.set(x, x);
            if (parent.get(x) !== x) {
                parent.set(x, find(parent.get(x)));
            }
            return parent.get(x);
        }

        function union(x, y) {
            const px = find(x);
            const py = find(y);
            if (px !== py) {
                // Prefer keeping ground node as parent
                const nodeX = nodes.find(n => n.id === px);
                const nodeY = nodes.find(n => n.id === py);
                if (nodeY && nodeY.isGround) {
                    parent.set(px, py);
                } else {
                    parent.set(py, px);
                }
            }
        }

        // Initialize
        for (const node of nodes) {
            parent.set(node.id, node.id);
        }

        // Union connected nodes (via wires)
        for (const wire of wires) {
            union(wire.startNodeId, wire.endNodeId);
        }

        // Build index map
        const nodeToIndex = new Map();
        let nextIndex = 1; // 0 is reserved for ground

        // First, find and assign ground
        for (const node of nodes) {
            const root = find(node.id);
            const rootNode = nodes.find(n => n.id === root);
            if (rootNode && rootNode.isGround) {
                if (!nodeToIndex.has(root)) {
                    nodeToIndex.set(root, 0);
                }
            }
        }

        // Then assign other nodes
        for (const node of nodes) {
            const root = find(node.id);
            if (!nodeToIndex.has(root)) {
                nodeToIndex.set(root, nextIndex++);
            }
        }

        // Map all nodes to their root's index
        const result = new Map();
        for (const node of nodes) {
            const root = find(node.id);
            result.set(node.id, nodeToIndex.get(root));
        }

        this.nodeIndexMap = result;
        return result;
    }

    /**
     * Get MNA node index for a component's terminal
     */
    getNodeIndex(componentId, terminalIndex) {
        const comp = this.components.get(componentId);
        if (!comp || terminalIndex >= comp.nodes.length) return 0;

        const nodeId = comp.nodes[terminalIndex];
        return this.nodeIndexMap ? this.nodeIndexMap.get(nodeId) || 0 : 0;
    }

    /**
     * Update component nodes after moving
     */
    updateComponentNodes(component) {
        const terminals = component.getTerminalPositions();

        for (let i = 0; i < terminals.length; i++) {
            const t = terminals[i];
            const nodeId = component.nodes[i];
            const node = this.nodeManager.getNode(nodeId);

            if (node) {
                // Update node position to match terminal
                node.setPosition(t.x, t.y);
            }
        }
    }

    /**
     * Update voltage values on nodes after simulation
     */
    updateNodeVoltages(solution) {
        this.solution = solution;

        if (!this.nodeIndexMap || !solution) return;

        const nodes = this.nodeManager.getAllNodes();
        for (const node of nodes) {
            const index = this.nodeIndexMap.get(node.id);
            if (index === 0) {
                node.voltage = 0; // Ground
            } else if (index > 0 && index <= solution.size) {
                node.voltage = solution.get(index - 1);
            }
        }
    }

    /**
     * Clear the circuit
     */
    clear() {
        this.components.clear();
        this.nodeManager.clear();
        this.wireManager.clear();
        this.solution = null;
        this.nodeIndexMap = null;
    }

    /**
     * Draw the circuit
     */
    draw(ctx, options = {}) {
        const {
            showVoltages = false,
            showCurrent = false
        } = options;

        // Draw wires first (behind components)
        this.wireManager.draw(ctx, showCurrent);

        // Draw nodes
        this.nodeManager.draw(ctx, showVoltages);

        // Draw components
        for (const comp of this.components.values()) {
            comp.draw(ctx);
        }
    }

    /**
     * Serialize circuit to JSON
     */
    toJSON() {
        return {
            components: Array.from(this.components.values()).map(c => c.toJSON()),
            nodes: this.nodeManager.toJSON(),
            wires: this.wireManager.toJSON()
        };
    }

    /**
     * Deserialize circuit from JSON
     */
    static fromJSON(json) {
        const circuit = new Circuit();

        // Restore nodes first
        circuit.nodeManager = NodeManager.fromJSON(json.nodes);

        // Restore wires
        circuit.wireManager = WireManager.fromJSON(json.wires, circuit.nodeManager);

        // Restore components
        for (const compJson of json.components) {
            const ComponentClass = ComponentRegistry.getClass(compJson.type);
            if (ComponentClass) {
                const comp = Component.fromJSON(compJson, ComponentClass);
                circuit.components.set(comp.id, comp);
            }
        }

        return circuit;
    }

    /**
     * Export circuit as string
     */
    export() {
        return JSON.stringify(this.toJSON(), null, 2);
    }

    /**
     * Import circuit from string
     */
    static import(jsonString) {
        try {
            const json = JSON.parse(jsonString);
            return Circuit.fromJSON(json);
        } catch (e) {
            console.error('Failed to import circuit:', e);
            return null;
        }
    }
}

// Export
window.Circuit = Circuit;
