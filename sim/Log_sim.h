#include "debug/Log.h"


class Log_sim : public Log {

public:
     virtual timeval gettimeofday_();
     Log_sim();
     static void init(log_level_t defaultlvl = LOG_DEFAULT_THRESHOLD,
                     const char *debug_path = LOG_DEFAULT_DBGFILE);

};
