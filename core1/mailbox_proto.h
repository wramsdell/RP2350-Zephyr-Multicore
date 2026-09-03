/*
 * Wire protocol for the core0 <-> core1 SIO FIFO mailbox. Shared verbatim by
 * both the Zephyr (core0) side and the bare-metal (core1) side.
 *
 * A single 32-bit word per direction. Core0 sends a request (GET or SET);
 * core1 always replies with the current rate (after applying a SET), so
 * core0 can decode every reply the same way regardless of what it asked.
 */
#ifndef CORE1_MAILBOX_PROTO_H
#define CORE1_MAILBOX_PROTO_H

#define MBOX_MSG_OP_MASK   0x80000000u
#define MBOX_MSG_OP_SET    0x80000000u
#define MBOX_MSG_RATE_MASK 0x7FFFFFFFu

#define MBOX_MSG_MAKE_GET()        (0u)
#define MBOX_MSG_MAKE_SET(rate_ms) (MBOX_MSG_OP_SET | ((rate_ms) & MBOX_MSG_RATE_MASK))
#define MBOX_MSG_IS_SET(word)      (((word) & MBOX_MSG_OP_MASK) != 0u)
#define MBOX_MSG_RATE(word)        ((word) & MBOX_MSG_RATE_MASK)

#define BLINK_DEFAULT_RATE_MS 500u

#endif /* CORE1_MAILBOX_PROTO_H */
