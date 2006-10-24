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


#ifndef _TCA_ENDPOINTID_H_
#define _TCA_ENDPOINTID_H_


// Warning: 
// There are 2 versions of the TcaEndpointID class, one which is a 
// subclass of EndpointID and is to be used on the servlib side.
// The other is a stand-alone class to be used on the applib side.
// The interface and functionality is otherwise the same.
// It would be nice to merge these into a single class someday.


#include <string>
#include "../../servlib/naming/EndpointID.h"

namespace dtn {


class TcaEndpointID : public EndpointID
{
public:
    TcaEndpointID() : EndpointID(), host_(""), app_("") { }
    TcaEndpointID(const EndpointID& eid);
    TcaEndpointID(const std::string& str);
    TcaEndpointID(const std::string& host, const std::string& app);
    TcaEndpointID(const TcaEndpointID& eid);

    const std::string& host() const { return host_; }
    const std::string& app() const { return app_; }
    
    void set_host(const std::string& host);
    void set_app(const std::string& app);

    const std::string get_hostid() const
        { return std::string("tca://") + host_; }

    static inline std::string
    build(const std::string& host, const std::string& app)
        { return std::string("tca://") + host + "/" + app; }
    
protected:
    // TcaEndpointID caches host part and app part for easy lookup, but
    // always maintains the base class strings as well.
    std::string host_;
    std::string app_;
    void parse();
};


} // namespace dtn

#endif /* _TCA_ENDPOINTID_H_ */
