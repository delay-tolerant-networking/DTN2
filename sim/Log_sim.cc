#include "Log_sim.h"
#include "Simulator.h"

void
Log_sim::init(log_level_t level, const char* path) {
    Log_sim* log = new Log_sim();
    log->do_init(level, path);
}

void
Log_sim::getlogtime(struct timeval *tv) {
    tv->tv_sec = (u_int32_t)Simulator::time();
    tv->tv_usec = 0;
}

