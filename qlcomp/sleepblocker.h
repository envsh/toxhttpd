#ifndef SLEEPBLOCKER_H
#define SLEEPBLOCKER_H

// SleepBlocker — 程序存活期间抑制系统屏保和显示器休眠
//
// 原理：
//   各操作系统提供 API 让应用程序声明"我正在执行需要用户持续关注的任务"，
//   系统收到声明后暂停屏保计时器和显示器/系统空闲休眠计时器，
//   直到应用释放声明。不修改系统设置，仅在进程级别临时生效。
//
// Windows: SetThreadExecutionState()  (kernel32.dll, 无需额外链接库)
//   ES_CONTINUOUS (0x80000000) — 声明持续生效，直到下次调用清除
//   ES_DISPLAY_REQUIRED (0x00000002) — 重置显示器空闲计时器，阻止显示器关闭
//   ES_SYSTEM_REQUIRED (0x00000001) — 重置系统空闲计时器，阻止系统睡眠
//   恢复: SetThreadExecutionState(prevState) 或传入 ES_CONTINUOUS 即可
//   参考: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setthreadexecutionstate
//
// macOS: IOPMAssertionCreateWithName / IOPMAssertionRelease  (IOKit framework)
//   创建一个 Power Assertion，类型 kIOPMAssertionTypeNoDisplaySleep 即禁止显示器休眠。
//   Assertion 在释放前一直有效；进程异常退出时系统自动回收。
//   参考: https://developer.apple.com/library/archive/qa/qa1340/_index.html
//
// Linux/X11: XScreenSaverSuspend()  (XScreenSaver 扩展, 需链接 libXss)
//   X11 ScreenSaver 扩展 API，suspend=True 暂停屏保和 DPMS (显示器省电) 计时器。
//   与 "xset s off" / "xset -dpms" 效果类似，但不修改系统全局设置，
//   且 X 客户端断开连接时系统自动恢复。
//   参考: man XScreenSaver(3)
//
// 用法：
//   {
//       SleepBlocker blocker;        // 构造时激活
//       // ... 播放视频 / 视频通话 / 长时间运算 ...
//   }   // 析构时自动释放
//
//   也可手动提前释放：
//   blocker.release();

class SleepBlocker {
public:
    SleepBlocker();
    ~SleepBlocker();

    // 手动提前释放，多次调用安全
    void release();

private:
    SleepBlocker(const SleepBlocker&);
    SleepBlocker& operator=(const SleepBlocker&);

#ifdef _WIN32
    unsigned long prevState_;
#elif defined(__APPLE__)
    unsigned int assertionID_;
#elif defined(__linux__)
    void* display_;
#endif
    bool active_;
};

#endif  // SLEEPBLOCKER_H
