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
#ifndef _FILE_CONVERGENCE_LAYER_H_
#define _FILE_CONVERGENCE_LAYER_H_

#include "ConvergenceLayer.h"
#include <oasys/thread/Thread.h>

namespace dtn {

class FileConvergenceLayer : public ConvergenceLayer {
public:
    /**
     * Constructor.
     */
    FileConvergenceLayer();
    
    /**
     * The meat of the initialization happens here.
     */
    virtual void init();

    /**
     * Hook for shutdown.
     */
    virtual void fini();

    /**
     * Hook to validate the admin part of a bundle tuple.
     *
     * @return true if the admin string is valid
     */
    virtual bool validate(const std::string& admin);

    /**
     * Hook to match the admin part of a bundle tuple.
     *
     * @return true if the demux matches the tuple.
     */
    virtual bool match(const std::string& demux, const std::string& admin);

    /**
     * Register a new interface.
     */
    virtual bool add_interface(Interface* iface, int argc, const char* argv[]);

    /**
     * Remove an interface
     */
    virtual bool del_interface(Interface* iface);
    
    /**
     * Open the connection to the given contact and prepare for
     * bundles to be transmitted.
     */
    virtual bool open_contact(Contact* contact);
    
    /**
     * Close the connnection to the contact.
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

protected:
    /**
     * Current version of the file cl protocol.
     */
    static const int CURRENT_VERSION = 0x1;
     
    /**
     * Framing header at the beginning of each bundle file.
     */
    struct FileHeader {
        u_int8_t version;		///< framing protocol version
        u_int8_t pad;			///< unused
        u_int16_t header_length;	///< length of the bundle header
        u_int32_t bundle_length;	///< length of the bundle + headers
    } __attribute__((packed));
    
    /**
     * Pull a filesystem directory out of the admin portion of a
     * tuple.
     */
    bool extract_dir(const BundleTuple& tuple, std::string* dirp);
    
    /**
     * Validate that a given directory exists and that the permissions
     * are correct.
     */
    bool validate_dir(const std::string& dir);
        
    /**
     * Helper class (and thread) that periodically scans a directory
     * for new bundle files.
     */
    class Scanner : public InterfaceInfo, public oasys::Logger, public oasys::Thread {
    public:
        /**
         * Constructor.
         */
        Scanner(int secs_per_scan, const std::string& dir);

    protected:
        /**
         * Main thread function.
         */
        void run();
        
        int secs_per_scan_;	///< scan interval
        std::string dir_;	///< directory to scan for bundles.
    };
};

} // namespace dtn

#endif /* _FILE_CONVERGENCE_LAYER_H_ */
