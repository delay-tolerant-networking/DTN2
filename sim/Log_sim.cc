#include "Log_sim.h"
#include "Simulator.h"

timeval
Log_sim::gettimeofday_() {
    timeval t;
    t.tv_sec = (long int) Simulator::time();
    t.tv_usec = 0;
    return t;
}


Log_sim::Log_sim() : Log() {}

void
Log_sim::init(log_level_t level, const char* path) {
    
// Initialize logging before anything else
    ASSERT(instance_ == NULL);
    instance_ =  new Log_sim();
    instance_->do_init(level,path);
    
}
