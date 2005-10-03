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
#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include <oasys/debug/Log.h>
#include <vector>

namespace dtn {

class ConvergenceLayer;
class CLInfo;

/**
 * Abstraction of a local dtn interface.
 *
 * Generally, interfaces are created by the configuration file /
 * console.
 */
class Interface {
public:
    // Data structure for the interface parameter list
    typedef std::vector<std::string> ParamVector;
    
    // Accessors
    const std::string& name()    const { return name_; }
    const std::string& proto()   const { return proto_; }
    ConvergenceLayer*  clayer()  const { return clayer_; }
    CLInfo*	       cl_info() const { return cl_info_; }

    /**
     * Store the ConvergenceLayer specific state.
     */
    void set_cl_info(CLInfo* cl_info)
    {
        ASSERT((cl_info_ == NULL && cl_info != NULL) ||
               (cl_info_ != NULL && cl_info == NULL));
        
        cl_info_ = cl_info;
    }

protected:
    friend class InterfaceTable;
    
    Interface(const std::string& name,
              const std::string& proto,
              ConvergenceLayer* clayer);
    ~Interface();
    
    std::string name_;		///< Name of the interface
    std::string proto_;		///< What type of CL
    ConvergenceLayer* clayer_;	///< Convergence layer to use
    CLInfo* cl_info_;		///< Convergence layer specific state
};

} // namespace dtn

#endif /* _INTERFACE_H_ */
