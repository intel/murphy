#ifndef __MURPHY_SIGNALLING_H__
#define __MURPHY_SIGNALLING_H__

#include <stdint.h>
#include <murphy/common.h>

typedef enum {
    MRP_TX_ERROR_UNDEFINED,
    MRP_TX_ERROR_NOT_ANSWERED,
    MRP_TX_ERROR_NACKED,
    MRP_TX_ERROR_MAX
} mrp_tx_error_t;

typedef void (*mrp_tx_success_cb) (uint32_t tx, void *data);
typedef void (*mrp_tx_error_cb) (uint32_t tx, mrp_tx_error_t err, void *data);

#if 0
/** Opens a new signal with given 'tx'. */

int mrp_tx_open_signal_with_id(uint32_t tx);
#endif

/** Opens a new signal. Returns the assigned signal 'tx'. */

uint32_t mrp_tx_open_signal();

/** Adds a policy domain to the signal identified by 'tx'. */

int mrp_tx_add_domain(uint32_t tx, const char *domain);

/** Adds a data row to the signal identified by 'tx'. */

int mrp_tx_add_data(uint32_t tx, const char *row);

/** Adds a success callback to the signal identified by 'tx'. The callback
    will be called if the signal is successfully ACKed by all enforcement
    points registered to listen for the domains. If the success or error
    callback isn't set, the enforcement points are not required to reply
    to the signal.*/

void mrp_tx_add_success_cb(uint32_t tx, mrp_tx_success_cb cb, void *data);

/** Adds an error callback to the signal identified by 'tx'. The callback
    will be called if the signal is NACKed or not answered to by one or more
    enforcement points registered to listen for the domains. If the success
    or error callback isn't set, the enforcement points are not required to
    reply to the signal.*/

void mrp_tx_add_error_cb(uint32_t tx, mrp_tx_error_cb cb, void *data);

/** Closes the signal identified by 'tx' and sends it onward to the enforcement
    points.*/

void mrp_tx_close_signal(uint32_t tx);

/** Cancels a signal identified by 'tx'.*/

void mrp_tx_cancel_signal(uint32_t tx);


#endif /* __MURPHY_SIGNALLING_H__ */
