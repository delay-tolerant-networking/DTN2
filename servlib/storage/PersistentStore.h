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
#ifndef _PERSISTENT_STORE_H_
#define _PERSISTENT_STORE_H_


#include <vector>
#include <oasys/serialize/Serialize.h>

/**
 * The abstract base class implementing a persistent storage system.
 * Specific implementations (i.e. Berkeley DB or SQL) should derive
 * from this class.
 *
 * TODO:
 *   * should the key be an int or a std::string?
 */
class PersistentStore {
public:
    
    /**
     * Fill in the fields of the object referred to by *obj with the
     * value stored at the given key.
     */
    virtual int get(SerializableObject* obj, const int key) = 0;
    
    /**
     * Store the object with the given key.
     */
    virtual int add(SerializableObject* obj, const int key) = 0;

    /**
     * Update the object with the given key.
     */
    virtual int update(SerializableObject* obj, const int key) = 0;

    /**
     * Delete the object at the given key.
     */
    virtual int del(const int key) = 0;

    /**
     * Return the number of elements in the table.
     */
    virtual int num_elements() = 0;
    
    /**
     * Fill in the given vector with the keys currently stored in the
     * table.
     */
    virtual void keys(std::vector<int> * v) = 0;

};

#endif /* _PERSISTENT_STORE_H_ */
