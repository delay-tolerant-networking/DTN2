#include "Event.h"


Event::Event(double time, Processable *handler, sim_event_type_t eventcode)
{
    time_ = time;
    handler_ = handler;
    type_ = eventcode;
    valid_ = true;
}


void
Event::cancel()
{
    valid_ = false;
}
