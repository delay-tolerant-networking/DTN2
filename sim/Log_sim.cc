#include "Log_sim.h"
#include "Simulator.h"

void
Log_sim::init(log_level_t level, const char* path) {
    ASSERT(instance_ == NULL);
    instance_ =  new Log_sim();
    instance_->do_init(level,path);
}

void
Log_sim::getlogtime(struct timeval *tv) {
    tv->tv_sec = (u_int32_t)Simulator::time();
    tv->tv_usec = 0;
}

