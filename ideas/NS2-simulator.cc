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
 */

#include "simulator.h"
#include "node.h"
#include "address.h"
#include "object.h"

//class ParentNode;

static class SimulatorClass : public TclClass {
public:
	SimulatorClass() : TclClass("Simulator") {}
	TclObject* create(int argc, const char*const* argv) {
		return (new Simulator);
	}
} simulator_class;

Simulator* Simulator::instance_;

int Simulator::command(int argc, const char*const* argv) {
	Tcl& tcl = Tcl::instance();
	if ((instance_ == 0) || (instance_ != this))
		instance_ = this;
	if (argc == 3) {
		if (strcmp(argv[1], "populate-flat-classifiers") == 0) {
			nn_ = atoi(argv[2]);
			populate_flat_classifiers();
			return TCL_OK;
		}
		if (strcmp(argv[1], "populate-hier-classifiers") == 0) {
			nn_ = atoi(argv[2]);
			populate_hier_classifiers();
			return TCL_OK;
		}
		if (strcmp(argv[1], "get-routelogic") == 0) {
			rtobject_ = (RouteLogic *)(TclObject::lookup(argv[2]));
			if (rtobject_ == NULL) {
				tcl.add_errorf("Wrong rtobject name %s", argv[2]);
				return TCL_ERROR;
			}
			return TCL_OK;
		}
		if (strcmp(argv[1], "mac-type") == 0) {
			if (strlen(argv[2]) >= SMALL_LEN) {
				tcl.add_errorf("Length of mac-type name must be < %d", SMALL_LEN);
				return TCL_ERROR;
			}
			strcpy(macType_, argv[2]);
			return TCL_OK;
		}
	}
	if (argc == 4) {
		if (strcmp(argv[1], "add-node") == 0) {
			Node *node = (Node *)(TclObject::lookup(argv[2]));
			if (node == NULL) {
				tcl.add_errorf("Wrong object name %s",argv[2]);
				return TCL_ERROR;
			}
			int id = atoi(argv[3]);
			add_node(node, id);
			return TCL_OK;
		} else if (strcmp(argv[1], "add-lannode") == 0) {
			LanNode *node = (LanNode *)(TclObject::lookup(argv[2]));
			if (node == NULL) {
				tcl.add_errorf("Wrong object name %s",argv[2]);
				return TCL_ERROR;
		  }
			int id = atoi(argv[3]);
			add_node(node, id);
			return TCL_OK;
		} else if (strcmp(argv[1], "add-abslan-node") == 0) {
			AbsLanNode *node = (AbsLanNode *)(TclObject::lookup(argv[2]));
			if (node == NULL) {
				tcl.add_errorf("Wrong object name %s",argv[2]);
				return TCL_ERROR;
			}
			int id = atoi(argv[3]);
			add_node(node, id);
			return TCL_OK;
		} else if (strcmp(argv[1], "add-broadcast-node") == 0) {
			BroadcastNode *node = (BroadcastNode *)(TclObject::lookup(argv[2]));
			if (node == NULL) {
				tcl.add_errorf("Wrong object name %s",argv[2]);
				return TCL_ERROR;
			}
			int id = atoi(argv[3]);
			add_node(node, id);
			return TCL_OK;
		} 
	}
	return (TclObject::command(argc, argv));
}

void Simulator::add_node(ParentNode *node, int id) {
	if (nodelist_ == NULL) 
		nodelist_ = new ParentNode*[SMALL_LEN]; 
	check(id);
	nodelist_[id] = node;
}

void Simulator::alloc(int n) {
        size_ = n;
	nodelist_ = new ParentNode*[n];
	for (int i=0; i<n; i++)
		nodelist_[i] = NULL;
}

// check if enough room in nodelist_
void Simulator::check(int n) {
        if (n < size_)
		return;
	ParentNode **old  = nodelist_;
	int osize = size_;
	int m = osize;
	if (m == 0)
		m = SMALL_LEN;
	while (m <= n)
		m <<= 1;
	alloc(m);
	for (int i=0; i < osize; i++) 
		nodelist_[i] = old[i];
	delete [] old;
}

void Simulator::populate_flat_classifiers() {
	// Set up each classifer (aka node) to act as a router.
	// Point each classifer table to the link object that
	// in turns points to the right node.
	char tmp[SMALL_LEN];
	if (nodelist_ == NULL)
		return;

	// Updating nodelist_ (total no of connected nodes)
	// size since size_ maybe smaller than nn_ (total no of nodes)
	check(nn_);    
	for (int i=0; i<nn_; i++) {
		if (nodelist_[i] == NULL) {
			i++; 
			continue;
		}
		nodelist_[i]->set_table_size(nn_);
		for (int j=0; j<nn_; j++) {
			if (i != j) {
				int nh = -1;
				nh = rtobject_->lookup_flat(i, j);
				if (nh >= 0) {
					NsObject *l_head = get_link_head(nodelist_[i], nh);
					sprintf(tmp, "%d", j);
					nodelist_[i]->add_route(tmp, l_head);
				}
			}  
		}
	}
}


void Simulator::populate_hier_classifiers() {
	// Set up each classifer (aka node) to act as a router.
	// Point each classifer table to the link object that
	// in turns points to the right node.
	int n_addr, levels, nh;
	int addr[TINY_LEN];
	char a[SMALL_LEN];
	// update the size of nodelist with nn_
	check(nn_);
	for (int i=0; i<nn_; i++) {
		if (nodelist_[i] == NULL) {
			i++; 
			continue;
		}
		n_addr = nodelist_[i]->address();
		char *addr_str = Address::instance().
			print_nodeaddr(n_addr);
		levels = Address::instance().levels_;
		int k;
		for (k=1; k <= levels; k++) 
			addr[k-1] = Address::instance().hier_addr(n_addr, k);
		for (k=1; k <= levels; k++) {
			int csize = rtobject_->elements_in_level(addr, k);
			nodelist_[i]->set_table_size(k, csize);
			
			char *prefix = NULL;
			if (k > 1)
				prefix = append_addr(k, addr);
			for (int m=0; m < csize; m++) {
				if (m == addr[k-1])
					continue;
				nh = -1;
				if (k > 1) {
					sprintf(a, "%s%d", prefix,m);
				} else
					sprintf(a, "%d", m);
				rtobject_->lookup_hier(addr_str, a, nh);
				if (nh == -1)
					continue;
				int n_id = node_id_by_addr(nh);
				if (n_id >= 0) {
					NsObject *l_head = get_link_head(nodelist_[i], n_id);
					nodelist_[i]->add_route(a, l_head);
				}	
			}
			if (prefix)
				delete [] prefix;
		}
		delete [] addr_str;
	}
}


int Simulator::node_id_by_addr(int address) {
	for (int i=0; i<nn_; i++) {
		if(nodelist_[i]->address() == address)
			return (nodelist_[i]->nodeid());
	}
	return -1;
}

char *Simulator::append_addr(int level, int *addr) {
	if (level > 1) {
		char tmp[TINY_LEN], a[SMALL_LEN];
		char *str;
		a[0] = '\0';
		for (int i=2; i<= level; i++) {
			sprintf(tmp, "%d.",addr[i-2]);
			strcat(a, tmp);
		}

		// To fix the bug of writing beyond the end of 
		// allocated memory for hierarchical addresses (xuanc, 1/14/02)
		// contributed by Joerg Diederich <dieder@ibr.cs.tu-bs.de>
		str = new char[strlen(a) + 1];
		strcpy(str, a);
		return (str);
	}
	return NULL;
}


NsObject* Simulator::get_link_head(ParentNode *node, int nh) {
	Tcl& tcl = Tcl::instance();
	tcl.evalf("[Simulator instance] get-link-head %d %d",
		  node->nodeid(), nh);
	NsObject *l_head = (NsObject *)TclObject::lookup(tcl.result());
	return l_head;
}

