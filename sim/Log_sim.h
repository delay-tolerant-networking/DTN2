#include "debug/Log.h"

/**
 * The Log_sim class is a simple override of the default Log class's
 * getlogtime() function so log messages include simulated time, not
 * real time.
 */
class Log_sim : public Log {
public:
     void getlogtime(struct timeval* tv);
     static void init(log_level_t defaultlvl = LOG_DEFAULT_THRESHOLD,
                     const char *debug_path = LOG_DEFAULT_DBGFILE);
};
