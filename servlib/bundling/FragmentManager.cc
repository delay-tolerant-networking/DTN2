
#include "Bundle.h"
#include "BundleList.h"
#include "FragmentManager.h"

FragmentManager FragmentManager::instance_;

FragmentManager::FragmentManager()
    : Logger("/bundle/fragment")
{
}

Bundle* 
FragmentManager::fragment(Bundle* bundle, int offset, size_t length)
{
    Bundle* fragment = new Bundle();
    int payload_offset = 0;
    
    fragment->source_ = bundle->source_;
    fragment->replyto_ = bundle->replyto_;
    fragment->custodian_ = bundle->custodian_;
    fragment->dest_ = bundle->dest_;
    fragment->priority_ = bundle->priority_;
    fragment->custreq_ = bundle->custreq_;
    fragment->custody_rcpt_ = bundle->custody_rcpt_;
    fragment->recv_rcpt_ = bundle->recv_rcpt_;
    fragment->fwd_rcpt_ = bundle->fwd_rcpt_;
    fragment->return_rcpt_ = bundle->return_rcpt_;
    fragment->creation_ts_ = bundle->creation_ts_;
    fragment->expiration_ = bundle->expiration_;

    // always creating a fragmentment
    fragment->is_fragment_ = true;

    // offset is given
    fragment->frag_offset_ = offset;

    // initialize the fragment's orig_length and figure out the offset
    // into the payload
    if (! bundle->is_fragment_) {
        fragment->orig_length_ = bundle->payload_.length();
        payload_offset = offset;

    } else {
        fragment->orig_length_ = bundle->orig_length_;
        payload_offset = offset - bundle->frag_offset_;

        if (payload_offset <= 0) {
            log_err("fragment range doesn't overlap: "
                    "orig_length %d frag_offset %d requested offset %d",
                    bundle->orig_length_, bundle->frag_offset_,
                    offset);
            delete fragment;
            return NULL;
        }
    }

    // check for overallocated length
    if ((offset + length) > fragment->orig_length_) {
        log_err("fragment length overrun: "
                "orig_length %d frag_offset %d requested offset %d length %d",
                fragment->orig_length_, fragment->frag_offset_,
                offset, length);
        delete fragment;
        return NULL;
    }


    // initialize payload
    // XXX/mho: todo - we can modify BundlePayload to use an offset/length in
    // the existing file rather than making a copy
    fragment->payload_.set_data(bundle->payload_.read_data(payload_offset, length),
                                length);

    return fragment;
}


Bundle * 
FragmentManager::process(Bundle * fragment)
{
    NOTIMPLEMENTED;
}
