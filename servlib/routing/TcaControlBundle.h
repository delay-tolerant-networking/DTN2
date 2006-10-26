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


#ifndef _TCA_CONTROLBUNDLE_H_
#define _TCA_CONTROLBUNDLE_H_


// Warning: This file is included by both the dtnd and the tca_admin app.
// Any changes here must be okay for both apps.

#include <string>
#include <vector>

namespace dtn {

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

} // namespace dtn

#endif /* _TCA_CONTROLBUNDLE_H_ */
