#ifndef __MSG_QUEUE_H__
#define __MSG_QUEUE_H__

#include <queue>
#include <climits>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/errno.h>

#include "lib/IO.h"
#include "lib/Log.h"
#include "lib/Debug.h"
#include "lib/SpinLock.h"

/*! 
 * Implements a producer/consumer queue for passing data between
 * threads in the system. Multiple threads can add to the queue, and
 * consumer threads will block on the queue if it is empty and wake up
 * when there are new items in the queue.
 *
 */
template<typename _elt_t>
class MsgQueue : private Logger {
public:    
    /*!
     * Constructor.
     * \param name Name of the MsgQueue.
     * \param low  Output warning message when low water mark is reached.
     * \param high Output warning message when high water mark is reached.
     */
    MsgQueue(const char* name = "",
             int low = -1, int high = INT_MAX);

    /*!
     * Destructor.
     */
    ~MsgQueue();

    /// Set the name of the MsgQueue post-construction
    void set_name(const char* name);

    /*!
     * Atomically add msg to the back of the queue and signal waiting
     * threads.
     */
    void enqueue(_elt_t msg, bool at_back = true);

    /*!
     * Atomically add msg to the front of the queue, and signal
     * waiting threads.
     */
    void enqueue_front(_elt_t msg)
    {
        enqueue(msg, false);
    }

    /*!
     * Block and dequeue msg from the queue.
     */
    _elt_t dequeue();

    /*!
     * Try to dequeue a msg from the queue, but don't block. Return
     * true if there was a message on the queue, false otherwise.
     */
    bool try_dequeue(_elt_t* eltp);
    
    /*!
     * \return Size of the queue.
     */
    size_t size()
    {
        ScopeLock l(lock_);
        return queue_.size();
    }

    /*!
     * Return the file descriptor for the read side of the pipe. This
     * allows the message queue to be put in a list for a call to
     * poll().
     */
    int read_fd() { return pipe_[0]; }

    /*!
     * Return the file descriptor for the write side of the pipe.
     */
    int write_fd() { return pipe_[1]; }

protected:
    // Statistics and debugging variables
    int low_water_;
    int high_water_;

    SpinLock* lock_;
    std::deque<_elt_t> queue_;

    int pipe_[2]; ///< pipe file descriptors
};

#include "MsgQueue.tcc"

#endif //__MSG_QUEUE_H__
