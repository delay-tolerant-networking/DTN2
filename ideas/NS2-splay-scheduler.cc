/* -*-  Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 2002 University of Southern California/Information
 * Sciences Institute.  All rights reserved.
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
 * @(#) $Header$
 */
#ifndef lint
static const char rcsid[] =
"@(#) $Header$";
#endif
/**
 *
 * Scheduler based on a splay tree.
 *
 * Daniel D. Sleator and Robert E. Tarjan. Self-adjusting binary
 * search trees. Journal of the ACM, 32(3):652--686, 1985. 118.
 *
 * Basic idea of this scheduler: it organizes the event queue into a
 * binary search tree.  For every insert and deque operation,
 * "splaying" is performed aimed at shortening the search path for
 * "similar" priorities (time_).  This should give O(log(N)) amortized
 * time for basic operations, may be even better for correlated inserts.
 *
 * Implementation notes: Event::next_ and Event::prev_ are used as
 * right and left pointers.  insert() and deque() use the "top-down"
 * splaying algorithm taken almost verbatim from the paper and in some
 * cases optimized for particular operations.  lookup() is extremely
 * inefficient, and should be avoided whenever possible.  cancel()
 * would be better off if we had a pointer to the parent, then there
 * wouldn't be any need to search for it (and use Event::uid_ to
 * resolve conflicts when same-priority events are both on the left
 * and right).  cancel() does not perform any splaying, while it
 * perhaps should.
 *
 * Memory used by this scheduler is O(1) (not counting the events).
 *
 **/
#include <scheduler.h>
#include <assert.h>


static class SplaySchedulerClass : public TclClass 
{
public:
        SplaySchedulerClass() : TclClass("Scheduler/Splay") {}
        TclObject* create(int /* argc */, const char*const* /* argv */) {
                return (new SplayScheduler);
        }
} class_splay_sched;

#undef LEFT
#undef RIGHT
#define LEFT(e)				((e)->prev_)	
#define RIGHT(e)			((e)->next_)

#define ROTATE_RIGHT(t, x)		\
    do {				\
    	LEFT(t) = RIGHT(x);	 	\
    	RIGHT(x) = (t);			\
    	(t) = (x);			\
    } while(0)

#define ROTATE_LEFT(t, x)		\
    do {				\
    	RIGHT(t)  = LEFT(x);	 	\
    	LEFT(x) = (t);			\
    	(t) = (x);			\
    } while(0)
#define LINK_LEFT(l, t)			\
    do {				\
	RIGHT(l) = (t);			\
	(l) = (t);			\
	(t) = RIGHT(t);			\
    } while (0)
#define LINK_RIGHT(r, t)		\
    do {				\
	LEFT(r) = (t);			\
	(r) = (t);			\
	(t) = LEFT(t);			\
    } while (0)

void
SplayScheduler::insert(Event *n) 
{
	Event *l;	// bottom right in the left tree
	Event *r;	// bottom left in the right tree
	Event *t;	// root of the remaining tree
	Event *x;   	// current node
    
	++qsize_;

	double time = n->time_;
    
	if (root_ == 0) {
		LEFT(n) = RIGHT(n) = 0;
		root_ = n;
		//validate();
		return;
	}
	t = root_;
	root_ = n;	// n is the new root
	l = n;
	r = n;
	for (;;) {
		if (time < t->time_) {
			x = LEFT(t);
			if (x == 0) {
				LEFT(r) = t;
				RIGHT(l) = 0;
				break;
			}
			if (time < x->time_) {
				ROTATE_RIGHT(t, x);
			}
			LINK_RIGHT(r, t);
			if (t == 0) {
				RIGHT(l) = 0;
				break;
			}
		} else {
			x = RIGHT(t);
			if (x == 0) {
				RIGHT(l) = t; 
				LEFT(r) = 0;
				break;	
			}
			if (time >= x->time_) {
				ROTATE_LEFT(t, x);
			}
			LINK_LEFT(l, t);
			if (t == 0) {
				LEFT(r) = 0;
				break;
			}
			
		}
	} /* for (;;) */

	// assemble:
	//   swap left and right children
	x = LEFT(n);
	LEFT(n) = RIGHT(n);
	RIGHT(n) = x;
	//validate();
}

const Event *
SplayScheduler::head()
{
	Event *t;
	Event *l;
#if 1
	if (root_ == 0)
		return 0;
	for (t = root_; (l = LEFT(t)); t = l)
		;

	return t;
#else
	Event *ll;
	Event *lll;

	if (root_ == 0)
		return 0;

	t = root_;
	l = LEFT(t);

	if (l == 0) {
		return t;
	}
	for (;;) { 
		ll = LEFT(l);
		if (ll == 0) {
			return l;
		}

		lll = LEFT(ll);
		if (lll == 0) {
			return ll;
		}

		// zig-zig: rotate l with ll
		LEFT(t) = ll;
		LEFT(l) = RIGHT(ll);
		RIGHT(ll) = l;

		t = ll;
		l = lll;
	}
#endif /* 1/0 */
}

Event *
SplayScheduler::deque()
{
	Event *t;
	Event *l;
	Event *ll;
	Event *lll;

	if (root_ == 0)
		return 0;

	--qsize_;

	t = root_;
	l = LEFT(t);

	if (l == 0) {			// root is the element to dequeue
		root_ = RIGHT(t);	// right branch becomes the root
		//validate();
		return t;
	}
	for (;;) { 
		ll = LEFT(l);
		if (ll == 0) {
			LEFT(t) = RIGHT(l);
			//validate();
			return l;
		}

		lll = LEFT(ll);
		if (lll == 0) {
			LEFT(l) = RIGHT(ll);
			//validate();
			return ll;
		}

		// zig-zig: rotate l with ll
		LEFT(t) = ll;
		LEFT(l) = RIGHT(ll);
		RIGHT(ll) = l;

		t = ll;
		l = lll;
	}
} 

void 
SplayScheduler::cancel(Event *e)
{

	if (e->uid_ <= 0) 
		return; // event not in queue

	Event **t;
	//validate();
	if (e == root_) {
		t = &root_;
	} else {
		// searching among same-time events is a real bugger,
		// all because we don't have a parent pointer; use
		// uid_ to resolve conflicts.
		for (t = &root_; *t;) {
			t = ((e->time_ > (*t)->time_) || 
			     ((e->time_ == (*t)->time_) && e->uid_ > (*t)->uid_))
				? &RIGHT(*t) : &LEFT(*t);
			if (*t == e)
				break;
		}
		if (*t == 0) {
			fprintf(stderr, "did not find it\n");
			abort(); // not found
		}
	}
	// t is the pointer to e in the parent or to root_ if e is root_
	e->uid_ = -e->uid_;
	--qsize_;

	if (RIGHT(e) == 0) {
		*t = LEFT(e);
		LEFT(e) = 0;
		//validate();
		return;
	} 
	if (LEFT(e) == 0) {
		*t = RIGHT(e);
		RIGHT(e) = 0;
		//validate();
		return;
	}

	// find successor
	Event *p = RIGHT(e), *q = LEFT(p);

	if (q == 0) {
		// p is the sucessor
		*t = p;
		LEFT(p) = LEFT(e);
		//validate();
		return;
	}
	for (; LEFT(q); p = q, q = LEFT(q)) 
		;
	// q is the successor
	// p is q's parent
	*t = q;
	LEFT(p) = RIGHT(q);
	LEFT(q) = LEFT(e);
	RIGHT(q) = RIGHT(e);
	RIGHT(e) = LEFT(e) = 0;
	//validate();
}


Event *
SplayScheduler::lookup(scheduler_uid_t uid) 
{
	lookup_uid_ = uid;
	return uid_lookup(root_);
}

Event *
SplayScheduler::uid_lookup(Event *root)
{
	if (root == 0)
		return 0;
	if (root->uid_ == lookup_uid_)
		return root;

	Event *res = uid_lookup(LEFT(root));
 
	if (res) 
		return res;

	return uid_lookup(RIGHT(root));
}

int
SplayScheduler::validate(Event *root) 
{
	int size = 0;
	if (root) {
		++size;
		assert(LEFT(root) == 0 || root->time_ >= LEFT(root)->time_);
		assert(RIGHT(root) == 0 || root->time_ <= RIGHT(root)->time_);
		size += validate(LEFT(root));
		size += validate(RIGHT(root));
		return size;
	}
	return 0;
}
