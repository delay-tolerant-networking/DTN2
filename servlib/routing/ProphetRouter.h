#ifndef _PROPHET_ROUTER_H_
#define _PROPHET_ROUTER_H_

#include "TableBasedRouter.h"
#include "reg/AdminRegistration.h"

namespace dtn {

class ProphetRouter : public TableBasedRouter {
public:
    ProphetRouter();

    void handle_link_created(LinkCreatedEvent* event);
    void handle_contact_down(ContactDownEvent* event);

}; // ProphetRouter

} // namespace dtn

#endif /* _PROPHET_ROUTER_H_ */
