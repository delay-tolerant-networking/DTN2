#ifndef _MSG_QUEUE_H_
#define _MSG_QUEUE_H_

#include <queue>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/errno.h>

#include "Notifier.h"
#include "SpinLock.h"
#include "debug/Log.h"
#include "debug/Debug.h"

/**
 * A producer/consumer queue for passing data between threads in the
 * system, using the Notifier functionality to block and wakeup
 * threads.
 */
template<typename _elt_t>
class MsgQueue : public Notifier {
public:    
    /*!
     * Constructor.
     */
    MsgQueue(const char* logpath = NULL, SpinLock* lock = NULL);
        
    /**
     * Destructor.
     */
    ~MsgQueue();

    /**
     * Atomically add msg to the back of the queue and signal a
     * waiting thread.
     */
    void push(_elt_t msg, bool at_back = true);
    
    /**
     * Atomically add msg to the front of the queue, and signal
     * waiting threads.
     */
    void push_front(_elt_t msg)
    {
        push(msg, false);
    }

    /**
     * Block and pop msg from the queue.
     */
    _elt_t pop_blocking();

    /**
     * Try to pop a msg from the queue, but don't block. Return
     * true if there was a message on the queue, false otherwise.
     */
    bool try_pop(_elt_t* eltp);
    
    /**
     * \return Size of the queue.
     */
    size_t size()
    {
        ScopeLock l(lock_);
        return queue_.size();
    }

protected:
    SpinLock* lock_;
    std::deque<_elt_t> queue_;
};

#include "MsgQueue.tcc"

#endif //_MSG_QUEUE_H_
