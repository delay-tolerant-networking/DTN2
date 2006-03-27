/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2005 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
    NOTREACHED;
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
