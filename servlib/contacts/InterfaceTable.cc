/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2004 Intel Corporation. All rights reserved. 
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

#include "InterfaceTable.h"
#include "BundleTuple.h"
#include "conv_layers/ConvergenceLayer.h"

InterfaceTable* InterfaceTable::instance_ = NULL;

InterfaceTable::InterfaceTable()
    : Logger("/interface")
{
}

InterfaceTable::~InterfaceTable()
{
    NOTREACHED;
}

/**
 * Internal method to find the location of the given interface
 */
bool
InterfaceTable::find(BundleTuple& tuple, ConvergenceLayer* cl,
                     InterfaceList::iterator* iter)
{
    Interface* iface;
    for (*iter = iflist_.begin(); *iter != iflist_.end(); ++(*iter)) {
        iface = **iter;
        
        if (iface->tuple().equals(tuple) && iface->clayer() == cl) {
            return true;
        }
    }        
    
    return false;
}

/**
 * Create and add a new interface to the table. Returns true if
 * the interface is successfully added, false if the interface
 * specification is invalid.
 */
bool
InterfaceTable::add(BundleTuple& tuple, ConvergenceLayer* cl,
                    const char* proto,
                    int argc, const char* argv[])
{
    InterfaceList::iterator iter;
    
    if (find(tuple, cl, &iter)) {
        log_err("interface %s already exists", tuple.c_str());
        return false;
    }
    
    log_info("adding interface %s %s", proto, tuple.c_str());

    Interface* iface = new Interface(tuple, cl);
    if (! cl->add_interface(iface, argc, argv)) {
        log_err("convergence layer error adding interface %s", tuple.c_str());
        return false;
    }

    iflist_.push_back(iface);

    return true;
}

/**
 * Remove the specified interface.
 */
bool
InterfaceTable::del(BundleTuple& tuple, ConvergenceLayer* cl,
                    const char* proto)
{
    Interface* iface;
    InterfaceList::iterator iter;
    
    log_info("removing interface %s %s", proto, tuple.c_str());

    if (! find(tuple, cl, &iter)) {
        log_err("error removing interface %s: no such interface",
                tuple.c_str());
        return false;
    }

    iface = *iter;
    iflist_.erase(iter);
    delete iface;
    
    return true;
}
