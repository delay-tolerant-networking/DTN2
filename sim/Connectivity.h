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
#ifndef _CONNECTIVITY_H_
#define _CONNECTIVITY_H_

#include "SimEventHandler.h"
#include <oasys/debug/Debug.h>
#include <oasys/debug/Logger.h>
#include <oasys/util/StringUtils.h>

namespace dtnsim {

class Node;


/**
 * Helper struct to store the current connectivity settings
 * between a pair (or set) of nodes.
 */
struct ConnState {
    /**
     * Default constructor, also implicitly the default connectivity
     * state.
     */
    ConnState(): open_(true), bw_(100000), latency_(10) {}

    /**
     * Constructor with explicit settings.
     */
    ConnState(bool open, int bw, int latency)
        : open_(open), bw_(bw), latency_(latency) {}

    /**
     * Utility function to parse a bandwidth specification.
     */
    bool parse_bw(const char* bw_str, int* bw);

    /**
     * Utility function to parse a time specification.
     */
    bool parse_time(const char* time_str, int* time);

    /**
     * Utility function to fill in the values from a set of options
     * (e.g. bw=10kbps, latency=10ms).
     */
    bool parse_options(int argc, const char** argv, const char** invalidp);

    
    bool open_;
    int	 bw_;
    int	 latency_;
};

/**
 * Base class for the underlying connectivity management between nodes
 * in the simulation.
 */
class Connectivity : public oasys::Logger, public SimEventHandler {
public:
    /**
     * Singleton accessor.
     */
    static Connectivity* instance()
    {
        if (!instance_) {
            ASSERT(type_ != ""); // type must be set;
            instance_ = create_conn();
        }
        
        return instance_;
    }

    /**
     * Constructor.
     */
    Connectivity();

    /**
     * Destructor.
     */
    virtual ~Connectivity() {}
    
    /**
     * Get the current connectivity state between two nodes.
     */
    const ConnState* lookup(Node* n1, Node* n2);

    /**
     * Event handler function.
     */
    virtual void process(SimEvent *e);

    /**
     * Hook so implementations can handle arbitrary commands.
     */
    virtual bool exec(int argc, const char** argv);
    
protected:
    friend class ConnCommand;
    
    /**
     * The state structures are stored in a table indexed by strings
     * of the form <node1>,<node2>. Defaults can be set in the config
     * with a node name of '*' (and are stored in the table as such).
     */
    typedef oasys::StringHashMap<ConnState> StateTable;
    StateTable state_;

    /**
     * Static bootstrap initializer.
     */
    static Connectivity* create_conn();

    /**
     * Set the current connectivity state.
     */
    void set_state(const char* n1, const char* n2, const ConnState& s);
    
    static std::string type_;		///< The module type
    static Connectivity* instance_;	///< Singleton instance
};

} // namespace dtnsim

#endif /* _CONNECTIVITY_H_ */
