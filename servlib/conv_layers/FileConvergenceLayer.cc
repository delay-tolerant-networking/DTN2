
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>

#include "FileConvergenceLayer.h"
#include "bundling/Bundle.h"
#include "bundling/BundleEvent.h"
#include "bundling/BundleList.h"
#include "bundling/BundleProtocol.h"
#include "routing/BundleRouter.h"
#include "io/IO.h"
#include "util/StringBuffer.h"
#include "util/URL.h"

/******************************************************************************
 *
 * FileConvergenceLayer
 *
 *****************************************************************************/
FileConvergenceLayer::FileConvergenceLayer()
    : ConvergenceLayer("/cl/file")
{
}

void
FileConvergenceLayer::init()
{
}

void
FileConvergenceLayer::fini()
{
}

bool
FileConvergenceLayer::validate(const std::string& admin)
{
    NOTIMPLEMENTED;
}

bool
FileConvergenceLayer::match(const std::string& demux, const std::string& admin)
{
    NOTIMPLEMENTED;
}

/**
 * Pull a filesystem directory out of the admin portion of a
 * tuple.
 */
bool
FileConvergenceLayer::extract_dir(const BundleTuple& tuple, std::string* dirp)
{
    URL url(tuple.admin());
    
    if (! url.valid()) {
        log_err("extract_dir: admin '%s' of tuple '%s' not a valid url",
                tuple.admin().c_str(), tuple.c_str());
        return false;
    }

    // the admin part of the URL should be of the form:
    // file:///path1/path2

    // validate that the "host" part of the url is empty, i.e. that
    // the filesystem path is absolute
    if (url.host_.length() != 0) {
        log_err("interface tuple '%s' specifies a non-absolute path",
                tuple.admin().c_str());
        return false;
    }

    // and make sure there wasn't a port that was parsed out
    if (url.port_ != 0) {
        log_err("interface tuple '%s' specifies a port",
                tuple.admin().c_str());
        return false;
    }

    dirp->assign("/");
    dirp->append(url.path_);
    return true;
}

/**
 * Validate that a given directory exists and that the permissions
 * are correct.
 */
bool
FileConvergenceLayer::validate_dir(const std::string& dir)
{
    struct stat st;
    if (stat(dir.c_str(), &st) != 0) {
        log_err("error running stat on %s: %s", dir.c_str(), strerror(errno));
        return false;
    }

    if (!S_ISDIR(st.st_mode)) {
        log_err("error: %s not a directory", dir.c_str());
        return false;
    }

    // XXX/demmer check permissions

    return true;
}

/**
 * Register a new interface.
 */
bool
FileConvergenceLayer::add_interface(Interface* iface, int argc, const char* argv[])
{
    // parse out the directory from the interface
    std::string dir;
    if (!extract_dir(iface->tuple(), &dir)) {
        return false;
    }
    
    // make sure the directory exists and is readable / executable
    if (!validate_dir(dir)) {
        return false;
    }
    
    // XXX/demmer parse argv for frequency
    int secs_per_scan = 5;

    // create a new thread to scan for new bundle files
    Scanner* scanner = new Scanner(secs_per_scan, dir);
    scanner->start();

    // store the new scanner in the cl specific part of the interface
    iface->set_info(scanner);

    return true;
}

/**
 * Remove an interface
 */
bool
FileConvergenceLayer::del_interface(Interface* iface)
{
    NOTIMPLEMENTED;
}

 
/**
 * Validate that the contact tuple specifies a legit directory.
 */
bool
FileConvergenceLayer::open_contact(Contact* contact)
{
    // parse out the directory from the contact
    std::string dir;
    if (!extract_dir(contact->tuple(), &dir)) {
        return false;
    }
    
    // make sure the directory exists and is readable / executable
    if (!validate_dir(dir)) {
        return false;
    }

    return true;
}

/**
 * Close the connnection to the contact.
 */
bool
FileConvergenceLayer::close_contact(Contact* contact)
{
    // nothing to do
    return true;
}
    
/**
 * Try to send the bundles queued up for the given contact.
 */
void
FileConvergenceLayer::send_bundles(Contact* contact)
{
    std::string dir;
    if (!extract_dir(contact->tuple(), &dir)) {
        PANIC("contact should have already been validated");
    }

    FileHeader filehdr;
    Bundle* bundle;
    BundleList* blist = contact->bundle_list();
    int iovcnt = BundleProtocol::MAX_IOVCNT + 2;
    struct iovec iov[iovcnt];

    filehdr.version = CURRENT_VERSION;
    
    StringBuffer fname("%s/bundle-XXXXXX", dir.c_str());
    
    while ((bundle = blist->pop_front()) != NULL) {
        iov[0].iov_base = &filehdr;
        iov[0].iov_len  = sizeof(FileHeader);

        // fill in the bundle header portion
        u_int16_t header_len = BundleProtocol::fill_header_iov(bundle, &iov[1], &iovcnt);

        // fill in the file header
        size_t payload_len = bundle->payload_.length();
        filehdr.header_length = htons(header_len);
        filehdr.bundle_length = htonl(header_len + payload_len);

        // and tack on the payload (adding one to iovcnt for the
        // FileHeader, then one for the payload)
        iovcnt++;
        iov[iovcnt].iov_base = (void*)bundle->payload_.data();
        iov[iovcnt].iov_len  = payload_len;
        iovcnt++;

        // open the bundle file 
        int fd = mkstemp(fname.c_str());
        if (fd == -1) {
            log_err("error opening temp file in %s: %s", fname.c_str(), strerror(errno));
            // XXX/demmer report error here?
            bundle->del_ref();
            continue;
        }

        log_debug("opened temp file %s for bundle id %d "
                  "fd %d header_length %d payload_length %d",
                  fname.c_str(), bundle->bundleid_, fd, header_len, payload_len);

        // now write everything out
        int total = sizeof(FileHeader) + header_len + payload_len;
        int cc = IO::writevall(fd, iov, iovcnt, logpath_);
        if (cc != total) {
            log_err("error writing out bundle (wrote %d/%d): %s",
                    cc, total, strerror(errno));
        }

        // free up the iovec data
        BundleProtocol::free_header_iovmem(bundle, &iov[1], iovcnt - 2);
        
        // close the file descriptor
        close(fd);

        // cons up a transmission event and pass it to the router
        bool acked = false;
        BundleRouter::dispatch(
            new BundleTransmittedEvent(bundle, contact, payload_len, acked));

        log_debug("bundle id %d successfully transmitted", bundle->bundleid_);

        // finally, remove the reference on the bundle (which may delete it)
        bundle->del_ref();
    }
}

/******************************************************************************
 *
 * FileConvergenceLayer::Scanner
 *
 *****************************************************************************/
FileConvergenceLayer::Scanner::Scanner(int secs_per_scan, const std::string& dir)
    : Logger("/cl/file/scanner"),
      secs_per_scan_(secs_per_scan), dir_(dir)
{
}

/**
 * Main thread function.
 */
void
FileConvergenceLayer::Scanner::run()
{
    FileHeader filehdr;
    DIR* dir = opendir(dir_.c_str());
    struct dirent* dirent;
    const char* fname;
    char* buf;
    int fd;
    
    if (!dir) {
        // XXX/demmer signal cl somehow?
        log_err("error in opendir");
        return;
    }
    
    while (1) {
        seekdir(dir, 0);

        while ((dirent = readdir(dir)) != 0) {
            fname = dirent->d_name;

            if ((fname[0] == '.') &&
                ((fname[1] == '\0') ||
                 (fname[1] == '.' && fname[2] == '\0')))
            {
                continue;
            }
            
            log_debug("scan found file %s", fname);

            // cons up the full path
            StringBuffer path("%s/%s", dir_.c_str(), fname);

            // malloc a buffer for it, open a file descriptor, and
            // read in the header
            if ((fd = open(path.c_str(), 0)) == -1) {
                log_err("error opening file %s: %s", path.c_str(), strerror(errno));
                continue;
            }

            int cc = IO::readall(fd, (char*)&filehdr, sizeof(FileHeader));
            if (cc != sizeof(FileHeader)) {
                log_warn("can't read in FileHeader (read %d/%d): %s",
                         cc, sizeof(FileHeader), strerror(errno));
                continue;
            }

            if (filehdr.version != CURRENT_VERSION) {
                log_warn("framing protocol version mismatch: %d != current %d",
                         filehdr.version, CURRENT_VERSION);
                continue;
            }

            u_int16_t header_len = ntohs(filehdr.header_length);
            size_t bundle_len = ntohl(filehdr.bundle_length);
            
            log_debug("found bundle file %s: header_length %d bundle_length %d",
                      path.c_str(), header_len, bundle_len);

            // read in and parse the headers
            buf = (char*)malloc(header_len);
            cc = IO::readall(fd, buf, header_len);
            if (cc != header_len) {
                log_err("error reading file %s header (read %d/%d): %s",
                        path.c_str(), cc, header_len, strerror(errno));
                free(buf);
                continue;
            }

            Bundle* bundle = new Bundle();
            if (! BundleProtocol::parse_headers(bundle, (u_char*)buf, header_len)) {
                log_err("error parsing bundle headers in file %s", path.c_str());
                free(buf);
                delete bundle;
                continue;
            }
            free(buf);

            // Now validate the lengths
            size_t payload_len = bundle->payload_.length();
            if (bundle_len != header_len + payload_len) {
                log_err("error in bundle lengths in file %s: "
                        "bundle_length %d, header_length %d, payload_length %d",
                        path.c_str(), bundle_len, header_len, payload_len);
                delete bundle;
                continue;
            }

            // Looks good, now read in and assign the data
            buf = (char*)malloc(payload_len);
            cc = IO::readall(fd, buf, payload_len);
            if (cc != (int)payload_len) {
                log_err("error reading file %s payload (read %d/%d): %s",
                        path.c_str(), cc, payload_len, strerror(errno));
                delete bundle;
                continue;
            }
            bundle->payload_.set_data(buf, payload_len);
            free(buf);
            
            // close the file descriptor and remove the file
            if (close(fd) != 0) {
                log_err("error closing file %s: %s", path.c_str(), strerror(errno));
            }
            
            if (unlink(path.c_str()) != 0) {
                log_err("error removing file %s: %s", path.c_str(), strerror(errno));
            }

            // all set, notify the router
            BundleRouter::dispatch(new BundleReceivedEvent(bundle));
            ASSERT(bundle->refcount() > 0);
        }
            
        //log_debug("sleeping...");
        sleep(secs_per_scan_);
    }
}

