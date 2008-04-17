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

#ifndef _SEQUENCE_ID_H_
#define _SEQUENCE_ID_H_

#include <oasys/debug/Formatter.h>
#include "../naming/EndpointID.h"

namespace dtn {

/**
 * A bundle SequenceID is a version vector of (EID, counter) tuples.
 */
class SequenceID : public oasys::Formatter {
public:
    SequenceID();
    
    /**
     * Valid return values from compare().
     */
    typedef enum {
        LT   = -1,               ///< Less than
        EQ   = 0,                ///< Equal to
        GT   = 1,                ///< Greater than
        NEQ  = 2,                ///< Incomparable
        ILL  = 999		 ///< Illegal state
    } comp_t;

    /// Pretty printer for comp_t.
    static const char* comp2str(comp_t eq);

    /// Add an (EID, counter) tuple to the vector
    void add(const EndpointID& eid, u_int64_t counter);

    /**
     * Get the counter for a particular EID.
     * If no entry is found, returns 0.
     */
    u_int64_t get_counter(const EndpointID& eid) const;
    
    /// Virtual from Formatter
    int format(char* buf, size_t sz) const;

    /// Get a string representation
    std::string to_str() const;

    /// Build a sequence id from a string representation
    bool parse(const std::string& str);

    /// Compare two vectors, returning a comp_t.
    comp_t compare(const SequenceID& v) const;

    /// @{ Operator overloads
    bool operator<(const SequenceID& v) const  { return compare(v) == LT; }
    bool operator>(const SequenceID& v) const  { return compare(v) == GT; }
    bool operator==(const SequenceID& v) const { return compare(v) == EQ; }

    /** Note! This means not comparable -NOT- not equal to */
    bool operator!=(const SequenceID& v) const { return compare(v) == NEQ; }
    
    bool operator<=(const SequenceID& v) const
    {
        int cmp = compare(v);
        return cmp == LT || cmp == EQ;
    }

    bool operator>=(const SequenceID& v) const
    {
        int cmp = compare(v);
        return cmp == GT || cmp == EQ;
    }
    /// @}

    /// Update the sequence id to include the max of all current
    /// entries and the new one.
    void update(const SequenceID& other);

    /// An entry in the vector
    struct Entry {
        Entry() : eid_(), counter_(0) {}
        Entry(const EndpointID& eid, u_int64_t counter)
            : eid_(eid), counter_(counter) {}
        Entry(const Entry& other)
            : eid_(other.eid_), counter_(other.counter_) {}

        EndpointID eid_;
        u_int64_t  counter_;
    };

    /// @{ Typedefs and wrappers for the entry vector and iterators
    typedef std::vector<Entry> EntryVec;
    typedef EntryVec::iterator iterator;
    typedef EntryVec::const_iterator const_iterator;
    bool                     empty() const { return vector_.empty(); }
    EntryVec::iterator       begin()       { return vector_.begin(); }
    EntryVec::iterator       end()         { return vector_.end();   }
    EntryVec::const_iterator begin() const { return vector_.begin(); }
    EntryVec::const_iterator end()   const { return vector_.end();   }
    /// @}

private:
    /// The entry vector
    EntryVec vector_;

    /// Compare two counters
    static comp_t compare_counters(u_int64_t left, u_int64_t right);
    
    /// Compare vectors in a single direction
    static comp_t compare_one_way(const SequenceID& lv,
                                  const SequenceID& rv,
                                  comp_t cur_state);
};

} // namespace dtn

#endif /* _SEQUENCE_ID_H_ */
