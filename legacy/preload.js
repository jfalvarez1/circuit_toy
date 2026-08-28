const { contextBridge, ipcRenderer } = require('electron');

// Expose protected methods that allow the renderer process to use
// the ipcRenderer without exposing the entire object
contextBridge.exposeInMainWorld('electronAPI', {
    // File operations
    saveFile: (data) => ipcRenderer.invoke('save-file', data),
    getFilePath: () => ipcRenderer.invoke('get-file-path'),

    // Menu event listeners
    onMenuNew: (callback) => ipcRenderer.on('menu-new', callback),
    onMenuUndo: (callback) => ipcRenderer.on('menu-undo', callback),
    onMenuRedo: (callback) => ipcRenderer.on('menu-redo', callback),
    onMenuCopy: (callback) => ipcRenderer.on('menu-copy', callback),
    onMenuCut: (callback) => ipcRenderer.on('menu-cut', callback),
    onMenuPaste: (callback) => ipcRenderer.on('menu-paste', callback),
    onMenuDuplicate: (callback) => ipcRenderer.on('menu-duplicate', callback),
    onMenuDelete: (callback) => ipcRenderer.on('menu-delete', callback),
    onMenuRun: (callback) => ipcRenderer.on('menu-run', callback),
    onMenuPause: (callback) => ipcRenderer.on('menu-pause', callback),
    onMenuStep: (callback) => ipcRenderer.on('menu-step', callback),
    onMenuReset: (callback) => ipcRenderer.on('menu-reset', callback),
    onMenuToggleGrid: (callback) => ipcRenderer.on('menu-toggle-grid', callback),
    onMenuToggleSnap: (callback) => ipcRenderer.on('menu-toggle-snap', callback),
    onMenuZoomIn: (callback) => ipcRenderer.on('menu-zoom-in', callback),
    onMenuZoomOut: (callback) => ipcRenderer.on('menu-zoom-out', callback),
    onMenuZoomReset: (callback) => ipcRenderer.on('menu-zoom-reset', callback),
    onMenuShortcuts: (callback) => ipcRenderer.on('menu-shortcuts', callback),

    // File events
    onFileOpened: (callback) => ipcRenderer.on('file-opened', (event, data) => callback(data)),
    onRequestSaveData: (callback) => ipcRenderer.on('request-save-data', callback),

    // Platform info
    platform: process.platform
});
