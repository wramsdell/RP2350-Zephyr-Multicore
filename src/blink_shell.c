#include <stdlib.h>
#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/mbox.h>
#include <zephyr/shell/shell.h>

#include "../core1/mailbox_proto.h"
#include "blink_shell.h"

static const struct device *mbox_dev;
static struct k_sem reply_sem;
static volatile uint32_t last_reply;

static void mbox_cb(const struct device *dev, mbox_channel_id_t channel_id,
		     void *user_data, struct mbox_msg *msg)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(channel_id);
	ARG_UNUSED(user_data);

	last_reply = *(const uint32_t *)msg->data;
	k_sem_give(&reply_sem);
}

int blink_mbox_init(void)
{
	mbox_dev = DEVICE_DT_GET(DT_NODELABEL(mbox));
	if (!device_is_ready(mbox_dev)) {
		return -ENODEV;
	}

	k_sem_init(&reply_sem, 0, 1);

	int ret = mbox_register_callback(mbox_dev, 0, mbox_cb, NULL);

	if (ret) {
		return ret;
	}

	return mbox_set_enabled(mbox_dev, 0, true);
}

static int blink_roundtrip(uint32_t word, uint32_t *reply_out)
{
	struct mbox_msg msg = {
		.data = &word,
		.size = sizeof(word),
	};

	int ret = mbox_send(mbox_dev, 0, &msg);

	if (ret) {
		return ret;
	}

	if (k_sem_take(&reply_sem, K_MSEC(1000)) != 0) {
		return -ETIMEDOUT;
	}

	*reply_out = last_reply;
	return 0;
}

static int cmd_blink_rate_get(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	uint32_t reply;

	if (blink_roundtrip(MBOX_MSG_MAKE_GET(), &reply)) {
		shell_error(sh, "core1 did not respond");
		return -1;
	}

	shell_print(sh, "blink rate: %u ms", MBOX_MSG_RATE(reply));
	return 0;
}

static int cmd_blink_rate_set(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);

	uint32_t ms = strtoul(argv[1], NULL, 10);
	uint32_t reply;

	if (blink_roundtrip(MBOX_MSG_MAKE_SET(ms), &reply)) {
		shell_error(sh, "core1 did not respond");
		return -1;
	}

	shell_print(sh, "blink rate set to: %u ms", MBOX_MSG_RATE(reply));
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_blink_rate,
	SHELL_CMD_ARG(get, NULL, "Get the core1 LED blink rate (ms)", cmd_blink_rate_get, 1, 0),
	SHELL_CMD_ARG(set, NULL, "Set the core1 LED blink rate (ms)", cmd_blink_rate_set, 2, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_blink,
	SHELL_CMD(rate, &sub_blink_rate, "Blink rate control", NULL),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(blink, &sub_blink, "Core1 LED blink control", NULL);
