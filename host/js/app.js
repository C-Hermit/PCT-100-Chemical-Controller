// ==================== 状态 ====================
let connected = false;
let deviceId = '';
let deviceAlive = false;
let deviceHeartbeat = null;
let suppressToggle = false;
let currentMode = 'auto';
let powerOn = false;

const DEVICE_TIMEOUT = 12000;

// ==================== DOM ====================
const $ = (id) => document.getElementById(id);

const pageDisplay  = $('page-display');
const pageSettings = $('page-settings');

// 设置页
const iptHost   = $('ipt-host');
const iptPort   = $('ipt-port');
const iptDevice = $('ipt-device');
const iptUser   = $('ipt-user');
const iptPass   = $('ipt-pass');

const btnConnDisplay = $('btn-connect-display');

// 状态指示点
const indMqtt   = $('ind-mqtt');
const indDevice = $('ind-device');

// 总闸
const powerSection = $('power-section');
const powerBadge   = $('power-badge');
const powerBody    = $('power-body');

// 模式
const valMode  = $('val-mode');
const toggleMode = $('toggle-mode');

// 传感器
const valTemp  = $('val-temp');
const valLight = $('val-light');

// 输出
const cardLed = $('card-led');
const cardFan = $('card-fan');
const valLed  = $('val-led');
const valFan  = $('val-fan');
const toggleLed  = $('toggle-led');
const toggleFan  = $('toggle-fan');
const hintLed = $('hint-led');
const hintFan = $('hint-fan');

// 阈值
const curTempTh  = $('cur-temp-th');
const curLightTh = $('cur-light-th');
const iptTempTh  = $('ipt-temp-th');
const iptLightTh = $('ipt-light-th');

// 日志
const logBox = $('log-box');
const btnLog = $('btn-toggle-log');

// ==================== 日志 ====================
function log(msg, cls) {
    const el = document.createElement('div');
    el.textContent = `[${new Date().toLocaleTimeString()}] ${msg}`;
    if (cls) el.className = cls;
    logBox.appendChild(el);
    logBox.scrollTop = logBox.scrollHeight;
}

// ==================== 页面切换 ====================
$('btn-settings').addEventListener('click', () => {
    pageDisplay.classList.add('hidden');
    pageSettings.classList.remove('hidden');
});

$('btn-back').addEventListener('click', () => {
    pageSettings.classList.add('hidden');
    pageDisplay.classList.remove('hidden');
});

// ==================== 日志折叠 ====================
btnLog.addEventListener('click', () => {
    const collapsed = logBox.classList.toggle('collapsed');
    btnLog.textContent = collapsed ? '打开日志' : '关闭日志';
});

// ==================== 设置持久化 ====================
const SETTINGS_KEY = 'pct100_settings';

function loadSettings() {
    try {
        const saved = JSON.parse(localStorage.getItem(SETTINGS_KEY));
        if (saved) {
            if (saved.host)     iptHost.value   = saved.host;
            if (saved.port)     iptPort.value   = saved.port;
            if (saved.deviceId) iptDevice.value = saved.deviceId;
            if (saved.user)     iptUser.value   = saved.user;
            if (saved.pass)     iptPass.value   = saved.pass;
        }
    } catch (_) { /* ignore corrupted data */ }
}

function saveSettings() {
    localStorage.setItem(SETTINGS_KEY, JSON.stringify({
        host:     iptHost.value.trim(),
        port:     iptPort.value.trim(),
        deviceId: iptDevice.value.trim(),
        user:     iptUser.value.trim(),
        pass:     iptPass.value.trim(),
    }));
}

// ==================== MQTT 连接 ====================
function doConnect() {
    saveSettings();
    deviceId = iptDevice.value.trim();
    window.host.connect(
        iptHost.value.trim(),
        iptPort.value.trim(),
        deviceId,
        iptUser.value.trim(),
        iptPass.value.trim()
    );
    log(`正在连接 ${iptHost.value}:${iptPort.value} ...`, 'info');
}

function doDisconnect() {
    window.host.disconnect();
}

btnConnDisplay.addEventListener('click', () => {
    if (connected) { doDisconnect(); return; }
    doConnect();
});

function updateConnectBtn() {
    if (connected) {
        btnConnDisplay.textContent = '断开';
        btnConnDisplay.className = 'btn conn-disconnect disconnect';
    } else {
        btnConnDisplay.textContent = '连接';
        btnConnDisplay.className = 'btn conn-disconnect connect';
    }
}

window.host.onStatus((data) => {
    connected = data.connected;
    updateConnectBtn();

    if (connected) {
        indMqtt.className = 'status-dot online';
        log('MQTT 已连接', 'info');
    } else {
        indMqtt.className = 'status-dot offline';
        indDevice.className = 'status-dot offline';
        deviceAlive = false;
        setAllDisabled(true);
        log('MQTT 已断开', 'error');
    }
});

window.host.onError((msg) => {
    log(`错误: ${msg}`, 'error');
});

// ==================== 约束控制 ====================

// 根据电源+模式 更新控件禁用状态
function updateOutputEnable() {
    const canControl = connected && powerOn && currentMode === 'manual';
    toggleLed.disabled = !canControl;
    toggleFan.disabled = !canControl;
    toggleMode.disabled = !(connected && powerOn);
}

function setAllDisabled(disabled) {
    [toggleLed, toggleFan, toggleMode].forEach(t => t.disabled = disabled);
}

function setCardActive(card, active) {
    card.classList.toggle('active', active);
}

function setToggleChecked(toggle, checked) {
    suppressToggle = true;
    toggle.checked = checked;
    suppressToggle = false;
}

// ==================== 设备心跳 ====================
function kickHeartbeat() {
    if (!deviceAlive) {
        deviceAlive = true;
        indDevice.className = 'status-dot online';
    }
    clearTimeout(deviceHeartbeat);
    deviceHeartbeat = setTimeout(() => {
        deviceAlive = false;
        indDevice.className = 'status-dot offline';
        setCardActive(cardLed, false);
        setCardActive(cardFan, false);
        powerOn = false;
        powerSection.className = 'off';
        setAllDisabled(true);
        log('设备离线（无响应）', 'error');
    }, DEVICE_TIMEOUT);
}

// ==================== 状态更新 ====================
window.host.onMessage((data) => {
    kickHeartbeat();

    // 传感器
    valTemp.textContent  = data.temperature != null ? `${data.temperature.toFixed(1)}°C` : '--';
    valLight.textContent = data.light != null ? `${data.light} Lux` : '--';

    // 总闸
    if (data.key1_lock !== undefined) {
        powerOn = data.key1_lock;
        powerSection.className = powerOn ? 'on' : 'off';
        const on = powerOn;
        powerBadge.textContent = on ? '开启' : '断开';

        if (!on) {
            setCardActive(cardLed, false);
            setCardActive(cardFan, false);
            setToggleChecked(toggleLed, false);
            setToggleChecked(toggleFan, false);
            valLed.textContent = '关';
            valLed.className = 'output-value off';
            valFan.textContent = '关';
            valFan.className = 'output-value off';
            hintLed.textContent = '';
            hintFan.textContent = '';
        }
        updateOutputEnable();
    }

    // 模式
    if (data.mode !== undefined) {
        currentMode = data.mode;
        const auto = data.mode === 'auto';
        valMode.textContent = auto ? '自动' : '手动';
        setToggleChecked(toggleMode, auto);
        updateOutputEnable();

        hintLed.textContent = auto ? '自动控制' : '';
        hintLed.className = 'ctrl-hint' + (auto ? ' auto' : '');
        hintFan.textContent = auto ? '自动控制' : '';
        hintFan.className = 'ctrl-hint' + (auto ? ' auto' : '');
    }

    if (data.relay3 !== undefined) {
        const on = data.relay3;
        valLed.textContent = on ? '开' : '关';
        valLed.className = 'output-value ' + (on ? 'on' : 'off');
        setToggleChecked(toggleLed, on);
        setCardActive(cardLed, on);
    }

    if (data.relay4 !== undefined) {
        const on = data.relay4;
        valFan.textContent = on ? '开' : '关';
        valFan.className = 'output-value ' + (on ? 'on' : 'off');
        setToggleChecked(toggleFan, on);
        setCardActive(cardFan, on);
    }

    // 阈值
    if (data.temp_th !== undefined)  curTempTh.textContent  = data.temp_th;
    if (data.light_th !== undefined) curLightTh.textContent = data.light_th;

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

// ==================== 开关事件 ====================

toggleLed.addEventListener('change', () => {
    if (suppressToggle) return;
    sendCmd({ cmd: 'set_relay', relay: 3, value: toggleLed.checked });
});

toggleFan.addEventListener('change', () => {
    if (suppressToggle) return;
    sendCmd({ cmd: 'set_relay', relay: 4, value: toggleFan.checked });
});

toggleMode.addEventListener('change', () => {
    if (suppressToggle) return;
    const manual = !toggleMode.checked;
    currentMode = manual ? 'manual' : 'auto';
    valMode.textContent = manual ? '手动' : '自动';
    updateOutputEnable();
    hintLed.textContent = manual ? '' : '自动控制';
    hintLed.className = 'ctrl-hint' + (manual ? '' : ' auto');
    hintFan.textContent = manual ? '' : '自动控制';
    hintFan.className = 'ctrl-hint' + (manual ? '' : ' auto');
    sendCmd({ cmd: 'set_mode', mode: manual ? 'manual' : 'auto' });
});

// ==================== 阈值设置 ====================
$('cmd-set-temp-th').addEventListener('click', () => {
    const val = parseFloat(iptTempTh.value);
    if (isNaN(val)) return;
    sendCmd({ cmd: 'set_threshold', temp: val });
});

$('cmd-set-light-th').addEventListener('click', () => {
    const val = parseInt(iptLightTh.value);
    if (isNaN(val)) return;
    sendCmd({ cmd: 'set_threshold', light: val });
});

// ==================== 初始化 ====================
loadSettings();
updateConnectBtn();
setAllDisabled(true);
