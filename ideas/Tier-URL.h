#ifndef TIER_URL_H
#define TIER_URL_H

#include <list>
#include <string>
#include <stdarg.h>
#include <stdio.h>

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
     * Construct a new URL from a base and a link (which may be
     * absolute or relative).
     */
    URL(const URL& url, const std::string& link)
    {
        merge(url, link);
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
          err_(copy.err_),
          flags_(copy.flags_),
          depth_(copy.depth_)
    {
    }

    /**
     * Clear out this url.
     */
    void clear()
    {
        url_.clear();
        err_ = URLPARSE_UNPARSED;
        memset(&flags_, 0, sizeof(flags_));
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
     * Merge a base url with a link and store the result.
     */
    void merge(const URL& base, const std::string& link);

    /**
     * Cons up this url from constituent pieces.
     */
    void format(const std::string& proto, const std::string& host, u_int16_t port,
                const std::string& path, const std::string& rest = "");
    
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
     * Return the length of URL's path. Path is considered to be
     * terminated by one of '?', ';', '#', or by the end of the
     * std::string.
     */
    size_t path_length() const
    {
        size_t ret = url_.find_first_of("?;#");
        if (ret == std::string::npos)
            return url_.length();
        return ret;
    }

    /**
     * Get the actual path of the url, i.e. everything in url_ up to
     * the path_length.
     */
    void get_path(std::string* path) const
    {
        path->assign(url_, 0, path_length());
    }

    /**
     * URL escape any reserved or unsafe chars in this url, coping the
     * result to the provided std::string. If the url doesn't need
     * modifying, this should be a zero-cost operation. Also, it
     * should be stable.
     */
    void escape(std::string* escaped);

    /**
     * Unencode any %hexhex encoded characters in the object,
     * replacing the url object in-place. Note that this is _always_
     * called when the URL is constructed so all in-memory
     * representations are unescaped.
     */
    void unescape();

    /**
     * Simplify the path part of the url, i.e. get rid of empty path
     * elements and . or .. parts. Also called whenever the url is
     * constructed, after unescape().
     *
     * @returns indication if the path was modified
     */
    bool simplify_path(size_t path_pos);

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
    u_int16_t port_;	/* the port (0 if doesn't exists) */
    std::string path_;	/* the path part */
    std::string rest_;	/* params, query, fragment, etc. */
    
    urlerr_t err_;	/* parse status */

    struct flags {
        u_int link_relative_   :1; /* the link was relative */
        u_int link_complete_   :1; /* the link was complete (had host name) */
        u_int link_base_       :1; /* the url came from <base href=...> */
        u_int link_inline_     :1; /* needed to render the page */
        u_int link_expect_html_:1; /* expected to contain HTML */
        u_int link_refresh_    :1; /* link was received from
 				      <meta http-equiv=refresh content=...> */
    } flags_;

    int depth_;		/* used during crawl to mark the link depth */
    
private:
    urlerr_t parse_internal();
};

/**
 * Convenient typedef for a list of URLs. Note that the list contains
 * pointers to URL objects, not URL objects.
 */
typedef class std::list<URL*> URLList;

#endif /* TIER_URL_H */

