#ifndef DTN_API_H
#define DTN_API_H

#include "dtn_types.h"

/**
 * The basic handle for communication with the dtn router.
 */
typedef void* dtn_handle_t;

/**
 * Default api port.
 */
#define DTN_API_PORT 6000

__BEGIN_DECLS

/**
 * Open a new connection to the router.
 * Returns the new handle on success, NULL on error.
 */
extern dtn_handle_t dtn_open(u_int16_t api_port);

/**
 * Close an open dtn handle.
 */
extern int dtn_close(dtn_handle_t handle);

/**
 * Create or modify a dtn registration.
 */
extern int dtn_register(dtn_handle_t handle,
                        dtn_reg_cookie_t* cookie,
                        dtn_reg_action_t action,
                        dtn_tuple_t endpoint);

/**
 * Send a bundle either from memory or from a file.
 */
extern int dtn_send_bundle(dtn_handle_t handle,
                           dtn_bundle_spec_t spec,
                           dtn_bundle_payload_t payload);

__END_DECLS

#endif /* DTN_API_H */
