#include "Simulator.h"


Simulator* Simulator::instance_; ///< singleton instance

int Simulator::runtill_ = -1;

Simulator::Simulator() 
    : Logger ("/sim/main"),
      eventq_()
 {
    time_ = 0;
 }


void
Simulator::add_event_(Event* e) {
    eventq_.push(e);
}

void
Simulator::remove_event_(Event* e) {
    e->cancel();
}

void
Simulator::exit_simulation() 
{
    exit(0);
}

/**
 * The main simulator thread that fires the next event
 */
void
Simulator::run()
{

    log_debug("Starting event loop \n");
    is_running_ = true;

     while(!eventq_.empty()) {
        if (is_running_) {
            Event* e = eventq_.top();
            eventq_.pop();
            /* Move the clock */
            time_ =  e->time();
            if (e->is_valid()) {
                ASSERT(e->handler() != NULL);
                /* Process the event */
                log_debug("Event:%p type %s at time %f",
                           e,e->type_str(),time_);
                e->handler()->process(e);
            }
            if ((time_ > Simulator::runtill_)) {
                log_info(
                     "Exiting simulation. Current time (%f) > Max time (%d)",
                     time_,Simulator::runtill_);
                exit_simulation();
            }
        } // if is_running_
    }
    log_info("eventq is empty, time is %f ",time_);
}


void
Simulator::process(Event *e)
{
    switch (e->type()) {
    case    SIM_PRINT_STATS: {
        log_info("SIM: print_stats");
        break;
        }
    default:
        PANIC("undefined event \n");
    }

}
