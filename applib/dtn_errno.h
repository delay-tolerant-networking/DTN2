/*
 * IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING. By
 * downloading, copying, installing or using the software you agree to
 * this license. If you do not agree to this license, do not download,
 * install, copy or use the software.
 * 
 * Intel Open Source License 
 * 
 * Copyright (c) 2005 Intel Corporation. All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 *   Neither the name of the Intel Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef DTN_ERRNO_H
#define DTN_ERRNO_H

/**
 * DTN API error codes
 */
#define DTN_SUCCESS 	0 		/* ok */
#define DTN_ERRBASE 	128		/* Base error code */
#define DTN_EINVAL 	(DTN_ERRBASE+1) /* invalid argument */
#define DTN_EXDR 	(DTN_ERRBASE+2) /* error in xdr routines */
#define DTN_ECOMM 	(DTN_ERRBASE+3) /* error in ipc communication */
#define DTN_ECONNECT 	(DTN_ERRBASE+4) /* error connecting to server */
#define DTN_ETIMEOUT 	(DTN_ERRBASE+5) /* operation timed out */
#define DTN_ESIZE 	(DTN_ERRBASE+6) /* payload / eid too large */
#define DTN_ENOTFOUND 	(DTN_ERRBASE+7) /* not found (e.g. reg) */
#define DTN_EINTERNAL 	(DTN_ERRBASE+8) /* misc. internal error */
#define DTN_EINPOLL 	(DTN_ERRBASE+9) /* illegal op. called after dtn_poll */
#define DTN_EBUSY 	(DTN_ERRBASE+10) /* registration already in use */
#define DTN_ERRMAX 255

#endif /* DTN_ERRNO_H */
