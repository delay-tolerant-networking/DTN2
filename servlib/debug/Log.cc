#include <ctype.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <algorithm>

#include "Debug.h"
#include "Log.h"
#include "thread/SpinLock.h"
#include "io/IO.h"

// we can't use the ASSERT from Debug.h since that one calls logf :-)
#undef ASSERT
#define ASSERT(x) __log_assert(x, #x, __FILE__, __LINE__)

void
__log_assert(bool x, const char* what, const char* file, int line)
{
    if (! (x)) {
        fprintf(stderr, "LOGGING ASSERTION FAILED (%s) at %s:%d\n",
                what, file, line);
        abort();
    }
}

level2str_t log_levelnames[] =
{
    { "debug",   LOG_DEBUG },
    { "info",    LOG_INFO },
    { "warn",    LOG_WARN },
    { "warning", LOG_WARN },
    { "err",     LOG_ERR },
    { "error",   LOG_ERR },
    { "crit",    LOG_CRIT },
    { NULL,      LOG_INVALID }
};

Log* Log::instance_ = NULL;

Log::Log()
    : inited_(false),
      default_threshold_(LOG_DEFAULT_THRESHOLD)
{
    rule_list_ = new RuleList();
    lock_ = new SpinLock();
}



void
Log::init(log_level_t defaultlvl, const char* debug_path)
{
    ASSERT(instance_ == NULL);
    instance_ = new Log();
    instance_->do_init(defaultlvl, debug_path);
}

void
Log::do_init(log_level_t defaultlvl, const char *debug_path)
{
    ASSERT(!inited_);

    ASSERT(lock_);
    ScopeLock lock(lock_);

    default_threshold_ = defaultlvl;

#ifdef _POSIX_THREAD_IS_CAPRICCIO
    /*
     * Make sure stdout is nonblocking so we don't ever block in
     * ::write while holding the spin lock.
     *
     * XXX/demmer for some reason this seems to set stdin to be
     * nonblocking as well when not running under capriccio
     */
    if (IO::set_nonblocking(1, true) != 0) {
        fprintf(stderr, "error setting stdout to nonblocking: %s\n",
                strerror(errno));
    }
#endif

    // short-circuit the case where there's no path at all
    if (debug_path == 0 || debug_path[0] == '\0') {
        inited_ = true;
        return;
    }

    parse_debug_file(debug_path);
}

void
Log::parse_debug_file(const char* debug_path)
{
    // handle ~/ in the debug_path
    if ((debug_path[0] == '~') && (debug_path[1] == '/')) {
        char path[256];
        const char* home = getenv("HOME");
        if (home && *home) {
            if (home[strlen(home) - 1] == '/') {
                // avoid // in expanded path
                snprintf(path, sizeof(path), "%s%s", home, debug_path + 2);
            } else {
                snprintf(path, sizeof(path), "%s%s", home, debug_path + 1);
            }
        } else {
            fprintf(stderr, "Can't expand ~ to HOME in debug_path %s\n",
                    debug_path);
            inited_ = true;
            return;
        }
        debug_path_.assign(path);
        debug_path = debug_path_.c_str();
    } else {
        debug_path_.assign(debug_path);
    }

    // check if we can open the file
    FILE *fp = fopen(debug_path, "r");
    if (fp == NULL) {
        if (! strcmp(debug_path, LOG_DEFAULT_DBGFILE)) {
            // error only if the user overrode the file
            fprintf(stderr, "Couldn't open debug file: %s\n", debug_path);
        }
        inited_ = true;
        return;
    }

    char buf[1024];
    int linenum = 0;
    
    while (!feof(fp)) {
        if (fgets(buf, sizeof(buf), fp) > 0) {
	    char *line = buf;
	    char *logpath;
	    char *level;
	    char *rest;

	    ++linenum;

            // parse the line
            logpath = line;

            // skip leading whitespace
            while (*logpath && isspace(*logpath)) ++logpath;
            if (! *logpath) {
                // blank line
                continue;
            }

            // skip comment lines
            if (logpath[0] == '#')
                continue;

            // find the end of path and null terminate
            level = logpath;
            while (*level && !isspace(*level)) ++level;
            *level = '\0';
            ++level;

            // skip any other whitespace
            while (level && isspace(*level)) ++level;
            if (!level) {
 parseerr:
                fprintf(stderr, "Error in log configuration %s line %d\n",
                        debug_path, linenum);
                continue;
            }

            // null terminate the level
            rest = level;
            while (rest && !isspace(*rest)) ++rest;
            if (rest) *rest = '\0';

            log_level_t threshold = str2level(level);
            if (threshold == LOG_INVALID) {
                goto parseerr;
            }

            rule_list_->push_back(Rule(logpath, threshold));
        }
    }
    
    fclose(fp);

    sort_rules();
}

void
Log::sort_rules()
{
    // Now that it's been parsed, sort the list based on the length
    std::sort(rule_list_->begin(), rule_list_->end(), RuleCompare());

#ifndef NDEBUG
    // Sanity assertion
    if (rule_list_->size() > 0) {
        RuleList::iterator itr;
        Rule* prev = 0;
        for (itr = rule_list_->begin(); itr != rule_list_->end(); ++itr) {
            if (prev != 0) {
                ASSERT(prev->path_.length() >= itr->path_.length());
                prev = &(*itr);
            }
        }
    }
#endif // NDEBUG

    inited_ = true;
    return;
}

void
Log::print_rules()
{
    RuleList::iterator iter = rule_list_->begin();
    printf("Logging rules:\n");
    for (iter = rule_list_->begin(); iter != rule_list_->end(); iter++) {
        printf("\t%s\n", iter->path_.c_str());
    }
}

Log::Rule *
Log::find_rule(const char *path)
{
    /*
     * The rules are stored in decreasing path lengths, so the first
     * match is the best (i.e. most specific).
     */
    size_t pathlen = strlen(path);

    RuleList::iterator iter;
    Rule* rule;
    for (iter = rule_list_->begin(); iter != rule_list_->end(); iter++) {
        rule = &(*iter);

        if (rule->path_.length() > pathlen) {
            continue; // can't be a match
        }
        
	if (rule->path_.compare(0, rule->path_.length(),
                                path, rule->path_.length()) == 0) {
            return rule; // match!
	}
    }

    return NULL; // no match :-(
}

void
Log::add_debug_rule(const char* path, log_level_t threshold)
{
    ASSERT(inited_);
    ScopeLock l(lock_);
    ASSERT(path);
    rule_list_->push_back(Rule(path, threshold));
    sort_rules();
}

class ScopedAtomicIncr {
public:
    ScopedAtomicIncr(int* v) : val_(v)
    {
        atomic_incr(val_);
    }
    ~ScopedAtomicIncr()
    {
        atomic_decr(val_);
    }
protected:
    int* val_;
};

log_level_t
Log::log_level(const char *path)
{
    Rule *r = find_rule(path);

    if (r) {
	return r->level_;
    } else {
	return default_threshold_;
    }
}

timeval 
Log::gettimeofday_() {
// tack on a timestamp
    timeval tv;
    gettimeofday(&tv, 0);
    return tv;
}

int
Log::vlogf(const char *path, log_level_t level, const char *fmt, va_list ap)
{
    ASSERT(inited_);

    char pathbuf[LOG_MAX_PATHLEN];

    // try to catch crashes due to buffer overflow with some guard
    // bytes at the end
    static const char guard[] = "[guard]";
    char buf[LOG_MAX_LINELEN + sizeof(guard)];
    memcpy(&buf[LOG_MAX_LINELEN], guard, sizeof(guard));
    
    // try to catch recursive calls that can result from an
    // assert/panic from somewhere within the logging code
    static int threads_in_vlogf = 0;
    ScopedAtomicIncr incr(&threads_in_vlogf);
    
    if (threads_in_vlogf > 10) {
        fprintf(stderr, "error: vlogf called recursively on log call:\n");
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n"); // since logf doesn't include newlines
        abort();
    }
    
    // throw a lock around the whole function
    ScopeLock l(lock_);

    /* Make sure that paths that don't start with a slash still show up. */
    if (*path != '/') {
	snprintf(pathbuf, sizeof pathbuf, "/%s", path);
	path = pathbuf;
    }

    // bail if we're not going to output the line.
    if (! __log_enabled(level, path))
        return 0;
    
    // format the log string
    char *ptr = buf;
    int buflen = LOG_MAX_LINELEN - 1; /* Save a character for newline. */
    int len;

    
    
    // tack on a timestamp
    timeval tv = gettimeofday_();
    len = snprintf(ptr, buflen, "[%ld.%06ld %s %s] ",
                   tv.tv_sec, tv.tv_usec, path, level2str(level));
       

    buflen -= len;
    ptr += len;

    // Generate string.
    len = vsnprintf(ptr, buflen, fmt, ap);

    if (memcmp(&buf[LOG_MAX_LINELEN], guard, sizeof(guard)) != 0) {
        PANIC("logf buffer overflow");
    }

    if (len >= buflen) {
        // handle truncated lines
        const char* trunc = "... (truncated)\n";
        len = strlen(trunc) + 1;
        memcpy(&buf[LOG_MAX_LINELEN - len], trunc, len);
        buflen = LOG_MAX_LINELEN;
    } else {
        buflen -= len;
        ptr += len;
        ASSERT(ptr <= (buf + LOG_MAX_LINELEN - 2));
        
        // make sure there's a trailing newline
        if (ptr[-1] != '\n') {
            *ptr++ = '\n';
            *ptr = 0;
        }
        
        buflen = ptr - buf;
    }

    // do the write, making sure to drain the buffer. since stdout was
    // set to nonblocking, the spin lock prevents other threads from
    // jumping in here
    int ret = IO::writeall(1, buf, buflen);
    ASSERT(ret == buflen);
    return buflen;
};


