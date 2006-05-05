/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * University of Waterloo Open Source License
 * 
 * Copyright (c) 2005 University of Waterloo. All rights reserved. 
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
 *   Neither the name of the University of Waterloo nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY
 * OF WATERLOO OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


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
