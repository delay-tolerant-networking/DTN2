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

#include "config.h"
#include "DTNServer.h"

#include "bundling/AddressFamily.h"
#include "bundling/BundleDaemon.h"
#include "bundling/InterfaceTable.h"
#include "bundling/ContactManager.h"

#include "cmd/BundleCommand.h"
#include "cmd/InterfaceCommand.h"
#include "cmd/LinkCommand.h"
#include "cmd/ParamCommand.h"
#include "cmd/RegistrationCommand.h"
#include "cmd/RouteCommand.h"
#include "cmd/StorageCommand.h"

#include "conv_layers/ConvergenceLayer.h"

#include "reg/AdminRegistration.h"
#include "reg/RegistrationTable.h"

#include "routing/BundleRouter.h"

#include "storage/BundleStore.h"
#include "storage/GlobalStore.h"
#include "storage/RegistrationStore.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <oasys/storage/StorageConfig.h>
#include <oasys/storage/BerkeleyDBStore.h>
//#include <oasys/storage/MySQLStore.h>
//#include <oasys/storage/PostgresqlStore.h>

namespace dtn {

void
DTNServer::init_commands()
{
    oasys::TclCommandInterp* interp = oasys::TclCommandInterp::instance();
    
    interp->reg(new BundleCommand());
    interp->reg(new InterfaceCommand());
    interp->reg(new LinkCommand());
    interp->reg(new ParamCommand());
    interp->reg(new RegistrationCommand());
    interp->reg(new RouteCommand());
    interp->reg(new StorageCommand());

    log_debug("/dtnserver", "registered dtn commands");
}

/**
 * Initialize all components before modifying any configuration.
 */
void
DTNServer::init_components()
{
    AddressFamilyTable::init();
    ConvergenceLayer::init_clayers();
    InterfaceTable::init();
    BundleDaemon::init(new BundleDaemon());
    
    log_debug("/dtnserver", "intialized dtn components");
}

/**
 * Post configuration, start up all components.
 */
void
DTNServer::init_datastore()
{
    oasys::StorageConfig* cfg = oasys::StorageConfig::instance();
    
    if (cfg->tidy_) 
    {
        // init is implicit with tidy
        cfg->init_ = true; 

        // remove bundle data directory (the db contents are cleaned
        // up by the implementation)
        DTNServer::tidy_dir(BundlePayload::payloaddir_.c_str());
    }

    if (cfg->init_)
    {
        // initialize data directories
        DTNServer::init_dir(BundlePayload::payloaddir_.c_str());
        DTNServer::init_dir(cfg->dbdir_.c_str());
    }

    // validation
    DTNServer::validate_dir(BundlePayload::payloaddir_.c_str());
    DTNServer::validate_dir(cfg->dbdir_.c_str());


    // initialize the oasys durable storage system based on the type

    if (0) {} // symmetry
    
#if LIBDB_ENABLED
    else if (cfg->type_.compare("berkeleydb") == 0)
    {
        oasys::BerkeleyDBStore::init();
    }
#endif

#if MYSQL_ENABLED
#error Mysql support not yet added to oasys
//     else if (cfg->type_.compare("mysql") == 0)
//     {
//         oasys::MySQLStore::init();
//     }
#endif

#if POSTGRES_ENABLED
#error Postgres support not yet added to oasys
//     else if (cfg->type_.compare("postgres") == 0)
//     {
//         oasys::PostgresqlStore::init();
//     }
#endif

    else
    {
        log_err("/dtnserver", "storage type %s not implemented, exiting...",
                cfg->type_.c_str());
        exit(1);
    }

    GlobalStore::init();
    BundleStore::init();
    RegistrationStore::init();
}
    
void
DTNServer::start()
{
    BundleRouter* router;
    router = BundleRouter::create_router(BundleRouter::Config.type_.c_str());

    BundleDaemon* daemon = BundleDaemon::instance();
    daemon->set_router(router);
    daemon->start();

    // create the registration table and the default administrative
    // registration (which registers itself)
    RegistrationTable::init(RegistrationStore::instance());
    new AdminRegistration();
    
    // load in the various storage tables
    GlobalStore::instance()->load();
    RegistrationTable::instance()->load();
    BundleStore::instance()->load();

    router->initialize();
    log_debug("/dtnserver", "started dtn server");
}

void
DTNServer::close_datastore()
{
    GlobalStore::instance()->close();
    BundleStore::instance()->close();
    RegistrationStore::instance()->close();

    oasys::DurableStore::shutdown();
}

void
DTNServer::init_dir(const char * dirname)
{
    struct stat st;
    int statret;
    
    statret = stat(dirname, &st);
    if (statret == -1 && errno == ENOENT)
    {
        if (mkdir(dirname, 0700) != 0) {
            log_crit("/dtnserver", "can't create directory %s: %s",
                     dirname, strerror(errno));
            exit(1);
        }
    }
    else if (statret == -1)
    {
        log_crit("/dtnserver", "invalid path %s: %s", dirname, strerror(errno));;
        exit(1);
    }
}

void
DTNServer::tidy_dir(const char * dirname)
{
    char cmd[256];
    struct stat st;
    
    if (stat(dirname, &st) == 0)
    {
        snprintf(cmd, sizeof(cmd), "/bin/rm -rf %s", dirname);
        log_warn("/dtnserver",
                 "tidy option removing directory '%s'", cmd);
        
        if (system(cmd))
        {
            log_crit("/dtnserver", "error removing directory %s", dirname);
            exit(1);
        }
        
    }
    else if (errno == ENOENT)
    {
        log_debug("/dtnserver", "directory already removed %s", dirname);
    }
    else
    {
        log_crit("/dtnserver", "invalid directory name %s: %s", dirname, strerror(errno));
        exit(1);
    }
}

void
DTNServer::validate_dir(const char * dirname)
{
    struct stat st;
    
    if (stat(dirname, &st) == 0 && S_ISDIR(st.st_mode))
    {
        log_debug("/dtnserver", "directory validated: %s", dirname);
    }
    else
    {
        log_crit("/dtnserver",
                 "invalid directory name %s: %s",
                 dirname, strerror(errno));
        exit(1);
    }
}

} // namespace dtn
