/*
 *    Copyright 2004-2006 Intel Corporation
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
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



