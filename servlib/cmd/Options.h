#ifndef _OPTIONS_H_
#define _OPTIONS_H_

#include <string>

/*
 * Wrapper class for getopt calls.
 */
class Opt;
class Options {
public:
    static void addopt(Opt* opt);
    static void getopt(const char* progname, int argc, char* const argv[]);
    static void usage(const char* progname);
    
protected:
    static Opt* opts_[];	// indexed by option character
    static Opt* head_;		// list of all registered Opts
};

class Opt {
    friend class Options;
    
protected:
    Opt(const char* opts, void* valp, bool* setp, bool hasval,
        const char* valdesc, const char* desc);
    virtual ~Opt();
    
    virtual int set(char* val) = 0;
    
    const char* opts_;
    void* valp_;
    bool* setp_;
    bool hasval_;
    const char* valdesc_;
    const char* desc_;
    Opt*  next_;
};

class BoolOpt : public Opt {
public:
    BoolOpt(const char* opts, bool* valp, const char* desc);
    BoolOpt(const char* opts, bool* valp, bool* setp, const char* desc);

protected:
    int set(char* val);
};

class IntOpt : public Opt {
public:
    IntOpt(const char* opts, int* valp,
           const char* valdesc, const char* desc);
    IntOpt(const char* opts, int* valp, bool* setp,
           const char* valdesc, const char* desc);

protected:
    int set(char* val);
};

class StringOpt : public Opt {
public:
    StringOpt(const char* opts, std::string* valp,
              const char* valdesc, const char* desc);
    StringOpt(const char* opts, std::string* valp, bool* setp,
              const char* valdesc, const char* desc);

protected:
    int set(char* val);
};

#endif /* _OPTIONS_H_ */
