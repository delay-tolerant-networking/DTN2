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
