#ifndef _SIM_CONTACT_INFO
#define _SIM_CONTACT_INFO

#include "bundling/Contact.h"

class SimContactInfo: public ContactInfo {

public:
    SimContactInfo(int id) : ContactInfo(),simid_(id) {}
    int id () { return simid_; }
    
private:
    int simid_; // id used to identify this contact globally
	
};

#endif /* _SIM_CONTACT_INFO */
