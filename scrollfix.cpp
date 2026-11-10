// scrollfix: 触控板自然滚动，滚轮鼠标传统滚动。
// 无窗口、无 Dock 图标、无菜单栏图标，只在后台监听滚动事件。
//
// 编译: clang++ -O2 -std=c++17 scrollfix.cpp -framework ApplicationServices -o scrollfix
#include <ApplicationServices/ApplicationServices.h>
#include <notify.h>
#include <unistd.h>

static CFMachPortRef gTap = nullptr;
// true: 反转滚轮鼠标(系统开着自然滚动)；false: 反转触控板(系统关着自然滚动)
static bool gInvertWheel = true;

static void invertAxis(CGEventRef e, CGEventField line, CGEventField fixed, CGEventField point) {
    int64_t l = CGEventGetIntegerValueField(e, line);
    double  f = CGEventGetDoubleValueField(e, fixed);
    int64_t p = CGEventGetIntegerValueField(e, point);
    // 必须先写行增量：写它会让系统重算另外两个字段
    CGEventSetIntegerValueField(e, line, -l);
    CGEventSetDoubleValueField(e, fixed, -f);
    CGEventSetIntegerValueField(e, point, -p);
}

static CGEventRef onEvent(CGEventTapProxy, CGEventType type, CGEventRef e, void *) {
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        CGEventTapEnable(gTap, true);
        return e;
    }
    if (type != kCGEventScrollWheel) return e;

    // 触控板(以及 Magic Mouse)产生“连续”滚动事件，普通滚轮鼠标不是
    bool continuous = CGEventGetIntegerValueField(e, kCGScrollWheelEventIsContinuous) != 0;
    if (continuous == gInvertWheel) return e;

    invertAxis(e, kCGScrollWheelEventDeltaAxis1, kCGScrollWheelEventFixedPtDeltaAxis1,
               kCGScrollWheelEventPointDeltaAxis1);
    invertAxis(e, kCGScrollWheelEventDeltaAxis2, kCGScrollWheelEventFixedPtDeltaAxis2,
               kCGScrollWheelEventPointDeltaAxis2);
    return e;
}

// 读取系统“自然滚动”开关，决定反转哪一类设备。开关没设置过时系统默认是开。
static void refreshDirection() {
    CFPreferencesSynchronize(kCFPreferencesAnyApplication, kCFPreferencesCurrentUser,
                             kCFPreferencesAnyHost);
    CFPropertyListRef v = CFPreferencesCopyValue(CFSTR("com.apple.swipescrolldirection"),
                                                 kCFPreferencesAnyApplication,
                                                 kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    bool natural = true;
    if (v) {
        if (CFGetTypeID(v) == CFBooleanGetTypeID()) {
            natural = CFBooleanGetValue((CFBooleanRef)v);
        } else if (CFGetTypeID(v) == CFNumberGetTypeID()) {
            int n = 1;
            CFNumberGetValue((CFNumberRef)v, kCFNumberIntType, &n);
            natural = n != 0;
        }
        CFRelease(v);
    }
    gInvertWheel = natural;
}

static void onTimer(CFRunLoopTimerRef, void *) { refreshDirection(); }
static void onPrefChanged(CFNotificationCenterRef, void *, CFNotificationName, const void *,
                          CFDictionaryRef) {
    refreshDirection();
}

int main() {
    refreshDirection();

    // 首次运行时弹出“辅助功能”授权提示
    const void *keys[] = {kAXTrustedCheckOptionPrompt};
    const void *vals[] = {kCFBooleanTrue};
    CFDictionaryRef opts = CFDictionaryCreate(nullptr, keys, vals, 1,
                                              &kCFTypeDictionaryKeyCallBacks,
                                              &kCFTypeDictionaryValueCallBacks);
    AXIsProcessTrustedWithOptions(opts);
    CFRelease(opts);

    // 未授权时创建会失败，等用户授权后自动生效
    while (!(gTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
                                     kCGEventTapOptionDefault,
                                     CGEventMaskBit(kCGEventScrollWheel), onEvent, nullptr))) {
        sleep(2);
    }

    CFRunLoopSourceRef src = CFMachPortCreateRunLoopSource(nullptr, gTap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), src, kCFRunLoopCommonModes);
    CGEventTapEnable(gTap, true);

    // 用户在系统设置里切换“自然滚动”后自动跟随：系统设置会发通知，另外每 2 秒兜底检查一次
    CFNotificationCenterAddObserver(CFNotificationCenterGetDistributedCenter(), nullptr,
                                    onPrefChanged,
                                    CFSTR("SwipeScrollDirectionDidChangeNotification"), nullptr,
                                    CFNotificationSuspensionBehaviorDeliverImmediately);
    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(nullptr, CFAbsoluteTimeGetCurrent() + 2.0, 2.0,
                                                   0, 0, onTimer, nullptr);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopCommonModes);

    // 告诉安装向导“已经生效”(只存在于内存里的系统通知状态，不写任何文件)
    int token = 0;
    if (notify_register_check("local.scrollfix.ready", &token) == NOTIFY_STATUS_OK)
        notify_set_state(token, 1);

    CFRunLoopRun();
    return 0;
}
