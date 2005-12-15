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

#include <oasys/storage/StorageConfig.h>

#include "StorageCommand.h"
#include "bundling/BundlePayload.h"

namespace dtn {

StorageCommand::StorageCommand(oasys::StorageConfig* cfg)
    : TclCommand(cfg->cmd_.c_str())
{
    inited_ = false;
    
    bind_s("type",	&cfg->type_, "What storage system to use.");
    bind_b("tidy",	&cfg->tidy_, "Same as the --tidy argument to dtnd.");
    bind_b("init_db",	&cfg->init_, "Same as the --init-db argument to dtnd.");
    bind_i("tidy_wait",	&cfg->tidy_wait_,
           "How long to wait before really doing the tidy operation.");
    bind_s("dbname",	&cfg->dbname_, "The database name.");
    bind_s("dbdir",	&cfg->dbdir_,  "The database directory.");
    bind_s("payloaddir",&BundlePayload::payloaddir_,
        "directory for payloads while in transit");
    bind_i("dbtxmax",   &cfg->dbtxmax_, "max # of active transactions in Berkeley DB");
    bind_b("dbsharefile", &cfg->dbsharefile_, "use shared database file");
}

} // namespace dtn
