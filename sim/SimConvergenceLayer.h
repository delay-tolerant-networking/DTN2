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
#ifndef _SIM_CONVERGENCE_LAYER_H_
#define _SIM_CONVERGENCE_LAYER_H_


#include "conv_layers/ConvergenceLayer.h"

#include "bundling/Contact.h"
#include "bundling/Link.h"

#include "bundling/Bundle.h"
#include "bundling/BundleTuple.h"

#include "SimCLInfo.h"

#include "SimContact.h"
#include "Message.h"

namespace dtn {

/**
 * Does the actual forwarding of abundle on a contact.
 * This is done by implementing -send_bundles- function.
 *
 * Also maintains, mapping between various components of DTN2 and 
 * Simulator. Specifically,  maps: DTN2 contacts <-> Simulator Contacts
 * ids from simulator <-> end point id's of DTN2
 * also, bundles <-> messages
 */

class SimConvergenceLayer : public ConvergenceLayer {
    
public:

    /**
     * Virtual functions from convergence layer
     */
    SimConvergenceLayer();
    void init();
    void fini() ;
    bool validate(const std::string& admin);
    void send_bundles(Contact* contact);
    bool match(const std::string& demux, const std::string& admin);
    
    bool open_contact(Contact* contact);
    bool close_contact(Contact* contact);


    /**
     * Functions below are to maintain mapping between simulator and DTN2
     */


    /**
     * Conversion from DTN2 end node to simulator id
     */
    static int node2id(BundleTuplePattern src);
    static std::string id2node(int i);


    /**
     * Conversion between simulator message and DTN2 bundles
     */
    static Message*  bundle2msg(Bundle* b);
    static Bundle*  msg2bundle(Message* m);
    

    static void create_ct(int id) ;

    
    /**
     * Mapping between simulator link model and DTN2 contacts/links
     */
    static SimContact* dtnlink2simlink(Link* link);
    static Link* simlink2dtnlink(SimContact* link);
    static Contact* simlink2ct(SimContact* link) ;
     

    static const int MAX_BUNDLES = 1024;
    //   static const int MAX_CONTACTS = 64;
   static const int MAX_LINKS = 64;
    static Bundle* bundles_[];
    static Message* messages_[];
//    static Contact* contacts_[];
    static Link* links_[];
    
};
} // namespace dtn

#endif /* _SIM_CONVERGENCE_LAYER_H_ */
