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
#ifndef _DTNSERVER_H_
#define _DTNSERVER_H_

#include <oasys/debug/Logger.h>
#include <oasys/storage/StorageConfig.h>

namespace oasys { class DurableStore; }

namespace dtn {

class APIServer;

/*!
 * Encapsulation class for the "guts" of the server library. All
 * functions and member variables are static.
 */
class DTNServer : public oasys::Logger {
public:
    DTNServer(oasys::StorageConfig* storage_config);
    ~DTNServer();
    
    oasys::StorageConfig* storage_config() { return storage_config_; }

    /*! Initialize storage, components
     *
     * NOTE: This needs to be called with thread barrier and timer
     * system off because of initialization ordering constraints.
     */
    void init();

    //! Initialize the datastore (used to create an empty database)
    void init_datastore();

    //! Start DTN daemon
    void start();

    //! Parse the conf file
    void parse_conf_file(std::string& conf_file,
                         bool         conf_file_set);

    //! Shut down the server
    void shutdown();

    //! Typedef for a shutdown procedure
    typedef void (*ShutdownProc) (void* args);

    //! Set an application-specific shutdown handler.
    void set_app_shutdown(ShutdownProc proc, void* data);

private:
    bool init_;

    oasys::StorageConfig* storage_config_;
    oasys::DurableStore*  store_;
    APIServer*            api_server_;

    void init_dir(const char* dirname);
    void tidy_dir(const char* dirname);
    void validate_dir(const char* dirname);    

    /**
     * Initialize and register all the server related dtn commands.
     */
    void init_commands();
    
    /**
     * Initialize all components before modifying any configuration.
     */
    void init_components();

    /**
     * Close and sync the data store.
     */
    void close_datastore();
};

} // namespace dtn

#endif /* _DTNSERVER_H_ */
