#include "Bundle.h"
#include "BundleList.h"
#include "FragmentManager.h"

FragmentManager FragmentManager::instance_;    

Bundle * 
FragmentManager::fragment(Bundle * bundle, int offset, size_t length)
{
    Bundle * ret = new Bundle();
    int payload_offset = 0;
    
    ret->source_ = bundle->source_;
    ret->replyto_ = bundle->replyto_;
    ret->custodian_ = bundle->custodian_;
    ret->dest_ = bundle->dest_;
    ret->priority_ = bundle->priority_;
    ret->custreq_ = bundle->custreq;
    ret->custody_rcpt_ = bundle->custody_rcpt_;
    ret->recv_rcpt_ = bundle->recv_rcpt_;
    ret->fwd_rcpt_ = bundle->fwd_rcpt_;
    ret->return_rcpt_ = bundle->return_rcpt_;
    ret->timestamp_t = bundle->timestamp_t;
    ret->expiration_ = bundle->expiration_;

    // always creating a fragment
    ret->fragment_ = true;

    // offset is given
    ret->frag_offset_ = offset;

    // initialize fragment parameters
    if (bundle->fragment_)
    {
        ret->orig_length_ = bundle->orig_length_;
        payload_offset = offset - bundle>frag_offset_;

        if (payload_offset <= 0)
        {
            log_crit("/bundle/fragment", 
                     "tried to create fragment from non-overlapping fragment",
                     path.c_str());
            return NULL;
        }
    }
    else 
    {
        ret->orig_length = bundle->payload_.length();
        payload_offset = offset;
    }

    // for over-allocated lengths, truncate
    if (offset + length < ret->orig_length)
    {
        length = ret->orig_length - offset;
    }

    // initialize payload
    // XXX: todo - we can modify BundlePayload to use an offset/length in
    // the existing file rather than making a copy
    ret->payload_.set_data(bundle->payload_.read_data(payload_offset, length),
                           length);

    return ret;
}


Bundle * 
FragmentManager::process(Bundle * fragment)
{

}
