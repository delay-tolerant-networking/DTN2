/*
 *    Copyright 2006 Baylor University
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

#include <oasys/util/OptParser.h>
#include "bundling/BundleDaemon.h"
#include "contacts/ContactManager.h"
#include "IPAnnounce.h"
#include "conv_layers/IPConvergenceLayerUtils.h"
#include "conv_layers/TCPConvergenceLayer.h"
#include "conv_layers/UDPConvergenceLayer.h"

namespace dtn {

IPAnnounce::IPAnnounce()
    : cl_addr_(INADDR_ANY), cl_port_(TCPConvergenceLayer::TCPCL_DEFAULT_PORT)
{
}

bool
IPAnnounce::configure(const std::string& name, ConvergenceLayer* cl, 
                      int argc, const char* argv[])
{
    if (cl == NULL) return false;

    cl_ = cl;
    name_ = name;
    type_.assign(cl->name());

    // validate convergence layer details
    if (strncmp(cl_->name(),"tcp",3) != 0 &&
        strncmp(cl_->name(),"udp",3) != 0)
    {
        log_err("ip announce does not support cl type %s",
                cl_->name());
        return false;
    }

    // parse the options
    oasys::OptParser p;
    p.addopt(new oasys::InAddrOpt("cl_addr",&cl_addr_));
    p.addopt(new oasys::UInt16Opt("cl_port",&cl_port_));
    p.addopt(new oasys::UIntOpt("interval",&interval_));

    const char* invalid;
    if (! p.parse(argc, argv, &invalid))
    {
        log_err("bad parameter %s",invalid);
        return false;
    }

    if (interval_ == 0)
    {
        log_err("interval must be greater than 0");
        return false;
    }

    // convert from seconds to ms
    interval_ *= 1000;

    oasys::StringBuffer buf("%s:%d",intoa(cl_addr_),cl_port_);
    local_.assign(buf.c_str());
    return true;
}

size_t
IPAnnounce::format_advertisement(u_char* bp, size_t len)
{
    EndpointID local(BundleDaemon::instance()->local_eid());
    size_t length = FOUR_BYTE_ALIGN(local.length() + sizeof(DiscoveryHeader));

    if (len <= length)
        return 0;

    DiscoveryHeader* hdr = (DiscoveryHeader*) bp;
    memset(hdr,0,len);

    hdr->cl_type = IPDiscovery::str_to_type(type().c_str());
    hdr->interval = interval_ / 100;
    hdr->length = htons(length);
    hdr->inet_addr = cl_addr_;
    hdr->inet_port = htons(cl_port_);
    hdr->name_len = htons(local.length());

    memcpy(hdr->sender_name,local.c_str(),local.length());
    ::gettimeofday(&data_sent_,0);
    return length;
}

void
IPAnnounce::handle_neighbor_discovered(const std::string& nexthop,
                                       const EndpointID& remote_eid)
{
    (void)remote_eid;
    in_addr_t remote_addr;
    u_int16_t remote_port;

    if (! IPConvergenceLayerUtils::parse_nexthop(logpath_, nexthop.c_str(),
                                                 &remote_addr, &remote_port))
        return;

    ContactManager* cm = BundleDaemon::instance()->contactmgr();
    Link* link = cm->find_link_to(cl_, nexthop);

    if (link != NULL)
        return; // XXX/wilson do we want to pass along the signal regardless?

    if (type() == "tcp")
    {
        TCPConvergenceLayer* tcpcl =
            dynamic_cast<TCPConvergenceLayer*>(cl_);
        ASSERT(tcpcl != NULL);

        TCPConvergenceLayer::TCPLinkParams* params =
            dynamic_cast<TCPConvergenceLayer::TCPLinkParams*>
                (tcpcl->new_link_params());

        params->remote_addr_ = remote_addr;
        params->remote_port_ = remote_port;

        CLConnection* conn = tcpcl->new_connection(params);
        conn->start();
    }
    else
    if (type() == "udp")
    {
        log_warn("UDP beacon received from %s but UDP not yet supported",
                 nexthop.c_str());
    }
    else
    {
        PANIC("unsupported CL type in IPAnnounce: %s",type().c_str());
    }
}

} // dtn
