/*
 *    Copyright 2005-2006 University of Waterloo
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

#include "TcaControlBundle.h"
#include "../../servlib/bundling/Bundle.h"


namespace dtn {


///////////////////////////////////////////////////////////////////////////////
// class TcaControlBundle


TcaControlBundle::TcaControlBundle(const std::string& payload)
{
    std::string s;
    parse_payload(payload, type_, code_, s);

    while (s.length() > 0)
    {
        args_.push_back(TcaControlBundle::eat_to_tab(s));
    }   
}


std::string
TcaControlBundle::str() const
{
    std::string s;
    s = code_;
    s += ":";
    
    int n = args_.size();
    if (n>=1) s += args_[0];
    for (int i=1; i<n; ++i)
    {
        s += "\t";
        s += args_[i];
    }

    return s;
}


bool
TcaControlBundle::parse_payload(const std::string& payload, TypeCode& type,
                                std::string& code, std::string& body)
{
    code = "";

    if (payload.length() == 0) return false;

    std::string::size_type colon = payload.substr().find(':');
    if (colon == std::string::npos)
    {
        // no colon -- assume it's a zero-operand code
        code = payload;
    }
    else
    {
        code = payload.substr(0,colon);
        body = payload.substr(colon+1, payload.length());
    }

    if      (code == "adv") type = CB_ADV;
    else if (code == "adv_sent") type = CB_ADV_SENT;
    else if (code == "ask") type = CB_ASK;
    else if (code == "ask_received") type = CB_ASK_RECEIVED;
    else if (code == "ask_sent") type = CB_ASK_SENT;
    else if (code == "coa") type = CB_COA;
    else if (code == "coa_sent") type = CB_COA_SENT;
    else if (code == "reg_received") type = CB_REG_RECEIVED;
    else if (code == "routes") type = CB_ROUTES;
    else if (code == "unb") type = CB_UNB;
    else if (code == "link_announce") type = CB_LINK_ANNOUNCE;
    else if (code == "link_available") type = CB_LINK_AVAILABLE;
    else if (code == "link_unavailable") type = CB_LINK_UNAVAILABLE;
    else if (code == "contact_up") type = CB_CONTACT_UP;
    else if (code == "contact_down") type = CB_CONTACT_DOWN;
    else type = CB_UNKNOWN;

    return true;
}


void
TcaControlBundle::dump(const std::string& intro) const
{
    printf("%s code='%s', args=%u\n", intro.c_str(), code_.c_str(),
           (u_int)args_.size());
    for (unsigned int i=0; i<args_.size(); ++i)
    {
        printf("    '%s'\n", args_[i].c_str());
    }
}


std::string
TcaControlBundle::eat_to_tab(std::string& s)
{   
    // return the first part of s, up to the first tab char or end of string
    // consuming it (ie. removing it and the tab from the front of s
    
    std::string::size_type tab = s.find('\t');
    if (tab == std::string::npos)
    {   
        std::string left = s;
        s = "";
        return left;
    }

    else
    {
        std::string left = s.substr(0,tab);
        s = s.substr(tab+1, s.length());
        return left;
    }
}


///////////////////////////////////////////////////////////////////////////////
// class TcaControlBundle


TcaWrappedBundle::TcaWrappedBundle(const std::string& code,
                                   const std::string& src,
                                   const std::string& dest)
    : TcaControlBundle(code)
{
    args_.push_back(src);
    args_.push_back(dest);
}


const std::string
TcaWrappedBundle::source() const
{
    if (args_.size() < 1) return "";
    return args_[0];
}


const std::string
TcaWrappedBundle::dest() const
{
    if (args_.size() < 2) return "";
    return args_[1];
}


} // namespace dtn
