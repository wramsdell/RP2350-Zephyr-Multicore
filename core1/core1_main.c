#include <stdint.h>

#include "regs.h"
#include "mailbox_proto.h"

#define LED_GPIO 25u

static void gpio25_init(void)
{
	/*
	 * RP2350-specific: pads reset with their isolation ("ISO") latch set,
	 * which leaves the pad electrically disconnected even after FUNCSEL
	 * and OE are configured correctly. This bit must be cleared or the
	 * LED silently never lights. Do not remove this without understanding
	 * why - it's the #1 RP2350 GPIO bring-up gotcha vs RP2040.
	 */
	REG_CLR(PADS_BANK0_GPIO(LED_GPIO), PADS_BANK0_GPIO0_ISO_BITS);

	/* FUNCSEL = SIO for this pin. */
	REG(IO_BANK0_GPIO_CTRL(LED_GPIO)) = 5u << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;

	/* Core1 owns this pin exclusively via its own per-core SIO block. */
	REG(SIO_GPIO_OE_SET) = (1u << LED_GPIO);
}

static void timer0_init(void)
{
	REG_CLR(RESETS_RESET, RESETS_RESET_TIMER0_BITS);
	while (!(REG(RESETS_RESET_DONE) & RESETS_RESET_TIMER0_BITS)) {
	}
}

static inline uint32_t now_us(void)
{
	return REG(TIMER0_TIMERAWL);
}

/*
 * Sleep for ms milliseconds, servicing the mailbox FIFO the whole time so a
 * blink-rate update is picked up promptly instead of only between blinks of
 * the *old*, possibly much longer, rate.
 */
static void delay_ms_polling_fifo(uint32_t ms, uint32_t *rate_ms)
{
	uint32_t start = now_us();

	while ((uint32_t)(now_us() - start) < ms * 1000u) {
		if (REG(SIO_FIFO_ST) & SIO_FIFO_ST_VLD_BITS) {
			uint32_t req = REG(SIO_FIFO_RD);

			if (MBOX_MSG_IS_SET(req)) {
				*rate_ms = MBOX_MSG_RATE(req);
			}

			while (!(REG(SIO_FIFO_ST) & SIO_FIFO_ST_RDY_BITS)) {
			}
			REG(SIO_FIFO_WR) = MBOX_MSG_RATE(*rate_ms);
			__asm volatile("sev");
		}
	}
}

void core1_main(void)
{
	uint32_t rate_ms = BLINK_DEFAULT_RATE_MS;

	gpio25_init();
	timer0_init();

	while (1) {
		REG(SIO_GPIO_OUT_XOR) = (1u << LED_GPIO);
		delay_ms_polling_fifo(rate_ms, &rate_ms);
	}
}
