/**
 * Oscilloscope display
 */

class Oscilloscope {
    constructor(canvasId) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');

        // Display settings
        this.timeDiv = 0.001;    // 1ms per division
        this.voltDiv = 1;        // 1V per division
        this.divisions = 10;     // Number of horizontal divisions
        this.vertDivisions = 8;  // Number of vertical divisions

        // Channels
        this.channels = [
            { enabled: true, color: '#ffff00', data: [], offset: 0, probeId: null },
            { enabled: false, color: '#00ffff', data: [], offset: 0, probeId: null }
        ];

        // Trigger settings
        this.triggerLevel = 0;
        this.triggerMode = 'auto'; // auto, normal, single

        // Data source
        this.simulationEngine = null;

        // UI elements
        this.setupControls();

        // Render
        this.render();
    }

    /**
     * Set simulation engine reference
     */
    setSimulationEngine(engine) {
        this.simulationEngine = engine;
    }

    /**
     * Setup control event listeners
     */
    setupControls() {
        // Time/div selector
        const timeSelect = document.getElementById('scope-time-div');
        if (timeSelect) {
            timeSelect.addEventListener('change', (e) => {
                this.timeDiv = parseFloat(e.target.value);
                this.render();
            });
        }

        // Volt/div selector
        const voltSelect = document.getElementById('scope-volt-div');
        if (voltSelect) {
            voltSelect.addEventListener('change', (e) => {
                this.voltDiv = parseFloat(e.target.value);
                this.render();
            });
        }

        // Channel enables
        const ch1Enable = document.getElementById('ch1-enabled');
        if (ch1Enable) {
            ch1Enable.addEventListener('change', (e) => {
                this.channels[0].enabled = e.target.checked;
                this.render();
            });
        }

        const ch2Enable = document.getElementById('ch2-enabled');
        if (ch2Enable) {
            ch2Enable.addEventListener('change', (e) => {
                this.channels[1].enabled = e.target.checked;
                this.render();
            });
        }
    }

    /**
     * Set probe for a channel
     */
    setChannelProbe(channelIndex, probeId) {
        if (channelIndex >= 0 && channelIndex < this.channels.length) {
            this.channels[channelIndex].probeId = probeId;
        }
    }

    /**
     * Update oscilloscope data from simulation
     */
    update(probes) {
        if (!this.simulationEngine) return;

        // Get data for each channel
        for (let i = 0; i < this.channels.length; i++) {
            const channel = this.channels[i];
            if (!channel.enabled || !channel.probeId) continue;

            const history = this.simulationEngine.getProbeHistory(channel.probeId);
            if (history && history.values) {
                channel.data = history.values.slice(-this.getMaxSamples());
            }
        }

        // Update channel value displays
        this.updateChannelValues(probes);
        this.render();
    }

    /**
     * Update channel voltage displays
     */
    updateChannelValues(probes) {
        const ch1Value = document.getElementById('ch1-value');
        const ch2Value = document.getElementById('ch2-value');

        if (probes && probes.length > 0 && ch1Value) {
            ch1Value.textContent = `${probes[0].voltage.toFixed(2)} V`;
            this.channels[0].probeId = `node_${probes[0].nodeId || probes[0].id}`;
        }

        if (probes && probes.length > 1 && ch2Value) {
            ch2Value.textContent = `${probes[1].voltage.toFixed(2)} V`;
            this.channels[1].probeId = `node_${probes[1].nodeId || probes[1].id}`;
        }
    }

    /**
     * Get maximum number of samples to display
     */
    getMaxSamples() {
        return this.canvas.width;
    }

    /**
     * Render the oscilloscope display
     */
    render() {
        const ctx = this.ctx;
        const width = this.canvas.width;
        const height = this.canvas.height;

        // Clear with black background
        ctx.fillStyle = '#000000';
        ctx.fillRect(0, 0, width, height);

        // Draw grid
        this.drawGrid(ctx, width, height);

        // Draw channels
        for (let i = 0; i < this.channels.length; i++) {
            if (this.channels[i].enabled) {
                this.drawChannel(ctx, width, height, i);
            }
        }

        // Draw trigger level
        this.drawTrigger(ctx, width, height);
    }

    /**
     * Draw oscilloscope grid
     */
    drawGrid(ctx, width, height) {
        const divWidth = width / this.divisions;
        const divHeight = height / this.vertDivisions;

        // Major grid lines
        ctx.strokeStyle = '#333333';
        ctx.lineWidth = 1;

        ctx.beginPath();

        // Vertical lines
        for (let i = 0; i <= this.divisions; i++) {
            const x = i * divWidth;
            ctx.moveTo(x, 0);
            ctx.lineTo(x, height);
        }

        // Horizontal lines
        for (let i = 0; i <= this.vertDivisions; i++) {
            const y = i * divHeight;
            ctx.moveTo(0, y);
            ctx.lineTo(width, y);
        }

        ctx.stroke();

        // Center lines (brighter)
        ctx.strokeStyle = '#555555';
        ctx.lineWidth = 1;

        ctx.beginPath();
        // Vertical center
        ctx.moveTo(width / 2, 0);
        ctx.lineTo(width / 2, height);
        // Horizontal center
        ctx.moveTo(0, height / 2);
        ctx.lineTo(width, height / 2);
        ctx.stroke();

        // Minor grid dots
        ctx.fillStyle = '#222222';
        const minorDiv = 5;
        for (let i = 0; i < this.divisions * minorDiv; i++) {
            for (let j = 0; j < this.vertDivisions * minorDiv; j++) {
                const x = (i / minorDiv) * divWidth;
                const y = (j / minorDiv) * divHeight;
                ctx.fillRect(x, y, 1, 1);
            }
        }
    }

    /**
     * Draw a channel trace
     */
    drawChannel(ctx, width, height, channelIndex) {
        const channel = this.channels[channelIndex];
        const data = channel.data;

        if (!data || data.length < 2) return;

        const centerY = height / 2 + channel.offset;
        const pixelsPerVolt = (height / this.vertDivisions) / this.voltDiv;

        ctx.strokeStyle = channel.color;
        ctx.lineWidth = 1.5;
        ctx.lineJoin = 'round';

        ctx.beginPath();

        const step = Math.max(1, Math.floor(data.length / width));
        let x = 0;

        for (let i = 0; i < data.length; i += step) {
            const voltage = data[i];
            const y = centerY - voltage * pixelsPerVolt;

            if (i === 0) {
                ctx.moveTo(x, y);
            } else {
                ctx.lineTo(x, y);
            }
            x++;
        }

        ctx.stroke();
    }

    /**
     * Draw trigger level indicator
     */
    drawTrigger(ctx, width, height) {
        const centerY = height / 2;
        const pixelsPerVolt = (height / this.vertDivisions) / this.voltDiv;
        const triggerY = centerY - this.triggerLevel * pixelsPerVolt;

        ctx.strokeStyle = '#ff8800';
        ctx.lineWidth = 1;
        ctx.setLineDash([5, 5]);

        ctx.beginPath();
        ctx.moveTo(0, triggerY);
        ctx.lineTo(20, triggerY);
        ctx.stroke();

        ctx.setLineDash([]);

        // Trigger arrow
        ctx.fillStyle = '#ff8800';
        ctx.beginPath();
        ctx.moveTo(0, triggerY);
        ctx.lineTo(8, triggerY - 4);
        ctx.lineTo(8, triggerY + 4);
        ctx.closePath();
        ctx.fill();
    }

    /**
     * Set trigger level
     */
    setTriggerLevel(level) {
        this.triggerLevel = level;
        this.render();
    }

    /**
     * Clear all channel data
     */
    clear() {
        for (const channel of this.channels) {
            channel.data = [];
        }
        this.render();
    }
}

// Export
window.Oscilloscope = Oscilloscope;
