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
#ifndef _BUNDLE_COMMAND_H_
#define _BUNDLE_COMMAND_H_

#include <oasys/tclcmd/TclCommand.h>

namespace dtn {

/**
 * Debug command for hand manipulation of bundles.
 */
class BundleCommand : public oasys::TclCommand {
public:
    BundleCommand();
    
    /**
     * Virtual from CommandModule.
     */
    virtual int exec(int objc, Tcl_Obj** objv, Tcl_Interp* interp);
    virtual const char* help_string();

private:

    /**
     * "bundle inject" command parameters/options
     */
    class InjectOpts {
    public:
        InjectOpts();
        
        bool receive_rcpt_;	///< Hop by hop reception receipt
        bool custody_rcpt_;	///< Custody xfer reporting
        bool forward_rcpt_;	///< Hop by hop forwarding reporting
        bool delivery_rcpt_;	///< End-to-end delivery reporting
        bool deletion_rcpt_;	///< Bundle deletion reporting
        size_t expiration_;     ///< Bundle TTL
        size_t length_;         ///< Bundle payload length
        std::string replyto_;   ///< Bundle Reply-To EID
    };

    /**
     * Parse the "bundle inject" command line options
     */
    bool parse_inject_options(InjectOpts* options, int objc, Tcl_Obj** objv,
                              const char** invalidp);
    
};

} // namespace dtn

#endif /* _BUNDLE_COMMAND_H_ */
