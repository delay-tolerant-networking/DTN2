#ifndef _SIM_CONVERGENCE_LAYER_H_
#define _SIM_CONVERGENCE_LAYER_H_


#include "conv_layers/ConvergenceLayer.h"

#include "bundling/Contact.h"
#include "bundling/Bundle.h"
#include "bundling/BundleTuple.h"

#include "SimContactInfo.h"

#include "SimContact.h"
#include "Message.h"

/**
 * Does the actual forwarding of abundle on a contact.
 * This is done by implementing -send_bundles- function.
 *
 * Also maintains, mapping between various components of DTN2 and 
 * Simulator. Specifically,  maps: DTN2 contacts <-> Simulator Contacts
 * ids from simulator <-> end point id's of DTN2
 * also, bundles <-> messages
 */

class SimConvergenceLayer : public ConvergenceLayer {
    
public:

    /**
     * Virtual functions from convergence layer
     */
    SimConvergenceLayer();
    void init();
    void fini() ;
    bool validate(const std::string& admin);
    void send_bundles(Contact* contact);
    bool match(const std::string& demux, const std::string& admin);
    
    bool open_contact(Contact* contact);
    bool close_contact(Contact* contact);


    /**
     * Functions below are to maintain mapping between simulator and DTN2
     */


    /**
     * Conversion from DTN2 end node to simulator id
     */
    static int node2id(BundleTuplePattern src);
    static std::string id2node(int i);


    /**
     * Conversion between simulator message and DTN2 bundles
     */
    static Message*  bundle2msg(Bundle* b);
    static Bundle*  msg2bundle(Message* m);
    

    static void create_ct(int id) ;

    
    /**
     * Mapping between simulator link model and DTN2 contacts
     */
    static SimContact* ct2simlink(Contact* contact);
    static Contact* simlink2ct(SimContact* link);

    static const int MAX_BUNDLES = 1024;
    static const int MAX_CONTACTS = 64;
    static Bundle* bundles_[];
    static Message* messages_[];
    static Contact* contacts_[];
    
};

#endif /* _SIM_CONVERGENCE_LAYER_H_ */
