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
#include "security/Ciphersuite_PI2.h"
#include "security/Ciphersuite_PC3.h"

namespace dtn {

SecurityCommand::SecurityCommand()
    : TclCommand("security")
{
    add_to_help("outgoing <bab|pib|pcb|esb> <num>",
    		    "Set the ciphersuites to use for each security block type.  Use 0 for none.");
    add_to_help("outgoing reset",
    		    "Turn off all outgoing BSP");
    add_to_help("incoming <bab|pib|pcb> [allowed value 1] [allowed value 2] [etc]",
    		    "Set the allowable incoming ciphersuites.  Use 0 to allow bundles without any BSP.");
    add_to_help("incoming reset",
    		    "Don't require any incoming BSP");
    add_to_help("listpolicy",
    		    "Display infomation on the incoming and outgoing ciphersuites");
    add_to_help("listpaths",
                "List the paths for the RSA private keys and public key certificates");
    add_to_help("certdir <directory>",
                "Set the directory where RSA certificates are stored");
    add_to_help("privdir <directory>",
                "Set the directory where RSA private keys are stored");
    add_to_help("keypaths [ pub_keys_enc | priv_keys_dec | priv_keys_sig | pub_keys_ver ] <csnum> <filename> ",
                "Set the filenames for RSA private keys and public key certificates. <NODE> may be used\n"
    		    "in the filename and will be replaced with the EID.");
    add_to_help("setkey <host> <cs_num> <key>",
                "Set the symmetric key to use for the specified host and ciphersuite\n"
                "number.  <host> may also be the wildcard symbol \"*\".");
    add_to_help("dumpkeys",
                "Dump the symmetric keys to the screen.");
    add_to_help("flushkeys",
                "Erase all the symmetric keys.");
}

bool SecurityCommand::is_a_bab(int num) {
            if(num != 1 && num != 0) {
                resultf("%d must be one of 0, 1", num);
                return false;
            }
            return true;
}
bool SecurityCommand::is_a_pcb(int num) {
            if(num != 3 && num != 0) {
                resultf("%d must be one of 0, 3", num);
                return false;
            }
            return true;
}
bool SecurityCommand::is_a_pib(int num) {
            if(num != 2 && num != 0) {
                resultf("%d must be one of 0, 2", num);
                return false;
            }
            return true;
}
bool SecurityCommand::is_a_esb(int num) {
            if(num != 4 && num != 0) {
                resultf("%d must be one of 0, 4", num);
                return false;
            }
            return true;
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

   if(strcmp(cmd, "outgoing") == 0) {
        if(argc == 3) {
        	// this must be the command "security outgoing reset"
        	if(strcmp(argv[2], "reset") != 0) {
                wrong_num_args(argc, argv, 2, 4, 4);
                return TCL_ERROR;
        	} else { // turn off all outgoing security
        		Ciphersuite::config->bab_out = 0;
        		Ciphersuite::config->pib_out = 0;
        		Ciphersuite::config->pcb_out = 0;
        		Ciphersuite::config->esb_out = 0;
                return TCL_OK;
        	}
        } else if(argc != 4) {
            wrong_num_args(argc, argv, 2, 4, 4);
            return TCL_ERROR;
        }
        int num = atoi(argv[3]);
        if(strcmp(argv[2], "bab") == 0) {
            if(!is_a_bab(num)) return TCL_ERROR;
            Ciphersuite::config->bab_out = num;
        } else if(strcmp(argv[2], "pib") == 0) {
            if(!is_a_pib(num)) return TCL_ERROR;
            Ciphersuite::config->pib_out = num;
        } else if(strcmp(argv[2], "pcb") == 0) {
            if(!is_a_pcb(num)) return TCL_ERROR;
            Ciphersuite::config->pcb_out = num;
        } else if(strcmp(argv[2], "esb") == 0) {
            if(!is_a_esb(num)) return TCL_ERROR;
            Ciphersuite::config->esb_out = num;
        } else {
            resultf("%s must be one of bab, pib, pcb, esb, or reset", argv[2]);
            return TCL_ERROR;
        }
    } else if(strcmp(cmd, "incoming") == 0) {
    	if(argc == 3) {
    		// this must be the command "security incoming reset"
    		if(strcmp(argv[2], "reset") != 0) {
    			wrong_num_args(argc, argv, 2, 3, 0);
    			return TCL_ERROR;
    		} else { // turn off all incoming security
                Ciphersuite::config->allowed_babs_in.clear();
                Ciphersuite::config->allowed_babs_in.insert(0);
                Ciphersuite::config->allowed_pcbs_in.clear();
                Ciphersuite::config->allowed_pcbs_in.insert(0);
                Ciphersuite::config->allowed_pibs_in.clear();
                Ciphersuite::config->allowed_pibs_in.insert(0);
                return TCL_OK;
    		}
    	} else if(argc < 4) {
            wrong_num_args(argc, argv, 2, 3, 0);
            return TCL_ERROR;
        }
        set<int> *numbers;
        if(strcmp(argv[2], "bab") == 0) {
            numbers = &Ciphersuite::config->allowed_babs_in;
            for(int i=3;i<argc;i++) {
                if(!is_a_bab(atoi(argv[i]))) return TCL_ERROR;
            }
        } else if(strcmp(argv[2], "pib") == 0){
            numbers = &Ciphersuite::config->allowed_pibs_in;
            for(int i=3;i<argc;i++) {
                if(!is_a_pib(atoi(argv[i]))) return TCL_ERROR;
            }
        } else if(strcmp(argv[2], "pcb") == 0){
            for(int i=3;i<argc;i++) {
                if(!is_a_pcb(atoi(argv[i]))) return TCL_ERROR;
            }
            numbers = &Ciphersuite::config->allowed_pcbs_in;
        } else {
            resultf("%s must be one of bab, pib, pcb, reset", argv[2]);
            return TCL_ERROR;
        }
        numbers->clear();
        for(int i=3;i<argc;i++) {
            numbers->insert(atoi(argv[i]));
        }
    } else if(strcmp(cmd, "listpolicy") == 0) {
        set_result(Ciphersuite::config->list_policy().c_str());

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

        delete[] key;
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
    } else if (strcmp(cmd, "certdir") == 0) {
        if (argc != 3) {
            wrong_num_args(argc, argv, 1, 3, 3);
            return TCL_ERROR;
        }
        const char *loc = argv[2];
        Ciphersuite::config->certdir = string(loc);
    } else if (strcmp(cmd, "privdir") == 0) {
        if (argc != 3) {
            wrong_num_args(argc, argv, 1, 3, 3);
            return TCL_ERROR;
        }
        const char *loc = argv[2];
        Ciphersuite::config->privdir = string(loc);
    } else if(strcmp(cmd, "keypaths") == 0) {
        if(argc != 5) {
            wrong_num_args(argc, argv, 2, 5, 5);
            return TCL_ERROR;
        }

        const char *map = argv[2];
        int csnum = atoi(argv[3]);
        const char *filename = argv[4];
        /*
     	 map<int, string> pub_keys_enc;
    	 map<int, string> pub_keys_ver;
    	 map<int, string> priv_keys_sig;
    	 map<int, string> priv_keys_dec;
         */
        if(strcmp(map, "priv_keys_dec")==0) {
        	Ciphersuite::config->priv_keys_dec[csnum] = string(filename);
        } else if(strcmp(map, "pub_keys_ver")==0) {
        	Ciphersuite::config->pub_keys_ver[csnum] = string(filename);
        } else if(strcmp(map, "pub_keys_enc")==0) {
        	Ciphersuite::config->pub_keys_enc[csnum] = string(filename);
        } else if(strcmp(map, "priv_keys_sig") == 0) {
        	Ciphersuite::config->priv_keys_sig[csnum] = string(filename);
        } else{
        	resultf("no such path exists");
        }
    } else if(strcmp(cmd, "listpaths") == 0) {
        set_result(Ciphersuite::config->list_maps().c_str());
    } else {
        resultf("no such security subcommand %s", cmd);
        return TCL_ERROR;
    }

    return TCL_OK;
}

} // namespace dtn

#endif  /* BSP_ENABLED */
