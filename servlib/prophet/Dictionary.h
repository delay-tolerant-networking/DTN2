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

#ifndef _PROPHET_DICTIONARY_H_
#define _PROPHET_DICTIONARY_H_

#include <string>
#include <map>
#include "Util.h"
#include "Bundle.h"
#include "BundleCore.h"

namespace prophet
{

/**
 * Utility class to facilitate converting to and from routing string
 * (endpoint ID) and 16-bit string IDs
 */
class Dictionary
{
public:
    typedef std::map<u_int16_t,std::string> rribd;
    typedef std::map<u_int16_t,std::string>::const_iterator
        const_iterator;

    /**
     * Reserve 0xffff for in-band error signal
     */
    static const u_int16_t INVALID_SID;

    /**
     * Used for in-band error signal
     */
    static const std::string NULL_STR;

    /**
     * Default constructor
     */
    Dictionary(const std::string& sender = "",
               const std::string& receiver = "");

    /**
     * Copy constructor
     */
    Dictionary(const Dictionary& d);

    /**
     * Destructor
     */
    ~Dictionary() {}

    /**
     * If dest_id is valid, returns sid; else returns INVALID_SID
     */
    u_int16_t find(const std::string& dest_id) const;

    /**
     * If id is valid, returns dest_id; else returns empty string
     */
    const std::string& find(u_int16_t sid) const;

    /**
     * Convenience function; sender is defined as sid == 0
     */
    const std::string& sender() const { return sender_; }

    /**
     * Convenience function; receiver is defined as sid == 1
     */
    const std::string& receiver() const { return receiver_; }

    /**
     * Insert dest_id and return valid id upon success; else
     * returns INVALID_SID
     */
    u_int16_t insert(const std::string& dest_id);

    /**
     * Convenience wrapper that accepts const Bundle* and
     * passes through a call to insert(const std::string&)
     */
    u_int16_t insert(const Bundle* b);

    /**
     * Assign dest_id to arbitrary id (used by TLV deserialization);
     * return true upon success, false upon collision
     */
    bool assign(const std::string& dest_id, u_int16_t sid);

    ///@{ Iterators 
    const_iterator begin() const { return rribd_.begin(); }
    const_iterator end() const { return rribd_.end(); }
    ///@}

    /**
     * Return the number of elements in the dictionary
     */ 
    size_t size() const { return ribd_.size(); }

    /**
     * Helper function for serializing, where RASsz is the
     * sizeof(RoutingAddressString), the overhead per RIBD entry
     */
    size_t guess_ribd_size(size_t RASsz) const;

    /**
     * Wipe out dictionary and prepare to deserialize new one
     */
    void clear();

    /**
     * Assignment operator
     */
    Dictionary& operator= (const Dictionary& d)
    {
        sender_.assign(d.sender_);
        receiver_.assign(d.receiver_);
        ribd_ = d.ribd_;
        rribd_ = d.rribd_;
        return *this;
    }

    /**
     * Debug method for printing out internal state to log
     */
    void dump(BundleCore* core,const char* file,u_int line) const;

protected:
    typedef std::map<std::string,u_int16_t,less_string> ribd;

    std::string sender_;   ///< destination id of peering initiator
    std::string receiver_; ///< destination id of passive peer
    ribd        ribd_;     ///< forward lookup, dest_id to string id
    rribd       rribd_;    ///< reverse lookup from string id to dest_id

}; // Dictionary

}; // namespace prophet
#endif // _PROPHET_DICTIONARY_H_
