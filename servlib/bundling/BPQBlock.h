/*
 *    Copyright 2010-2011 Trinity College Dublin
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

#ifndef _BPQ_BLOCK_H_
#define _BPQ_BLOCK_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BPQ_ENABLED

#include "BlockInfo.h"
#include "BPQFragmentList.h"
#include <oasys/debug/Log.h>
namespace dtn {



class BPQBlock : public oasys::Logger
{
public:
    BPQBlock(const Bundle* bundle);
    ~BPQBlock();

    int write_to_buffer(u_char* buf, size_t len);

    typedef enum {
        KIND_QUERY          			= 0x00,
        KIND_RESPONSE       			= 0x01,
        KIND_RESPONSE_DO_NOT_CACHE_FRAG	= 0x02,
    } kind_t;

    /// @{ Accessors
    kind_t          		kind()          const { return kind_; }
    u_int           		matching_rule() const { return matching_rule_; }
    const BundleTimestamp& 	creation_ts()  	const { return creation_ts_; }
    const EndpointID& 		source()        const { return source_; }
    u_int           		query_len()     const { return query_len_; }
    u_char*        		 	query_val()     const { return query_val_; }
    u_int           		length()        const;
    u_int					frag_len()		const { return fragments_.size(); }
    const BPQFragmentList& 	fragments()  	const { return fragments_; }
    /// @}

    bool match				(const BPQBlock* other)  const;

    /**
     * Add the new fragment in sorted order
     */
    void add_fragment (BPQFragment* fragment);

private:
    int initialise(BlockInfo* block, bool created_locally, const Bundle* bundle);       ///< Wrapper function called by constructor

    void log_block_info(BlockInfo* block);

    int extract_kind			(const u_char* buf, u_int* buf_index, u_int buf_length);
    int extract_matching_rule	(const u_char* buf, u_int* buf_index, u_int buf_length);
    int extract_creation_ts		(const u_char* buf, u_int* buf_index, u_int buf_length, bool local, const Bundle* bundle);
    int extract_source			(const u_char* buf, u_int* buf_index, u_int buf_length, bool local, const Bundle* bundle);
    int extract_query			(const u_char* buf, u_int* buf_index, u_int buf_length);
    int extract_fragments		(const u_char* buf, u_int* buf_index, u_int buf_length);

    kind_t kind_;               	///< Query || Response
    u_int matching_rule_;       	///< Exact
    BundleTimestamp creation_ts_;	///< Original Creation Timestamp
    EndpointID source_;				///< Original Source EID
    u_int query_len_;           	///< Length of the query value
    u_char* query_val_;         	///< Query value
    BPQFragmentList fragments_;  	///< List of fragments returned
};

} // namespace dtn

#endif /* BPQ_ENABLED */

#endif /* _BPQ_BLOCK_H_ */

