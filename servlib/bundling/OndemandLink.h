#ifndef _ONDEMAND_LINK_H_
#define _ONDEMAND_LINK_H_

#include "Link.h"

/**
 * Abstraction for a ONDEMAND link. It has to be opened everytime
 * one wants to use it. It has by definition only -one contact-
 * that is associated and no future contacts/history.
 */
class OndemandLink : public Link {
public:
    /**
     * Constructor / Destructor
     */
    OndemandLink(std::string name, const char* conv_layer,
                 const BundleTuple& tuple)
        :Link(name,ONDEMAND, conv_layer,tuple) {}
    
    virtual ~OndemandLink()  {}
protected:
  
};


#endif /* _ONDEMAND_LINK_H_ */
