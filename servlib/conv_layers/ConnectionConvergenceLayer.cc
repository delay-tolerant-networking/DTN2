/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2006 Intel Corporation. All rights reserved. 
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

#include <oasys/util/OptParser.h>

#include "ConnectionConvergenceLayer.h"
#include "CLConnection.h"
#include "bundling/BundleDaemon.h"

namespace dtn {

ConnectionConvergenceLayer::LinkParams
    ConnectionConvergenceLayer::default_link_params_;

//----------------------------------------------------------------------
ConnectionConvergenceLayer::LinkParams::LinkParams()
    : busy_queue_depth_(10),
      reactive_frag_enabled_(true),
      sendbuf_len_(32768),
      recvbuf_len_(32768),
      data_timeout_(30000), // msec
      test_read_delay_(0),
      test_write_delay_(0),
      test_recv_delay_(0)
{
}

//----------------------------------------------------------------------
ConnectionConvergenceLayer::ConnectionConvergenceLayer(const char* classname,
                                                       const char* cl_name)
    : ConvergenceLayer(classname, cl_name)
{
}

//----------------------------------------------------------------------
bool
ConnectionConvergenceLayer::parse_link_params(LinkParams* params, Link* link,
                                              int argc, const char** argv,
                                              const char** invalidp)
{
    (void)link;
    oasys::OptParser p;
    
    p.addopt(new oasys::UIntOpt("busy_queue_depth", &params->busy_queue_depth_));
    p.addopt(new oasys::BoolOpt("reactive_frag_enabled",
                                &params->reactive_frag_enabled_));
    p.addopt(new oasys::UIntOpt("sendbuf_len", &params->sendbuf_len_));
    p.addopt(new oasys::UIntOpt("recvbuf_len", &params->recvbuf_len_));
    p.addopt(new oasys::UIntOpt("data_timeout", &params->data_timeout_));

    p.addopt(new oasys::UIntOpt("test_read_delay",
                                &params->test_read_delay_));
    p.addopt(new oasys::UIntOpt("test_write_delay",
                                &params->test_write_delay_));
    p.addopt(new oasys::UIntOpt("test_recv_delay",
                                &params->test_recv_delay_));
    
    if (! p.parse(argc, argv, invalidp)) {
        return false;
    }
    
    if (params->sendbuf_len_ == 0) {
        *invalidp = "sendbuf_len must not be zero";
        return false;
    }

    if (params->recvbuf_len_ == 0) {
        *invalidp = "recvbuf_len must not be zero";
        return false;
    }
    
    return true;
}


//----------------------------------------------------------------------
bool
ConnectionConvergenceLayer::init_link(Link* link, int argc, const char* argv[])
{
    log_debug("adding %s link %s", link->type_str(), link->nexthop());

    // Create a new parameters structure, parse the options, and store
    // them in the link's cl info slot.
    LinkParams* params = new_link_params();
    
    const char* invalid;
    if (! parse_link_params(params, link,argc, argv, &invalid)) {
        log_err("error parsing link options: invalid option '%s'", invalid);
        delete params;
        return false;
    }

    link->set_cl_info(params);

    return true;
}

//----------------------------------------------------------------------
bool
ConnectionConvergenceLayer::reconfigure_link(Link* link,
                                             int argc, const char* argv[])
{
    LinkParams* params = dynamic_cast<LinkParams*>(link->cl_info());
    ASSERT(params != NULL);
    
    const char* invalid;
    if (! parse_link_params(params, link, argc, argv, &invalid)) {
        log_err("reconfigure_link: invalid parameter %s", invalid);
        return false;
    }
        
    return true;
}

//----------------------------------------------------------------------
bool
ConnectionConvergenceLayer::open_contact(const ContactRef& contact)
{
    Link* link = contact->link();
    log_debug("opening contact on link *%p", link);
    
    LinkParams* params = dynamic_cast<LinkParams*>(link->cl_info());
    ASSERT(params != NULL);
    
    // create a new connection for the contact
    CLConnection* conn = new_connection(params);
    conn->set_contact(contact);
    contact->set_cl_info(conn);
    conn->start();

    return true;
}

//----------------------------------------------------------------------
bool
ConnectionConvergenceLayer::close_contact(const ContactRef& contact)
{
    log_info("close_contact *%p", contact.object());

    CLConnection* conn = dynamic_cast<CLConnection*>(contact->cl_info());
    ASSERT(conn != NULL);
    
    conn->cmdqueue_.push_back(
        CLConnection::CLMsg(CLConnection::CLMSG_BREAK_CONTACT));
    
    while (!conn->is_stopped()) {
        log_debug("waiting for connection thread to stop...");
        usleep(100000);
        oasys::Thread::yield();
    }

    conn->close_contact();
    delete conn;

    contact->set_cl_info(NULL);

    return true;
}

//----------------------------------------------------------------------
void
ConnectionConvergenceLayer::send_bundle(const ContactRef& contact,
                                        Bundle* bundle)
{
    log_debug("send_bundle *%p to *%p", bundle, contact.object());

    CLConnection* conn = dynamic_cast<CLConnection*>(contact->cl_info());
    ASSERT(conn != NULL);

    LinkParams* params = dynamic_cast<LinkParams*>(contact->link()->cl_info());
    ASSERT(params != NULL);
    
    conn->cmdqueue_.push_back(
        CLConnection::CLMsg(CLConnection::CLMSG_SEND_BUNDLE, bundle));
    
    // to prevent the queue from filling up, set the busy state to
    // apply backpressure
    if ((conn->cmdqueue_.size() + 1) >= params->busy_queue_depth_)
    {
        contact->link()->set_state(Link::BUSY);
    }
}

} // namespace dtn
