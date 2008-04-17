/*
 *    Copyright 2008 Intel Corporation
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

#include <ctype.h>
#include <oasys/util/SafeRange.h>
#include <oasys/util/StringAppender.h>
#include <oasys/util/StringBuffer.h>
#include "SequenceID.h"

namespace dtn {

//----------------------------------------------------------------------
SequenceID::SequenceID()
{
}

//----------------------------------------------------------------------------
const char* 
SequenceID::comp2str(comp_t eq)
{
    switch(eq) {
    case LT:  return "less than";
    case GT:  return "greater than";
    case EQ:  return "equal to";
    case NEQ: return "uncomparable to";
    default:  NOTREACHED;
    }
}

//----------------------------------------------------------------------
void
SequenceID::add(const EndpointID& eid, u_int64_t counter)
{
    vector_.push_back(Entry(eid, counter));
}

//----------------------------------------------------------------------------
u_int64_t
SequenceID::get_counter(const EndpointID& eid) const
{
    for (const_iterator i = begin(); i != end(); ++i) {
        if (i->eid_ == eid) {
            return i->counter_;
        }
    }
    
    return 0;
}

//----------------------------------------------------------------------------
int
SequenceID::format(char* buf, size_t sz) const
{
    oasys::StringAppender sa(buf, sz);

    sa.append('<');
    for (const_iterator i = begin(); i != end(); ++i)
    {
	sa.appendf("%s(%s %llu)", 
                   (i == begin()) ? "" : " ",
                   i->eid_.c_str(), 
                   U64FMT(i->counter_));
    }
    sa.append('>');
    
    return sa.desired_length();
}

//----------------------------------------------------------------------
std::string
SequenceID::to_str() const
{
    oasys::StaticStringBuffer<256> buf;
    buf.appendf("*%p", this);
    return (std::string)buf.c_str();
}

//----------------------------------------------------------------------------
bool
SequenceID::parse(const std::string& str)
{
#define EAT_WS() while (isspace(sr[idx])) { idx++; }
#define MATCH_CHAR(_c) do {                          \
    if (sr[idx] != _c) { goto bad; } else { idx++; } \
} while (0)

    EntryVec old_vec = vector_;
    vector_.clear();

    oasys::SafeRange<const char> sr(str.c_str(), str.size());

    try 
    {
        size_t idx = 0;
        const char *start;
        char *end;
        
        MATCH_CHAR('<');
        EAT_WS();
        while (sr[idx] == '(')
        {
            MATCH_CHAR('(');
            start = &sr[idx];

            do {
                ++idx;
            } while (!isspace(sr[idx]));
            
            EndpointID eid(start, &sr[idx] - start);
            if (! eid.valid()) {
                goto bad;
            }

            EAT_WS();
            
            u_int64_t counter = strtoull(&sr[idx], &end, 10);
            
            if (*end != ')' || end == &sr[idx]) {
                goto bad;
            }

            vector_.push_back(Entry(eid, counter));
            idx += end - &sr[idx];
            MATCH_CHAR(')');
            EAT_WS();
        }
        
        if (sr[idx] != '>')
        {
            goto bad;
        }

        return true;
    }
    catch (oasys::SafeRange<const char>::Error e)
    { 
        // drop through to bad below
    }

  bad:
    vector_.swap(old_vec);
    return false;
    
#undef EAT_WS
#undef MATCH_CHAR
}

//----------------------------------------------------------------------------
SequenceID::comp_t
SequenceID::compare(const SequenceID& other) const
{
    if (empty() && other.empty())
    {
        return EQ;
    }

    // We need to compare the sequence vectors in both directions,
    // since entries that exist in one but not the other are
    // implicitly zero. This approach unnecessarily repeats
    // comparisons but it's not a big deal since the vectors shouldn't
    // be very long.
    
    comp_t cur_state = EQ;
    cur_state = compare_one_way(other, *this, cur_state);
    ASSERT(cur_state != ILL);
    
    if (cur_state != NEQ) 
    {
        // invert the state and call again, this time in the "forward"
        // direction
        switch (cur_state) {
        case LT:   cur_state = GT; break;
        case GT:   cur_state = LT; break;
        case EQ:   cur_state = EQ; break;
        case NEQ:
        case ILL:
            NOTREACHED;
        }
            
        cur_state = compare_one_way(*this, other, cur_state);
    }

    return cur_state;
}

//----------------------------------------------------------------------------
SequenceID::comp_t
SequenceID::compare_one_way(const SequenceID& lv, 
                            const SequenceID& rv,
                            comp_t            cur_state)
{
    // State transition for comparing the vectors
    static comp_t states[4][3] = {
        /* cur_state   cur_comparison */
        /*              LT    EQ    GT  */
        /* LT   */    { LT,   LT,   NEQ }, 
        /* EQ   */    { LT,   EQ,   GT  }, //<- new
        /* GT   */    { NEQ,  GT,   GT  }, //<- state
        /* NEQ  */    { NEQ,  NEQ,  NEQ }
    };
    
// To align the enum (i.e. values -1 to 2) with the correct array
// indices, we have to add one to the index values
#define next_state(_cur_state, _cur_comp) \
    states[(_cur_state) + 1][(_cur_comp) + 1]

    for (const_iterator i = lv.begin(); i != lv.end(); ++i)
    {
        u_int64_t left  = lv.get_counter(i->eid_);
        u_int64_t right = rv.get_counter(i->eid_);

        cur_state = next_state(cur_state, compare_counters(left, right));

        if (cur_state == NEQ)
        {
            return NEQ;
        }
    }

    return cur_state;

#undef next_state
}

//----------------------------------------------------------------------------
SequenceID::comp_t
SequenceID::compare_counters(u_int64_t left, u_int64_t right)
{
    if (left == right)
	return EQ;

    if (left < right)
    {
        return LT;
    }

    return GT;
}

//----------------------------------------------------------------------
void
SequenceID::update(const SequenceID& other)
{
    for (const_iterator other_entry = other.begin();
         other_entry != other.end();
         ++other_entry)
    {
        iterator my_entry = begin();
        while (my_entry != end()) {
            if (other_entry->eid_ == my_entry->eid_)
                break;
            ++my_entry;
        }

        if (my_entry == end())
        {
            add(other_entry->eid_, other_entry->counter_);
        }
        else if (other_entry->counter_ > my_entry->counter_)
        {
            my_entry->counter_ = other_entry->counter_;
        }
    }
}

} // namespace dtn
