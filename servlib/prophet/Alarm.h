/*
 *    Copyright 2007 Baylor University
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

#ifndef _PROPHET_ALARM_H_
#define _PROPHET_ALARM_H_

#include <sys/time.h>
#include <sys/types.h>
#include <vector>
#include <string>

namespace prophet
{

/**
 * Alarm expiration handler
 */
class ExpirationHandler
{
public:
    /**
     * Constructor
     */
    ExpirationHandler(const std::string& name = "")
        : name_(name) {}

    /**
     * Destructor
     */
    virtual ~ExpirationHandler() {}

    /**
     * Method to invoke when associated alarm expires
     */
    virtual void handle_timeout() = 0;

    /**
     * Accessor
     */
    const char* name() { return name_.c_str(); }

    /**
     * Mutator
     */
    void set_name(const char* name) { name_.assign(name); }

protected:
    std::string name_;

}; // class ExpirationHandler

/**
 * Alarm registration
 */
class Alarm
{
public:
    /**
     * Constructor.
     */
    Alarm(ExpirationHandler* handler)
        : handler_(handler) {}
    
    /**
     * Destructor.
     */
    virtual ~Alarm() {}

    /**
     * How many milliseconds in the future to schedule this alarm
     */
    virtual void schedule(u_int milliseconds) = 0;

    /**
     * Milliseconds remaining until alarm expires
     */
    virtual u_int time_remaining() const = 0;

    /**
     * Disable the alarm; do not execute the handler. It is expected
     * that the host's timer implementation will cleanup a cancelled
     * Alarm's memory ...
     */
    virtual void cancel() = 0;

    /**
     * Invoke timeout handler. It is expected that a successfully
     * executed ExpirationHandler will clean up Alarm's memory
     */
    virtual void timeout() { handler_->handle_timeout(); }

    ///@{ Accessors
    virtual bool pending() const = 0;
    virtual bool cancelled() const = 0;
    ///@}

protected: 
    ExpirationHandler* const handler_; ///< action to perform when
                                       ///  alarm expires
}; // class Alarm

}; // namespace prophet

#endif // _PROPHET_ALARM_H_
