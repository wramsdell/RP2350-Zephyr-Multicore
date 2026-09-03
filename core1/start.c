#include <stdint.h>

extern uint32_t __bss_start__;
extern uint32_t __bss_end__;

void core1_main(void);

void core1_reset_handler(void)
{
	for (uint32_t *p = &__bss_start__; p < &__bss_end__; p++) {
		*p = 0;
	}

	core1_main();

	while (1) {
	}
}
