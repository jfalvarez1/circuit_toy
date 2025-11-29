/**
 * Canvas Manager - handles rendering and interaction
 */

class CanvasManager {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');

        // Viewport settings
        this.offset = { x: 0, y: 0 };
        this.zoom = 1;
        this.minZoom = 0.25;
        this.maxZoom = 4;

        // Grid settings
        this.gridSize = 20;
        this.showGrid = true;
        this.snapToGrid = true;

        // Interaction state
        this.isDragging = false;
        this.isPanning = false;
        this.dragStart = { x: 0, y: 0 };
        this.lastMousePos = { x: 0, y: 0 };

        // Current tool
        this.currentTool = 'select'; // select, wire, delete, probe, component
        this.selectedComponent = null;
        this.placingComponent = null;

        // Wire drawing state
        this.wireStart = null;
        this.wirePoints = [];

        // Probe tracking
        this.probes = [];
        this.selectedProbe = null;

        // References
        this.circuit = null;
        this.app = null;

        // Initialize
        this.resize();
        this.setupEventListeners();
    }

    /**
     * Set circuit reference
     */
    setCircuit(circuit) {
        this.circuit = circuit;
    }

    /**
     * Set app reference
     */
    setApp(app) {
        this.app = app;
    }

    /**
     * Resize canvas to container
     */
    resize() {
        const container = this.canvas.parentElement;
        this.canvas.width = container.clientWidth;
        this.canvas.height = container.clientHeight;
        this.render();
    }

    /**
     * Setup event listeners
     */
    setupEventListeners() {
        // Resize
        window.addEventListener('resize', () => this.resize());

        // Mouse events
        this.canvas.addEventListener('mousedown', (e) => this.onMouseDown(e));
        this.canvas.addEventListener('mousemove', (e) => this.onMouseMove(e));
        this.canvas.addEventListener('mouseup', (e) => this.onMouseUp(e));
        this.canvas.addEventListener('wheel', (e) => this.onWheel(e));
        this.canvas.addEventListener('dblclick', (e) => this.onDoubleClick(e));
        this.canvas.addEventListener('contextmenu', (e) => this.onContextMenu(e));

        // Keyboard events
        document.addEventListener('keydown', (e) => this.onKeyDown(e));
    }

    /**
     * Convert screen coordinates to world coordinates
     */
    screenToWorld(screenX, screenY) {
        return {
            x: (screenX - this.offset.x) / this.zoom,
            y: (screenY - this.offset.y) / this.zoom
        };
    }

    /**
     * Convert world coordinates to screen coordinates
     */
    worldToScreen(worldX, worldY) {
        return {
            x: worldX * this.zoom + this.offset.x,
            y: worldY * this.zoom + this.offset.y
        };
    }

    /**
     * Snap position to grid
     */
    snapToGridPos(x, y) {
        if (!this.snapToGrid) return { x, y };
        return {
            x: Math.round(x / this.gridSize) * this.gridSize,
            y: Math.round(y / this.gridSize) * this.gridSize
        };
    }

    /**
     * Set current tool
     */
    setTool(tool) {
        this.currentTool = tool;
        this.cancelCurrentAction();

        // Update cursor
        switch (tool) {
            case 'wire':
                this.canvas.style.cursor = 'crosshair';
                break;
            case 'delete':
                this.canvas.style.cursor = 'not-allowed';
                break;
            case 'probe':
                this.canvas.style.cursor = 'cell';
                break;
            default:
                this.canvas.style.cursor = 'default';
        }
    }

    /**
     * Start placing a component
     */
    startPlacingComponent(componentType) {
        this.currentTool = 'component';
        this.placingComponent = ComponentRegistry.create(componentType, 0, 0);
        this.canvas.style.cursor = 'copy';
    }

    /**
     * Cancel current action
     */
    cancelCurrentAction() {
        this.placingComponent = null;
        this.wireStart = null;
        this.wirePoints = [];
        if (this.circuit) {
            this.circuit.wireManager.cancelTempWire();
        }
        this.render();
    }

    /**
     * Mouse down handler
     */
    onMouseDown(e) {
        const rect = this.canvas.getBoundingClientRect();
        const screenX = e.clientX - rect.left;
        const screenY = e.clientY - rect.top;
        const world = this.screenToWorld(screenX, screenY);
        const snapped = this.snapToGridPos(world.x, world.y);

        this.dragStart = { x: screenX, y: screenY };
        this.lastMousePos = world;

        // Middle mouse or space+click for panning
        if (e.button === 1 || (e.button === 0 && e.shiftKey)) {
            this.isPanning = true;
            this.canvas.style.cursor = 'grabbing';
            return;
        }

        if (e.button !== 0) return;

        switch (this.currentTool) {
            case 'select':
                this.handleSelectClick(world, snapped, e);
                break;

            case 'component':
                this.handleComponentClick(snapped);
                break;

            case 'wire':
                this.handleWireClick(snapped);
                break;

            case 'delete':
                this.handleDeleteClick(world);
                break;

            case 'probe':
                this.handleProbeClick(snapped);
                break;
        }

        this.render();
    }

    /**
     * Handle select tool click
     */
    handleSelectClick(world, snapped, e) {
        if (!this.circuit) return;

        // Check for component selection
        const comp = this.circuit.findComponentAt(world.x, world.y);

        if (comp) {
            // Deselect previous
            if (this.selectedComponent && this.selectedComponent !== comp) {
                this.selectedComponent.selected = false;
            }

            comp.selected = true;
            this.selectedComponent = comp;
            this.isDragging = true;

            // Notify app
            if (this.app) {
                this.app.onComponentSelected(comp);
            }
        } else {
            // Deselect
            if (this.selectedComponent) {
                this.selectedComponent.selected = false;
                this.selectedComponent = null;
                if (this.app) {
                    this.app.onComponentDeselected();
                }
            }
        }
    }

    /**
     * Handle component placement click
     */
    handleComponentClick(snapped) {
        if (!this.circuit || !this.placingComponent) return;

        // Place the component
        this.placingComponent.x = snapped.x;
        this.placingComponent.y = snapped.y;

        this.circuit.addComponent(this.placingComponent);

        // Create another component for continuous placement
        const type = this.placingComponent.type;
        this.placingComponent = ComponentRegistry.create(type, 0, 0);

        if (this.app) {
            this.app.updateStatus(`Placed ${type}`);
            this.app.updateCounts();
        }
    }

    /**
     * Handle wire tool click
     */
    handleWireClick(snapped) {
        if (!this.circuit) return;

        // Find or create node at click position
        const node = this.circuit.nodeManager.findOrCreateNode(snapped.x, snapped.y, 10);

        if (!this.wireStart) {
            // Start wire
            this.wireStart = node;
            this.circuit.wireManager.startTempWire(node.id);
        } else {
            // End wire or add bend point
            if (node.id === this.wireStart.id) {
                // Cancel if clicking same node
                this.cancelCurrentAction();
            } else {
                // Complete wire
                this.circuit.wireManager.finishTempWire(node.id);
                this.wireStart = null;

                if (this.app) {
                    this.app.updateStatus('Wire connected');
                }
            }
        }
    }

    /**
     * Handle delete tool click
     */
    handleDeleteClick(world) {
        if (!this.circuit) return;

        // Check for component
        const comp = this.circuit.findComponentAt(world.x, world.y);
        if (comp) {
            this.circuit.removeComponent(comp.id);
            if (this.selectedComponent === comp) {
                this.selectedComponent = null;
                if (this.app) {
                    this.app.onComponentDeselected();
                }
            }
            if (this.app) {
                this.app.updateStatus('Component deleted');
                this.app.updateCounts();
            }
            return;
        }

        // Check for wire
        const wire = this.circuit.wireManager.findWireAt(world.x, world.y);
        if (wire) {
            this.circuit.wireManager.deleteWire(wire.id);
            if (this.app) {
                this.app.updateStatus('Wire deleted');
            }
            return;
        }

        // Check for probe
        const probeIndex = this.probes.findIndex(p => {
            const dx = p.x - world.x;
            const dy = p.y - world.y;
            return Math.sqrt(dx * dx + dy * dy) < 10;
        });
        if (probeIndex >= 0) {
            this.probes.splice(probeIndex, 1);
            if (this.app) {
                this.app.updateProbes(this.probes);
                this.app.updateStatus('Probe deleted');
            }
        }
    }

    /**
     * Handle probe tool click
     */
    handleProbeClick(snapped) {
        if (!this.circuit) return;

        // Find nearest node
        const node = this.circuit.nodeManager.findNodeAt(snapped.x, snapped.y, 15);

        if (node) {
            // Add probe
            const probe = {
                id: Date.now(),
                nodeId: node.id,
                x: node.x,
                y: node.y,
                color: this.getProbeColor(this.probes.length)
            };
            this.probes.push(probe);

            if (this.app) {
                this.app.updateProbes(this.probes);
                this.app.updateStatus('Probe placed');
            }
        }
    }

    /**
     * Get probe color based on index
     */
    getProbeColor(index) {
        const colors = ['#ffff00', '#00ffff', '#ff00ff', '#00ff00', '#ff8800', '#8888ff'];
        return colors[index % colors.length];
    }

    /**
     * Mouse move handler
     */
    onMouseMove(e) {
        const rect = this.canvas.getBoundingClientRect();
        const screenX = e.clientX - rect.left;
        const screenY = e.clientY - rect.top;
        const world = this.screenToWorld(screenX, screenY);
        const snapped = this.snapToGridPos(world.x, world.y);

        // Update cursor info
        this.updateCursorInfo(snapped);

        if (this.isPanning) {
            const dx = screenX - this.dragStart.x;
            const dy = screenY - this.dragStart.y;
            this.offset.x += dx;
            this.offset.y += dy;
            this.dragStart = { x: screenX, y: screenY };
            this.render();
            return;
        }

        if (this.isDragging && this.selectedComponent) {
            // Move component
            this.selectedComponent.x = snapped.x;
            this.selectedComponent.y = snapped.y;
            this.circuit.updateComponentNodes(this.selectedComponent);
            this.render();
            return;
        }

        // Update placing component position
        if (this.placingComponent) {
            this.placingComponent.x = snapped.x;
            this.placingComponent.y = snapped.y;
            this.render();
            return;
        }

        // Update wire preview
        if (this.wireStart && this.circuit) {
            this.circuit.wireManager.updateTempWire(snapped.x, snapped.y);
            this.render();
            return;
        }

        // Highlight on hover
        if (this.circuit && this.currentTool === 'select') {
            let needsRender = false;

            for (const comp of this.circuit.getAllComponents()) {
                const wasHighlighted = comp.highlighted;
                comp.highlighted = comp.containsPoint(world.x, world.y);
                if (wasHighlighted !== comp.highlighted) {
                    needsRender = true;
                }
            }

            if (needsRender) {
                this.render();
            }
        }
    }

    /**
     * Update cursor info display
     */
    updateCursorInfo(pos) {
        const cursorInfo = document.getElementById('cursor-info');
        if (cursorInfo) {
            cursorInfo.textContent = `X: ${pos.x.toFixed(0)}, Y: ${pos.y.toFixed(0)}`;
        }
    }

    /**
     * Mouse up handler
     */
    onMouseUp(e) {
        this.isDragging = false;
        this.isPanning = false;

        if (this.currentTool === 'select') {
            this.canvas.style.cursor = 'default';
        }
    }

    /**
     * Mouse wheel handler
     */
    onWheel(e) {
        e.preventDefault();

        const rect = this.canvas.getBoundingClientRect();
        const mouseX = e.clientX - rect.left;
        const mouseY = e.clientY - rect.top;

        // Zoom centered on mouse position
        const zoomFactor = e.deltaY > 0 ? 0.9 : 1.1;
        const newZoom = Math.max(this.minZoom, Math.min(this.maxZoom, this.zoom * zoomFactor));

        if (newZoom !== this.zoom) {
            const worldBefore = this.screenToWorld(mouseX, mouseY);
            this.zoom = newZoom;
            const worldAfter = this.screenToWorld(mouseX, mouseY);

            this.offset.x += (worldAfter.x - worldBefore.x) * this.zoom;
            this.offset.y += (worldAfter.y - worldBefore.y) * this.zoom;
        }

        this.render();
    }

    /**
     * Double click handler
     */
    onDoubleClick(e) {
        if (!this.circuit) return;

        const rect = this.canvas.getBoundingClientRect();
        const world = this.screenToWorld(e.clientX - rect.left, e.clientY - rect.top);

        // Rotate component
        const comp = this.circuit.findComponentAt(world.x, world.y);
        if (comp) {
            comp.rotate();
            this.circuit.updateComponentNodes(comp);
            this.render();
        }
    }

    /**
     * Context menu handler
     */
    onContextMenu(e) {
        e.preventDefault();
        this.cancelCurrentAction();
        this.setTool('select');
    }

    /**
     * Keyboard handler
     */
    onKeyDown(e) {
        // Escape to cancel
        if (e.key === 'Escape') {
            this.cancelCurrentAction();
            this.setTool('select');
        }

        // Delete selected component
        if (e.key === 'Delete' || e.key === 'Backspace') {
            if (this.selectedComponent && this.circuit) {
                this.circuit.removeComponent(this.selectedComponent.id);
                this.selectedComponent = null;
                if (this.app) {
                    this.app.onComponentDeselected();
                    this.app.updateCounts();
                }
                this.render();
            }
        }

        // Rotate selected component
        if (e.key === 'r' || e.key === 'R') {
            if (this.selectedComponent) {
                this.selectedComponent.rotate();
                this.circuit.updateComponentNodes(this.selectedComponent);
                this.render();
            }
        }

        // Grid toggle
        if (e.key === 'g' || e.key === 'G') {
            this.showGrid = !this.showGrid;
            this.render();
        }

        // Snap toggle
        if (e.key === 's' && !e.ctrlKey && !e.metaKey) {
            this.snapToGrid = !this.snapToGrid;
        }
    }

    /**
     * Render the canvas
     */
    render() {
        const ctx = this.ctx;
        const width = this.canvas.width;
        const height = this.canvas.height;

        // Clear
        ctx.fillStyle = '#1a1a2e';
        ctx.fillRect(0, 0, width, height);

        // Save state
        ctx.save();

        // Apply transform
        ctx.translate(this.offset.x, this.offset.y);
        ctx.scale(this.zoom, this.zoom);

        // Draw grid
        if (this.showGrid) {
            this.drawGrid(ctx);
        }

        // Draw circuit
        if (this.circuit) {
            this.circuit.draw(ctx, {
                showVoltages: this.app?.showVoltages || false,
                showCurrent: this.app?.showCurrent || false
            });
        }

        // Draw probes
        this.drawProbes(ctx);

        // Draw placing component preview
        if (this.placingComponent) {
            ctx.globalAlpha = 0.6;
            this.placingComponent.draw(ctx);
            ctx.globalAlpha = 1;
        }

        // Restore state
        ctx.restore();
    }

    /**
     * Draw background grid
     */
    drawGrid(ctx) {
        const viewBounds = this.getViewBounds();

        ctx.strokeStyle = '#2a2a4e';
        ctx.lineWidth = 0.5;

        // Calculate grid start/end
        const startX = Math.floor(viewBounds.left / this.gridSize) * this.gridSize;
        const startY = Math.floor(viewBounds.top / this.gridSize) * this.gridSize;
        const endX = Math.ceil(viewBounds.right / this.gridSize) * this.gridSize;
        const endY = Math.ceil(viewBounds.bottom / this.gridSize) * this.gridSize;

        ctx.beginPath();

        // Vertical lines
        for (let x = startX; x <= endX; x += this.gridSize) {
            ctx.moveTo(x, startY);
            ctx.lineTo(x, endY);
        }

        // Horizontal lines
        for (let y = startY; y <= endY; y += this.gridSize) {
            ctx.moveTo(startX, y);
            ctx.lineTo(endX, y);
        }

        ctx.stroke();

        // Draw origin marker
        ctx.strokeStyle = '#3a3a5e';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(-20, 0);
        ctx.lineTo(20, 0);
        ctx.moveTo(0, -20);
        ctx.lineTo(0, 20);
        ctx.stroke();
    }

    /**
     * Draw probes
     */
    drawProbes(ctx) {
        for (const probe of this.probes) {
            ctx.fillStyle = probe.color;
            ctx.strokeStyle = '#ffffff';
            ctx.lineWidth = 2;

            // Probe marker
            ctx.beginPath();
            ctx.arc(probe.x, probe.y, 8, 0, Math.PI * 2);
            ctx.fill();
            ctx.stroke();

            // Probe label
            ctx.fillStyle = probe.color;
            ctx.font = 'bold 10px sans-serif';
            ctx.textAlign = 'left';
            ctx.fillText(`P${this.probes.indexOf(probe) + 1}`, probe.x + 12, probe.y + 4);
        }
    }

    /**
     * Get visible area in world coordinates
     */
    getViewBounds() {
        const topLeft = this.screenToWorld(0, 0);
        const bottomRight = this.screenToWorld(this.canvas.width, this.canvas.height);
        return {
            left: topLeft.x,
            top: topLeft.y,
            right: bottomRight.x,
            bottom: bottomRight.y
        };
    }

    /**
     * Center view on origin
     */
    centerView() {
        this.offset.x = this.canvas.width / 2;
        this.offset.y = this.canvas.height / 2;
        this.zoom = 1;
        this.render();
    }

    /**
     * Get probe voltage readings
     */
    getProbeReadings() {
        if (!this.circuit) return [];

        return this.probes.map(probe => {
            const node = this.circuit.nodeManager.getNode(probe.nodeId);
            return {
                id: probe.id,
                color: probe.color,
                voltage: node ? node.voltage : 0
            };
        });
    }
}

// Export
window.CanvasManager = CanvasManager;
