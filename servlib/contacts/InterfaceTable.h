/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2005 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _INTERFACE_TABLE_H_
#define _INTERFACE_TABLE_H_

#include <string>
#include <list>
#include <oasys/debug/DebugUtils.h>
#include <oasys/util/StringBuffer.h>

namespace dtn {

class ConvergenceLayer;
class Interface;

/**
 * The list of interfaces.
 */
typedef std::list<Interface*> InterfaceList;

/**
 * Class for the in-memory interface table.
 */
class InterfaceTable : public oasys::Logger {
public:
    /**
     * Singleton instance accessor.
     */
    static InterfaceTable* instance() {
        if (instance_ == NULL) {
            PANIC("InterfaceTable::init not called yet");
        }
        return instance_;
    }

    /**
     * Boot time initializer that takes as a parameter the actual
     * storage instance to use.
     */
    static void init() {
        if (instance_ != NULL) {
            PANIC("InterfaceTable::init called multiple times");
        }
        instance_ = new InterfaceTable();
    }

    /**
     * Constructor
     */
    InterfaceTable();

    /**
     * Destructor
     */
    virtual ~InterfaceTable();

    /**
     * Add a new interface to the table. Returns true if the interface
     * is successfully added, false if the interface specification is
     * invalid (or it already exists).
     */
    bool add(const std::string& name, ConvergenceLayer* cl, const char* proto,
             int argc, const char* argv[]);
    
    /**
     * Remove the specified interface.
     */
    bool del(const std::string& name);
    
    /**
     * List the current interfaces.
     */
    void list(oasys::StringBuffer *buf);

protected:
    static InterfaceTable* instance_;

    /**
     * All interfaces are tabled in-memory in a flat list. It's
     * non-obvious what else would be better since we need to do a
     * prefix match on demux strings in matching_interfaces.
     */
    InterfaceList iflist_;

    /**
     * Internal method to find the location of the given interface in
     * the list.
     */
    bool find(const std::string& name, InterfaceList::iterator* iter);
};

} // namespace dtn

#endif /* _INTERFACE_TABLE_H_ */
