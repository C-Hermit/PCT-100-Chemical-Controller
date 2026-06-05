#include "ws2812_indicator.h"
#include "ws2812_drv.h"
#include "device.h"
#include "scheduler.h"

// ==================== 动画段定义 ====================

struct AnimStep {
    uint8_t r, g, b;
    uint16_t hold_ms;
    bool fade;
};

// 模式1: 严重警报（两路继电器都通）
// R100 灭50 R100 灭50 G100 灭50 G100 灭50 B100 灭50 B100 灭350
static const AnimStep PAT_SEVERE[] = {
    {255, 0, 0, 100, false}, {0, 0, 0, 50, false},
    {255, 0, 0, 100, false}, {0, 0, 0, 50, false},
    {0, 255, 0, 100, false}, {0, 0, 0, 50, false},
    {0, 255, 0, 100, false}, {0, 0, 0, 50, false},
    {0, 0, 255, 100, false}, {0, 0, 0, 50, false},
    {0, 0, 255, 100, false}, {0, 0, 0, 350, false},
};

// 模式2: 光照越界（仅 LED 继电器通）
// R300 灭200 G300 灭200 B300 灭200
static const AnimStep PAT_LIGHT[] = {
    {255, 0, 0, 300, false}, {0, 0, 0, 200, false},
    {0, 255, 0, 300, false}, {0, 0, 0, 200, false},
    {0, 0, 255, 300, false}, {0, 0, 0, 200, false},
};

// 模式3: 温度越界（仅风扇继电器通）
// R500 G500 B500 灭700
static const AnimStep PAT_TEMP[] = {
    {255, 0, 0, 500, false},
    {0, 255, 0, 500, false},
    {0, 0, 255, 500, false},
    {0, 0, 0, 700, false},
};

// 模式4: MQTT 未连接
// 红快照 → 渐绿500 → 渐灭500 → 渐蓝500 → 渐灭500
static const AnimStep PAT_MQTT_DOWN[] = {
    {255, 0, 0, 10, false},
    {0, 255, 0, 500, true},
    {0, 0, 0, 500, true},
    {0, 0, 255, 500, true},
    {0, 0, 0, 500, true},
};

// 模式5: WiFi 未连接
// R200 G200 灭500
static const AnimStep PAT_WIFI_DOWN[] = {
    {255, 0, 0, 200, false},
    {0, 255, 0, 200, false},
    {0, 0, 0, 500, false},
};

// 模式6: 全部正常
// 灭快照 → 渐红1s → 渐绿1s → 渐蓝1s → 渐灭1s → 灭200
static const AnimStep PAT_NORMAL[] = {
    {0, 0, 0, 10, false},
    {255, 0, 0, 1000, true},
    {0, 255, 0, 1000, true},
    {0, 0, 255, 1000, true},
    {0, 0, 0, 1000, true},
    {0, 0, 0, 200, false},
};

// ==================== 模式枚举与描述表 ====================

enum PatternId {
    PAT_NORMAL_IDX,
    PAT_WIFI_DOWN_IDX,
    PAT_MQTT_DOWN_IDX,
    PAT_TEMP_ALARM_IDX,
    PAT_LIGHT_ALARM_IDX,
    PAT_SEVERE_ALARM_IDX,
    PAT_COUNT
};

struct PatternDef {
    const AnimStep* steps;
    uint8_t count;
};

static const PatternDef PATTERNS[PAT_COUNT] = {
    {PAT_NORMAL,     sizeof(PAT_NORMAL)     / sizeof(PAT_NORMAL[0])},
    {PAT_WIFI_DOWN,  sizeof(PAT_WIFI_DOWN)  / sizeof(PAT_WIFI_DOWN[0])},
    {PAT_MQTT_DOWN,  sizeof(PAT_MQTT_DOWN)  / sizeof(PAT_MQTT_DOWN[0])},
    {PAT_TEMP,       sizeof(PAT_TEMP)       / sizeof(PAT_TEMP[0])},
    {PAT_LIGHT,      sizeof(PAT_LIGHT)      / sizeof(PAT_LIGHT[0])},
    {PAT_SEVERE,     sizeof(PAT_SEVERE)     / sizeof(PAT_SEVERE[0])},
};

// ==================== 状态机变量 ====================

static PatternId  _active = (PatternId)-1;
static int        _step = 0;
static unsigned long _step_start = 0;
static uint8_t    _from_r, _from_g, _from_b;
static uint8_t    _cur_r,  _cur_g,  _cur_b;
static bool       _initialized = false;

// ==================== 模式选择 ====================

static PatternId resolve_pattern(void) {
    if (sys.led_relay_on && sys.fan_relay_on) return PAT_SEVERE_ALARM_IDX;
    if (sys.led_relay_on)                 return PAT_LIGHT_ALARM_IDX;
    if (sys.fan_relay_on)                 return PAT_TEMP_ALARM_IDX;
    if (!sys.mqtt_connected&&sys.wifi_connected)              return PAT_MQTT_DOWN_IDX;
    if (!sys.wifi_connected)              return PAT_WIFI_DOWN_IDX;
    return PAT_NORMAL_IDX;
}

// ==================== 步进推进 ====================

static void advance_step(void) {
    const PatternDef& pat = PATTERNS[_active];
    const AnimStep& cur = pat.steps[_step];

    _from_r = cur.r;
    _from_g = cur.g;
    _from_b = cur.b;

    _step = (_step + 1) % pat.count;
    _step_start = millis();

    const AnimStep& next = pat.steps[_step];
    if (!next.fade) {
        _cur_r = next.r;
        _cur_g = next.g;
        _cur_b = next.b;
        ws2812_drv_set_color(0, _cur_r, _cur_g, _cur_b);
    }
}

// ==================== 公开接口 ====================

void ws2812_indicator_init(void) {
    _active = (PatternId)-1;
    _step = 0;
    _step_start = 0;
    _from_r = _from_g = _from_b = 0;
    _cur_r  = _cur_g  = _cur_b  = 0;
    _initialized = true;

    sched_add(ws2812_indicator_loop, 20, 2048);
}

void ws2812_indicator_loop(void) {
    if (!_initialized) return;

    PatternId want = resolve_pattern();

    if (want != _active) {
        // 模式切换：快照到新模式第一帧
        _active = want;
        _step = 0;
        _step_start = millis();
        const AnimStep& s = PATTERNS[_active].steps[0];
        _cur_r = s.r; _cur_g = s.g; _cur_b = s.b;
        _from_r = s.r; _from_g = s.g; _from_b = s.b;
        ws2812_drv_set_color(0, _cur_r, _cur_g, _cur_b);
        return;
    }

    // 当前步处理
    const PatternDef& pat = PATTERNS[_active];
    const AnimStep& seg = pat.steps[_step];
    unsigned long now = millis();
    unsigned long elapsed = now - _step_start;

    if (seg.fade && elapsed < seg.hold_ms) {
        float t = (float)elapsed / seg.hold_ms;
        _cur_r = (uint8_t)(_from_r + ((int16_t)seg.r - (int16_t)_from_r) * t);
        _cur_g = (uint8_t)(_from_g + ((int16_t)seg.g - (int16_t)_from_g) * t);
        _cur_b = (uint8_t)(_from_b + ((int16_t)seg.b - (int16_t)_from_b) * t);
        ws2812_drv_set_color(0, _cur_r, _cur_g, _cur_b);
    }

    if (elapsed >= seg.hold_ms) {
        advance_step();
    }
}
