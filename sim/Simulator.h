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

#include <queue>
#include <oasys/debug/DebugUtils.h>
#include <oasys/debug/Log.h>

#include "SimEvent.h"
#include "SimEventHandler.h"
#include "storage/DTNStorageConfig.h"

#include <oasys/storage/MemoryStore.h>

namespace dtnsim {

/**
 * The main simulator class. This defines the main event loop
 */
class Simulator : public oasys::Logger, public SimEventHandler {
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

    static double time() { return time_; }

    /**
     * Constructor.
     */
    Simulator(DTNStorageConfig* storage_config);
	
    /**
     * Destructor.
     */
    virtual ~Simulator() {}

    /**
     * Add an event to the main event queue.
     */
    static void post(SimEvent *e);
    
    /**
     * Stops simulation and exits the loop
     */
    void exit();

    /**
     * Main run loop.
     */
    void run();
    
    static double runtill_;		///< time to end the simulation
    
    DTNStorageConfig* storage_config() { return storage_config_; }
    bool init_datastore();
    void close_datastore();
	
protected:
    static Simulator* instance_;	///< singleton instance
    void process(SimEvent* e);		///< virtual from SimEventHandler

private:
    
    static double time_;	///< current time (static to avoid object)

    bool is_running_;	///< maintains the state if the simulator is
    			///< running or paused
                     
    
    std::priority_queue<SimEvent*,
                        std::vector<SimEvent*>,
                        SimEventCompare> eventq_;
	
    DTNStorageConfig*    storage_config_;
    oasys::DurableStore* store_;
};

} // namespace dtnsim
    
