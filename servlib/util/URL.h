#ifndef _URL_H_
#define _URL_H_

#include <list>
#include <string>

typedef enum urlerr_t {
    URLPARSE_OK,	/* parsed ok */
    URLPARSE_UNPARSED,	/* not parsed yet */
    URLPARSE_NOURL,	/* no url in object */
    URLPARSE_BADSEP,	/* bad or missing separator char */
    URLPARSE_BADPROTO,	/* bad protocol */
    URLPARSE_BADPORT,	/* bad port */
    URLPARSE_NOHOST,	/* no host */
};


/**
 * A simple class for managing urls that supports parsing, merging,
 * copying, etc.
 */
class URL {
public:
    /**
     * Default constructor
     */
    URL()
    {
        clear();
    }

    /**
     * Construct the url from the given std::string.
     */
    URL(const std::string& url)
    {
        url_.assign(url.data(), url.length());
        parse();
    }

    /**
     * Construct the url from the given std::string.
     */
    URL(const char* url)
    {
        url_.assign(url);
        parse();
    }

    /**
     * Deep copy constructor.
     */
    URL(const URL& copy)
        : url_(copy.url_),
          proto_(copy.proto_),
          host_(copy.host_),
          port_(copy.port_),
          path_(copy.path_),
          err_(copy.err_)
    {
    }

    /**
     * Clear out this url.
     */
    void clear()
    {
        url_.erase();
        err_ = URLPARSE_UNPARSED;
    }

    /**
     * Parse the internal url_ into its constituent parts.
     */
    urlerr_t parse();

    /**
     * Parse the internal url_ into its constituent parts.
     */
    urlerr_t parse(const std::string& url)
    {
        url_.assign(url);
        return parse();
    }

    /**
     * Cons up this url from constituent pieces.
     */
    void format(const std::string& proto, const std::string& host, u_int16_t port,
                const std::string& path);
    
    /**
     * Return the result of the parse operation.
     */
    urlerr_t status() const
    {
        return err_;
    }

    /**
     * Return an indication of whether or not this url is valid.
     */
    bool valid() const
    {
        return (err_ == URLPARSE_OK);
    }

    /**
     * Wrappers around some basic std::string accessors to simplify
     * things.
     */
    ///@{
    const char* c_str()  const { return url_.c_str(); } 
    const char* data()   const { return url_.data(); }
    size_t      length() const { return url_.length(); }

    /*
     * The constituent fields are public to avoid the need for a bunch
     * of accessors. Assume that users of the class don't actually
     * muck with the objects or things may not work well.
     */
    std::string url_;	/* the url std::string */
    std::string proto_;	/* the protocol */
    std::string host_;	/* the hostname part */
    u_int16_t   port_;	/* the port (0 if doesn't exists) */
    std::string path_;	/* the path part */
    
    urlerr_t err_;	/* parse status */

private:
    urlerr_t parse_internal();
};

#endif /* _URL_H_ */

