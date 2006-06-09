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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <oasys/storage/StorageConfig.h>
#include <oasys/storage/BerkeleyDBStore.h>
#include <oasys/io/FileUtils.h>

#include "config.h"
#include "DTNServer.h"

#include "applib/APIServer.h"

#include "bundling/BundleDaemon.h"

#include "contacts/InterfaceTable.h"
#include "contacts/ContactManager.h"

#include "cmd/CompletionNotifier.h"
#include "cmd/BundleCommand.h"
#include "cmd/InterfaceCommand.h"
#include "cmd/LinkCommand.h"
#include "cmd/ParamCommand.h"
#include "cmd/RegistrationCommand.h"
#include "cmd/RouteCommand.h"
#include "cmd/ShutdownCommand.h"
#include "cmd/StorageCommand.h"

#include "conv_layers/ConvergenceLayer.h"

#include "naming/SchemeTable.h"

#include "reg/AdminRegistration.h"
#include "reg/RegistrationTable.h"

#include "routing/BundleRouter.h"

#include "storage/BundleStore.h"
#include "storage/GlobalStore.h"
#include "storage/RegistrationStore.h"

//#include <oasys/storage/MySQLStore.h>
//#include <oasys/storage/PostgresqlStore.h>

namespace dtn {

DTNServer::DTNServer(const char* logpath,
                     oasys::StorageConfig* storage_config)
    : Logger("DTNServer", logpath),
      init_(false),
      in_shutdown_(0),
      storage_config_(storage_config),
      store_(0)
{}

DTNServer::~DTNServer()
{
    log_notice("daemon exiting...");
}

void
DTNServer::init()
{
    ASSERT(oasys::Thread::start_barrier_enabled());
    
    init_commands();
    init_components();
}

void
DTNServer::start()
{
    BundleDaemon* daemon = BundleDaemon::instance();
    daemon->start();
    log_debug("started dtn server");
}

bool
DTNServer::init_datastore()
{
    if (storage_config_->tidy_) 
    {
        storage_config_->init_ = true; // init is implicit with tidy

        // remove bundle data directory (the db contents are cleaned
        // up by the implementation)
        if (!tidy_dir(BundlePayload::payloaddir_.c_str())) {
            return false;
        }
    }

    if (storage_config_->init_)
    {
        if (!init_dir(BundlePayload::payloaddir_.c_str())) {
            return false;
        }
    }

    if (!validate_dir(BundlePayload::payloaddir_.c_str())) {
        return false;
    }

    store_ = new oasys::DurableStore("/dtn/storage");
    int err = store_->create_store(*storage_config_);
    if (err != 0) {
        log_crit("error creating storage system");
        return false;
    }

    if ((GlobalStore::init(*storage_config_, store_)       != 0) || 
        (BundleStore::init(*storage_config_, store_)       != 0) ||
        (RegistrationStore::init(*storage_config_, store_) != 0))
    {
        log_crit("error initializing data store");
        return false;
    }

    // load in the global store here since that will check the
    // database version and exit if there's a mismatch
    if (!GlobalStore::instance()->load()) {
        return false;
    }

    return true;
}
 
bool
DTNServer::parse_conf_file(std::string& conf_file,
                           bool         conf_file_set)
{
    // Check the supplied config file and/or check for defaults, as
    // long as the user didn't explicitly call with no conf file 
    if (conf_file.size() != 0) 
    {
        if (!oasys::FileUtils::readable(conf_file.c_str(), logpath()))
        {
            log_err("configuration file \"%s\" not readable",
                    conf_file.c_str());
            return false;
        }
    }
    else if (!conf_file_set) 
    {
        const char* default_conf[] = { "/etc/dtn.conf", 
                                       "daemon/dtn.conf", 
                                       0 };
        conf_file.clear();
        for (int i=0; default_conf[i] != 0; ++i) 
        {
            if (oasys::FileUtils::readable(default_conf[i], logpath())) 
            {
                conf_file.assign(default_conf[i]);
                break;
            }
        }
        if (conf_file.size() == 0)
        {
            log_warn("can't read default config file "
                     "(tried /etc/dtn.conf and daemon/dtn.conf)...");
        }
    }

    if (conf_file.size() == 0) {
        log_info("No config file specified.");
        return false;
    }

    log_info("parsing configuration file %s...", conf_file.c_str());
    if (oasys::TclCommandInterp::instance()->exec_file(conf_file.c_str()) != 0)
    {
        return false;
    }

    return true;
}

void
DTNServer::init_commands()
{
    oasys::TclCommandInterp* interp = oasys::TclCommandInterp::instance();

    CompletionNotifier::create();
    interp->reg(new BundleCommand());
    interp->reg(new InterfaceCommand());
    interp->reg(new LinkCommand());
    interp->reg(new ParamCommand());
    interp->reg(new RegistrationCommand());
    interp->reg(new RouteCommand());
    interp->reg(new ShutdownCommand(this, "shutdown"));
    interp->reg(new ShutdownCommand(this, "quit"));
    interp->reg(new StorageCommand(storage_config_));

    log_debug("registered dtn commands");
}

void
DTNServer::init_components()
{
    SchemeTable::create();
    ConvergenceLayer::init_clayers();
    InterfaceTable::init();
    BundleDaemon::init();
    
    log_debug("intialized dtn components");
}
   
void
DTNServer::close_datastore()
{
    log_notice("closing persistent data store");
    
    RegistrationStore::instance()->close();
    BundleStore::instance()->close();
    GlobalStore::instance()->close();

    delete_z(store_);
}

void
DTNServer::shutdown()
{
    log_notice("shutting down dtn server");
    
    // make sure only one thread does this
    u_int32_t old_val = atomic_incr_ret(&in_shutdown_);
    if (old_val != 1) {
        while (1) {
            sleep(1000000);
        }
    }

    oasys::Notifier done("/dtnserver/shutdown");
    log_info("DTNServer shutdown called, posting shutdown request to daemon");
    BundleDaemon::instance()->post_and_wait(new ShutdownRequest(), &done);
    
    close_datastore();
    
    oasys::Log::shutdown();
    exit(0);
}

void
DTNServer::set_app_shutdown(ShutdownProc proc, void* data)
{
    BundleDaemon::instance()->set_app_shutdown(proc, data);
}

bool
DTNServer::init_dir(const char* dirname)
{
    struct stat st;
    int statret;
    
    statret = stat(dirname, &st);
    if (statret == -1 && errno == ENOENT)
    {
        if (mkdir(dirname, 0700) != 0) {
            log_crit("can't create directory %s: %s",
                     dirname, strerror(errno));
            return false;
        }
    }
    else if (statret == -1)
    {
        log_crit("invalid path %s: %s", dirname, strerror(errno));
        return false;
    }

    return true;
}

bool
DTNServer::tidy_dir(const char* dirname)
{
    char cmd[256];
    struct stat st;
    
    if (stat(dirname, &st) == 0)
    {
        snprintf(cmd, sizeof(cmd), "/bin/rm -rf %s", dirname);
        log_notice("tidy option removing directory '%s'", cmd);
        
        if (system(cmd))
        {
            log_crit("error removing directory %s", dirname);
            return false;
        }
        
    }
    else if (errno == ENOENT)
    {
        log_debug("directory already removed %s", dirname);
    }
    else
    {
        log_crit("invalid directory name %s: %s", dirname, strerror(errno));
        return false;
    }

    return true;
}

bool
DTNServer::validate_dir(const char* dirname)
{
    struct stat st;
    
    if (stat(dirname, &st) == 0 && S_ISDIR(st.st_mode))
    {
        log_debug("directory validated: %s", dirname);
    }
    else
    {
        log_crit("invalid directory name %s: %s", dirname, strerror(errno));
        return false;
    }

    if (access(dirname, R_OK | W_OK | X_OK) == 0)
    {
        log_debug("directory access validated: %s", dirname);
    }
    else
    {
        log_crit("access failed on directory %s: %s",
                 dirname, strerror(errno));
        return false;
    }

    return true;
}

} // namespace dtn
