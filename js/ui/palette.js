/**
 * Component Palette - handles component and tool selection
 */

class ComponentPalette {
    constructor(canvasManager) {
        this.canvasManager = canvasManager;
        this.selectedItem = null;

        this.setupEventListeners();
    }

    /**
     * Setup event listeners for palette items
     */
    setupEventListeners() {
        // Component items
        const componentItems = document.querySelectorAll('.component-item[data-component]');
        componentItems.forEach(item => {
            item.addEventListener('click', (e) => {
                this.onComponentClick(item);
            });
        });

        // Tool items
        const toolItems = document.querySelectorAll('.component-item[data-tool]');
        toolItems.forEach(item => {
            item.addEventListener('click', (e) => {
                this.onToolClick(item);
            });
        });
    }

    /**
     * Handle component selection
     */
    onComponentClick(item) {
        const componentType = item.dataset.component;

        // Update selection state
        this.clearSelection();
        item.classList.add('selected');
        this.selectedItem = item;

        // Start placing component
        this.canvasManager.startPlacingComponent(componentType);
    }

    /**
     * Handle tool selection
     */
    onToolClick(item) {
        const tool = item.dataset.tool;

        // Update selection state
        this.clearSelection();
        item.classList.add('selected');
        this.selectedItem = item;

        // Set tool
        this.canvasManager.setTool(tool);
    }

    /**
     * Clear selection
     */
    clearSelection() {
        if (this.selectedItem) {
            this.selectedItem.classList.remove('selected');
            this.selectedItem = null;
        }

        // Also clear any other selected items
        document.querySelectorAll('.component-item.selected').forEach(item => {
            item.classList.remove('selected');
        });
    }

    /**
     * Select the default tool (select)
     */
    selectDefaultTool() {
        const selectItem = document.querySelector('.component-item[data-tool="select"]');
        if (selectItem) {
            this.onToolClick(selectItem);
        }
    }
}

// Export
window.ComponentPalette = ComponentPalette;
