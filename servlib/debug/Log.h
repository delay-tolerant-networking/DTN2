#ifndef _LOG_H_
#define _LOG_H_

/*!
 * \class Log
 *  
 * Dynamic Log system implementation.
 *
 * Basic idea is that a logf command contains an arbitrary log path, a
 * level, and a printf-style body. For example:
 *
 * \code
 * logf("/some/path", LOG_INFO, "something happened %d times", count);
 * \endcode
 *
 * Each application should at intialization call Log::init(level) to
 * prime the default level. All log messages with a level equal or
 * higher than the default are output, others aren't. All output goes
 * to stdout.
 *
 * The code also checks for a ~/.debug file which can optionally
 * contain log paths and other levels. For example:
 *
 * \code
 * # my ~/.debug file
 * /some/path debug
 * /other info
 * /other/more/specific debug
 * \endcode
 *
 * The paths are interpreted to include everything below them, thus in
 * the example above, all messages to /some/path/... would be output at
 * level debug.
 *
 * In addition to the basic logf interface, there are also a set of
 * macros (log_debug(), log_info(), log_warn(), log_err(), log_crit())
 * that are more efficient. As an example, log_debug expands to
 *
 * @code
 * #define log_debug(path, ...)                         \
 *    if (log_enabled(LOG_DEBUG, path)) {               \
 *        logf(LOG_DEBUG, path, ## __VA_ARGS__);        \
 *   }                                                  \
 * @endcode
 *
 * In this way, the check for whether or not the path is enabled at
 * level debug occurs before the call to logf(). As such, the
 * formatting of the log string, including any inline calls in the arg
 * list are avoided unless the log line will actually be output.
 *
 * In addition, under non-debug builds, all calls to log_debug are
 * completely compiled out.
 *
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <string>
#include <vector>

#ifdef __GNUC__
# define PRINTFLIKE(fmt, arg) __attribute__((format (printf, fmt, arg)))
#else
# define PRINTFLIKE(a, b)
#endif

#define LOG_DEFAULT_THRESHOLD   LOG_INFO
#define LOG_DEFAULT_DBGFILE "~/.dtndebug"

#define LOG_MAX_PATHLEN (64)
#define LOG_MAX_LINELEN (256)

typedef enum {
    LOG_INVALID = -1,
    LOG_DEBUG   = 1,
    LOG_INFO    = 2,
    LOG_WARN    = 3,
    LOG_ERR     = 4,
    LOG_CRIT    = 5
} log_level_t;

struct level2str_t {
    const char* str;
    log_level_t level;
};

extern level2str_t log_levelnames[];

inline const char *level2str(log_level_t level) {
    for (int i = 0; log_levelnames[i].str != 0; i++) {
        if (log_levelnames[i].level == level) {
            return log_levelnames[i].str;
        }
    }
    
    return "(unknown level)";
}

inline log_level_t str2level(const char *level)
{
    for (int i = 0; log_levelnames[i].str && i < 20; i++) {
        if (strcasecmp(log_levelnames[i].str, level) == 0) {
            return log_levelnames[i].level;
        }
    }

    return LOG_INVALID;
}

void __log_assert(bool x, const char* what, const char* file, int line);

class SpinLock;

class Log {
public:
    /**
     * Singleton instance accessor. Note that Log::init must be called
     * before this function, hence static initializers can't use the
     * logging system.
     */
    static Log *instance() {
        __log_assert(instance_ != 0, "Log::init not called yet",
                     __FILE__, __LINE__);
        return instance_; 
    }

    /**
     * Initialize the logging system. Must be called exactly once.
     */
    static void init(log_level_t defaultlvl = LOG_DEFAULT_THRESHOLD,
                     const char *debug_path = LOG_DEFAULT_DBGFILE);

    /**
     *  Sets the time to print for the logging 
     */
    virtual void getlogtime(struct timeval* tv);
    
    /**
     * Core logging function that is the guts of the implementation of
     * all other variants. Returns the number of bytes written, i.e.
     * zero if the log line was suppressed.
     */
    int vlogf(const char *path, log_level_t level, const char *fmt, va_list ap);

    /**
     * Return the log level currently enabled for the path.
     */
    log_level_t log_level(const char *path);

    /**
     * Parse (or reparse) the debug file and repopulate the rule list
     * (called from init).
     */
    void parse_debug_file(const char* debug_path);

    /**
     * Add a new rule to the list.
     */
    void add_debug_rule(const char* path, log_level_t threshold);


    Log();
    virtual ~Log() {} // never called


    /**
     * Initialize logging, should be called exactly once from the
     * static Log::init.
     */
    void do_init(log_level_t defaultlvl, const char* debug_path);


protected:
    /**
     * Singleton instance of the Logging system
     */
    static Log* instance_;

private:
    /**
     * Structure used to store a log rule as parsed from the debug
     * file.
     */
    struct Rule {
        Rule(const char* path, log_level_t level)
            : path_(path), level_(level) {}

        Rule(const Rule& rule)
            : path_(rule.path_), level_(rule.level_) {}
        
        std::string path_;
        log_level_t level_;
    };

    /**
     * Sorting function for log rules. The rules are stored in
     * descending length order, so a linear scan through the list will
     * always return the most specific match.
     */
    struct RuleCompare {
        bool operator()(const Rule& rule1, const Rule& rule2)
        {
            return (rule1.path_.length() > rule2.path_.length());
        }
    };

    /**
     * Use a vector for the list of rules.
     */
    typedef std::vector<Rule> RuleList;
    
    /**
     * Find a rule given a path.
     */
    Rule *find_rule(const char *path);

    /**
     * Sort the rules list.
     */
    void sort_rules();

    /**
     * Debugging function to print the rules list.
     */
    void print_rules();

   
    
    bool inited_;		///< Flag to ensure one-time intialization
    
    RuleList* rule_list_;	///< List of logging rules

    SpinLock* lock_;		///< Lock for logf and re-parsing

    std::string debug_path_;    ///< Path to the debug file
    log_level_t default_threshold_; ///< The default threshold for log messages
};

/**
 * Global vlogf function.
 */
inline int
vlogf(const char *path, log_level_t level, const char *fmt, va_list ap)
{
    if (!path) return -1;
    return Log::instance()->vlogf(path, level, fmt, ap);
}

/**
 * Global logf function.
 */
inline int
logf(const char *path, log_level_t level, const char *fmt, ...)
    PRINTFLIKE(3, 4);

inline int
logf(const char *path, log_level_t level, const char *fmt, ...)
{
    if (!path) return -1;
    va_list ap;
    va_start(ap, fmt);
    int ret = Log::instance()->vlogf(path, level, fmt, ap);
    va_end(ap);
    return ret;
}

/**
 * The set of macros below are implemented for more efficient
 * implementation of logging functions. As noted in the comment above,
 * these macros first check whether logging is enabled on the path and
 * then call the output formatter. As such, all output string
 * formatting and argument calculations are only done if the log path
 * is enabled.
 *
 * Note that the implementation is slightly confusing due to the need
 * to support the Logger class. Logger implements a logf() and vlogf()
 * function that doesn't take the path as the first parameter, but
 * instead uses an instance variable to keep the path recorded.
 * Thus, the macros need to support both a non-Logger call of the form:
 *
 *    log_debug("/path", "format string %s", "arguments");
 *
 * and of the form:
 *
 *    log_debug("Logger format string %s", "arguments");
 *
 * To implement this, the macro calls __log_enabled(path, level) which
 * calls either the Logger member function or the global
 * implementation with whatever the first argument to the macro is.
 * The Logger::__log_enabled() implementation actually ignores this
 * parameter (since it's actually the format string) and just checks
 * whether its logpath_ path is enabled.
 *
 * In addition, these macros call __logf() instead of logf, and both
 * the global and the Logger class implementation take the log level
 * as the first parameter.
 *
 * What makes this slightly more complicated is that derived classes
 * of Logger can optionally call logging functions with an explicit
 * path, ignoring the logpath_ instance.
 *
 * This poses a problem for the macro, since at compile time, it's
 * impossible to distinguish between the following two cases:
 *
 *    log_debug("this is just %s", "a const char* argument");
 *    log_debug("/path", "this is a path %s", "and a const char* argument");
 *
 * The second call will actually generate a compilation error, since
 * the compiler will assume that the first argument to the macro is
 * the format string (which it isn't). To handle this case, the set of
 * __log_debug style macros always assumes that the path is the first
 * argument and can therefore be used in any context where the path is
 * explicitly provided.
 */

// compile out all log_debug calls when not debugging
#ifdef NDEBUG
#define log_debug(...)
#define __log_debug(...)
#else
#define log_debug(p, args...)                     \
    ((__log_enabled(LOG_DEBUG, (p))) ?            \
     __logf(LOG_DEBUG, (p)  , ## args) : 0)

#define __log_debug(p, args...)                   \
    ((::__log_enabled(LOG_DEBUG, (p))) ?          \
     ::__logf(LOG_DEBUG, (p) , ## args) : 0)
#endif // NDEBUG

#define log_info(p, args...)                      \
    ((__log_enabled(LOG_INFO, (p))) ?             \
     __logf(LOG_INFO, (p) , ## args) : 0)

#define __log_info(p, args...)                    \
    ((::__log_enabled(LOG_INFO, (p))) ?           \
     ::__logf(LOG_INFO, (p) , ## args) : 0)

#define log_warn(p, args...)                      \
    ((__log_enabled(LOG_WARN, (p))) ?             \
     __logf(LOG_WARN, (p) , ## args) : 0)

#define __log_warn(p, args...)                    \
    ((::__log_enabled(LOG_WARN, (p))) ?           \
     ::__logf(LOG_WARN, (p) , ## args) : 0)

#define log_err(p, args...)                       \
    ((__log_enabled(LOG_ERR, (p))) ?              \
     __logf(LOG_ERR, (p) , ## args) : 0)

#define __log_err(p, args...)                     \
    ((::__log_enabled(LOG_ERR, (p))) ?            \
     ::__logf(LOG_ERR, (p) , ## args) : 0)

#define log_crit(p, args...)                      \
    ((__log_enabled(LOG_CRIT, (p))) ?             \
     __logf(LOG_CRIT, (p) , ## args) : 0)

#define __log_crit(p, args...)                    \
    ((::__log_enabled(LOG_CRIT, (p))) ?           \
     ::__logf(LOG_CRIT, (p) , ## args) : 0)

/**
 * As noted in the big comment above, the log_debug style macros call
 * this function which takes the level as the first parameter so it
 * can work with the Logger class as well.
 */
inline int
__logf(log_level_t level, const char *path, const char *fmt, ...)
    PRINTFLIKE(3, 4);

inline int
__logf(log_level_t level, const char *path, const char *fmt, ...)
{
    if (!path) return -1;
    va_list ap;
    va_start(ap, fmt);
    int ret = vlogf(path, level, fmt, ap);
    va_end(ap);
    return ret;
}

/**
 * Global function to determine if the log path is enabled. Overridden
 * by the Logger class.
 */
inline bool
__log_enabled(log_level_t level, const char* path)
{
    log_level_t threshold = Log::instance()->log_level(path);
    return (level >= threshold);
}


// Include Logger.h here (after all the Log classes have been defined
// of course) for simplicity.
#include "Logger.h"

#endif /* _LOG_H_ */
