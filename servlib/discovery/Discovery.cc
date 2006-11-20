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

#include <config.h>

#include <oasys/util/Options.h>
#include <oasys/util/StringBuffer.h>
#include "IPDiscovery.h"

#ifdef OASYS_BLUETOOTH_ENABLED
#include "BluetoothDiscovery.h"
#endif

#include "Announce.h"
#include "Discovery.h"

namespace dtn {

Discovery::Discovery(const std::string& name,
                     const std::string& af)
    : oasys::Logger("Discovery","/dtn/discovery/%s",af.c_str()),
      name_(name), af_(af)
{
}

Discovery*
Discovery::create_discovery(const std::string& name,
                            const std::string& af,
                            int argc, const char* argv[])
{
    Discovery* disc = NULL;
    if (af == "ip")
    {
        disc = new IPDiscovery(name);
    }
#ifdef OASYS_BLUETOOTH_ENABLED 
    else
    if (af == "bt")
    {
        disc = new BluetoothDiscovery(name);
    }
#endif
    else
    {
        // not a recognized address family
        return NULL;
    }

    if (! disc->configure(argc,argv))
    {
        delete disc;
        return NULL;
    }

    return disc;
}

Discovery::~Discovery()
{
    for (iterator i = list_.begin(); i != list_.end(); i++)
    {
        delete (*i);
    }
}

void
Discovery::dump(oasys::StringBuffer* buf)
{
    buf->appendf("%s af %s: %zu announce %s\n",
                 name_.c_str(),af_.c_str(),list_.size(),to_addr_.c_str());
    for (iterator i = list_.begin(); i != list_.end(); i++)
    {
        buf->appendf("\tannounce %s type %s advertising %s every %d sec\n",
                     (*i)->name().c_str(),
                     (*i)->type().c_str(),
                     (*i)->local_addr().c_str(),
                     (*i)->interval()/1000);
    }
}

bool
Discovery::announce(const char* name, int argc, const char* argv[])
{
    iterator iter;
    if (find(name,&iter))
    {
        log_err("discovery for %s already exists",name);
        return false;
    }

    if (argc < 1)
    {
        log_err("cl type not specified");
        return false;
    }

    const char* cltype = argv[0];
    ConvergenceLayer* cl = ConvergenceLayer::find_clayer(cltype);
    if (cl == NULL)
    {
        log_err("invalid convergence layer type (%s)",cltype);
        return false;
    }

    Announce* announce = Announce::create_announce(name,cl,argc,argv);
    if (announce == NULL)
    {
        log_err("no announce implemented for %s convergence layer",cltype);
        return false;
    }

    list_.push_back(announce);
    // alert derived classes to new registration
    handle_announce();

    return true;
}

bool
Discovery::remove(const char* name)
{
    iterator iter;

    if (! find(name,&iter))
    {
        log_err("error removing announce %s: no such object",name);
        return false;
    }

    Announce *announce = *iter;
    list_.erase(iter);
    delete announce;
    return true;
}

void
Discovery::handle_neighbor_discovered(const std::string& type,
                                      const std::string& cl_addr,
                                      const EndpointID& remote_eid)
{
    for (iterator i = list_.begin(); i != list_.end(); i++)
    {
        if ((*i)->type() == type)
        {
            (*i)->handle_neighbor_discovered(cl_addr,remote_eid);
        }
    }
}

bool
Discovery::find(const char* name, Discovery::iterator* iter)
{
    Announce* announce;
    for (*iter = list_.begin(); *iter != list_.end(); (*iter)++)
    {
        announce = **iter;
        if (announce->name() == name)
        {
            return true; 
        }
    }
    return false;
}

};
