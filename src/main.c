#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(multicore_demo, LOG_LEVEL_INF);

#include "core1_launch.h"
#include "blink_shell.h"

int main(void)
{
	LOG_INF("Core0 up; launching core1");

	if (core1_launch()) {
		LOG_ERR("Failed to launch core1");
	}

	if (blink_mbox_init()) {
		LOG_ERR("Failed to init core1 mailbox");
	}

	while (1) {
		k_sleep(K_MSEC(1000));
	}

	return 0;
}
