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
#include "ConvergenceLayer.h"
#include "TCPConvergenceLayer.h"
#include "UDPConvergenceLayer.h"
#include "FileConvergenceLayer.h"

namespace dtn {

ConvergenceLayer::~ConvergenceLayer()
{
}

ConvergenceLayer::Protocol* ConvergenceLayer::protocol_list_ = NULL;

void
ConvergenceLayer::add_clayer(const char* proto, ConvergenceLayer* cl)
{
    Protocol* p = new Protocol();
    p->proto_ = proto;
    p->cl_ = cl;

    p->next_ = protocol_list_;
    protocol_list_ = p;
}

void
ConvergenceLayer::init_clayers()
{
    add_clayer("tcp", new TCPConvergenceLayer());
    add_clayer("udp", new UDPConvergenceLayer());
    add_clayer("file", new FileConvergenceLayer());
}

ConvergenceLayer*
ConvergenceLayer::find_clayer(const char* proto)
{
    Protocol* p;
    for (p = protocol_list_; p != NULL; p = p->next_)
    {
        if (strcasecmp(proto, p->proto_) == 0) {
            return p->cl_;
        }
    }
    return NULL;
}


/**
 * Register a new interface.
 *
 * The default implementation doesn't do anything but log.
 */
bool
ConvergenceLayer::add_interface(Interface* iface,
                                int argc, const char* argv[])
{
    log_debug("adding interface %s", iface->admin().c_str());
    return true;
}

/**
 * Remove an interface
 */
bool
ConvergenceLayer::del_interface(Interface* iface)
{
    log_debug("removing interface %s", iface->admin().c_str());
    return true;
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
 * Open the connection to the given contact and prepare for
 * bundles to be transmitted.
 */
bool
ConvergenceLayer::open_contact(Contact* contact)
{
    log_debug("opening contact *%p", contact);
    return true;
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

/**
 * Try to send the bundles queued up for the given contact. In
 * some cases (e.g. tcp) this is a no-op because open_contact spun
 * a thread which is blocked on the bundle list associated with
 * the contact. In others (e.g. file) there is no thread, so this
 * callback is used to send the bundle.
 */
void
ConvergenceLayer::send_bundles(Contact* contact)
{
    log_debug("sending bundles for contact *%p", contact);
}

} // namespace dtn
