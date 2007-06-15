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

#include "Dictionary.h"

namespace prophet
{

// Reserve 0xffff as in-band error report
const u_int16_t Dictionary::INVALID_SID = 0xffff;
const std::string Dictionary::NULL_STR;

Dictionary::Dictionary(const std::string& sender, 
                       const std::string& receiver)
    : sender_(sender), receiver_(receiver) {}

Dictionary::Dictionary(const Dictionary& d)
    : sender_(d.sender_), receiver_(d.receiver_),
      ribd_(d.ribd_), rribd_(d.rribd_) {}

u_int16_t
Dictionary::find(const std::string& dest_id) const
{
    // weed out the oddball
    if (dest_id == "") return INVALID_SID;

    // answer the easy ones before troubling map::find
    if (dest_id == sender_) return 0;
    if (dest_id == receiver_) return 1;

    // OK, now we fall back to the Google
    ribd::const_iterator i = (ribd::const_iterator) ribd_.find(dest_id);
    if (i != ribd_.end()) return (*i).second;

    // you fail it!
    return INVALID_SID;
}

const std::string&
Dictionary::find(u_int16_t id) const
{
    // answer the easy ones first
    if (id == 0) return sender_;
    if (id == 1) return receiver_;

    // fall back to reverse RIBD
    const_iterator i = (const_iterator) rribd_.find(id);
    if (i != rribd_.end()) return (*i).second;

    // no dice
    return NULL_STR;
}

u_int16_t
Dictionary::insert(const std::string& dest_id)
{
    // weed out the oddball
    if (dest_id == "") return INVALID_SID;

    // only assign once
    if (find(dest_id) != INVALID_SID)
        return INVALID_SID;

    // our dirty little secret:  just increment the internal
    // SID by skipping the first two (implicit sender & receiver)
    u_int16_t sid = ribd_.size() + 2;

    // somehow the dictionary filled up!?
    if (sid == INVALID_SID) return INVALID_SID;

    bool res = assign(dest_id,sid);
    while (res == false)
    {
        res = assign(dest_id,++sid);
        if (sid == INVALID_SID)
            // sad times ... ?
            return INVALID_SID;
    }
    return sid;
}

bool
Dictionary::assign(const std::string& dest_id, u_int16_t sid)
{
    // weed out the oddball
    if (dest_id == "") return false;

    // these shouldn't get out of sync
    if (ribd_.size() != rribd_.size()) return false;

    // enforce sender/receiver definitions
    if (sid == 0)
    {
        if (sender_ == "" && dest_id != "")
        {
            sender_.assign(dest_id);
            return true;
        }
        return false;
    }
    else if (sid == 1)
    {
        if (receiver_ == "" && dest_id != "")
        {
            receiver_.assign(dest_id);
            return true;
        }
        return false;
    }
    else if (sid == INVALID_SID)
    {
        // what were you thinking?
        return false;
    }

    // attempt to insert into forward lookup
    bool res = ribd_.insert(ribd::value_type(dest_id,sid)).second;
    if ( ! res ) return false;

    // move on to reverse lookup
    res = rribd_.insert(rribd::value_type(sid,dest_id)).second;
    if ( ! res )
        ribd_.erase(dest_id);

               // complain if we get out of sync
    return res && (ribd_.size() == rribd_.size());
}

size_t
Dictionary::guess_ribd_size(size_t RASsz) const
{
    size_t retval = 0;
    for (const_iterator i = rribd_.begin(); i != rribd_.end(); i++)
    {
        retval += FOUR_BYTE_ALIGN( RASsz + (*i).second.length() );
    }
    return retval;
}

void
Dictionary::clear()
{
    // wipe out everything from the dictionary
    sender_.assign("");
    receiver_.assign("");
    ribd_.clear();
    rribd_.clear();
}

void 
Dictionary::dump(BundleCore* core,const char* file,u_int line) const
{
    if (core == NULL) return;
    core->print_log("dictionary", BundleCore::LOG_DEBUG,
            "%s(%u): 0 -> %s", file, line, sender_.c_str());
    core->print_log("dictionary", BundleCore::LOG_DEBUG,
            "%s(%u): 1 -> %s", file, line, receiver_.c_str());
    for (const_iterator i = rribd_.begin(); i != rribd_.end(); i++)
        core->print_log("dictionary", BundleCore::LOG_DEBUG,
                "%s(%u): %u -> %s", file, line, (*i).first,
                (*i).second.c_str());
}

}; // namespace prophet
