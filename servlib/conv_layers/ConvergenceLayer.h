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
#ifndef _CONVERGENCE_LAYER_H_
#define _CONVERGENCE_LAYER_H_

#include <string>
#include <oasys/debug/Log.h>

#include "bundling/Contact.h"
#include "bundling/Interface.h"

namespace dtn {

/**
 * The abstract interface for a convergence layer.
 */
class ConvergenceLayer : public oasys::Logger {
public:
    /**
     * Constructor. In general, most initialization is deferred to the
     * init() function.
     */
    ConvergenceLayer(const char* logpath)
        : Logger(logpath)
    {
    }

    /**
     * Need a virtual destructor to define the vtable.
     */
    virtual ~ConvergenceLayer();
    
    /**
     * The meat of the initialization happens here.
     */
    virtual void init() = 0;

    /**
     * Hook for shutdown.
     */
    virtual void fini() = 0;

    /**
     * Hook to validate the admin part of a bundle tuple.
     *
     * @return true if the admin string is valid
     */
    virtual bool validate(const std::string& admin) = 0;

    /**
     * Hook to match the admin part of a bundle tuple.
     *
     * @return true if the demux matches the tuple.
     */
    virtual bool match(const std::string& demux, const std::string& admin) = 0;

    /**
     * Register a new interface.
     */
    virtual bool add_interface(Interface* iface, int argc, const char* argv[]);

    /**
     * Remove an interface
     */
    virtual bool del_interface(Interface* iface);

    /**
     * Accessor to protocol string/name ?
     * XXX/ Sushant need way to identify a conv layer
     */
    // const char* proto() { return proto_ ; }

    
    /**
     * Open the connection to the given contact and prepare for
     * bundles to be transmitted.
     */
    virtual bool open_contact(Contact* contact);
    
    /**
     * Close the connnection to the contact.
     * Mainly, used to clean the state that is associated
     * with this contact. This is called by the link->close()
     * function.
     * After calling this function, the contact can be deleted
     * from the system.
     */
    virtual bool close_contact(Contact* contact);

    /**
     * Try to send the bundles queued up for the given contact. In
     * some cases (e.g. tcp) this is a no-op because open_contact spun
     * a thread which is blocked on the bundle list associated with
     * the contact. In others (e.g. file) there is no thread, so this
     * callback is used to send the bundle.
     */
    virtual void send_bundles(Contact* contact);
    
    /**
     * Boot-time initialization and registration of convergence
     * layers.
     */
    static void init_clayers();
    static void add_clayer(const char* proto, ConvergenceLayer* cl);
    
    /**
     * Find the appropriate convergence layer for the given protocol
     * string.
     */
    static ConvergenceLayer* find_clayer(const char* proto);

    
protected:
    /**
     * Struct that maps a convergence layer protocol to its
     * implementation.
     */
    struct Protocol {
        const char* proto_;	///< protocol name
        ConvergenceLayer* cl_;	///< the registered convergence layer
        Protocol* next_;	///< link to next registered protocol
    };
    
    /**
     * The linked list of all protocols
     */
    static Protocol* protocol_list_;
};

} // namespace dtn

#endif /* _CONVERGENCE_LAYER_H_ */
