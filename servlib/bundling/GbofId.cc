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
#  include <dtn-config.h>
#endif

#include "GbofId.h"
#include <sstream>

// GBOFID -- Global Bundle Or Fragment ID

namespace oasys {

//----------------------------------------------------------------------
template<>
const char*
InlineFormatter<dtn::GbofId>
::format(const dtn::GbofId& id)
{
    if (! id.is_fragment_) {
        buf_.appendf("<%s, %u.%u>",
                     id.source_.c_str(),
                     id.creation_ts_.seconds_,
                     id.creation_ts_.seqno_);
    } else {
        buf_.appendf("<%s, %u.%u, FRAG len %u offset %u>",
                     id.source_.c_str(),
                     id.creation_ts_.seconds_,
                     id.creation_ts_.seqno_,
                     id.frag_length_, id.frag_offset_);
    }
    return buf_.c_str();
}
} // namespace oasys

namespace dtn {

//----------------------------------------------------------------------
GbofId::GbofId()
{
}

//----------------------------------------------------------------------
GbofId::GbofId(EndpointID      source,
               BundleTimestamp creation_ts,
               bool            is_fragment,
               u_int32_t       frag_length,
               u_int32_t       frag_offset)
    : source_(source),
      creation_ts_(creation_ts),
      is_fragment_(is_fragment),
      frag_length_(frag_length),
      frag_offset_(frag_offset)
{
}

//----------------------------------------------------------------------
GbofId::~GbofId()
{
}

//----------------------------------------------------------------------
bool
GbofId::equals(const GbofId& id) const
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

//----------------------------------------------------------------------
bool
GbofId::operator<(const GbofId& other) const
{
    if (source_ < other.source_) return true;
    if (other.source_ < source_) return false;

    if (creation_ts_ < other.creation_ts_) return true;
    if (creation_ts_ > other.creation_ts_) return false;

    if (is_fragment_) {
        if (frag_length_ < other.frag_length_) return true;
        if (other.frag_length_ < frag_length_) return false;

        if (frag_offset_ < other.frag_offset_) return true;
        if (other.frag_offset_ < frag_offset_) return false;
    }

    return false; // all equal
}

//----------------------------------------------------------------------
bool
GbofId::equals(EndpointID source,
               BundleTimestamp creation_ts,
               bool is_fragment,
               u_int32_t frag_length,
               u_int32_t frag_offset) const
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

//----------------------------------------------------------------------
std::string
GbofId::str() const
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
