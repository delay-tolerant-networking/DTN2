
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

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

    Bundle* bundle;
    BundleList* blist = contact->bundle_list();
    int iovcnt = BundleProtocol::MAX_IOVCNT;
    struct iovec iov[iovcnt];
    
    while (1) {
        bundle = blist->pop_front();

        if (!bundle) {
            return;
        }

        int len = BundleProtocol::fill_iov(bundle, iov, &iovcnt);
        log_debug("bundle id %d format completed, %d byte bundle",
                  bundle->bundleid_, len);


        StringBuffer fname("%s/bundle-%d", dir.c_str(), bundle->bundleid_);

        int fd = open(fname.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
        if (fd == -1) {
            log_err("error opening file %s: %s", fname.c_str(), strerror(errno));
            // XXX/demmer report error here?
            bundle->del_ref();
            continue;
        }

        log_debug("opened bundle file %s (fd %d)", fname.c_str(), fd);

        int cc = IO::writevall(fd, iov, iovcnt, logpath_);
        if (cc != len) {
            log_err("error writing out bundle (%d/%d): %s", cc, len, strerror(errno));
        }

        // free up the iovec data
        BundleProtocol::free_iovmem(bundle, iov, iovcnt);

        // close the file descriptor
        close(fd);

        // cons up a transmission event and pass it to the router
        bool acked = false;
        size_t bytes_sent = bundle->payload_.length();
        BundleRouter::dispatch(
            new BundleTransmittedEvent(bundle, contact, bytes_sent, acked));

        log_debug("bundle id %d successfully transmitted", bundle->bundleid_);

        // finally, remove the reference on the bundle (may delete it)
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
    DIR* dir = opendir(dir_.c_str());
    struct dirent* dirent;
    const char* fname;
    struct stat st;
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

            // call stat to see how big it is
            if (stat(path.c_str(), &st) != 0) {
                log_err("error running stat %s: %s", path.c_str(), strerror(errno));
                continue;
            }

            // malloc a buffer for it, open a file descriptor, and
            // read in the contents
            if ((fd = open(path.c_str(), 0)) == -1) {
                log_err("error opening file %s: %s", path.c_str(), strerror(errno));
                continue;
            }

            buf = (char*)malloc(st.st_size);
            if (IO::readall(fd, buf, st.st_size) != st.st_size) {
                log_err("error reading file %s data: %s", path.c_str(), strerror(errno));
                delete buf;
                continue;
            }

            // parse and process the received data
            if (! BundleProtocol::process_buf((u_char*)buf, st.st_size)) {
                log_err("error parsing bundle");
            }

            // close the file descriptor and remove the file
            if (close(fd) != 0) {
                log_err("error closing file: %s", strerror(errno));
            }
            
            if (unlink(path.c_str()) != 0) {
                log_err("error removing bundle file: %s", strerror(errno));
            }
            
            // clean up the buffer and do another
            free(buf);
        }
            
        //log_debug("sleeping...");
        sleep(secs_per_scan_);
    }
}

