/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- 
 *
 * Copyright (C) 2000 by USC/ISI
 * All rights reserved.                                            
 *                                                                
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation, advertising
 * materials, and other materials related to such distribution and use
 * acknowledge that the software was developed by the University of
 * Southern California, Information Sciences Institute.  The name of the
 * University may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *
 * Simulator object that handles population of classifiers 
 */

#ifndef ns_simulator_h
#define ns_simulator_h

#include <tclcl.h>
#include "object.h"

class ParentNode;
class RouteLogic;

class Simulator : public TclObject {
public:
	static Simulator& instance() { return (*instance_); }
      Simulator() : nodelist_(NULL), rtobject_(NULL), nn_(0), \
	size_(0) {}
      ~Simulator() {
	    delete []nodelist_; 
      }
	char* macType() { return macType_; }
	int command(int argc, const char*const* argv);
	void populate_flat_classifiers();
	void populate_hier_classifiers();
	void add_node(ParentNode *node, int id);
	NsObject* get_link_head(ParentNode *node, int nh);
	int node_id_by_addr(int address);
	char *append_addr(int level, int *addr);
	void alloc(int n);
	void check(int n);
	
private:
        ParentNode **nodelist_;
	RouteLogic *rtobject_;
	int nn_;
	int size_;
	char macType_[SMALL_LEN];
	static Simulator* instance_;
};

#endif /* ns_simulator_h */
