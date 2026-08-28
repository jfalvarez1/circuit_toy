const { app, BrowserWindow, ipcMain, dialog, Menu } = require('electron');
const path = require('path');
const fs = require('fs');

let mainWindow;

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 1400,
        height: 900,
        minWidth: 1024,
        minHeight: 768,
        webPreferences: {
            nodeIntegration: false,
            contextIsolation: true,
            preload: path.join(__dirname, 'preload.js')
        },
        icon: path.join(__dirname, 'assets', 'icon.png'),
        title: 'Circuit Playground'
    });

    mainWindow.loadFile('index.html');

    // Create application menu
    const template = [
        {
            label: 'File',
            submenu: [
                {
                    label: 'New Circuit',
                    accelerator: 'CmdOrCtrl+N',
                    click: () => mainWindow.webContents.send('menu-new')
                },
                {
                    label: 'Open...',
                    accelerator: 'CmdOrCtrl+O',
                    click: () => handleOpen()
                },
                {
                    label: 'Save',
                    accelerator: 'CmdOrCtrl+S',
                    click: () => handleSave()
                },
                {
                    label: 'Save As...',
                    accelerator: 'CmdOrCtrl+Shift+S',
                    click: () => handleSaveAs()
                },
                { type: 'separator' },
                {
                    label: 'Exit',
                    accelerator: 'Alt+F4',
                    click: () => app.quit()
                }
            ]
        },
        {
            label: 'Edit',
            submenu: [
                {
                    label: 'Undo',
                    accelerator: 'CmdOrCtrl+Z',
                    click: () => mainWindow.webContents.send('menu-undo')
                },
                {
                    label: 'Redo',
                    accelerator: 'CmdOrCtrl+Shift+Z',
                    click: () => mainWindow.webContents.send('menu-redo')
                },
                { type: 'separator' },
                {
                    label: 'Copy',
                    accelerator: 'CmdOrCtrl+C',
                    click: () => mainWindow.webContents.send('menu-copy')
                },
                {
                    label: 'Cut',
                    accelerator: 'CmdOrCtrl+X',
                    click: () => mainWindow.webContents.send('menu-cut')
                },
                {
                    label: 'Paste',
                    accelerator: 'CmdOrCtrl+V',
                    click: () => mainWindow.webContents.send('menu-paste')
                },
                {
                    label: 'Duplicate',
                    accelerator: 'CmdOrCtrl+D',
                    click: () => mainWindow.webContents.send('menu-duplicate')
                },
                { type: 'separator' },
                {
                    label: 'Delete',
                    accelerator: 'Delete',
                    click: () => mainWindow.webContents.send('menu-delete')
                }
            ]
        },
        {
            label: 'Simulation',
            submenu: [
                {
                    label: 'Run',
                    accelerator: 'F5',
                    click: () => mainWindow.webContents.send('menu-run')
                },
                {
                    label: 'Pause',
                    accelerator: 'F6',
                    click: () => mainWindow.webContents.send('menu-pause')
                },
                {
                    label: 'Step',
                    accelerator: 'F10',
                    click: () => mainWindow.webContents.send('menu-step')
                },
                {
                    label: 'Reset',
                    accelerator: 'F7',
                    click: () => mainWindow.webContents.send('menu-reset')
                }
            ]
        },
        {
            label: 'View',
            submenu: [
                {
                    label: 'Toggle Grid',
                    accelerator: 'G',
                    click: () => mainWindow.webContents.send('menu-toggle-grid')
                },
                {
                    label: 'Toggle Snap',
                    accelerator: 'S',
                    click: () => mainWindow.webContents.send('menu-toggle-snap')
                },
                { type: 'separator' },
                {
                    label: 'Zoom In',
                    accelerator: 'CmdOrCtrl+Plus',
                    click: () => mainWindow.webContents.send('menu-zoom-in')
                },
                {
                    label: 'Zoom Out',
                    accelerator: 'CmdOrCtrl+-',
                    click: () => mainWindow.webContents.send('menu-zoom-out')
                },
                {
                    label: 'Reset Zoom',
                    accelerator: 'CmdOrCtrl+0',
                    click: () => mainWindow.webContents.send('menu-zoom-reset')
                },
                { type: 'separator' },
                {
                    label: 'Toggle Developer Tools',
                    accelerator: 'F12',
                    click: () => mainWindow.webContents.toggleDevTools()
                }
            ]
        },
        {
            label: 'Help',
            submenu: [
                {
                    label: 'Keyboard Shortcuts',
                    accelerator: 'F1',
                    click: () => mainWindow.webContents.send('menu-shortcuts')
                },
                {
                    label: 'About',
                    click: () => {
                        dialog.showMessageBox(mainWindow, {
                            type: 'info',
                            title: 'About Circuit Playground',
                            message: 'Circuit Playground v1.0.0',
                            detail: 'An interactive circuit simulator.\n\nInspired by The Powder Toy.'
                        });
                    }
                }
            ]
        }
    ];

    const menu = Menu.buildFromTemplate(template);
    Menu.setApplicationMenu(menu);
}

let currentFilePath = null;

async function handleOpen() {
    const result = await dialog.showOpenDialog(mainWindow, {
        properties: ['openFile'],
        filters: [
            { name: 'Circuit Files', extensions: ['circuit', 'json'] },
            { name: 'All Files', extensions: ['*'] }
        ]
    });

    if (!result.canceled && result.filePaths.length > 0) {
        const filePath = result.filePaths[0];
        try {
            const data = fs.readFileSync(filePath, 'utf8');
            currentFilePath = filePath;
            mainWindow.webContents.send('file-opened', { path: filePath, data: data });
            mainWindow.setTitle(`Circuit Playground - ${path.basename(filePath)}`);
        } catch (err) {
            dialog.showErrorBox('Error', `Failed to open file: ${err.message}`);
        }
    }
}

async function handleSave() {
    if (currentFilePath) {
        mainWindow.webContents.send('request-save-data');
    } else {
        handleSaveAs();
    }
}

async function handleSaveAs() {
    const result = await dialog.showSaveDialog(mainWindow, {
        filters: [
            { name: 'Circuit Files', extensions: ['circuit'] },
            { name: 'JSON Files', extensions: ['json'] }
        ]
    });

    if (!result.canceled && result.filePath) {
        currentFilePath = result.filePath;
        mainWindow.webContents.send('request-save-data');
        mainWindow.setTitle(`Circuit Playground - ${path.basename(result.filePath)}`);
    }
}

// IPC handlers
ipcMain.handle('save-file', async (event, data) => {
    if (currentFilePath) {
        try {
            fs.writeFileSync(currentFilePath, data, 'utf8');
            return { success: true };
        } catch (err) {
            return { success: false, error: err.message };
        }
    }
    return { success: false, error: 'No file path set' };
});

ipcMain.handle('get-file-path', () => {
    return currentFilePath;
});

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') {
        app.quit();
    }
});

app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
        createWindow();
    }
});
