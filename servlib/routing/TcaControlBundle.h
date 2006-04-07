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

#ifndef _TCA_CONTROLBUNDLE_H_
#define _TCA_CONTROLBUNDLE_H_


// Warning: This file is included by both the dtnd and the tca_admin app.
// Any changes here must be okay for both apps.

#include <string>
#include <vector>


// TODO: Make individual subclass for each TcaControlBundle type
// and add strong argument checking (number of args, syntax, even semantic
// correctness in some cases)

// Note: args can be appended manually, using args_.push_back(arg)

class TcaControlBundle
{
public:

    enum TypeCode {
        CB_NULL,
        CB_ADV,
        CB_ADV_SENT,
        CB_ASK,
        CB_ASK_RECEIVED,
        CB_ASK_SENT,
        CB_COA,
        CB_COA_SENT,
        CB_REG_RECEIVED,
        CB_ROUTES,
        CB_UNB,
        CB_LINK_ANNOUNCE,
        CB_LINK_AVAILABLE,
        CB_LINK_UNAVAILABLE,
        CB_CONTACT_UP,
        CB_CONTACT_DOWN,
        CB_UNKNOWN              // error: unrecognized code
    };

    TypeCode type_;
    std::string code_;
    std::vector<std::string> args_;

    TcaControlBundle() : type_(CB_NULL), code_(),  args_() { };

    // construct from Bundle payload, parsing out code and args
    // you can also just give a code here to construct an argless bundle
    TcaControlBundle(const std::string& payload);
    
    virtual ~TcaControlBundle() { }

    // retrieve as string (suitable for sending as bundle payload)
    virtual std::string str() const;

    void dump(const std::string& intro) const;

    // todo: remove this from public interface someday
    static std::string eat_to_tab(std::string& s);

protected:

    static bool parse_payload(const std::string& payload,
                              TypeCode& type,
                              std::string& code, std::string& body);

};


// A TcaWrappedBundle is a TcaControlBundle that includes a source and
// dest field as args_[0] and args_[1] respectively. This is useful for
// "tunnelling" regular addressed bundles over the control API, preserving
// their source and dest fields.


class TcaWrappedBundle : public TcaControlBundle
{
public:

    // construct directly from string
    TcaWrappedBundle(const std::string& payload) : TcaControlBundle(payload) {}
    
    // construct from existing ControlBundle
    TcaWrappedBundle(const TcaControlBundle& cb) : TcaControlBundle(cb) {}

    // construct empty but addressed bundle
    TcaWrappedBundle(const std::string& code,
                     const std::string& src, const std::string& dest);

    const std::string source() const;
    const std::string dest() const;

    void append_arg(const std::string& arg) { args_.push_back(arg); }
};


#endif /* _TCA_CONTROLBUNDLE_H_ */
