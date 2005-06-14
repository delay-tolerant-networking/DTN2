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
#ifndef __FRAGMENT_MANAGER_H__
#define __FRAGMENT_MANAGER_H__

#include <string>
#include <oasys/debug/Log.h>
#include <oasys/util/StringUtils.h>

namespace oasys {
class SpinLock;
}

namespace dtn {

class Bundle;
class BundleList;

// XXX/demmer should change the overall flow of the reassembly so all
// arriving bundle fragments are enqueued onto the appropriate
// reassembly state list immediately, not based on the
// FORWARD_REASSEMBLE action

/**
 * The Fragment Manager maintains state for all of the fragmentary
 * bundles, reconstructing whole bundles from partial bundles.
 *
 * It also implements the routine for creating bundle fragments from
 * larger bundles.
 */
class FragmentManager : public oasys::Logger {
public:
    /**
     * Constructor.
     */
    FragmentManager();
    
    /**
     * Create a bundle fragment from another bundle.
     *
     * @param bundle
     *   the source bundle from which we create the
     *   fragment. Note: the bundle may itself be a fragment
     *
     * @param offset
     *   the offset relative to this bundle (not the
     *   original) for the for the new fragment. note that if this
     *   bundle is already a fragment, the offset into the original
     *   bundle will be this bundle's frag_offset + offset
     *
     * @param length
     *   the length of the fragment we want
     * 
     * @return
     *   pointer to the newly created bundle
     */
    Bundle* create_fragment(Bundle* bundle, int offset, size_t length);

    /**
     * Turn a bundle into a fragment. Note this is used just for
     * reactive fragmentation on a newly received partial bundle and
     * therefore the offset is implicitly zero (unless the bundle was
     * already a fragment).
     */
    void convert_to_fragment(Bundle* bundle, size_t length);

    /**
     * Given the given fragmentation threshold, determine whether the
     * given bundle should be split into several smaller bundles. If
     * so, this returns true and generates a bunch of bundle received
     * events for the individual fragments.
     *
     * Return the number of fragments created or zero if none were
     * created.
     */
    int proactively_fragment(Bundle* bundle, size_t max_length);

    /**
     * If only part of the given bundle was sent successfully, create
     * a new fragment for the unsent portion.
     *
     * Return 1 if a fragment was created, 0 otherwise.
     */
    int reactively_fragment(Bundle* bundle, size_t bytes_sent);

    /**
     * Given a newly arrived bundle fragment, append it to the table
     * of fragments and see if it allows us to reassemble the bundle.
     */
    Bundle* process_for_reassembly(Bundle* fragment);

 protected:
    /// Reassembly state structure
    struct ReassemblyState;
    
    /**
     * Calculate a hash table key from a bundle
     */
    void get_hash_key(const Bundle*, std::string* key);

    /**
     * Check if the bundle has been completely reassembled.
     */
    bool check_completed(ReassemblyState* state);

    /// Table of partial bundles
    typedef oasys::StringHashMap<ReassemblyState*> ReassemblyTable;
    ReassemblyTable reassembly_table_;

    /// Lock
    oasys::SpinLock* lock_;
};

} // namespace dtn

#endif /* __FRAGMENT_MANAGER_H__ */
