#include "LogSim.h"
#include "Simulator.h"

LogSim::LogSim() : Log()
{
}

void
LogSim::getlogtime(struct timeval* t) 
{
    t->tv_sec = (long int) Simulator::time();
    t->tv_usec = 0;
}

void
LogSim::init(log_level_t level, const char* path) 
{
    LogSim* log = new LogSim();
    log->do_init(1, level, path); 
}
