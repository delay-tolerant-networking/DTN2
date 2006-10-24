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


#include <string>
#include "dtn_types.h"


// Warning: This is a special version of the TcaEndpointID class for use
// on the client side of the interface. The difference is that the other
// TcaEndpointID class (used in the servlib source tree) is subclassed from
// EndpointID, which isn't available here.  
// The interface is almost identical.

class TcaEndpointID
{
public:
    TcaEndpointID() : valid_(false), host_(""), app_("") { }
    TcaEndpointID(const dtn_endpoint_id_t& eid);
    TcaEndpointID(const std::string& str);
    TcaEndpointID(const std::string& host, const std::string& app);
    TcaEndpointID(const TcaEndpointID& eid);

    const std::string& host() const { return host_; }
    const std::string& app() const { return app_; }
    
    const std::string str() const { return "tca://" + host_ + "/" + app_; }
    const char* c_str() const { return str().c_str(); }

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
    bool valid_;
    std::string host_;
    std::string app_;
    void parse(const std::string& str);
    
};



#endif /* _TCA_ENDPOINTID_H_ */
