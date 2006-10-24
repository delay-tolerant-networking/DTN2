/*
 *    Copyright 2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

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
