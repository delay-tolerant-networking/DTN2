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

class FragmentationAPI
{

    /**
     * Given a bundle, creates a bundle fragment from this bundle
     *
     * If the original bundle is a fragment, then the new bundle fragment
     * offset and length should be within the range of data represented
     * in the original bundle, and are relative to the complete bundle
     */
    int makeBundleFragmentFromBundleMetaData(BUNDLE_METADATA *orig, BUNDLE_METADATA *frag, int offset, int length);

    /**
     * Given a bundle fragment, attempts to reconstruct the bundle from other
     * fragments already stored in the database. If it succeeds, it writes the
     * bundle metadata of the new complete bundle to the reconstructed_m pointer.
     *
     * If the fragment was previously reconstructed, and already delivered, this
     * fragment falls to the floor.
     */
    int addFragment(BUNDLE_METADATA * m, BUNDLE_METADATA * reconstructed_m);
}
