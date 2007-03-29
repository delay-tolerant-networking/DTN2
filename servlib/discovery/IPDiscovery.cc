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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <oasys/util/OptParser.h>
#include <oasys/util/StringBuffer.h>
#include "bundling/BundleDaemon.h"
#include "IPDiscovery.h"
#include "IPAnnounce.h"

extern int errno;

namespace dtn {

IPDiscovery::IPDiscovery(const std::string& name)
    : Discovery(name,"ip"),
      oasys::Thread("IPDiscovery")
{
    remote_addr_ = 0xffffffff;
    local_addr_ = INADDR_ANY;
    mcast_ttl_ = 1;
    port_ = 0;
    shutdown_ = false;

    socket_.logpathf("%s/sock", logpath_);
}

bool
IPDiscovery::configure(int argc, const char* argv[])
{
    if (oasys::Thread::started())
    {
        log_warn("reconfiguration of IPDiscovery not supported");
        return false;
    }

    oasys::OptParser p;

    bool portSet = false;
    bool unicast = false;
    p.addopt(new oasys::UInt16Opt("port",&port_,"","",&portSet));
    p.addopt(new oasys::InAddrOpt("addr",&remote_addr_));
    p.addopt(new oasys::InAddrOpt("local_addr",&local_addr_));
    p.addopt(new oasys::UIntOpt("multicast_ttl",&mcast_ttl_));
    p.addopt(new oasys::BoolOpt("unicast",&unicast));

    const char* invalid;
    if (! p.parse(argc,argv,&invalid))
    {
        log_err("bad option for IP discovery: %s",invalid);
        return false;
    }

    if (! portSet)
    {
        log_err("must specify port");
        return false;
    }

    // Assume everything is broadcast unless unicast flag is set
    // or if multicast address is set
    if (! unicast)
    {
        socket_.set_remote_addr(remote_addr_);
        in_addr_t mcast_addr = inet_addr("224.0.0.0");
        // infer multicast option from remote address
        if (mcast_addr & remote_addr_ == mcast_addr)
        {
            socket_.params_.multicast_ = true;
            socket_.params_.mcast_ttl_ = mcast_ttl_;
        }
        // everything else is assumed broadcast
        else
        {
            socket_.params_.broadcast_ = true;
        }
    }

    // allow new announce registration to upset poll() in run() below
    socket_.set_notifier(new oasys::Notifier(socket_.logpath()));

    // configure base class variables 
    oasys::StringBuffer buf("%s:%d",intoa(local_addr_),port_);
    local_.assign(buf.c_str());

    oasys::StringBuffer to("%s:%d",intoa(remote_addr_),port_);
    to_addr_.assign(to.c_str());

    if (socket_.bind(local_addr_,port_) != 0)
    {
        log_err("bind failed");
        return false;
    }

    log_debug("starting thread"); 
    start();

    return true;
}

void
IPDiscovery::run()
{
    log_debug("discovery thread running");
    oasys::ScratchBuffer<u_char*> buf(1024);
    u_char* bp = buf.buf(1024);

    size_t len = 0;
    int cc = 0;

    while (true)
    {
        if (shutdown_) break;

        // only send out beacon(s) once per interval
        u_int min_diff = INT_MAX;
        for (iterator iter = list_.begin(); iter != list_.end(); iter++)
        {
            IPAnnounce* announce = dynamic_cast<IPAnnounce*>(*iter);
            u_int remaining = announce->interval_remaining();
            if (remaining == 0)
            {
                log_debug("announce ready for sending");
                len = announce->format_advertisement(bp,1024);
                cc = socket_.sendto((char*)bp,len,0,remote_addr_,port_);
                if (cc != (int) len)
                {
                    log_err("sendto failed: %s (%d)",
                            strerror(errno),errno);

                    // quit thread on error
                    return;
                }
                min_diff = announce->interval();
            }
            else
            {
                log_debug("announce not ready: %u ms remaining", remaining);
                if (remaining < min_diff) {
                    min_diff = announce->interval_remaining();
                }
            }
        }

        // figure out whatever time is left (if poll has already fired within
        // interval ms) and set up poll timeout
        u_int timeout = min_diff;

        log_debug("polling on socket: timeout %u", timeout);
        cc = socket_.poll_sockfd(POLLIN,NULL,timeout);

        if (shutdown_) {
            log_debug("shutdown bit set, exiting thread");
            break;
        }

        // if timeout, then flip back around and send beacons
        if (cc == oasys::IOTIMEOUT || cc == oasys::IOINTR)
        {
            continue;
        }
        else if (cc < 0)
        {
            log_err("poll error (%d): %s (%d)",
                    cc, strerror(errno), errno);
            return;
        }
        else if (cc == 1)
        {
            in_addr_t remote_addr;
            u_int16_t remote_port;

            cc = socket_.recvfrom((char*)bp,1024,0,&remote_addr,&remote_port);
            if (cc < 0)
            {
                log_err("error on recvfrom (%d): %s (%d)",cc,
                        strerror(errno),errno);
                return;
            }

            EndpointID remote_eid;
            u_int8_t cl_type;
            std::string nexthop;
            if (!parse_advertisement(bp,cc,remote_addr,cl_type,nexthop,
                                     remote_eid))
            {
                log_warn("unable to parse beacon from %s:%d",
                         intoa(remote_addr),remote_port);
                return;
            }

            if (remote_eid.equals(BundleDaemon::instance()->local_eid()))
            {
                log_debug("ignoring beacon from self (%s:%d)",
                          intoa(remote_addr),remote_port);
                continue;
            }

            // distribute to all beacons registered for this CL type
            handle_neighbor_discovered(
                    IPDiscovery::type_to_str((IPDiscovery::cl_type_t)cl_type),
                    nexthop, remote_eid);
        }
        else
        {
            PANIC("unexpected result from poll (%d)",cc);
        }
    }
}

bool
IPDiscovery::parse_advertisement(u_char* bp, size_t len,
                                 in_addr_t remote_addr, u_int8_t& cl_type,
                                 std::string& nexthop, EndpointID& remote_eid)
{
    if (len <= sizeof(DiscoveryHeader))
        return false;

    DiscoveryHeader* hdr = (DiscoveryHeader*) bp;
    size_t length = ntohs(hdr->length);
    if (len < length)
        return false;

    in_addr_t cl_addr;
    u_int16_t cl_port;

    cl_type = hdr->cl_type;
    cl_addr = (hdr->inet_addr == INADDR_ANY) ? remote_addr : hdr->inet_addr;
    cl_port = ntohs(hdr->inet_port);

    oasys::StringBuffer buf("%s:%d",intoa(cl_addr),cl_port);
    nexthop.assign(buf.c_str());

    size_t name_len = ntohs(hdr->name_len);
    std::string eidstr(hdr->sender_name,name_len);

    return remote_eid.assign(eidstr);
}

} // namespace dtn
