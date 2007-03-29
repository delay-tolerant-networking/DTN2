/* Copyright 2004-2006 BBN Technologies Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "GbofId.h"
#include <sstream>

// GBOFID -- Global Bundle Or Fragment ID

namespace dtn {

GbofId::GbofId()
{

}

GbofId::~GbofId()
{

}

bool
GbofId::equals(GbofId id)
{
    if(source_.equals(id.source_) &&
       creation_ts_.seconds_ == id.creation_ts_.seconds_ &&
       creation_ts_.seqno_ == id.creation_ts_.seqno_ &&
       is_fragment_ == id.is_fragment_ &&
            (!is_fragment_ || 
            (frag_length_ == id.frag_length_ &&
             frag_offset_ == id.frag_offset_)))
    {
        return true;
    }
    else
        return false;
}

bool
GbofId::equals(EndpointID source,
               BundleTimestamp creation_ts,
               bool is_fragment,
               u_int32_t frag_length,
               u_int32_t frag_offset)
{
    if(source_.equals(source) &&
       creation_ts_.seconds_ == creation_ts.seconds_ &&
       creation_ts_.seqno_ == creation_ts.seqno_ &&
       is_fragment_ == is_fragment &&
            (!is_fragment || 
            (frag_length_ == frag_length &&
             frag_offset_ == frag_offset)))
    {
        return true;
    }
    else
        return false;
}

std::string GbofId::str()
{
        std::string toReturn;
        
        toReturn.append(source_.c_str());
        
        toReturn.append(",");
        
        std::ostringstream oss1;
        oss1<< creation_ts_.seconds_;
        toReturn.append(oss1.str());
        
        toReturn.append(",");
        
        std::ostringstream oss2;
        oss2<< creation_ts_.seqno_;
        toReturn.append(oss2.str());
        
        toReturn.append(",");
        
        std::ostringstream oss3;
        oss3<< is_fragment_;
        toReturn.append(oss3.str());
        
        toReturn.append(",");
        
        std::ostringstream oss4;
        oss4<< frag_length_;
        toReturn.append(oss4.str());
        
        toReturn.append(",");
        
        std::ostringstream oss5;
        oss5<< frag_offset_;
        toReturn.append(oss5.str());
        return toReturn;
}

} // namespace dtn
