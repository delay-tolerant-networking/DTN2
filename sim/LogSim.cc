#include "LogSim.h"
#include "Simulator.h"

timeval
LogSim::gettimeofday_() {
    timeval t;
    t.tv_sec = (long int) Simulator::time();
    t.tv_usec = 0;
    return t;
}


LogSim::LogSim() : Log() {}

void
LogSim::init(log_level_t level, const char* path) {
    
// Initialize logging before anything else
    ASSERT(instance_ == NULL);
    LogSim* tmp =  new LogSim();
    tmp->do_init(level,path);
    
}
