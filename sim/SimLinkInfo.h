#ifndef _SIM_LINK_INFO
#define _SIM_LINK_INFO

#include "bundling/Link.h"

class SimLinkInfo: public LinkInfo {

public:
    SimLinkInfo(int id) : LinkInfo(),simid_(id) {}
    int id () { return simid_; }
    
private:
    int simid_; // id used to identify this contact globally
	
};

#endif /* _SIM_LINK_INFO */
