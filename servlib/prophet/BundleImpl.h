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

#ifndef _PROPHET_BUNDLE_IMPL_H_
#define _PROPHET_BUNDLE_IMPL_H_

#include "Bundle.h"
#include <string>

namespace prophet
{

/**
 * Facade interface between Prophet router and host implmentation's
 * Bundle representation. Rather than duplicate the extensive Bundle API,
 * this facade is only interested in a subset of the metadata.
 */
class BundleImpl : public Bundle
{
public:
    /**
     * Default constructor
     */
    BundleImpl()
        : Bundle(),
          dest_id_(""),
          src_id_(""),
          cts_(0),
          seq_(0),
          ets_(0),
          size_(0),
          num_fwd_(0),
          custody_requested_(false)
    {}

    /**
     * Constructor
     */
    BundleImpl(const std::string& destination_id,
           u_int32_t creation_ts = 0,
           u_int32_t sequence_num = 0,
           u_int32_t expiration_ts = 0,
           u_int size = 0,
           u_int num_forward = 0,
           bool custody_requested = false)
        : Bundle(),
          dest_id_(destination_id),
          src_id_(""),
          cts_(creation_ts),
          seq_(sequence_num),
          ets_(expiration_ts),
          size_(size),
          num_fwd_(num_forward),
          custody_requested_(custody_requested)
    {}

    /**
     * Constructor
     */
    BundleImpl(const std::string& source_id,
           const std::string& destination_id,
           u_int32_t creation_ts = 0,
           u_int32_t sequence_num = 0,
           u_int32_t expiration_ts = 0,
           u_int size = 0,
           u_int num_forward = 0,
           bool custody_requested = false)
        : Bundle(),
          dest_id_(destination_id),
          src_id_(source_id),
          cts_(creation_ts),
          seq_(sequence_num),
          ets_(expiration_ts),
          size_(size),
          num_fwd_(num_forward),
          custody_requested_(custody_requested)
    {}

    /**
     * Copy constructor
     */
    BundleImpl(const BundleImpl& b)
        : Bundle(b),
          dest_id_(b.dest_id_),
          src_id_(b.src_id_), cts_(b.cts_),
          seq_(b.seq_), ets_(b.ets_),
          size_(b.size_), num_fwd_(b.num_fwd_),
          custody_requested_(b.custody_requested_)
    {}

    /**
     * Destructor
     */
    virtual ~BundleImpl() {}

    ///@{ Virtual from Bundle
    virtual const std::string& destination_id() const { return dest_id_; }
    virtual const std::string& source_id() const { return src_id_; }
    virtual u_int32_t   creation_ts()    const { return cts_; }
    virtual u_int32_t   sequence_num()   const { return seq_; }
    virtual u_int32_t   expiration_ts()  const { return ets_; }
    virtual u_int       size()           const { return size_; }
    virtual u_int       num_forward()    const { return num_fwd_; }
    virtual bool     custody_requested() const { return custody_requested_; }
    ///@}

    ///@{ Mutators
    virtual void set_destination_id( const std::string& id ) { dest_id_.assign(id); }
    virtual void set_source_id( const std::string& id ) { src_id_.assign(id); }
    virtual void set_creation_ts( u_int32_t cts ) { cts_ = cts; }
    virtual void set_sequence_num( u_int32_t seq ) { seq_ = seq; }
    virtual void set_expiration_ts( u_int32_t ets ) { ets_ = ets; }
    virtual void set_size( u_int sz ) { size_ = sz; }
    virtual void set_num_forward( u_int nf ) { num_fwd_ = nf; }
    virtual void set_custody_requested( bool c ) { custody_requested_ = c; }
    ///@}

    ///@{ Operators
    virtual BundleImpl& operator= (const BundleImpl& b)
    {
        dest_id_.assign(b.dest_id_);
        src_id_.assign(b.src_id_);
        cts_     = b.cts_;
        seq_     = b.seq_;
        ets_     = b.ets_;
        size_    = b.size_;
        num_fwd_ = b.num_fwd_;
        return *this;
    }
    ///@}

protected:
    std::string dest_id_; ///< string representation of route to destination
    std::string src_id_;  ///< string rep of bundle source
    u_int32_t cts_;       ///< creation timestamp (epoch since 2000/01/01)
    u_int32_t seq_;       ///< subsecond sequence number
    u_int32_t ets_;       ///< expiration offset (in seconds)
    u_int     size_;      ///< size of Bundle payload
    u_int     num_fwd_;   ///< number of times this Bundle has been forwarded
    bool custody_requested_; ///< whether to request custody on this bundle
}; // class BundleImpl

}; // namespace prophet

#endif // _PROPHET_BUNDLE_IMPL_H_
