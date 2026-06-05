// ==================== 状态 ====================
let connected = false;
let deviceId = '';

// ==================== DOM 引用 ====================
const $ = (id) => document.getElementById(id);

const iptHost   = $('ipt-host');
const iptPort   = $('ipt-port');
const iptDevice = $('ipt-device');
const btnConn   = $('btn-connect');
const connSt    = $('conn-status');

const valTemp  = $('val-temp');
const valLight = $('val-light');
const valLed   = $('val-led');
const valFan   = $('val-fan');
const valMode  = $('val-mode');
const valPower = $('val-power');

const logBox = $('log-box');

// ==================== 日志 ====================
function log(msg, cls) {
    const el = document.createElement('div');
    el.textContent = `[${new Date().toLocaleTimeString()}] ${msg}`;
    if (cls) el.className = cls;
    logBox.appendChild(el);
    logBox.scrollTop = logBox.scrollHeight;
}

// ==================== 连接管理 ====================
btnConn.addEventListener('click', () => {
    if (connected) {
        window.host.disconnect();
        return;
    }
    deviceId = iptDevice.value.trim();
    window.host.connect(iptHost.value.trim(), iptPort.value.trim(), deviceId);
    log(`正在连接 ${iptHost.value}:${iptPort.value} ...`, 'info');
});

window.host.onStatus((data) => {
    connected = data.connected;
    if (connected) {
        btnConn.textContent = '断开';
        btnConn.classList.add('disconnect');
        connSt.textContent = `已连接 (${deviceId})`;
        connSt.className = 'conn-status online';
        log('MQTT 已连接', 'info');
    } else {
        btnConn.textContent = '连接';
        btnConn.classList.remove('disconnect');
        connSt.textContent = '未连接';
        connSt.className = 'conn-status offline';
        log('MQTT 已断开', 'error');
    }
});

window.host.onError((msg) => {
    log(`错误: ${msg}`, 'error');
});

// ==================== 状态更新 ====================
window.host.onMessage((data) => {
    valTemp.textContent  = data.temperature ? `${data.temperature.toFixed(1)}°C` : '--';
    valLight.textContent = data.light != null ? `${data.light} Lux` : '--';
    valMode.textContent  = data.mode === 'auto' ? '自动' : '手动';
    valPower.textContent = data.key1_lock ? 'ON' : 'OFF';

    // 继电器状态
    if (data.relay3 !== undefined) {
        valLed.textContent = data.relay3 ? 'ON' : 'OFF';
        valLed.className = 'stat-val ' + (data.relay3 ? 'on' : 'off');
    }
    if (data.relay4 !== undefined) {
        valFan.textContent = data.relay4 ? 'ON' : 'OFF';
        valFan.className = 'stat-val ' + (data.relay4 ? 'on' : 'off');
    }

    log('状态已更新');
});

// ==================== 发送命令 ====================
function sendCmd(cmd) {
    if (!connected) {
        log('未连接，无法发送命令', 'error');
        return;
    }
    window.host.send(deviceId, cmd);
    log(`命令已发送: ${JSON.stringify(cmd)}`, 'info');
}

// 继电器控制
$('cmd-led-on').addEventListener('click',  () => sendCmd({ cmd: 'set_relay', relay: 3, value: true }));
$('cmd-led-off').addEventListener('click', () => sendCmd({ cmd: 'set_relay', relay: 3, value: false }));
$('cmd-fan-on').addEventListener('click',  () => sendCmd({ cmd: 'set_relay', relay: 4, value: true }));
$('cmd-fan-off').addEventListener('click', () => sendCmd({ cmd: 'set_relay', relay: 4, value: false }));

// 模式切换
$('cmd-mode-auto').addEventListener('click',   () => sendCmd({ cmd: 'set_mode', mode: 'auto' }));
$('cmd-mode-manual').addEventListener('click',  () => sendCmd({ cmd: 'set_mode', mode: 'manual' }));

// 刷新状态
$('cmd-get-status').addEventListener('click', () => sendCmd({ cmd: 'get_status' }));

// 阈值设置
$('cmd-set-temp-th').addEventListener('click',  () => {
    const val = parseFloat($('ipt-temp-th').value);
    if (!isNaN(val)) sendCmd({ cmd: 'set_threshold', temp: val });
});
$('cmd-set-light-th').addEventListener('click', () => {
    const val = parseInt($('ipt-light-th').value);
    if (!isNaN(val)) sendCmd({ cmd: 'set_threshold', light: val });
});
