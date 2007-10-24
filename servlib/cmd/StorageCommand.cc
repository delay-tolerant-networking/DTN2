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

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "StorageCommand.h"
#include "bundling/BundlePayload.h"
#include "storage/BundleStore.h"
#include "storage/DTNStorageConfig.h"

namespace dtn {

StorageCommand::StorageCommand(DTNStorageConfig* cfg)
    : TclCommand(cfg->cmd_.c_str())
{
    inited_ = false;
    
    bind_var(new oasys::StringOpt("type", &cfg->type_,
                                  "type", "What storage system to use."));
    bind_var(new oasys::StringOpt("dbname", &cfg->dbname_,
                                  "name", "The database name."));
    bind_var(new oasys::StringOpt("dbdir", &cfg->dbdir_,
                                  "dir", "The database directory."));

    bind_var(new oasys::BoolOpt("init_db", &cfg->init_,
                                "Same as the --init-db argument to dtnd."));
    bind_var(new oasys::BoolOpt("tidy", &cfg->tidy_,
                                "Same as the --tidy argument to dtnd."));
    bind_var(new oasys::IntOpt("tidy_wait", &cfg->tidy_wait_,
                               "time",
                               "How long to wait before really doing "
                               "the tidy operation."));

    bind_var(new oasys::IntOpt("fs_fd_cache_size", &cfg->fs_fd_cache_size_, 
                               "num", "number of open fds to cache"));

    bind_var(new oasys::BoolOpt("db_mpool", &cfg->db_mpool_,
                                "use mpool in Berkeley DB"));
    bind_var(new oasys::BoolOpt("db_log", &cfg->db_log_,
                                "use logging in Berkeley DB"));
    bind_var(new oasys::BoolOpt("db_txn", &cfg->db_txn_,
                                "use transactions in Berkeley DB"));
    bind_var(new oasys::BoolOpt("db_sharefile", &cfg->db_sharefile_,
                                "use shared database file"));
    bind_var(new oasys::IntOpt("db_max_tx", &cfg->db_max_tx_,
                               "num", "max # of active transactions in Berkeley DB"));
    bind_var(new oasys::IntOpt("db_max_locks", &cfg->db_max_locks_,
                               "num", "max # of active locks in Berkeley DB"));
    bind_var(new oasys::IntOpt("db_max_lockers", &cfg->db_max_lockers_,
                               "num", "max # of active locking threads in Berkeley DB"));
    bind_var(new oasys::IntOpt("db_max_lockedobjs", &cfg->db_max_lockedobjs_,
                               "num", "max # of active locked objects in Berkeley DB"));
    bind_var(new oasys::IntOpt("db_lockdetect", &cfg->db_lockdetect_,
                               "freq", "frequency to check for Berkeley DB deadlocks "
                               "(zero disables locking)"));

    bind_var(new oasys::StringOpt("payloaddir", &cfg->payload_dir_, "dir",
                                  "directory for payloads while in transit"));
    
    bind_var(new oasys::UInt64Opt("payload_quota",
                                  &cfg->payload_quota_, "bytes",
                                  "storage quota for bundle payloads "
                                  "(0 is unlimited)"));
    
    bind_var(new oasys::UIntOpt("payload_fd_cache_size",
                                &cfg->payload_fd_cache_size_, "num",
                                "number of payload file descriptors to keep "
                                "open in a cache"));

    bind_var(new oasys::UInt16Opt("server_port",
                                  &cfg->server_port_,
                                  "port number",
                                  "TCP port for IPC to external data store"));

    bind_var(new oasys::StringOpt("schema", &cfg->schema_, "pathname",
                                  "File containing the XML schema for the "
                                  "external data store interface"));

    add_to_help("usage", "print the current storage usage");
}

//----------------------------------------------------------------------
int
StorageCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;
    
    if (argc < 2) {
        resultf("need a storage subcommand");
        return TCL_ERROR;
    }

    const char* cmd = argv[1];

    if (!strcmp(cmd, "usage")) {
        // storage usage
        resultf("bundles %llu", U64FMT(BundleStore::instance()->total_size()));
        return TCL_OK;
    }

    resultf("unknown storage subcommand %s", cmd);
    return TCL_ERROR;
}

} // namespace dtn
