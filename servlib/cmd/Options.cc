
#include <unistd.h>
#include "Options.h"
#include "debug/Debug.h"

Opt* Options::opts_[256];	// indexed by option character
Opt* Options::head_;		// list of all registered Opts

void
Options::addopt(Opt* opt)
{
    for (unsigned int i = 0; i < strlen(opt->opts_); ++i) {
        int c = opt->opts_[i];
        if (opts_[c]) {
            logf("/options", LOG_CRIT, "multiple addopt calls for char '%c'", c);
            NOTIMPLEMENTED;
        }
        
        opts_[c] = opt;
    }

    /*
     * tack onto the tail to keep the usage order consistent with the
     * registration order
     */
    Opt** p = &head_;
    while (*p)
        p = &((*p)->next_);
    *p = opt;
    ASSERT(opt->next_ == 0);
}

void
Options::getopt(const char* progname, int argc, char* const argv[])
{
    Opt* opt;
    char optstring[64];
    char* bp = &optstring[0];
    int c;

    *bp++ = '?';
    *bp++ = 'h';
    *bp++ = 'H';

    for (int i = 0; i < 256; ++i) {
        if (opts_[i] == 0)
            continue;
        *bp++ = i;
        if (opts_[i]->hasval_)
            *bp++ = ':';
    }

    *bp = '\0';

    logf("/getopt", LOG_DEBUG, "optstring: %s", optstring);

    while (1) {
        c = ::getopt(argc, argv, optstring);
        switch(c) {
        case 1:
            continue;
        case '?':
        case 'h':
        case 'H':
            usage(progname);
            exit(0);
        case -1:
            return;
        default:
            ASSERT(c >= 0 && c < 256);
            opt = opts_[c];

            if (opt) {
                opt->set(optarg);
                if (opt->setp_)
                    *opt->setp_ = true;
            } else {
                logf("/getopt", LOG_ERR,
                     "Unknown char '%c' returned from getopt", c);
            }
        }
    }
}

void
Options::usage(const char* progname)
{
    char opts[32];
    fprintf(stderr, "%s usage:\n", progname);

    sprintf(opts, "-[hH?]");
    fprintf(stderr, "    %-16s%s\n", opts, "show usage");

    for (Opt* opt = head_; opt != NULL; opt = opt->next_) {
        if (opt->opts_[1] == 0) {
            snprintf(opts, 32, "-%c %s", opt->opts_[0], opt->valdesc_);
        } else {
            snprintf(opts, 32, "-[%s] %s", opt->opts_, opt->valdesc_);
        }
        fprintf(stderr, "    %-16s%s\n", opts, opt->desc_);
    }
}

Opt::Opt(const char* opts, void* valp, bool* setp, bool hasval,
         const char* valdesc, const char* desc)
    : opts_(opts),
      valp_(valp),
      setp_(setp),
      hasval_(hasval),
      valdesc_(valdesc),
      desc_(desc),
      next_(0)
{
    Options::addopt(this);
    if (setp) *setp = false;
}

Opt::~Opt()
{
    logf("/getopt", LOG_ERR, "opt classes should never be destroyed");
    NOTREACHED;
}

BoolOpt::BoolOpt(const char* opts, bool* valp, const char* desc)
    : Opt(opts, valp, NULL, 0, "", desc)
{
}

BoolOpt::BoolOpt(const char* opts, bool* valp, bool* setp, const char* desc)
    : Opt(opts, valp, setp, 0, "", desc)
{
}

int
BoolOpt::set(char* val)
{
    *((bool*)valp_) = true;
    return 0;
}

IntOpt::IntOpt(const char* opts, int* valp,
               const char* valdesc, const char* desc)
    : Opt(opts, valp, NULL, 1, valdesc, desc)
{
}

IntOpt::IntOpt(const char* opts, int* valp, bool* setp,
               const char* valdesc, const char* desc)
    : Opt(opts, valp, setp, 1, valdesc, desc)
{
}

int
IntOpt::set(char* val)
{
    int newval;
    char* endptr = 0;

    newval = strtol(val, &endptr, 0);
    if (*endptr != '\0')
        return -1;

    *((int*)valp_) = newval;

    return 0;
}

StringOpt::StringOpt(const char* opts, std::string* valp,
                     const char* valdesc, const char* desc)
    : Opt(opts, valp, NULL, 1, valdesc, desc)
{
}

StringOpt::StringOpt(const char* opts, std::string* valp, bool* setp,
                     const char* valdesc, const char* desc)
    : Opt(opts, valp, setp, 1, valdesc, desc)
{
}

int
StringOpt::set(char* val)
{
    ((std::string*)valp_)->assign(val);
    return 0;
}
