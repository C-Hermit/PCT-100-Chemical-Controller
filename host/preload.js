const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('host', {
    // MQTT 连接管理
    connect: (host, port, deviceId) =>
        ipcRenderer.invoke('mqtt-connect', host, port, deviceId),

    disconnect: () =>
        ipcRenderer.invoke('mqtt-disconnect'),

    send: (deviceId, command) =>
        ipcRenderer.invoke('mqtt-send', deviceId, command),

    // 事件监听（单向推送到渲染进程）
    onStatus: (cb) => {
        ipcRenderer.on('mqtt-status', (_, data) => cb(data));
    },

    onMessage: (cb) => {
        ipcRenderer.on('mqtt-message', (_, data) => cb(data));
    },

    onError: (cb) => {
        ipcRenderer.on('mqtt-error', (_, msg) => cb(msg));
    },
});
