/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * timer-handler.cc
 * Copyright (C) 1997 by USC/ISI
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
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header$ (USC/ISI)";
#endif

#include <stdlib.h>  // abort()
#include "timer-handler.h"

void
TimerHandler::cancel()
{
	if (status_ != TIMER_PENDING) {
		fprintf(stderr,
		  "Attempting to cancel timer at %p which is not scheduled\n",
		  this);
		abort();
	}
	_cancel();
	status_ = TIMER_IDLE;
}

/* sched checks the state of the timer before shceduling the
 * event. It the timer is already set, abort is called.
 * This is different than the OTcl timers in tcl/ex/timer.tcl,
 * where sched is the same as reshced, and no timer state is kept.
 */
void
TimerHandler::sched(double delay)
{
	if (status_ != TIMER_IDLE) {
		fprintf(stderr,"Couldn't schedule timer");
		abort();
	}
	_sched(delay);
	status_ = TIMER_PENDING;
}

void
TimerHandler::resched(double delay)
{
	if (status_ == TIMER_PENDING)
		_cancel();
	_sched(delay);
	status_ = TIMER_PENDING;
}

void
TimerHandler::handle(Event *e)
{
	if (status_ != TIMER_PENDING)   // sanity check
		abort();
	status_ = TIMER_HANDLING;
	expire(e);
	// if it wasn't rescheduled, it's done
	if (status_ == TIMER_HANDLING)
		status_ = TIMER_IDLE;
}
