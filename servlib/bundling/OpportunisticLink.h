#ifndef _OPPORTUNISTIC_LINK_H_
#define _OPPORTUNISTIC_LINK_H_

class Link;

#include "Link.h"

/**
 * Abstraction for a OPPORTUNISTIC link. It has to be opened
 * everytime one wants to use it. It has by definition only
 * -one contact- that is associated with the current
 * opportunity. The difference between opportunistic link
 * and ondemand link is that the ondemand link does not have
 * a queue of its own.
 */
class OpportunisticLink : public Link {
public:
        /**
     * Constructor / Destructor
     */
    OpportunisticLink(std::string name, const char* conv_layer,
                      const BundleTuple& tuple)
        :Link(name,OPPORTUNISTIC, conv_layer,tuple)
    {
    }
    virtual ~OpportunisticLink()  {}
    
protected:
};

#endif /* _OPPORTUNISTIC_LINK_H_ */
