
#ifndef _PROCESSABLE_H_
#define _PROCESSABLE_H_


/**
 * Interface implemented by all objects that handle simulator events
 */

#include "Event.h"

class Event;

class Processable {

public:
    virtual void process(Event* e) = 0; 

};


#endif /* _PROCESSABLE_H_ */
