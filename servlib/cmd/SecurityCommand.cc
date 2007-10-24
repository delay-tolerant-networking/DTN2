/*
 * Copyright 2007 BBN Technologies Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You
 * may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 */

/*
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#ifdef BSP_ENABLED

#include <cstring>
#include <cstdio>
#include <oasys/util/Base16.h>

#include "SecurityCommand.h"
#include "security/SPD.h"
#include "security/KeyDB.h"
#include "security/Ciphersuite.h"

namespace dtn {

SecurityCommand::SecurityCommand()
    : TclCommand("security")
{
    add_to_help("setpolicy [in | out] [psb] [cb] [bab]",
                "Set inbound or outbound policy to require the specified\n"
                "combination of PSB, CB, and/or BAB.");
    add_to_help("setkey <host> <cs_num> <key>",
                "Set the key to use for the specified host and ciphersuite\n"
                "number.  <host> may also be the wildcard symbol \"*\".");
    add_to_help("dumpkeys",
                "Dump the contents of the keystore to the screen.");
    add_to_help("flushkeys",
                "Erase all keys from the keystore.");
}

int
SecurityCommand::exec(int argc, const char** argv, Tcl_Interp* interp)
{
    (void)interp;

    if (argc < 2) {
        resultf("need a security subcommand");
        return TCL_ERROR;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "setpolicy") == 0) {
        // security setpolicy [in | out] [psb] [cb] [bab]
        if (argc < 3 || argc > 6) {
            wrong_num_args(argc, argv, 2, 3, 6);
            return TCL_ERROR;
        }

        SPD::spd_direction_t direction;
        if (strcmp(argv[2], "in") == 0)
            direction = SPD::SPD_DIR_IN;
        else if (strcmp(argv[2], "out") == 0)
            direction = SPD::SPD_DIR_OUT;
        else {
            resultf("invalid direction argument \"%s\" (must be one of "
                    "\"in\" or \"out\")", argv[2]);
            return TCL_ERROR;
        }
        
        int policy = SPD::SPD_USE_NONE;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "psb") == 0)
                policy |= SPD::SPD_USE_PSB;
            else if (strcmp(argv[i], "cb") == 0)
                policy |= SPD::SPD_USE_CB;
            else if (strcmp(argv[i], "bab") == 0)
                policy |= SPD::SPD_USE_BAB;
            else {
                resultf("invalid argument \"%s\"", argv[i]);
                return TCL_ERROR;
            }
        }

        SPD::set_global_policy(direction, (SPD::spd_policy_t)policy);

        return TCL_OK;

    } else if (strcmp(cmd, "setkey") == 0) {
        // security setkey <host> <cs_num> <key>
        if (argc != 5) {
            wrong_num_args(argc, argv, 2, 5, 5);
            return TCL_ERROR;
        }

        const char* host_arg   = argv[2];
        const char* cs_num_arg = argv[3];
        const char* key_arg    = argv[4];

        int cs_num;
        if (sscanf(cs_num_arg, "%i", &cs_num) != 1) {
            resultf("invalid cs_num argument \"%s\"", cs_num_arg);
            return TCL_ERROR;
        }
        if (! KeyDB::validate_cs_num(cs_num)) {
            resultf("invalid ciphersuite number %#x", cs_num);
            return TCL_ERROR;
        }

        size_t key_arg_len = strlen(key_arg);
        if ((key_arg_len % 2) != 0) {
            resultf("invalid key argument (must be even length)");
            return TCL_ERROR;
        }

        size_t key_len = key_arg_len / 2;
        if (! KeyDB::validate_key_len(cs_num, &key_len)) {
            resultf("wrong key length for ciphersuite (expected %d bytes)",
                    (int)key_len);
            return TCL_ERROR;
        }
            
        // convert key from ASCII hexadecimal to raw bytes
        u_char* key = new u_char[key_len];
        for (int i = 0; i < (int)key_len; i++)
        {
            int b = 0;

            for (int j = 0; j <= 1; j++)
            {
                int c = (int)key_arg[2*i+j];
                b <<= 4;

                if (c >= '0' && c <= '9')
                    b |= c - '0';
                else if (c >= 'A' && c <= 'F')
                    b |= c - 'A' + 10;
                else if (c >= 'a' && c <= 'f')
                    b |= c - 'a' + 10;
                else {
                    resultf("invalid character '%c' in key argument",
                            (char)c);
                    delete key;
                    return TCL_ERROR;
                }
            }
            
            key[i] = (u_char)(b);
        }

        KeyDB::Entry new_entry =
            KeyDB::Entry(host_arg, cs_num, key, key_len);
        KeyDB::set_key(new_entry);

        delete key;
        return TCL_OK;

    } else if (strcmp(cmd, "dumpkeys") == 0) {
        // security dumpkeys
        if (argc != 2) {
            wrong_num_args(argc, argv, 2, 2, 2);
            return TCL_ERROR;
        }

        oasys::StringBuffer buf;
        KeyDB::dump_header(&buf);
        KeyDB::dump(&buf);
        set_result(buf.c_str());
        return TCL_OK;

    } else if (strcmp(cmd, "flushkeys") == 0) {
        // security flushkeys
        if (argc != 2) {
            wrong_num_args(argc, argv, 2, 2, 2);
            return TCL_ERROR;
        }

        KeyDB::flush_keys();

    } else {
        resultf("no such security subcommand %s", cmd);
        return TCL_ERROR;
    }

    return TCL_OK;
}

} // namespace dtn

#endif  /* BSP_ENABLED */
