/*
 *    Copyright 2007 Baylor University
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

#ifndef _PROPHET_ACK_H_
#define _PROPHET_ACK_H_

#include <string>
#include <oasys/oasys-config.h>
#include <oasys/compat/inttypes.h>

namespace prophet
{

/**
 * A Prophet ACK signals successful delivery of a Bundle to its
 * final destination within the Prophet region (whether actual
 * bundle destination or gateway to non-Prophet region).
 */
class Ack
{
public:
    /**
     * Default constructor
     */
    Ack()
        : dest_id_(""), cts_(0), seq_(0), ets_(0) {}

    /**
     * Constructor
     */
    Ack(std::string endpoint_id,
        u_int32_t cts = 0,
        u_int32_t seq = 0,
        u_int32_t ets = 0)
        : dest_id_(endpoint_id), cts_(cts),
          seq_(seq), ets_(ets) {}

    /**
     * Copy constructor
     */
    Ack(const Ack& a)
        : dest_id_(a.dest_id_), cts_(a.cts_),
          seq_(a.seq_), ets_(a.ets_) {}

    ///@{ Accessors
    std::string dest_id() const { return dest_id_; }
    u_int32_t cts() const { return cts_; }
    u_int32_t seq() const { return seq_; }
    u_int32_t ets() const { return ets_; }
    ///@}

    ///@{ Mutators
    void set_dest_id ( const std::string& dest_id )
    {
        dest_id_.assign(dest_id);
    }
    void set_cts (u_int32_t cts) { cts_ = cts; }
    void set_seq (u_int32_t seq) { seq_ = seq; }
    void set_ets (u_int32_t ets) { ets_ = ets; }
    ///@}

    /**
     * Comparison operator for STL sort
     */
    bool operator< ( const Ack& a ) const
    {
        int comp = dest_id_.compare(a.dest_id_);
        if (comp == 0)
        {
            if (cts_ == a.cts_)
                return seq_ < a.seq_;
            else
                return (cts_ < a.cts_);
        }
        return (comp < 0);
    }

    /**
     * Assignment operator
     */
    Ack& operator= ( const Ack& a ) 
    {
        dest_id_.assign(a.dest_id_);
        cts_ = a.cts_;
        seq_ = a.seq_;
        ets_ = a.ets_;
        return *this;
    }

protected:
    std::string dest_id_;
    u_int32_t cts_;
    u_int32_t seq_;
    u_int32_t ets_;
    
}; // class Ack

}; // namespace prophet

#endif // _PROPHET_ACK_H_
