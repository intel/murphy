#ifndef __MURPHY_PULSE_H__
#define __MURPHY_PULSE_H__

/** Register the given murphy mainloop with the given pulse mainloop. */
int mrp_mainloop_register_with_pulse(mrp_mainloop_t *ml, pa_mainloop_api *pa);

/** Unrgister the given murphy mainloop from the given pulse mainloop. */
int mrp_mainloop_unregister_from_pulse(mrp_mainloop_t *ml, pa_mainloop_api *pa);

#endif /* __MURPHY_PULSE_H__ */
