#ifndef _TR_AGENT_H
#define _TR_AGENT_H


#include "Event.h"

#include "debug/Debug.h"
#include "debug/Log.h"


class TrAgent : public Processable, public Logger {

public:

    TrAgent(double t,int src, int dst, int nobatches, int batchsize, int gap, int size) ; ///< Constructor

    void process(Event *e) ;     ///< Implementation of process. 
    void start();

private:
    
    void send(double time, int size); //   Inserts message into simulation queue.
    
    double start_time_; ///< time at which this agent starts
    int src_;
    int dst_;
    int reps_;      ///< total number of reps/batches
    int batchsize_ ;     ///< no of messages in each batch
    int gap_ ;           ///< time gap between two batches
    int size_ ;          ///< size of each message

    int repsdone_;      ///< state about no. of reps done
};

#endif /* _TR_AGENT_H */
