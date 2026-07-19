#include "sleepblocker.h"
#include "limelog.h"

#ifdef _WIN32
#include <windows.h>
// Windows: SetThreadExecutionState 是 kernel32.dll 导出函数，
// 所有 Windows 版本均可用，无需额外链接库。
// 传入 ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED 表示：
//   ES_CONTINUOUS (0x80000000) — 状态持续生效，直到下次调用清除
//   ES_DISPLAY_REQUIRED (0x00000002) — 阻止显示器关闭
//   ES_SYSTEM_REQUIRED (0x00000001) — 阻止系统睡眠
// 恢复方法：传入之前的 prevState 或仅传 ES_CONTINUOUS

#elif defined(__APPLE__)
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
// macOS: IOPMAssertionCreateWithName 是 IOKit 电源管理框架 API。
// kIOPMAssertionTypePreventUserIdleDisplaySleep — 阻止屏保激活和显示器关闭（默认）
// kIOPMAssertionTypePreventUserIdleSystemSleep  — 阻止系统空闲休眠（preventSystemSleep=true 时启用）
// kIOPMAssertionLevelOn (255) — 最高优先级
// IOPMAssertionRelease(assertionID) 释放 assertion，系统恢复正常休眠策略。
// IOKit framework 在 macOS 上始终可用，无需额外安装。
// 参考: https://developer.apple.com/library/archive/qa/qa1340/_index.html

#elif defined(__linux__)
#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>
// Linux/X11: XScreenSaverSuspend 是 XScreenSaver 扩展 API。
// 传入 True 暂停屏保和 DPMS 计时器，传入 False 恢复。
// 优势：
//   1. 不修改系统全局设置（对比 xset s off / xset -dpms）
//   2. X 客户端断开连接时系统自动恢复（进程崩溃安全）
//   3. 多个客户端可独立抑制，计数器机制
// 需要 libXss (-lXss) 和 libX11 (-lX11)。
// 参考: man XScreenSaver(3)
#endif

#ifdef __APPLE__
// ── macOS: 屏保锁屏 tick 机制 ──
// IOPMAssertion 只阻止显示器/系统休眠，不阻止屏保锁屏。
// 屏保锁屏由 HIDIdleTime（距上次用户输入的时间）控制。
// 通过定时发送 CGEvent 鼠标微移重置 HIDIdleTime。

static bool s_sbActive = false;

SleepBlockerTickHelper::SleepBlockerTickHelper(QObject* parent)
    : QObject(parent), timerId_(0), ticking_(false)
{
}

void SleepBlockerTickHelper::activate() {
    s_sbActive = true;
    ticking_ = false;
    if (timerId_ == 0) {
        timerId_ = startTimer(50000);
    }
}

void SleepBlockerTickHelper::deactivate() {
    s_sbActive = false;
    if (timerId_ != 0) {
        killTimer(timerId_);
        timerId_ = 0;
    }
    ticking_ = false;
}

void SleepBlockerTickHelper::timerEvent(QTimerEvent* e) {
    if (!s_sbActive || ticking_) return;
    // 用户刚动过（空闲 < 45s），真实输入已重置 HIDIdleTime，跳过
    CFTimeInterval idle = CGEventSourceSecondsSinceLastEventType(
        kCGEventSourceStateHIDSystemState, kCGAnyInputEventType);
    if (idle < 45.0) return;
    ticking_ = true;
    // 鼠标微移 0→1→0，重置 HIDIdleTime，视觉无感知
    CGEventRef ev1 = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved,
                                             CGPointMake(0, 0), kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, ev1);
    CFRelease(ev1);
    CGEventRef ev2 = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved,
                                             CGPointMake(1, 0), kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, ev2);
    CFRelease(ev2);
    ticking_ = false;
}
#endif

SleepBlocker::SleepBlocker(bool preventSystemSleep)
    : active_(false)
    , preventSystemSleep_(preventSystemSleep)
#ifdef _WIN32
    , prevState_(0)
#elif defined(__APPLE__)
    , displayAssertionID_(0)
    , systemAssertionID_(0)
    , helper_(nullptr)
#elif defined(__linux__)
    , display_(0)
#endif
{
#ifdef _WIN32
    prevState_ = ::SetThreadExecutionState(
        ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
    active_ = (prevState_ != 0);
    ALOG_INFO("SleepBlocker: activated (Windows), prevState=0x%lx", prevState_);

#elif defined(__APPLE__)
    CFStringRef reason = CFSTR("qlcomp: preventing screensaver and display sleep");
    IOReturn ret = IOPMAssertionCreateWithName(
        kIOPMAssertionTypePreventUserIdleDisplaySleep,
        kIOPMAssertionLevelOn,
        reason,
        &displayAssertionID_);
    if (preventSystemSleep_ && ret == kIOReturnSuccess) {
        CFStringRef sysReason = CFSTR("qlcomp: preventing system idle sleep");
        IOPMAssertionCreateWithName(
            kIOPMAssertionTypePreventUserIdleSystemSleep,
            kIOPMAssertionLevelOn,
            sysReason,
            &systemAssertionID_);
    }
    active_ = (ret == kIOReturnSuccess);
    ALOG_INFO("SleepBlocker: activated (macOS), displayID=%u systemID=%u ret=%d",
              displayAssertionID_, systemAssertionID_, ret);
    if (active_) {
        helper_ = new SleepBlockerTickHelper();
        static_cast<SleepBlockerTickHelper*>(helper_)->activate();
    }

#elif defined(__linux__)
    Display* dpy = XOpenDisplay(NULL);
    if (dpy) {
        XScreenSaverSuspend(dpy, True);
        display_ = dpy;
        active_ = true;
        ALOG_INFO("SleepBlocker: activated (Linux/X11)");
    } else {
        ALOG_WARN("SleepBlocker: XOpenDisplay failed, cannot inhibit screensaver");
    }
#endif
}

SleepBlocker::~SleepBlocker() {
    release();
}

void SleepBlocker::release() {
    if (!active_) {
        return;
    }
    active_ = false;

#ifdef _WIN32
    ::SetThreadExecutionState(prevState_);
    ALOG_INFO("SleepBlocker: released (Windows), prevState=0x%lx", prevState_);

#elif defined(__APPLE__)
    IOPMAssertionRelease(displayAssertionID_);
    displayAssertionID_ = 0;
    if (helper_) {
        static_cast<SleepBlockerTickHelper*>(helper_)->deactivate();
        delete static_cast<SleepBlockerTickHelper*>(helper_);
        helper_ = nullptr;
    }
    if (systemAssertionID_) {
        IOPMAssertionRelease(systemAssertionID_);
        systemAssertionID_ = 0;
    }
    ALOG_INFO("SleepBlocker: released (macOS)");

#elif defined(__linux__)
    if (display_) {
        XScreenSaverSuspend((Display*)display_, False);
        XCloseDisplay((Display*)display_);
        display_ = 0;
        ALOG_INFO("SleepBlocker: released (Linux/X11)");
    }
#endif
}
