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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "InterfaceTable.h"
#include "conv_layers/ConvergenceLayer.h"
#include "oasys/util/StringBuffer.h"

namespace dtn {

InterfaceTable* InterfaceTable::instance_ = NULL;

InterfaceTable::InterfaceTable()
    : Logger("InterfaceTable", "/dtn/interface/table")
{
}

InterfaceTable::~InterfaceTable()
{
    //NOTREACHED;
    printf("Deleting InterfaceTable\n");
    InterfaceList::iterator iter;
    for(iter = iflist_.begin();iter != iflist_.end(); iter++) {
        (*iter)->clayer()->interface_down(*iter);
        delete *iter;
    }

}
void InterfaceTable::shutdown() {
    printf("In InterfaceTable::shutdown\n");
    delete instance_;
}

/**
 * Internal method to find the location of the given interface in the
 * list.
 */
bool
InterfaceTable::find(const std::string& name,
                     InterfaceList::iterator* iter)
{
    Interface* iface;
    for (*iter = iflist_.begin(); *iter != iflist_.end(); ++(*iter)) {
        iface = **iter;
        
        if (iface->name() == name) {
            return true;
        }
    }        
    
    return false;
}

/**
 * Create and add a new interface to the table. Returns true if
 * the interface is successfully added, false if the interface
 * specification is invalid.
 */
bool
InterfaceTable::add(const std::string& name,
                    ConvergenceLayer* cl,
                    const char* proto,
                    int argc, const char* argv[])
{
    InterfaceList::iterator iter;
    
    if (find(name, &iter)) {
        log_err("interface %s already exists", name.c_str());
        return false;
    }
    
    log_info("adding interface %s (%s)", name.c_str(), proto);

    Interface* iface = new Interface(name, proto, cl);
    if (! cl->interface_up(iface, argc, argv)) {
        log_err("convergence layer error adding interface %s", name.c_str());
        delete iface;
        return false;
    }

    iflist_.push_back(iface);

    return true;
}

/**
 * Remove the specified interface.
 */
bool
InterfaceTable::del(const std::string& name)
{
    InterfaceList::iterator iter;
    Interface* iface;
    bool retval = false;
    
    log_info("removing interface %s", name.c_str());

    if (! find(name, &iter)) {
        log_err("error removing interface %s: no such interface",
                name.c_str());
        return false;
    }

    iface = *iter;
    iflist_.erase(iter);

    if (iface->clayer()->interface_down(iface)) {
        retval = true;
    } else {
        log_err("error deleting interface %s from the convergence layer.",
                name.c_str());
        retval = false;
    }

    delete iface;
    return retval;
}

/**
 * Dumps the interface table into a string.
 */
void
InterfaceTable::list(oasys::StringBuffer *buf)
{
    InterfaceList::iterator iter;
    Interface* iface;

    for (iter = iflist_.begin(); iter != iflist_.end(); ++(iter)) {
        iface = *iter;
        buf->appendf("%s: Convergence Layer: %s\n",
                     iface->name().c_str(), iface->proto().c_str());
        iface->clayer()->dump_interface(iface, buf);
        buf->append("\n");
    }
}

} // namespace dtn
