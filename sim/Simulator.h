/**
 * The main simulator class. This defines the main event loop
 */

#include <queue>
#include "Event.h"

#include "debug/Debug.h"
#include "debug/Log.h"


class Event;
class EventCompare;

class Event_print_stats;

class Simulator : public Logger, public Processable {
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

    /**
     * Return true if initialization has completed.
     */
    static bool initialized() { return (instance_ != NULL); }
    
    static double time() { return instance_->time_; }
    
    //  static void run() { return instance_->run() ; }
    
    static void add_event(Event* e) { instance_->add_event_(e) ; }

    static void remove_event(Event* e) { instance_->remove_event_(e) ; }


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
    void add_event_(Event *e);

    
    /**
     * Remove an event from the main event queue.
     */
    void remove_event_(Event* e);


    /**
     * Update time till simulation has to be run
     */
//    void set_runtill(double t) { runtill_ = t ; }

    /**
     * Stops simulation and exits the loop
     */

    void exit_simulation() ; 

    void run() ;

    static int runtill_ ; /// time when to end the simulation. int bcos of tcl limitation

protected:
    static Simulator* instance_; ///< singleton instance
    void  process(Event* e) ;       ///> Inherited from processable

private:
    
    double time_; /// current time

    bool is_running_; /// maintains the state if the simulator is running or paused
    std::priority_queue<Event*, std::vector<Event*>, EventCompare> eventq_;

};
