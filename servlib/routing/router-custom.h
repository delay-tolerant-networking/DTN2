/*
 * License Agreement
 * 
 * NOTICE
 * This software (or technical data) was produced for the U. S.
 * Government under contract W15P7T-05-C-F600, and is
 * subject to the Rights in Data-General Clause 52.227-14 (JUNE 1987)
 * 
 * Copyright (C) 2006. The MITRE Corporation (http://www.mitre.org/).
 * All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * The US Government will not be charged any license fee and/or
 * royalties related to this software.
 * 
 * * Neither name of The MITRE Corporation; nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ROUTER_CUSTOM_H_
#define _ROUTER_CUSTOM_H_

#include <config.h>
#ifdef XERCES_C_ENABLED

#include "router.h"
#include "RouteEntry.h"
#include <bundling/Bundle.h>
#include <contacts/Link.h>
#include <reg/Registration.h>

namespace dtn {
namespace rtrmessage {

class linkType : public linkType_base
{
public:
    linkType (const remote_eid::type&,
              const type::type_&,
              const nexthop::type&,
              const name::type&,
              const state::type&,
              const reliable::type&,
              const clayer::type&,
              const min_retry_interval::type&,
              const max_retry_interval::type&,
              const idle_close_time::type&);
  
    linkType (const ::xercesc::DOMElement&,
              ::xml_schema::flags = 0,
              ::xml_schema::type* = 0);

    linkType (const linkType&,
              ::xml_schema::flags = 0,
              ::xml_schema::type* = 0);

    linkType (Link*);

    virtual linkType*
    _clone (::xml_schema::flags = 0,
            ::xml_schema::type* = 0) const;
};

class bundleType : public bundleType_base
{
public:
    bundleType (const source::type&,
                const dest::type&,
                const custodian::type&,
                const replyto::type&,
                const prevhop::type&,
                const length::type&,
                const location::type&,
                const bundleid::type&,
                const is_fragment::type&,
                const is_admin::type&,
                const do_not_fragment::type&,
                const priority::type&,
                const custody_requested::type&,
                const local_custody::type&,
                const singleton_dest::type&,
                const custody_rcpt::type&,
                const receive_rcpt::type&,
                const forward_rcpt::type&,
                const delivery_rcpt::type&,
                const deletion_rcpt::type&,
                const app_acked_rcpt::type&,
                const creation_ts_seconds::type&,
                const creation_ts_seqno::type&,
                const expiration::type&,
                const orig_length::type&,
                const frag_offset::type&,
                const owner::type&);

    bundleType (const ::xercesc::DOMElement&,
                ::xml_schema::flags = 0,
                ::xml_schema::type* = 0);

    bundleType (const bundleType&,
                ::xml_schema::flags = 0,
                ::xml_schema::type* = 0);

    bundleType (Bundle*);

    virtual bundleType*
    _clone (::xml_schema::flags = 0,
            ::xml_schema::type* = 0) const;

private:
    const char * location_to_str(int location);
};

class contactType : public contactType_base
{
public:
    contactType (const link::type&,
                 const start_time_sec::type&,
                 const start_time_usec::type&,
                 const duration::type&,
                 const bps::type&,
                 const latency::type&);

    contactType (const ::xercesc::DOMElement&,
                 ::xml_schema::flags = 0,
                 ::xml_schema::type* = 0);

    contactType (const contactType&,
                 ::xml_schema::flags = 0,
                 ::xml_schema::type* = 0);

    contactType (Contact*);

    virtual contactType*
    _clone (::xml_schema::flags = 0,
            ::xml_schema::type* = 0) const;
};

class routeEntryType : public routeEntryType_base
{
public:
    routeEntryType (const dest_pattern::type&,
                    const source_pattern::type&,
                    const route_priority::type&,
                    const action::type&,
                    const link::type&);

    routeEntryType (const ::xercesc::DOMElement&,
                    ::xml_schema::flags = 0,
                    ::xml_schema::type* = 0);

    routeEntryType (const routeEntryType&,
                    ::xml_schema::flags = 0,
                    ::xml_schema::type* = 0);

    routeEntryType (RouteEntry*);

    virtual routeEntryType*
    _clone (::xml_schema::flags = 0,
            ::xml_schema::type* = 0) const;
};

class registrationType : public registrationType_base
{
public:
    registrationType (const endpoint::type&,
                      const regid::type&,
                      const action::type&,
                      const script::type&,
                      const expiration::type&);

    registrationType (const ::xercesc::DOMElement&,
                      ::xml_schema::flags = 0,
                      ::xml_schema::type* = 0);

    registrationType (const registrationType&,
                      ::xml_schema::flags = 0,
                      ::xml_schema::type* = 0);

    registrationType (Registration*);

    virtual registrationType*
    _clone (::xml_schema::flags = 0,
            ::xml_schema::type* = 0) const;
};

class to_lower
{
public:
    char operator() (char c) const
    {
        return tolower(c);
    }
};

} // namespace rtrmessage
} // namespace dtn

#endif // XERCES_C_ENABLED
#endif // _ROUTER_CUSTOM_H_
