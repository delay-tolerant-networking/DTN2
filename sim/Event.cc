#include "Event.h"

Event::Event(double time, Processable *handler, sim_event_type_t eventcode)
{
    time_ = time;
    handler_ = handler;
    type_ = eventcode;
    valid_ = true;
}

Processable* 
Event::handler() 
{
    return handler_;
}

double 
Event::time() 
{
    return time_ ;
}

bool
Event::is_valid() 
{
    return valid_ ;
}
    
void 
Event::cancel() 
{ 
    valid_ = false; 
}
    
bool 
Event::sameTypeAs(Event* e) 
{ 
    if (e->type_ == type_) 
	return true; 
    return false ; 
}


bool 
Event::sameTypeAs(sim_event_type_t t) 
{ 
    if (t == type_) 
	return true; 
    return false ; 
}

