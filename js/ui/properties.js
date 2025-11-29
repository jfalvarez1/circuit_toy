/**
 * Properties Panel - displays and edits component properties
 */

class PropertiesPanel {
    constructor() {
        this.panel = document.getElementById('properties-content');
        this.currentComponent = null;
        this.onPropertyChange = null;
    }

    /**
     * Show properties for a component
     */
    showComponent(component) {
        this.currentComponent = component;
        this.render();
    }

    /**
     * Clear the properties panel
     */
    clear() {
        this.currentComponent = null;
        this.panel.innerHTML = '<p class="placeholder">Select a component to edit its properties</p>';
    }

    /**
     * Render the properties panel
     */
    render() {
        if (!this.currentComponent) {
            this.clear();
            return;
        }

        const comp = this.currentComponent;
        const ComponentClass = ComponentRegistry.getClass(comp.type);
        const propDefs = ComponentClass ? ComponentClass.getPropertyDefinitions() : [];

        let html = `
            <div class="property-group">
                <div class="property-row">
                    <label>Type:</label>
                    <span style="color: #00d9ff">${ComponentClass ? ComponentClass.getDisplayName() : comp.type}</span>
                </div>
                <div class="property-row">
                    <label>Label:</label>
                    <input type="text" id="prop-label" value="${comp.label || ''}" />
                </div>
            </div>
        `;

        if (propDefs.length > 0) {
            html += '<div class="property-group">';
            html += '<h4>Parameters</h4>';

            for (const def of propDefs) {
                const value = comp.properties[def.name];
                const displayValue = this.formatPropertyValue(value, def);

                html += `
                    <div class="property-row">
                        <label>${def.label}:</label>
                        <input type="${def.type === 'number' ? 'text' : def.type}"
                               id="prop-${def.name}"
                               value="${displayValue}"
                               data-property="${def.name}"
                               data-type="${def.type}" />
                        <span class="unit">${def.unit || ''}</span>
                    </div>
                `;
            }

            html += '</div>';
        }

        // Position info
        html += `
            <div class="property-group">
                <h4>Position</h4>
                <div class="property-row">
                    <label>X:</label>
                    <input type="number" id="prop-x" value="${comp.x}" step="20" />
                </div>
                <div class="property-row">
                    <label>Y:</label>
                    <input type="number" id="prop-y" value="${comp.y}" step="20" />
                </div>
                <div class="property-row">
                    <label>Rotation:</label>
                    <select id="prop-rotation">
                        <option value="0" ${comp.rotation === 0 ? 'selected' : ''}>0 deg</option>
                        <option value="90" ${comp.rotation === 90 ? 'selected' : ''}>90 deg</option>
                        <option value="180" ${comp.rotation === 180 ? 'selected' : ''}>180 deg</option>
                        <option value="270" ${comp.rotation === 270 ? 'selected' : ''}>270 deg</option>
                    </select>
                </div>
            </div>
        `;

        this.panel.innerHTML = html;

        // Setup change handlers
        this.setupChangeHandlers();
    }

    /**
     * Format a property value for display
     */
    formatPropertyValue(value, def) {
        if (def.type !== 'number') return value;

        // Use engineering notation for very small or large values
        const absVal = Math.abs(value);
        if (absVal !== 0 && (absVal < 0.001 || absVal >= 1e6)) {
            return value.toExponential(3);
        }

        return value.toString();
    }

    /**
     * Parse a property value from input
     */
    parsePropertyValue(inputValue, def) {
        if (def.type !== 'number') return inputValue;

        // Handle engineering notation (e.g., 1k, 1M, 1u, 1n)
        let value = inputValue.trim();
        const multipliers = {
            'T': 1e12, 'G': 1e9, 'M': 1e6, 'k': 1e3, 'K': 1e3,
            'm': 1e-3, 'u': 1e-6, 'n': 1e-9, 'p': 1e-12
        };

        const lastChar = value.slice(-1);
        if (multipliers[lastChar]) {
            value = parseFloat(value.slice(0, -1)) * multipliers[lastChar];
        } else {
            value = parseFloat(value);
        }

        if (isNaN(value)) return null;

        // Apply limits
        if (def.min !== undefined) value = Math.max(def.min, value);
        if (def.max !== undefined) value = Math.min(def.max, value);

        return value;
    }

    /**
     * Setup change handlers for inputs
     */
    setupChangeHandlers() {
        const comp = this.currentComponent;
        if (!comp) return;

        const ComponentClass = ComponentRegistry.getClass(comp.type);
        const propDefs = ComponentClass ? ComponentClass.getPropertyDefinitions() : [];
        const propDefMap = {};
        propDefs.forEach(def => propDefMap[def.name] = def);

        // Label
        const labelInput = document.getElementById('prop-label');
        if (labelInput) {
            labelInput.addEventListener('change', (e) => {
                comp.label = e.target.value;
                this.notifyChange();
            });
        }

        // Component properties
        const propInputs = document.querySelectorAll('[data-property]');
        propInputs.forEach(input => {
            input.addEventListener('change', (e) => {
                const propName = e.target.dataset.property;
                const def = propDefMap[propName];
                if (def) {
                    const value = this.parsePropertyValue(e.target.value, def);
                    if (value !== null) {
                        comp.properties[propName] = value;
                        // Update display with normalized value
                        e.target.value = this.formatPropertyValue(value, def);
                        this.notifyChange();
                    }
                }
            });
        });

        // Position X
        const xInput = document.getElementById('prop-x');
        if (xInput) {
            xInput.addEventListener('change', (e) => {
                comp.x = parseFloat(e.target.value) || 0;
                this.notifyChange();
            });
        }

        // Position Y
        const yInput = document.getElementById('prop-y');
        if (yInput) {
            yInput.addEventListener('change', (e) => {
                comp.y = parseFloat(e.target.value) || 0;
                this.notifyChange();
            });
        }

        // Rotation
        const rotInput = document.getElementById('prop-rotation');
        if (rotInput) {
            rotInput.addEventListener('change', (e) => {
                comp.rotation = parseInt(e.target.value) || 0;
                this.notifyChange();
            });
        }
    }

    /**
     * Notify that a property changed
     */
    notifyChange() {
        if (this.onPropertyChange) {
            this.onPropertyChange(this.currentComponent);
        }
    }
}

// Export
window.PropertiesPanel = PropertiesPanel;
