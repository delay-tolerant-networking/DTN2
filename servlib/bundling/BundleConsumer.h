#ifndef _BUNDLE_CONSUMER_H_
#define _BUNDLE_CONSUMER_H_

class Bundle;
class BundleTuple;

/**
 * Abstract interface used to implement a "next hop" to the bundle
 * forwarder, i.e either a local registration or a real next hop
 * contact.
 */
class BundleConsumer {
public:
    /**
     * Constructor.
     */
    BundleConsumer(const BundleTuple* dest_tuple, bool is_local)
        : dest_tuple_(dest_tuple), is_local_(is_local) {}
    
    /**
     * Consume the given bundle, queueing it if required.
     */
    virtual void consume_bundle(Bundle* bundle) = 0;

    /**
     * Each BundleConsumer has a tuple (either the registration
     * endpoint or the next-hop address).
     */
    const BundleTuple* dest_tuple() { return dest_tuple_; }

    /**
     * Is the consumer a local registration or a peer.
     */
    bool is_local() { return is_local_; }
    
protected:
    const BundleTuple* dest_tuple_;
    bool is_local_;

private:
    /**
     * Default constructor should not be used.
     */
    BundleConsumer();
};

#endif /* _BUNDLE_CONSUMER_H_ */
