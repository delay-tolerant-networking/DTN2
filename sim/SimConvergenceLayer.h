#ifndef _SIM_CONVERGENCE_LAYER_H_
#define _SIM_CONVERGENCE_LAYER_H_


#include "conv_layers/ConvergenceLayer.h"

#include "bundling/Contact.h"
#include "bundling/Bundle.h"
#include "bundling/BundleTuple.h"


#include "SimContact.h"
#include "Message.h"


/******************************************************************************
 *
 * SimConvergenceLayer
 *
 *****************************************************************************/

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
    static int node2id(BundleTuplePattern src) ;

    static const char* id2node(int i) ;
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



/******************************************************************************
 *
 * SimContactInfo
 *
 *****************************************************************************/

// may be need to move this to a new file

class SimContactInfo: public ContactInfo {

public:
    SimContactInfo(int id) : ContactInfo(),simid_(id) {}
    int id () { return simid_; }
    
private:
    int simid_; // id used to identify this contact globally
	
};
#endif /* _SIM_CONVERGENCE_LAYER_H_ */
