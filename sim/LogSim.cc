#include "LogSim.h"
#include "Simulator.h"

void
LogSim::getlogtime(struct timeval* t) 
{
    t->tv_sec = (long int) Simulator::time();
    t->tv_usec = 0;

}


LogSim::LogSim() : Log() {}

void
LogSim::init(log_level_t level, const char* path) 
{
    

    LogSim* tmp =  new LogSim();
    /**
     * This will initialize instance_ to point to tmp
     */
    tmp->do_init(level,path); 
    
}
