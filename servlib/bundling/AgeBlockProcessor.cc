#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include "AgeBlockProcessor.h"
#include "Bundle.h"
#include "BundleDaemon.h"
#include "BundleProtocol.h"
#include "contacts/Link.h"
#include "SDNV.h"

namespace dtn {

AgeBlockProcessor::AgeBlockProcessor()
    : BlockProcessor(BundleProtocol::AGE_BLOCK) 
{
}

int
AgeBlockProcessor::prepare(const Bundle*	bundle,
						   BlockInfoVec*	xmit_blocks,
					       const BlockInfo*	source,
						   const LinkRef&	link,
						   list_owner_t		list) 
{
	/* 
     * Where does prepare fit in the bundle processing? It doesn't seem
     * like we need to do much here aside from calling the parent class
     * function.
     *
     * Would functionality such as checking timestamps happen here? 
     *
     * The parent class function will make room for the outgoing bundle
     * and place this block after the primary block and before the
     * payload. Unsure (w.r.t. prepare()) where or how you'd place the
     * block *after* the payload (see BSP?)
	 */	
    static const char* log = "/dtn/bundle/protocol";
	log_err_p(log, "AEB: prepare()");

	/* @returns BP_SUCCESS */
	return BlockProcessor::prepare(bundle, xmit_blocks, source, link, list);
}

int
AgeBlockProcessor::generate(const Bundle*  bundle,
                            BlockInfoVec*  xmit_blocks,
                            BlockInfo*     block,
                            const LinkRef& link,
                            bool           last)
{
	// This is only done to suppress warnings. Anything unused typecast
	// to (void)
    (void)bundle;
    (void)link;
    (void)xmit_blocks;

    static const char* log = "/dtn/bundle/protocol";
	log_err_p(log, "AEB: generate()");

	//XXX/if the creation timestamp isn't 0, this shouldn't exist.
	//ASSERT(bundle->creation_ts().seconds_ == 0);

	//XXX/what flags do we need to set
	u_int64_t flags = BundleProtocol::BLOCK_FLAG_DISCARD_BUNDLE_ONERROR |
                        (last ? BundleProtocol::BLOCK_FLAG_LAST_BLOCK : 0);

	//XXX/fake age for now
	u_int64_t age = 12345;
	// calculate length of ... age? (SDNV) len(SDNV_VAR)
	size_t length = SDNV::encoding_len(age);

	BlockProcessor::generate_preamble(xmit_blocks,
									  block,
				                      BundleProtocol::AGE_BLOCK,
					                  flags,
									  length); 

	// store contents of our sdnv
	//u_int8_t* bp;
    BlockInfo::DataBuffer* contents = block->writable_contents();
    contents->reserve(block->data_offset() + length);
    contents->set_len(block->data_offset() + length);

	u_int8_t* bp = contents->buf() + block->data_offset();

    //memcpy(contents->buf() + block->data_offset(), bp, length);

	// This copies our encoded SDNV into bp.
	int len = SDNV::encode(age, bp, length);
	// if assertion fails, we ran out of buffer space. 
	ASSERT(len > 0);	

	bp += len; //move pointer?

	// return
	return BP_SUCCESS;	
}
    
int 
AgeBlockProcessor::consume(Bundle*    bundle,
                           BlockInfo* block,
                           u_char*    buf,
                           size_t     len)
{
	/* 
	 * Any special processing that we need to do while consuming the
	 * block we'll put here. It doesn't seem like the logic for what we
	 * do after we consume is handled here; perhaps we should extract
	 * the Age field and modify a new data structure inserted inside a
	 * Bundle?
     *
     * E.g., bundle->age()->assign((int)block->data);
     *
     * Hence we call the parent class consume, which will place the data
     * inside block->data. (Look at BlockInfo class for more
     * information?)
     *
     * For now, consume the block and do nothing.
     */

    static const char* log = "/dtn/bundle/protocol";
	log_err_p(log, "AEB: consume()");

    int cc = BlockProcessor::consume(bundle, block, buf, len);

    if (cc == -1) {
        return -1; // protocol error
    }
    
    if (! block->complete()) {
        ASSERT(cc == (int)len);
        return cc;
    }

	return cc;
}
} // namespace dtn
