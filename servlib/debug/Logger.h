#ifndef _LOGGER_H_
#define _LOGGER_H_

#include "Debug.h"

/**
 * @class Logger
 *
 * Many objects will, at constructor time, format a log target and
 * then use it throughout the code implementation -- the Logger class
 * encapsulates this common behavior.
 *
 * It therefore exports a set of functions (set_logpath, logpathf,
 * logpath_appendf) to manipulate the logging path post-construction.
 *
 * For example:
 *
 * @code
 * class LoggerTest : public Logger {
 * public:
 *   LoggerTest() : Logger("/loggertest")
 *   {
 *       logf(LOG_DEBUG, "initializing"); 		 // no path needed
 *       logf("/loggertest" LOG_DEBUG, "intiializing");  // but can be given
 *
 *       log_debug("loggertest initializing");           // macros work
 *       __log_debug("/loggertest", "initializing");     // but this case needs
 *                                                       // to be __log_debug
 *       set_logpath("/path");
 *       logpath_appendf("/a"); // logpath is "/path/a"
 *       logpath_appendf("/b"); // logpath is "/path/b"
 *   }
 * };
 * @endcode
 */

class Logger {
public:
    /**
     * Default constructor, initializes the logpath to the address of
     * the object.
     */
    Logger()
    {
        set_logpath(NULL);
    }

    /**
     * Constructor that initializes the logpath to a constant string.
     */
    Logger(const char* logpath)
    {
        set_logpath(logpath);
    }
    
    /**
     * Format function for logpath_.
     */
    inline void logpathf(const char* fmt, ...) PRINTFLIKE(2, 3);

    /**
     * Format function that appends to the end of the base path
     * instead of overwriting.
     *
     * For example:
     *
     * \code
     *   set_logpath("/path");
     *   logpath_appendf("/a"); // logpath is "/path/a"
     *   logpath_appendf("/b"); // logpath is "/path/b"
     * \endcode
     */
    inline void logpath_appendf(const char* fmt, ...) PRINTFLIKE(2, 3);
    
    /**
     * Assignment function.
     */
    void set_logpath(const char* logpath)
    {
        if (logpath == 0) {
            strncpy(logpath_, "/", sizeof(logpath_));
            baselen_ = 1;
        } else {
            strncpy(logpath_, logpath, sizeof(logpath_));
            baselen_ = strlen(logpath);
        }
    }

    /**
     * Wrapper around vlogf that uses the logpath_ instance.
     */
    inline int vlogf(log_level_t level, const char *fmt, va_list args)
    {
        return ::vlogf(logpath_, level, fmt, args);
    }

    /**
     * Wrapper around logf that uses the logpath_ instance.
     */
    inline int logf(log_level_t level, const char *fmt, ...)
        PRINTFLIKE(3, 4);

    /**
     * Yet another wrapper that just passes the log path straight
     * through, ignoring the logpath_ instance altogether. This means
     * a derived class doesn't need to explicitly qualify ::logf.
     */
    inline int logf(const char* logpath, log_level_t level, const char* fmt, ...)
        PRINTFLIKE(4, 5);

    /**
     * Wrapper around __logf, used by the log_debug style macros.
     *
     * (See Log.h for a full explanation of the need for __logf)
     */
    inline int __logf(log_level_t level, const char *fmt, ...)
        PRINTFLIKE(3, 4);

    /**
     * As decribed in Log.h, the log_debug style macros call
     * log_enabled(level, path) before calling __logf. In the case of
     * the Logger, the path parameter isn't really the path, but is
     * actually the format string, so we actually call log_enabled on
     * the logpath_ instance.
     */
    inline bool __log_enabled(log_level_t level, const char* path)
    {
        return ::__log_enabled(level, logpath_);
    }

protected:
    char logpath_[LOG_MAX_PATHLEN];
    size_t baselen_;
};

void
Logger::logpathf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(logpath_, sizeof(logpath_), fmt, ap);
    va_end(ap);
}

void
Logger::logpath_appendf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(&logpath_[baselen_], sizeof(logpath_) - baselen_, fmt, ap);
    va_end(ap);
}

int
Logger::logf(log_level_t level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vlogf(level, fmt, ap);
    va_end(ap);
    return ret;
}

int
Logger::__logf(log_level_t level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vlogf(level, fmt, ap);
    va_end(ap);
    return ret;
}

int
Logger::logf(const char* logpath, log_level_t level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = ::vlogf(logpath, level, fmt, ap);
    va_end(ap);
    return ret;
}


#endif /* _LOGGER_H_ */
