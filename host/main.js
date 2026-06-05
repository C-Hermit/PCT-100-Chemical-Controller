const { app, BrowserWindow, ipcMain } = require('electron');
const path = require('path');
const mqtt = require('mqtt');

let win;
let mqttClient = null;

function createWindow() {
    win = new BrowserWindow({
        width: 480,
        height: 640,
        resizable: false,
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            contextIsolation: true,
            nodeIntegration: false,
        },
    });
    win.setMenuBarVisibility(false);
    win.loadFile('index.html');
}

// ─── MQTT 连接 ─────────────────────────────────────────

function mqttConnect(host, port, deviceId) {
    if (mqttClient) mqttClient.end(true);

    const url = `mqtt://${host}:${port}`;
    mqttClient = mqtt.connect(url, {
        protocolVersion: 4,           // MQTT 3.1.1 (兼容 EMQX)
        clientId: `pct100_host_${Date.now()}`,
        reconnectPeriod: 5000,
    });

    mqttClient.on('connect', () => {
        const topic = `chemctrl/${deviceId}/status`;
        mqttClient.subscribe(topic, { qos: 0 });
        win.webContents.send('mqtt-status', { connected: true });
    });

    mqttClient.on('message', (topic, payload) => {
        try {
            const data = JSON.parse(payload.toString());
            win.webContents.send('mqtt-message', data);
        } catch (e) {
            // 非 JSON 消息，忽略
        }
    });

    mqttClient.on('close', () => {
        win.webContents.send('mqtt-status', { connected: false });
    });

    mqttClient.on('error', (err) => {
        win.webContents.send('mqtt-error', err.message);
    });
}

function mqttDisconnect() {
    if (mqttClient) {
        mqttClient.end(true);
        mqttClient = null;
    }
}

function mqttSendCommand(deviceId, command) {
    if (!mqttClient || !mqttClient.connected) return;
    const topic = `chemctrl/${deviceId}/command`;
    mqttClient.publish(topic, JSON.stringify(command), { qos: 0 });
}

// ─── IPC 处理 ──────────────────────────────────────────

ipcMain.handle('mqtt-connect', (_, host, port, deviceId) => {
    mqttConnect(host, port, deviceId);
});

ipcMain.handle('mqtt-disconnect', () => {
    mqttDisconnect();
});

ipcMain.handle('mqtt-send', (_, deviceId, command) => {
    mqttSendCommand(deviceId, command);
});

// ─── 生命周期 ──────────────────────────────────────────

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
    mqttDisconnect();
    if (process.platform !== 'darwin') app.quit();
});
