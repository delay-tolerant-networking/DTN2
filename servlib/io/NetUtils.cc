/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include "NetUtils.h"
#include "debug/Debug.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/**
 * A faster replacement for inet_ntoa().
 * (Copied from the tcpdump source ).
 *
 * Modified to take the buffer as an argument.
 * Returns a pointer within buf where the string starts
 *
 */
const char *
_intoa(u_int32_t addr, char* buf, size_t bufsize)
{
	register char *cp;
	register u_int byte;
	register int n;
        
 	addr = ntohl(addr);
	cp = &buf[bufsize];
	*--cp = '\0';

	n = 4;
	do {
		byte = addr & 0xff;
		*--cp = byte % 10 + '0';
		byte /= 10;
		if (byte > 0) {
			*--cp = byte % 10 + '0';
			byte /= 10;
			if (byte > 0)
				*--cp = byte + '0';
		}
		*--cp = '.';
		addr >>= 8;
	} while (--n > 0);

	return cp + 1;
}

int
gethostbyname(const char* name, in_addr_t* addr)
{
    struct hostent h;
    char buf[2048];
    struct hostent* ret;
    int h_err;
    
    // first check if it's a valid ip address, in which case just
    // return immediately
    if (inet_aton(name, (struct in_addr*)addr) != 0) {
        return 0;
    }
        
    if (::gethostbyname_r(name, &h, buf, sizeof(buf), &ret, &h_err) < 0) {
        ::logf("/net", LOG_ERR, "error return from gethostbyname_r: %s",
               strerror(h_err));
        return -1;
    }

    if (ret == NULL) {
        return -1;
    }

    *addr = ((struct in_addr**)h.h_addr_list)[0]->s_addr;
    return 0;
}
