/*
 * Minimal, self-contained RP2350 register access for the bare-metal core1
 * image.
 *
 * Deliberately does NOT include the hardware/structs headers or
 * hardware/address_mapped.h from the vendored Pico SDK: those pull in
 * pico.h, which in this Zephyr checkout drags in Zephyr-only shim headers
 * (zephyr/modules/hal_rpi_pico/pico/config_autogen.h -> zephyr/toolchain.h
 * etc.) that don't exist in a freestanding, non-Zephyr build. The plain
 * hardware/regs headers are pure, dependency-free macro files (no #include
 * beyond each other), so this uses those directly.
 *
 * Field offsets/bit values below are cross-checked against the real
 * generated headers at:
 *   modules/hal/rpi_pico/src/rp2350/hardware_regs/include/hardware/regs/
 */
#ifndef CORE1_REGS_H
#define CORE1_REGS_H

#include <stdint.h>

#include <hardware/regs/addressmap.h>
#include <hardware/regs/sio.h>
#include <hardware/regs/psm.h>
#include <hardware/regs/pads_bank0.h>
#include <hardware/regs/io_bank0.h>
#include <hardware/regs/resets.h>
#include <hardware/regs/timer.h>

#define REG(addr) (*(volatile uint32_t *)(addr))

/* REG_ALIAS_{XOR,SET,CLR}_BITS (atomic register access aliases) come from
 * hardware/regs/addressmap.h, included above. */
#define REG_SET(addr, mask) (REG((addr) + REG_ALIAS_SET_BITS) = (mask))
#define REG_CLR(addr, mask) (REG((addr) + REG_ALIAS_CLR_BITS) = (mask))

#define SIO_GPIO_OUT     (SIO_BASE + SIO_GPIO_OUT_OFFSET)
#define SIO_GPIO_OUT_XOR (SIO_BASE + SIO_GPIO_OUT_XOR_OFFSET)
#define SIO_GPIO_OE_SET  (SIO_BASE + SIO_GPIO_OE_SET_OFFSET)
#define SIO_FIFO_ST      (SIO_BASE + SIO_FIFO_ST_OFFSET)
#define SIO_FIFO_WR      (SIO_BASE + SIO_FIFO_WR_OFFSET)
#define SIO_FIFO_RD      (SIO_BASE + SIO_FIFO_RD_OFFSET)

#define PSM_FRCE_OFF (PSM_BASE + PSM_FRCE_OFF_OFFSET)

#define PADS_BANK0_GPIO(n) (PADS_BANK0_BASE + PADS_BANK0_GPIO0_OFFSET + 4u * (n))
#define IO_BANK0_GPIO_CTRL(n) (IO_BANK0_BASE + IO_BANK0_GPIO0_CTRL_OFFSET + 8u * (n))

#define RESETS_RESET      (RESETS_BASE + RESETS_RESET_OFFSET)
#define RESETS_RESET_DONE (RESETS_BASE + RESETS_RESET_DONE_OFFSET)

#define TIMER0_TIMERAWL (TIMER0_BASE + TIMER_TIMERAWL_OFFSET)

#endif /* CORE1_REGS_H */
