#ifndef _LOG_SIM_H
#define _LOG_SIM_H

#include "debug/Log.h"


class LogSim : public Log {

public:
    virtual void getlogtime(struct timeval* tv);
    LogSim();
    static void init(log_level_t defaultlvl = LOG_DEFAULT_THRESHOLD,
                     const char *debug_path = LOG_DEFAULT_DBGFILE);
    
};

#endif /* _LOG_SIM_H */
