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
// macOS: IOPMAssertionCreateWithName 是 IOKit 电源管理框架 API。
// kIOPMAssertionTypeNoDisplaySleep — 阻止显示器休眠和屏保
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

SleepBlocker::SleepBlocker()
    : active_(false)
#ifdef _WIN32
    , prevState_(0)
#elif defined(__APPLE__)
    , assertionID_(0)
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
    CFStringRef reason = CFSTR("qlcomp: preventing display sleep");
    IOReturn ret = IOPMAssertionCreateWithName(
        kIOPMAssertionTypeNoDisplaySleep,
        kIOPMAssertionLevelOn,
        reason,
        &assertionID_);
    active_ = (ret == kIOReturnSuccess);
    ALOG_INFO("SleepBlocker: activated (macOS), assertionID=%u ret=%d",
              assertionID_, ret);

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
    IOPMAssertionRelease(assertionID_);
    assertionID_ = 0;
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
