/*
 *    Copyright 2007 Intel Corporation
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

#include <map>
#include <string>

using namespace std;

typedef map<unsigned int, dtn_handle_t> HandleMap;

HandleMap    Handles;
static unsigned int HandleID = 0;

//----------------------------------------------------------------------
int
dtn_open()
{
    dtn_handle_t ret = 0;
    int err = dtn_open(&ret);
    if (err != DTN_SUCCESS) {
        return -1;
    }

    unsigned int i = HandleID++;
    Handles[i] = ret;
    return i;
}

//----------------------------------------------------------------------
static dtn_handle_t
find_handle(int i)
{
    HandleMap::iterator iter = Handles.find(i);
    if (iter == Handles.end())
        return NULL;
    return iter->second;
}

//----------------------------------------------------------------------
void
dtn_close(int handle)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return;
    dtn_close(h);
}

//----------------------------------------------------------------------
int
dtn_errno(int handle)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return DTN_EINVAL;
    return dtn_errno(h);
}

//----------------------------------------------------------------------
string
dtn_build_local_eid(int handle, const char* service_tag)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return "";
    
    dtn_endpoint_id_t eid;
    memset(&eid, 0, sizeof(eid));
    dtn_build_local_eid(h, &eid, service_tag);
    return string(eid.uri);
}

//----------------------------------------------------------------------
static int
build_reginfo(dtn_reg_info_t* reginfo,
              const string&   endpoint,
              unsigned int    action,
              unsigned int    expiration,
              bool            init_passive,
              const string&   script)
{
    memset(reginfo, 0, sizeof(dtn_reg_info_t));
    
    strcpy(reginfo->endpoint.uri, endpoint.c_str());
    reginfo->failure_action    = (dtn_reg_failure_action_t)action;
    reginfo->expiration        = expiration;
    reginfo->init_passive      = init_passive;
    reginfo->script.script_len = script.length();
    reginfo->script.script_val = (char*)script.c_str();

    return 0;
}
    
//----------------------------------------------------------------------
int
dtn_register(int           handle,
             const string& endpoint,
             unsigned int  action,
             int           expiration,
             bool          init_passive,
             const string& script)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return -1;

    dtn_reg_info_t reginfo;
    build_reginfo(&reginfo, endpoint, action, expiration,
                  init_passive, script);
        
    dtn_reg_id_t regid = 0;
    int ret = dtn_register(h, &reginfo, &regid);
    if (ret != DTN_SUCCESS) {
        return -1;
    }
    return regid;
}

//----------------------------------------------------------------------
int
dtn_unregister(int handle, dtn_reg_id_t regid)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return -1;
    
    return dtn_unregister(h, regid);
}

//----------------------------------------------------------------------
int
dtn_find_registration(int handle, const string& endpoint)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return -1;

    dtn_endpoint_id_t eid;
    strcpy(eid.uri, endpoint.c_str());

    dtn_reg_id_t regid = 0;
    
    int err = dtn_find_registration(h, &eid, &regid);
    if (err != DTN_SUCCESS) {
        return -1;
    }

    return regid;
}

//----------------------------------------------------------------------
int
dtn_change_registration(int           handle,
                        dtn_reg_id_t  regid,
                        const string& endpoint,
                        unsigned int  action,
                        int           expiration,
                        bool          init_passive,
                        const string& script)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return -1;

    dtn_reg_info_t reginfo;
    build_reginfo(&reginfo, endpoint, action, expiration,
                  init_passive, script);
        
    return dtn_change_registration(h, regid, &reginfo);
}

//----------------------------------------------------------------------
int
dtn_bind(int handle, int regid)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return -1;
    return dtn_bind(h, regid);
}

//----------------------------------------------------------------------
int
dtn_unbind(int handle, int regid)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return -1;
    return dtn_unbind(h, regid);
}

//----------------------------------------------------------------------
struct dtn_bundle_id {
    string       source;
    unsigned int creation_secs;
    unsigned int creation_seqno;
};

//----------------------------------------------------------------------
dtn_bundle_id*
dtn_send(int           handle,
         const string& source,
         const string& dest,
         const string& replyto,
         unsigned int  priority,
         unsigned int  dopts,
         unsigned int  expiration,
         unsigned int  payload_location,
         const string& payload_data)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return NULL;
    
    dtn_bundle_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    
    dtn_bundle_payload_t payload;
    memset(&payload, 0, sizeof(payload));

    strcpy(spec.source.uri, source.c_str());
    strcpy(spec.dest.uri, dest.c_str());
    strcpy(spec.replyto.uri, replyto.c_str());
    spec.priority   = (dtn_bundle_priority_t)priority;
    spec.dopts      = dopts;
    spec.expiration = expiration;

    switch (payload_location) {
    case DTN_PAYLOAD_MEM:
        payload.location    = DTN_PAYLOAD_MEM;
        payload.buf.buf_val = (char*)payload_data.data();
        payload.buf.buf_len = payload_data.length();
        break;
    case DTN_PAYLOAD_FILE:
        payload.location = DTN_PAYLOAD_FILE;
        payload.filename.filename_val = (char*)payload_data.data();
        payload.filename.filename_len = payload_data.length();
        break;
    case DTN_PAYLOAD_TEMP_FILE:
        payload.location = DTN_PAYLOAD_TEMP_FILE;
        payload.filename.filename_val = (char*)payload_data.data();
        payload.filename.filename_len = payload_data.length();
        break;
    default:
        return NULL;
    }

    dtn_bundle_id_t id;
    int err = dtn_send(h, &spec, &payload, &id);
    if (err != DTN_SUCCESS) {
        return NULL;
    }

    dtn_bundle_id* ret = new dtn_bundle_id();
    ret->source         = id.source.uri;
    ret->creation_secs  = id.creation_ts.secs;
    ret->creation_seqno = id.creation_ts.seqno;
    
    return ret;
}

//----------------------------------------------------------------------
int
dtn_cancel(int handle, const dtn_bundle_id& id)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return -1;
    
    dtn_bundle_id_t id2;
    strcpy(id2.source.uri, id.source.c_str());
    id2.creation_ts.secs  = id.creation_secs;
    id2.creation_ts.seqno = id.creation_seqno;
    return dtn_cancel(h, &id2);
}

//----------------------------------------------------------------------
struct dtn_bundle {
    string       source;
    string       dest;
    string       replyto;
    unsigned int priority;
    unsigned int dopts;
    unsigned int expiration;
    unsigned int creation_secs;
    unsigned int creation_seqno;
    string       payload;
};

//----------------------------------------------------------------------
dtn_bundle*
dtn_recv(int handle, int timeout)
{
    dtn_handle_t h = find_handle(handle);
    if (!h) return NULL;
    
    dtn_bundle_spec_t spec;
    memset(&spec, 0, sizeof(spec));
    
    dtn_bundle_payload_t payload;
    memset(&payload, 0, sizeof(payload));

    int err = dtn_recv(h, &spec, DTN_PAYLOAD_MEM, &payload, timeout);
    if (err != DTN_SUCCESS) {
        return NULL;
    }
    
    dtn_bundle* bundle = new dtn_bundle();
    bundle->source         = spec.source.uri;
    bundle->dest           = spec.dest.uri;
    bundle->replyto        = spec.replyto.uri;
    bundle->priority       = spec.priority;
    bundle->dopts          = spec.dopts;
    bundle->expiration     = spec.expiration;
    bundle->creation_secs  = spec.creation_ts.secs;
    bundle->creation_seqno = spec.creation_ts.seqno;
    bundle->payload.assign(payload.buf.buf_val,
                           payload.buf.buf_len);

    return bundle;
}

