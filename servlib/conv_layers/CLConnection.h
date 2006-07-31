/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2006 Intel Corporation. All rights reserved. 
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
#ifndef _CLCONNECTION_H_
#define _CLCONNECTION_H_

#include <list>
#include <oasys/debug/Log.h>
#include <oasys/thread/MsgQueue.h>
#include <oasys/thread/Thread.h>
#include <oasys/util/SparseBitmap.h>
#include <oasys/util/StreamBuffer.h>

#include "ConnectionConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"

namespace dtn {

/**
 * Helper class (and thread) that manages an established connection
 * with a peer daemon.
 */
class CLConnection : public CLInfo,
                     public oasys::Thread,
                     public oasys::Logger {
public:
    friend class ConnectionConvergenceLayer;
    typedef ConnectionConvergenceLayer::LinkParams LinkParams;
    
    /**
     * Constructor.
     */
    CLConnection(const char* classname,
                 const char* logpath,
                 ConvergenceLayer* cl,
                 LinkParams* params);

    /**
     * Destructor.
     */
    virtual ~CLConnection();

    /**
     * Attach to the given contact.
     */
    void set_contact(const ContactRef& contact) { contact_ = contact; }

protected:
    void run();
    void contact_up();
    void break_contact(ContactEvent::reason_t reason);
    void close_contact();
    void process_command();
    void set_nexthop(const std::string& nexthop) {
        nexthop_ = nexthop;
    }

    /**
     * Initiate a connection to the remote side.
     */
    virtual void connect() = 0;

    /**
     * Accept a connection from the remote side.
     */
    virtual void accept() = 0;

    /**
     * Shut down the connection
     */
    virtual void disconnect() = 0;

    /**
     * Fill in the pollfds array with any file descriptors that should
     * be waited on for activity from the peer.
     */
    virtual void initialize_pollfds() = 0;

    /**
     * Handle a newly arriving bundle for transmission.
     */
    virtual void handle_send_bundle(Bundle* b) = 0;

    /**
     * Handle a cancel bundle request
     */
    virtual void handle_cancel_bundle(Bundle* b) = 0;

    /**
     * Start or continue transmission of bundle data or cl acks. This
     * is called each time through the main run loop.
     */
    virtual void send_pending_data() = 0;
    
    /**
     * Handle network activity from the remote side.
     */
    virtual void handle_poll_activity() = 0;

    /**
     * Handle network activity from the remote side.
     */
    virtual void handle_poll_timeout() = 0;

    /**
     * Enum for messages from the daemon thread to the connection
     * thread.
     */
    typedef enum {
        CLMSG_INVALID       = 0,
        CLMSG_SEND_BUNDLE   = 1,
        CLMSG_CANCEL_BUNDLE = 2,
        CLMSG_BREAK_CONTACT = 3,
    } clmsg_t;

    /**
     * Message to string conversion.
     */
    const char* clmsg_to_str(clmsg_t type) {
        switch(type) {
        case CLMSG_INVALID:		return "CLMSG_INVALID";
        case CLMSG_SEND_BUNDLE:		return "CLMSG_SEND_BUNDLE";
        case CLMSG_CANCEL_BUNDLE:	return "CLMSG_CANCEL_BUNDLE";
        case CLMSG_BREAK_CONTACT:	return "CLMSG_BREAK_CONTACT";
        default:			PANIC("bogus clmsg_t");
        }
    }
    
    /**
     * struct used for messages going from the daemon thread to the
     * connection thread.
     */
    struct CLMsg {
        CLMsg()
            : type_(CLMSG_INVALID),
              bundle_("ConnectionConvergenceLayer::CLMsg") {}
        
        CLMsg(clmsg_t type, Bundle* bundle = NULL)
            : type_(type),
              bundle_(bundle, "ConnectedConvergenceLayer::CLMsg") {}
        
        clmsg_t   type_;
        BundleRef bundle_;
    };

    /**
     * Typedef for bitmaps used to record sent/received/acked data.
     */
    typedef oasys::SparseBitmap<size_t> DataBitmap;
   
    /**
     * Struct used to record bundles that are in-flight along with
     * their transmission state and optionally acknowledgement data.
     */
    class InFlightBundle {
    public:
        InFlightBundle(Bundle* b)
            : bundle_(b, "CLConnection::InFlightBundle"),
              formatted_length_(0),
              header_block_length_(0),
              tail_block_length_(0),
              partial_block_todo_(0) {}
        
        BundleRef bundle_;

        size_t formatted_length_;
        size_t header_block_length_;
        size_t tail_block_length_;
        
        size_t partial_block_todo_;
        
        DataBitmap sent_data_;
        DataBitmap ack_data_;

    private:
        // make sure we don't copy the structure by leaving the copy
        // constructor undefined
        InFlightBundle(const InFlightBundle& copy);
    };

    /**
     * Typedef for the list of in-flight bundles.
     */
    typedef std::list<InFlightBundle*> InFlightList;

    /**
     * Struct used to record bundles that are in the process of being
     * received along with their transmission state and relevant
     * acknowledgement data.
     */
    class IncomingBundle {
    public:
        IncomingBundle(Bundle* b)
            : bundle_(b, "CLConnection::IncomingBundle"),
              total_rcvd_length_(0),
              header_block_length_(0) {}

        BundleRef bundle_;
        
        size_t total_rcvd_length_;
        size_t header_block_length_;

        DataBitmap rcvd_data_;
        DataBitmap ack_data_;

    private:
        // make sure we don't copy the structure by leaving the copy
        // constructor undefined
        IncomingBundle(const IncomingBundle& copy);
    };

    /**
     * Typedef for the list of in-flight bundles.
     */
    typedef std::list<IncomingBundle*> IncomingList;
    
    ContactRef          contact_;	///< Ref to the Contact
    oasys::MsgQueue<CLMsg> cmdqueue_;	///< Queue of commands from daemon
    ConvergenceLayer*   cl_;		///< Pointer to the CL
    LinkParams*		params_;	///< Pointer to Link parameters, or
                                        ///< to defaults until Link is bound
    std::string		nexthop_;	///< Nexthop identifier set by CL
    int    		num_pollfds_;   ///< Number of pollfds in use
    static const int	MAXPOLL = 8;	///< Maximum number of pollfds
    struct pollfd       pollfds_[MAXPOLL]; ///< Array of pollfds
    int                 poll_timeout_;	///< Timeout to wait for poll data
    oasys::StreamBuffer sendbuf_; 	///< Buffer for outgoing data
    oasys::StreamBuffer recvbuf_;	///< Buffer for incoming data
    InFlightList	inflight_;	///< Bundles going out the wire
    IncomingList	incoming_;	///< Bundles arriving on the wire
    volatile bool	contact_broken_; ///< Contact has been broken
};

} // namespace dtn

#endif /* _CLCONNECTION_H_ */
