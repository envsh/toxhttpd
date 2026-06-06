#ifndef APILOG_H_
#define APILOG_H_

#include "compat34.h"
#include <qstring.h>
#include <qobject.h>
#include <vector>
#include <utility>
#include <type_traits>

// 兼容 Qt3/Qt4 的 QObject 名称获取函数
inline QString getObjectName(QObject* obj) {
#ifdef QT3_BUILD
    return QString(obj->name());
#else
    return obj->objectName();
#endif
}

// 兼容 Qt3/Qt4 的 QObject 类名获取函数
inline const char* getClassName(QObject* obj) {
#ifdef QT3_BUILD
    // Qt3 没有 className()，直接使用静态类名或通过 QT_OBJECT 宏
    // 这里返回一个默认值，因为 Qt3 中无法通过 QObject 直接获取类名
    Q_UNUSED(obj);
    return "QObject";
#else
    return obj->metaObject()->className();
#endif
}

enum ApiLogLevel { ApiLogDebug, ApiLogInfo, ApiLogWarning, ApiLogError };

enum ApiLogTimeFormat { 
    ApiTimeFull,    // 完整时间：yyyy-MM-dd hh:mm:ss.zzz
    ApiTimeShort,   // 简短时间：hh:mm:ss
    ApiTimeCustom   // 自定义格式（通过 ApiLogImpl 处理）
};

class ALogStream {
public:
    ALogStream() {}
    
    // 整数类型：用模板+SFINAE，只接受整数类型（含 unsigned long long）
    template<typename T>
    typename std::enable_if<std::is_integral<T>::value, ALogStream&>::type
    operator<<(T n) {
        m_buffer += QString::number((long long)n) + " ";
        return *this;
    }
    
    // 浮点类型
    ALogStream& operator<<(double d) {
        m_buffer += QString::number(d, 'g', 6) + " ";
        return *this;
    }
    
    ALogStream& operator<<(float f) {
        m_buffer += QString::number((double)f, 'g', 6) + " ";
        return *this;
    }
    
    // 字符串类型
    ALogStream& operator<<(const QString& s) {
        m_buffer += s + " ";
        return *this;
    }
    
    ALogStream& operator<<(const char* s) {
        m_buffer += qFromUtf8(s) + " ";
        return *this;
    }
    
    ALogStream& operator<<(const std::string& s) {
        m_buffer += qFromUtf8(s) + " ";
        return *this;
    }
    
    ALogStream& operator<<(bool b) {
        m_buffer += (b ? QString("true ") : QString("false "));
        return *this;
    }
    
    // QObject* 支持：使用兼容 Qt3/Qt4 的辅助函数
    ALogStream& operator<<(QObject* obj) {
        if (!obj) {
            m_buffer += "QObject(nullptr) ";
            return *this;
        }
        const char* cls = getClassName(obj);
        m_buffer += QString(cls) + "(name=";
        m_buffer += getObjectName(obj) + ") ";
        return *this;
    }
    
    // std::vector<T> 支持
    template<typename T>
    ALogStream& operator<<(const std::vector<T>& vec) {
        m_buffer += "[";
        for (size_t i = 0; i < vec.size(); ++i) {
            if (i > 0) m_buffer += ", ";
            *this << vec[i];
        }
        m_buffer += "] ";
        return *this;
    }
    
    // std::pair<T1,T2> 支持
    template<typename T1, typename T2>
    ALogStream& operator<<(const std::pair<T1,T2>& p) {
        m_buffer += "(";
        *this << p.first;
        m_buffer += ", ";
        *this << p.second;
        m_buffer += ") ";
        return *this;
    }
    
    // 获取拼接结果
    QString str() const { 
#ifdef QT3_BUILD
        return m_buffer.stripWhiteSpace();
#else
        return m_buffer.trimmed();
#endif
    }
    
private:
    QString m_buffer;
};

// 可变参数模板：展开参数包到 ALogStream
template<typename... Args>
QString collectFields(const Args&... args) {
    ALogStream stream;
    using Expander = int[];
    (void)Expander{0, ((stream << args), void(), 0)...};
    return stream.str();
}

void apiLogImpl(ApiLogLevel level, ApiLogTimeFormat timeFmt, const char* file, int line, const QString& msg);

template<typename... Args>
void aLog(ApiLogLevel level, ApiLogTimeFormat timeFmt, const char* file, int line, const Args&... args) {
    QString msg = collectFields(args...);
    apiLogImpl(level, timeFmt, file, line, msg);
}

// ===== 宏定义：自动捕获 __FILE__ 和 __LINE__，默认使用简短时间 =====
#define ALOG_DEBUG(...) aLog(ApiLogDebug, ApiTimeShort, __FILE__, __LINE__, __VA_ARGS__)
#define ALOG_INFO(...)  aLog(ApiLogInfo, ApiTimeShort, __FILE__, __LINE__, __VA_ARGS__)
#define ALOG_WARN(...)  aLog(ApiLogWarning, ApiTimeShort, __FILE__, __LINE__, __VA_ARGS__)
#define ALOG_ERROR(...) aLog(ApiLogError, ApiTimeShort, __FILE__, __LINE__, __VA_ARGS__)

// ===== 宏定义：支持自定义时间格式 =====
#define ALOG_DEBUG_T(fmt, ...) aLog(ApiLogDebug, fmt, __FILE__, __LINE__, __VA_ARGS__)
#define ALOG_INFO_T(fmt, ...)  aLog(ApiLogInfo, fmt, __FILE__, __LINE__, __VA_ARGS__)
#define ALOG_WARN_T(fmt, ...)  aLog(ApiLogWarning, fmt, __FILE__, __LINE__, __VA_ARGS__)
#define ALOG_ERROR_T(fmt, ...) aLog(ApiLogError, fmt, __FILE__, __LINE__, __VA_ARGS__)

#endif
