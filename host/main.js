const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const mqtt = require('mqtt');

let win;
let mqttClient = null;
let pollTimer = null;
let currentDeviceId = '';

const POLL_INTERVAL = 5000; // 5s

function createWindow() {
    win = new BrowserWindow({
        width: 1200,
        height: 750,
        minWidth: 960,
        minHeight: 600,
        resizable: true,
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            contextIsolation: true,
            nodeIntegration: false,
        },
    });
    win.setMenuBarVisibility(false);
    win.loadFile('index.html');
}

// ─── MQTT ──────────────────────────────────────────────

function mqttConnect(host, port, deviceId, user, password) {
    if (mqttClient) mqttClient.end(true);
    stopPolling();

    currentDeviceId = deviceId;
    const url = `mqtt://${host}:${port}`;
    const opts = {
        protocolVersion: 4,
        clientId: `pct100_host_${Date.now()}`,
        reconnectPeriod: 5000,
    };
    if (user)     opts.username = user;
    if (password) opts.password = password;
    mqttClient = mqtt.connect(url, opts);

    mqttClient.on('connect', () => {
        const topic = `chemctrl/${deviceId}/status`;
        mqttClient.subscribe(topic, { qos: 0 });
        win.webContents.send('mqtt-status', { connected: true });
        startPolling();
    });

    mqttClient.on('message', (topic, payload) => {
        try {
            const data = JSON.parse(payload.toString());
            win.webContents.send('mqtt-message', data);
        } catch (e) {
            // non-JSON, ignore
        }
    });

    mqttClient.on('close', () => {
        stopPolling();
        win.webContents.send('mqtt-status', { connected: false });
    });

    mqttClient.on('error', (err) => {
        if (win && !win.isDestroyed()) {
            win.webContents.send('mqtt-error', err.message);
        }
    });
}

function mqttDisconnect() {
    stopPolling();
    if (mqttClient) {
        mqttClient.removeAllListeners();
        mqttClient.end(true);
        mqttClient = null;
    }
}

function mqttSendCommand(deviceId, command) {
    if (!mqttClient || !mqttClient.connected) return;
    const topic = `chemctrl/${deviceId}/command`;
    mqttClient.publish(topic, JSON.stringify(command), { qos: 0 });
}

// ─── Polling ───────────────────────────────────────────

function startPolling() {
    stopPolling();
    pollTimer = setInterval(() => {
        if (mqttClient && mqttClient.connected && currentDeviceId) {
            mqttSendCommand(currentDeviceId, { cmd: 'get_status' });
        }
    }, POLL_INTERVAL);
}

function stopPolling() {
    if (pollTimer) {
        clearInterval(pollTimer);
        pollTimer = null;
    }
}

// ─── IPC ───────────────────────────────────────────────

ipcMain.handle('mqtt-connect', (_, host, port, deviceId, user, password) => {
    mqttConnect(host, port, deviceId, user, password);
});

ipcMain.handle('mqtt-disconnect', () => {
    mqttDisconnect();
});

ipcMain.handle('mqtt-send', (_, deviceId, command) => {
    mqttSendCommand(deviceId, command);
});

// ─── Lifecycle ─────────────────────────────────────────

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
    mqttDisconnect();
    if (process.platform !== 'darwin') app.quit();
});
