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

    /// @{
    
    virtual bool interface_up(Interface* iface,
                              int argc, const char* argv[]);
    virtual bool interface_down(Interface* iface);
    virtual bool open_contact(const ContactRef& contact);
    virtual bool close_contact(const ContactRef& contact);
    virtual void send_bundle(const ContactRef& contact, Bundle* bundle);

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
     * Pull a filesystem directory out of the next hop admin address.
     */
    bool extract_dir(const char* nexthop, std::string* dirp);
    
    /**
     * Validate that a given directory exists and that the permissions
     * are correct.
     */
    bool validate_dir(const std::string& dir);
        
    /**
     * Helper class (and thread) that periodically scans a directory
     * for new bundle files.
     */
    class Scanner : public CLInfo, public oasys::Logger, public oasys::Thread {
    public:
        /**
         * Constructor.
         */
        Scanner(int secs_per_scan, const std::string& dir);

        /**
         * Set the flag to ask it to stop next loop.
         */
        void stop();

    protected:
        /**
         * Main thread function.
         */
        void run();
        
        int secs_per_scan_;	///< scan interval
        std::string dir_;	///< directory to scan for bundles.
        bool run_;              ///< keep running?
    };
};

} // namespace dtn

#endif /* _FILE_CONVERGENCE_LAYER_H_ */
