#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <dlfcn.h>
#include <cxxabi.h>
#include <execinfo.h>

#ifdef QT3_BUILD

static const char* demangleSym(const char* raw) {
    if (!raw) return "??";
    const char* paren = strchr(raw, '(');
    const char* plus  = strchr(raw, '+');
    const char* name = raw;
    if (paren && plus && paren < plus) {
        size_t len = plus - paren - 1;
        char* buf = (char*)alloca(len + 1);
        strncpy(buf, paren + 1, len);
        buf[len] = '\0';
        name = buf;
    }
    int status;
    char* dem = abi::__cxa_demangle(name, nullptr, nullptr, &status);
    if (dem) {
        char* p = strchr(dem, '(');
        if (p) *p = '\0';
    }
    return dem ? dem : name;
}

static const char* callerBacktrace() {
    void* stack[3];
    int n = backtrace(stack, 3);
    char** syms = backtrace_symbols(stack, n);
    const char* result = (syms && n >= 2) ? demangleSym(syms[2]) : "??";
    free(syms);
    return result;
}

static const char* callerRetaddr() {
    void* addr = __builtin_return_address(2);
    Dl_info info;
    if (dladdr(addr, &info) && info.dli_sname)
        return demangleSym(info.dli_sname);
    return "??";
}

#define USE_RETADDR

static void __attribute__((noinline)) logWithCaller(const char* label, const char* fmt, va_list ap) {
    char buf[4096];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);

    if ((size_t)n >= sizeof(buf)) {
        char marker[64];
        int ml = snprintf(marker, sizeof(marker), " [Truncated %d]", n);
        if (ml > 0 && (size_t)ml < sizeof(buf))
            strcpy(buf + sizeof(buf) - 1 - ml, marker);
    }

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char tbuf[16];
    strftime(tbuf, sizeof(tbuf), "%H:%M:%S", t);

#ifdef USE_RETADDR
    const char* caller = callerRetaddr();
#else
    const char* caller = callerBacktrace();
#endif

    fprintf(stderr, "[%s] [%s] %s [from %s]\n", tbuf, label, buf, caller);
}

void qDebug(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    logWithCaller("DEBUG", fmt, ap);
    va_end(ap);
}

void qWarning(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    logWithCaller("WARN", fmt, ap);
    va_end(ap);
}

void qFatal(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    logWithCaller("FATAL", fmt, ap);
    va_end(ap);
    abort();
}

void qInfo(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    logWithCaller("INFO", fmt, ap);
    va_end(ap);
}

#endif
