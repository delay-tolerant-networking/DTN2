#ifndef SECURITY_CONFIG_H
#define SECURITY_CONFIG_H

#include <sys/types.h>
#include <dirent.h>
#include <strings.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <iostream>
#include <sstream>

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif


#ifdef BSP_ENABLED

using namespace std;

class SecurityConfig {
  public:
    string privdir;
    string certdir;
    map<int, string> pub_keys_enc;
    map<int, string> pub_keys_ver;
    map<int, string> priv_keys_sig;
    map<int, string> priv_keys_dec;

    // In all of the following, we use the convention that 0=no ciphersuite.
    set<int> allowed_babs_in;
    set<int> allowed_pibs_in;
    set<int> allowed_pcbs_in;
    int pcb_out;
    int pib_out;
    int bab_out;
    int esb_out;

    SecurityConfig(): privdir("/usr/local/ssl/private/"), certdir("/usr/local/ssl/certs/") {
        pcb_out = 0;
        pib_out = 0;
        bab_out = 0;
        esb_out = 0;
        allowed_babs_in.insert(0);
        allowed_pibs_in.insert(0);
        allowed_pcbs_in.insert(0);

        // define which keys are used with each ciphersuite
        // PIB-RSA-SHA256: ciphersuite value: 0x02
        pub_keys_ver[2] = string("RSA_sig_cert_<NODE>.pem");
        priv_keys_sig[2] = string("RSA_sig_priv_key.pem");

        // PCB-RSA-AES128-PAYLOAD-PIB-PCB: ciphersuite value: 0x03
        pub_keys_enc[3] = string("RSA_enc_cert_<NODE>.pem");
        priv_keys_dec[3] = string("RSA_enc_priv_key.pem");

        // ESB-RSA-AES128-EXT: ciphersuite value: 0x04
        pub_keys_enc[4] = string("RSA_enc_cert_<NODE>.pem");
        priv_keys_dec[4] = string("RSA_enc_priv_key.pem");
    }

    vector<int> get_out_bps() {
       vector<int> res;
       if(pib_out != 0) {
           res.push_back(pib_out);
       }
       if(pcb_out != 0) {
           res.push_back(pcb_out);
       }
       if(esb_out != 0) {
           res.push_back(esb_out);
       }
       if(bab_out != 0) {
           res.push_back(bab_out);
       }
       return res;
    }

    set<int> *get_allowed_pibs() {
        return &allowed_pibs_in;
    }
    set<int> *get_allowed_babs() {
        return &allowed_babs_in;
    }
    set<int> *get_allowed_pcbs() {
        return &allowed_pcbs_in;
    }

    string list_policy() {
        stringstream foo(stringstream::out);
        set<int>::iterator it;
        foo << "Outgoing" << endl;
        foo << "--------" << endl;
        foo << "BAB: " << bab_out << endl;
        foo << "PIB: " << pib_out << endl;
        foo << "PCB: " << pcb_out << endl;
        foo << "ESB: " << esb_out << endl;
        foo << endl;
        foo << "Incoming" << endl;
        foo << "--------" << endl;
        foo << "BABs: ";
        for(it = allowed_babs_in.begin(); it != allowed_babs_in.end(); it++){
            foo << *it << " ";
        }
        foo << endl;
        foo << "PIBs: ";
        for(it = allowed_pibs_in.begin(); it != allowed_pibs_in.end(); it++){
            foo << *it << " ";
        }
        foo << endl;
        foo << "PCBs: ";
        for(it = allowed_pcbs_in.begin(); it != allowed_pcbs_in.end(); it++){
            foo << *it << " ";
        }
        foo << endl;
        return foo.str();
    }
    string list_maps() {
       stringstream foo(stringstream::out);
       map<int, string>::iterator it;

       foo << "certdir = " << certdir << endl;
       foo << "privdir = " << privdir << endl << endl;

       foo << "pub_keys_enc:" <<endl;
       for(it = pub_keys_enc.begin(); it != pub_keys_enc.end();it++) {
           foo << "(cs num " << (*it).first << ")" << " => " << (*it).second << endl;
       }
       foo << endl;

       foo << "priv_keys_dec:" <<endl;
       for(it = priv_keys_dec.begin(); it != priv_keys_dec.end();it++) {
           foo << "(cs num " << (*it).first << ")" << " => " << (*it).second << endl;
       }
       foo << endl;

       foo << "pub_keys_ver:" << endl;
       for(it = pub_keys_ver.begin(); it != pub_keys_ver.end();it++) {
           foo << "(cs num " << (*it).first << ")"  << " => " << (*it).second << endl;
       }
       foo << endl;

       foo << "priv_keys_sig:" << endl;
       for(it = priv_keys_sig.begin(); it != priv_keys_sig.end();it++) {
           foo << "(cs num " << (*it).first <<  ")" << " => " << (*it).second << endl;
       }
       foo << endl;

       return foo.str();
    }

    string replace_stuff(string input, const char *node) {
        string n = string("<NODE>");
        if(input.find(n) != string::npos) {
            input.replace(input.find(n),n.length(),node);
        }
        return input;
    }

    string get_pub_key_enc(const char *dest, int cs=3) {
            return certdir + replace_stuff(pub_keys_enc[cs], dest);
    }
    string get_pub_key_ver(const char *src, const int csnum) {
            return certdir + replace_stuff(pub_keys_ver[csnum], src);
    }
    string get_priv_key_sig(const char *src, const int csnum) {
            return privdir + replace_stuff(priv_keys_sig[csnum], src);
    }
    string get_priv_key_dec(const char *dest, const int csnum) {
            return privdir + replace_stuff(priv_keys_dec[csnum], dest);
    }

};
#endif
#endif
