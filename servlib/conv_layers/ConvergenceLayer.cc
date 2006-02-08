/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
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
#include <config.h>
#include "ConvergenceLayer.h"
#include "EthConvergenceLayer.h"
#include "FileConvergenceLayer.h"
#include "NullConvergenceLayer.h"
#include "TCPConvergenceLayer.h"
#include "UDPConvergenceLayer.h"
#include "BluetoothConvergenceLayer.h"

namespace dtn {

ConvergenceLayer::CLVec ConvergenceLayer::clayers_;

ConvergenceLayer::~ConvergenceLayer()
{
}

void
ConvergenceLayer::add_clayer(ConvergenceLayer* cl)
{
    clayers_.push_back(cl);
}
    
void
ConvergenceLayer::init_clayers()
{
    add_clayer(new NullConvergenceLayer());
    add_clayer(new TCPConvergenceLayer());
    add_clayer(new UDPConvergenceLayer());
#ifdef XXX_demmer_fixme__linux
    add_clayer(new EthConvergenceLayer());
#endif
#ifdef OASYS_BLUETOOTH_ENABLED
    add_clayer(new BluetoothConvergenceLayer());
#endif
    // XXX/demmer fixme
    // add_clayer("file", new FileConvergenceLayer());
}

ConvergenceLayer*
ConvergenceLayer::find_clayer(const char* name)
{
    CLVec::iterator iter;
    for (iter = clayers_.begin(); iter != clayers_.end(); ++iter)
    {
        if (strcasecmp(name, (*iter)->name()) == 0) {
            return *iter;
        }
    }

    return NULL;
}


/**
 * Bring up a new interface.
 *
 * The default implementation doesn't do anything but log.
 */
bool
ConvergenceLayer::interface_up(Interface* iface,
                               int argc, const char* argv[])
{
    log_debug("init interface %s", iface->name().c_str());
    return true;
}

/**
 * Bring down the interface.
 */
bool
ConvergenceLayer::interface_down(Interface* iface)
{
    log_debug("stopping interface %s", iface->name().c_str());
    return true;
}

/**
 * Dump out CL specific interface information.
 */
void
ConvergenceLayer::dump_interface(Interface* iface, oasys::StringBuffer* buf)
{
}

/**
 * Create any CL-specific components of the Link.
 *
 * The default implementation doesn't do anything but log.
 */
bool
ConvergenceLayer::init_link(Link* link, int argc, const char* argv[])
{
    log_debug("init link %s", link->nexthop());
    return true;
}

/**
 * Dump out CL specific link information.
 */
void
ConvergenceLayer::dump_link(Link* link, oasys::StringBuffer* buf)
{
}

/**
 * Close the connnection to the contact.
 */
bool
ConvergenceLayer::close_contact(Contact* contact)
{
    log_debug("closing contact *%p", contact);
    return true;
}

} // namespace dtn
