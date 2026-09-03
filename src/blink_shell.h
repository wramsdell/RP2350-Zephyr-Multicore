#ifndef BLINK_SHELL_H
#define BLINK_SHELL_H

/* Sets up the mbox device and callback used by the `blink rate` shell
 * commands to talk to core1. Returns 0 on success.
 */
int blink_mbox_init(void);

#endif /* BLINK_SHELL_H */
