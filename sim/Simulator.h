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
#include <queue>
#include <oasys/debug/Debug.h>
#include <oasys/debug/Log.h>

#include "Event.h"
#include "EventHandler.h"

namespace dtnsim {

/**
 * The main simulator class. This defines the main event loop
 */
class Simulator : public oasys::Logger, public EventHandler {
public:
    /**
     * Singleton instance accessor.
     */
    static Simulator* instance() {
        if (instance_ == NULL) {
            PANIC("Simulator::init not called yet");
        }
        return instance_;
    }

    /**
     *  Initialization
     */
    static void init(Simulator* instance) {
        if (instance_ != NULL) {
            PANIC("Simulator::init called multiple times");
        }
        instance_ = instance;
    }

    static double time() { return instance_->time_; }

    /**
     * Constructor.
     */
    Simulator();
    
    /**
     * Destructor.
     */
    virtual ~Simulator() {}

    /**
     * Add an event to the main event queue.
     */
    static void add_event(Event *e);

    
    /**
     * Remove an event from the main event queue.
     */
    static void remove_event(Event* e);

    /**
     * Stops simulation and exits the loop
     */
    void exit();

    /**
     * Main run loop.
     */
    void run();
    
    static int runtill_; 	 /// time to end the simulation
    
protected:
    static Simulator* instance_; ///< singleton instance
    void process(Event* e);      ///< virtual from processable

private:
    
    double time_;     /// current time

    bool is_running_; /// maintains the state if the simulator is
                      /// running or paused
    
    std::priority_queue<Event*, std::vector<Event*>, EventCompare> eventq_;
};

} // namespace dtnsim
    
