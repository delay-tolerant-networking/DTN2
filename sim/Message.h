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
#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <vector>

namespace dtnsim {

/**
 * Defines a message. A message is the basic entity that is routed
 * by the simulator. Every message has a unique id.
 * If the message is fragmented, the fragments have the same id
 * but different offsets.
 */

class Message  {
    
public:
    static long total_ ;
    static long next() {
        return total_ ++ ;
    }

    Message();
    Message(int src, int dst, double size);
    
    int id () ; /// returns id of the message
    void set_size(double x) { size_ = x;}
    double size () { return size_ ; }
    void rm_bytes(double len) ;
    int src() { return src_ ; }
    int dst() { return dst_ ; }

    Message* clone() ;
    

    std::vector<int>    hop_idx_;
    std::vector<double> hop_time_;

    void add_hop(int nodeid,double time) 
    { 
        //hop_idx_->push_back(nodeid); hop_time_->push_back(time);
    }
protected:

    int id_; ///> id of the message
    int src_;
    int dst_;
    double size_;
    double origsize_;
    double offset_;


};

} // namespace dtnsim

#endif /* _MESSAGE_H_ */



