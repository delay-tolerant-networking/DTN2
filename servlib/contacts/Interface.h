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
#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include "BundleTuple.h"
#include "Contact.h"
#include <oasys/debug/Log.h>

class ConvergenceLayer;
class InterfaceInfo;

/**
 * Abstraction of a local dtn interface.
 *
 * Generally, interfaces are created by the console.
 */
class Interface {
public:
    // Accessors
    const std::string& region() const { return tuple_.region(); }
    const std::string& admin()  const { return tuple_.admin(); }
    const BundleTuple& tuple()  const { return tuple_; }
    ConvergenceLayer*  clayer() const { return clayer_; }
    InterfaceInfo*     info()   const { return info_; }

    /**
     * Store the ConvergenceLayer specific state.
     */
    void set_info(InterfaceInfo* info) { info_ = info; }

protected:
    friend class InterfaceTable;
    
    Interface(const BundleTuple& tuple,
              ConvergenceLayer* clayer);
    ~Interface();
    
    BundleTuple tuple_;			///< Local tuple of the interface
    ConvergenceLayer* clayer_;		///< Convergence layer to use
    InterfaceInfo* info_;		///< Convergence layer specific state
};

/**
 * Abstract base class for convergence layer specific portions of an
 * interface.
 */
class InterfaceInfo {
public:
    virtual ~InterfaceInfo() {}
};

#endif /* _INTERFACE_H_ */
