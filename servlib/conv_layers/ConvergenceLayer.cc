/*
 *    Copyright 2004-2006 Intel Corporation
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

#include <config.h>
#include "ConvergenceLayer.h"
#include "EthConvergenceLayer.h"
#include "FileConvergenceLayer.h"
#include "NullConvergenceLayer.h"
#include "TCPConvergenceLayer.h"
#include "UDPConvergenceLayer.h"
#include "BluetoothConvergenceLayer.h"

namespace dtn {

template<>
CLVector* oasys::Singleton<CLVector>::instance_ = NULL;

//----------------------------------------------------------------------
ConvergenceLayer::~ConvergenceLayer()
{
}

//----------------------------------------------------------------------
void
ConvergenceLayer::add_clayer(ConvergenceLayer* cl)
{
    CLVector::instance()->push_back(cl);
}
    
//----------------------------------------------------------------------
void
ConvergenceLayer::init_clayers()
{
    add_clayer(new NullConvergenceLayer());
    add_clayer(new TCPConvergenceLayer());
    add_clayer(new UDPConvergenceLayer());
#ifdef __linux__
    add_clayer(new EthConvergenceLayer());
#endif
#ifdef OASYS_BLUETOOTH_ENABLED
    add_clayer(new BluetoothConvergenceLayer());
#endif
    // XXX/demmer fixme
    // add_clayer("file", new FileConvergenceLayer());
}

//----------------------------------------------------------------------
CLVector::~CLVector()
{
    while (!empty()) {
        delete back();
        pop_back();
    }
}

//----------------------------------------------------------------------
ConvergenceLayer*
ConvergenceLayer::find_clayer(const char* name)
{
    CLVector::iterator iter;
    for (iter = CLVector::instance()->begin();
         iter != CLVector::instance()->end();
         ++iter)
    {
        if (strcasecmp(name, (*iter)->name()) == 0) {
            return *iter;
        }
    }

    return NULL;
}

//----------------------------------------------------------------------
bool
ConvergenceLayer::set_interface_defaults(int argc, const char* argv[],
                                         const char** invalidp)
{
    if (argc == 0) {
        return true;
    } else {
        invalidp = &argv[0];
        return false;
    }
}

//----------------------------------------------------------------------
bool
ConvergenceLayer::set_link_defaults(int argc, const char* argv[],
                                    const char** invalidp)
{
    if (argc == 0) {
        return true;
    } else {
        invalidp = &argv[0];
        return false;
    }
}

//----------------------------------------------------------------------
bool
ConvergenceLayer::interface_up(Interface* iface,
                               int argc, const char* argv[])
{
    (void)iface;
    (void)argc;
    (void)argv;
    log_debug("init interface %s", iface->name().c_str());
    return true;
}

//----------------------------------------------------------------------
bool
ConvergenceLayer::interface_down(Interface* iface)
{
    (void)iface;
    log_debug("stopping interface %s", iface->name().c_str());
    return true;
}

//----------------------------------------------------------------------
void
ConvergenceLayer::dump_interface(Interface* iface, oasys::StringBuffer* buf)
{
    (void)iface;
    (void)buf;
}

//----------------------------------------------------------------------
bool
ConvergenceLayer::init_link(const LinkRef& link, int argc, const char* argv[])
{
    (void)link;
    (void)argc;
    (void)argv;
    log_debug("init link %s", link->nexthop());
    return true;
}

//----------------------------------------------------------------------
void
ConvergenceLayer::delete_link(const LinkRef& link)
{
    ASSERT(link != NULL);
    ASSERT(!link->isdeleted());
    log_debug("ConvergenceLayer::delete_link: link %s deleted", link->name());
}

//----------------------------------------------------------------------
void
ConvergenceLayer::dump_link(const LinkRef& link, oasys::StringBuffer* buf)
{
    (void)link;
    (void)buf;
}

//----------------------------------------------------------------------
bool
ConvergenceLayer::reconfigure_link(const LinkRef& link,
                                   int argc, const char* argv[])
{
    (void)link;
    (void)argv;
    return (argc == 0);
}

//----------------------------------------------------------------------
bool
ConvergenceLayer::close_contact(const ContactRef& contact)
{
    (void)contact;
    log_debug("closing contact *%p", contact.object());
    return true;
}

} // namespace dtn
