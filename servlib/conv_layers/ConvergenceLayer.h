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
     * Constructor.
     */
    ConvergenceLayer(const char* logpath)
        : Logger(logpath)
    {
    }

    /**
     * Destructor.
     */
    virtual ~ConvergenceLayer();
    
    /**
     * Register a new interface.
     */
    virtual bool add_interface(Interface* iface, int argc, const char* argv[]);

    /**
     * Remove an interface
     */
    virtual bool del_interface(Interface* iface);

    /**
     * Create any CL-specific components of the Link.
     */
    virtual bool init_link(Link* link, int argc, const char* argv[]);
    
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
     * Try to send the bundles given bundle on the current link.
     *
     * In some cases (e.g. TCP) this just sticks bundles on a queue
     * for another thread to consume (after setting the link state to
     * BUSY). In others (e.g. UDP) there is no per-contact thread, so
     * this callback is used to send the bundle.
     */
    virtual void send_bundle(Contact* contact, Bundle* bundle) = 0;
    
    /**
     * Try to cancel transmission of a given bundle on the contact.
     */
    virtual bool cancel_bundle(Contact* contact, Bundle* bundle)
    {
        return false;
    }
    
    /**
     * Hook to see if the given bundle is queued for transmission on
     * the given contact.
     */
    virtual bool is_queued(Contact* contact, Bundle* bundle)
    {
        return false;
    }
    
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

    /**
     * Magic number used for DTN convergence layers     
     */
    static const u_int32_t MAGIC = 0x64746e21; //'DTN!'
    
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

/**
 * Abstract base class for convergence layer specific state stored in
 * an interface / contact / link.
 */
class CLInfo {
public:
    virtual ~CLInfo() {}
};

} // namespace dtn

#endif /* _CONVERGENCE_LAYER_H_ */
