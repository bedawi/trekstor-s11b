// SPDX-License-Identifier: GPL-2.0-only
/*
 *  Driver for Goodix Touchscreens
 *
 *  Copyright (c) 2014 Red Hat Inc.
 *  Copyright (c) 2015 K. Merker <merker@debian.org>
 *
 *  This code is based on gt9xx.c authored by andrew@goodix.com:
 *
 *  2010 - 2012 Goodix Technology.
 */


#include <linux/kernel.h>
#include <linux/dmi.h>
#include <linux/firmware.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <asm/unaligned.h>

#define GOODIX_GPIO_INT_NAME		"irq"
#define GOODIX_GPIO_RST_NAME		"reset"

#define GOODIX_MAX_HEIGHT		4096
#define GOODIX_MAX_WIDTH		4096
#define GOODIX_INT_TRIGGER		1
#define GOODIX_CONTACT_SIZE		8
#define GOODIX_MAX_CONTACT_SIZE		9
#define GOODIX_MAX_CONTACTS		10

#define GOODIX_CONFIG_MAX_LENGTH	240
#define GOODIX_CONFIG_911_LENGTH	186
#define GOODIX_CONFIG_967_LENGTH	228

/* Register defines */
#define GOODIX_REG_COMMAND		0x8040
#define GOODIX_CMD_SCREEN_OFF		0x05

#define GOODIX_READ_COOR_ADDR		0x814E
#define GOODIX_GT1X_REG_CONFIG_DATA	0x8050
#define GOODIX_GT9X_REG_CONFIG_DATA	0x8047
#define GOODIX_REG_ID			0x8140

#define GOODIX_BUFFER_STATUS_READY	BIT(7)
#define GOODIX_BUFFER_STATUS_TIMEOUT	20

#define RESOLUTION_LOC		1
#define MAX_CONTACTS_LOC	5
#define TRIGGER_LOC		6

struct goodix_ts_data;

enum goodix_irq_pin_access_method {
	irq_pin_access_none,
	irq_pin_access_gpio,
	irq_pin_access_acpi_gpio,
	irq_pin_access_acpi_method,
};

struct goodix_chip_data {
	u16 config_addr;
	int config_len;
	int (*check_config)(struct goodix_ts_data *ts, const u8 *cfg, int len);
	void (*fix_config)(struct goodix_ts_data *ts);
};

struct goodix_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	const struct goodix_chip_data *chip;
	struct touchscreen_properties prop;
	unsigned int max_touch_num;
	unsigned int int_trigger_type;
	struct regulator *avdd28;
	struct regulator *vddio;
	struct gpio_desc *gpiod_int;
	struct gpio_desc *gpiod_rst;
	int gpio_count;
	int gpio_int_idx;
	u16 id;
	u16 version;
	const char *cfg_name;
	bool reset_controller_at_probe;
	bool load_cfg_from_disk;
	struct completion firmware_loading_complete;
	unsigned long irq_flags;
	enum goodix_irq_pin_access_method irq_pin_access_method;
	unsigned int contact_size;
	u8 config[GOODIX_CONFIG_MAX_LENGTH];
};

static int goodix_check_cfg_8(struct goodix_ts_data *ts,
			      const u8 *cfg, int len);
static int goodix_check_cfg_16(struct goodix_ts_data *ts,
			       const u8 *cfg, int len);
static void goodix_fix_cfg_8(struct goodix_ts_data *ts);
static void goodix_fix_cfg_16(struct goodix_ts_data *ts);

static const struct goodix_chip_data gt1x_chip_data = {
	.config_addr		= GOODIX_GT1X_REG_CONFIG_DATA,
	.config_len		= GOODIX_CONFIG_MAX_LENGTH,
	.check_config		= goodix_check_cfg_16,
	.fix_config		= goodix_fix_cfg_16,
};

static const struct goodix_chip_data gt911_chip_data = {
	.config_addr		= GOODIX_GT9X_REG_CONFIG_DATA,
	.config_len		= GOODIX_CONFIG_911_LENGTH,
	.check_config		= goodix_check_cfg_8,
	.fix_config		= goodix_fix_cfg_8,
};

static const struct goodix_chip_data gt967_chip_data = {
	.config_addr		= GOODIX_GT9X_REG_CONFIG_DATA,
	.config_len		= GOODIX_CONFIG_967_LENGTH,
	.check_config		= goodix_check_cfg_8,
	.fix_config		= goodix_fix_cfg_8,
};

static const struct goodix_chip_data gt9x_chip_data = {
	.config_addr		= GOODIX_GT9X_REG_CONFIG_DATA,
	.config_len		= GOODIX_CONFIG_MAX_LENGTH,
	.check_config		= goodix_check_cfg_8,
	.fix_config		= goodix_fix_cfg_8,
};

static const unsigned long goodix_irq_flags[] = {
	IRQ_TYPE_EDGE_RISING,
	IRQ_TYPE_EDGE_FALLING,
	IRQ_TYPE_LEVEL_LOW,
	IRQ_TYPE_LEVEL_HIGH,
};

/*
 * Those tablets have their coordinates origin at the bottom right
 * of the tablet, as if rotated 180 degrees
 */
static const struct dmi_system_id rotated_screen[] = {
#if defined(CONFIG_DMI) && defined(CONFIG_X86)
	{
		.ident = "Teclast X89",
		.matches = {
			/* tPAD is too generic, also match on bios date */
			DMI_MATCH(DMI_BOARD_VENDOR, "TECLAST"),
			DMI_MATCH(DMI_BOARD_NAME, "tPAD"),
			DMI_MATCH(DMI_BIOS_DATE, "12/19/2014"),
		},
	},
	{
		.ident = "WinBook TW100",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "WinBook"),
			DMI_MATCH(DMI_PRODUCT_NAME, "TW100")
		}
	},
	{
		.ident = "WinBook TW700",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "WinBook"),
			DMI_MATCH(DMI_PRODUCT_NAME, "TW700")
		},
	},
#endif
	{}
};

static const struct dmi_system_id nine_bytes_report[] = {
#if defined(CONFIG_DMI) && defined(CONFIG_X86)
	{
		.ident = "Lenovo YogaBook",
		/* YB1-X91L/F and YB1-X90L/F */
		.matches = {
			DMI_MATCH(DMI_PRODUCT_NAME, "Lenovo YB1-X9")
		}
	},
#endif
	{}
};

/**
 * goodix_i2c_read - read data from a register of the i2c slave device.
 *
 * @client: i2c device.
 * @reg: the register to read from.
 * @buf: raw write data buffer.
 * @len: length of the buffer to write
 */
static int goodix_i2c_read(struct i2c_client *client,
			   u16 reg, u8 *buf, int len)
{
	struct i2c_msg msgs[2];
	__be16 wbuf = cpu_to_be16(reg);
	int ret;

	msgs[0].flags = 0;
	msgs[0].addr  = client->addr;
	msgs[0].len   = 2;
	msgs[0].buf   = (u8 *)&wbuf;

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = client->addr;
	msgs[1].len   = len;
	msgs[1].buf   = buf;

	ret = i2c_transfer(client->adapter, msgs, 2);
	return ret < 0 ? ret : (ret != ARRAY_SIZE(msgs) ? -EIO : 0);
}

/**
 * goodix_i2c_write - write data to a register of the i2c slave device.
 *
 * @client: i2c device.
 * @reg: the register to write to.
 * @buf: raw data buffer to write.
 * @len: length of the buffer to write
 */
static int goodix_i2c_write(struct i2c_client *client, u16 reg, const u8 *buf,
			    unsigned len)
{
	u8 *addr_buf;
	struct i2c_msg msg;
	int ret;

	addr_buf = kmalloc(len + 2, GFP_KERNEL);
	if (!addr_buf)
		return -ENOMEM;

	addr_buf[0] = reg >> 8;
	addr_buf[1] = reg & 0xFF;
	memcpy(&addr_buf[2], buf, len);

	msg.flags = 0;
	msg.addr = client->addr;
	msg.buf = addr_buf;
	msg.len = len + 2;

	ret = i2c_transfer(client->adapter, &msg, 1);
	kfree(addr_buf);
	return ret < 0 ? ret : (ret != 1 ? -EIO : 0);
}

static int goodix_i2c_write_u8(struct i2c_client *client, u16 reg, u8 value)
{
	return goodix_i2c_write(client, reg, &value, sizeof(value));
}

static const struct goodix_chip_data *goodix_get_chip_data(u16 id)
{
	switch (id) {
	case 1151:
	case 5663:
	case 5688:
		return &gt1x_chip_data;

	case 911:
	case 9271:
	case 9110:
	case 927:
	case 928:
		return &gt911_chip_data;

	case 912:
	case 967:
		return &gt967_chip_data;

	default:
		return &gt9x_chip_data;
	}
}

static int goodix_ts_read_input_report(struct goodix_ts_data *ts, u8 *data)
{
	unsigned long max_timeout;
	int touch_num;
	int error;

	/*
	 * The 'buffer status' bit, which indicates that the data is valid, is
	 * not set as soon as the interrupt is raised, but slightly after.
	 * This takes around 10 ms to happen, so we poll for 20 ms.
	 */
	max_timeout = jiffies + msecs_to_jiffies(GOODIX_BUFFER_STATUS_TIMEOUT);
	do {
		error = goodix_i2c_read(ts->client, GOODIX_READ_COOR_ADDR,
					data, ts->contact_size + 1);
		if (error) {
			dev_err(&ts->client->dev, "I2C transfer error: %d\n",
					error);
			return error;
		}

		if (data[0] & GOODIX_BUFFER_STATUS_READY) {
			touch_num = data[0] & 0x0f;
			if (touch_num > ts->max_touch_num)
				return -EPROTO;

			if (touch_num > 1) {
				data += 1 + ts->contact_size;
				error = goodix_i2c_read(ts->client,
						GOODIX_READ_COOR_ADDR +
							1 + ts->contact_size,
						data,
						ts->contact_size *
							(touch_num - 1));
				if (error)
					return error;
			}

			return touch_num;
		}

		usleep_range(1000, 2000); /* Poll every 1 - 2 ms */
	} while (time_before(jiffies, max_timeout));

	/*
	 * The Goodix panel will send spurious interrupts after a
	 * 'finger up' event, which will always cause a timeout.
	 */
	return 0;
}

static void goodix_ts_report_touch_8b(struct goodix_ts_data *ts, u8 *coor_data)
{
	int id = coor_data[0] & 0x0F;
	int input_x = get_unaligned_le16(&coor_data[1]);
	int input_y = get_unaligned_le16(&coor_data[3]);
	int input_w = get_unaligned_le16(&coor_data[5]);

	input_mt_slot(ts->input_dev, id);
	input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
	touchscreen_report_pos(ts->input_dev, &ts->prop,
			       input_x, input_y, true);
	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, input_w);
	input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, input_w);
}

static void goodix_ts_report_touch_9b(struct goodix_ts_data *ts, u8 *coor_data)
{
	int id = coor_data[1] & 0x0F;
	int input_x = get_unaligned_le16(&coor_data[3]);
	int input_y = get_unaligned_le16(&coor_data[5]);
	int input_w = get_unaligned_le16(&coor_data[7]);

	input_mt_slot(ts->input_dev, id);
	input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
	touchscreen_report_pos(ts->input_dev, &ts->prop,
			       input_x, input_y, true);
	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, input_w);
	input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, input_w);
}

/**
 * goodix_process_events - Process incoming events
 *
 * @ts: our goodix_ts_data pointer
 *
 * Called when the IRQ is triggered. Read the current device state, and push
 * the input events to the user space.
 */
static void goodix_process_events(struct goodix_ts_data *ts)
{
	u8  point_data[1 + GOODIX_MAX_CONTACT_SIZE * GOODIX_MAX_CONTACTS];
	int touch_num;
	int i;

	touch_num = goodix_ts_read_input_report(ts, point_data);
	if (touch_num < 0)
		return;

	/*
	 * Bit 4 of the first byte reports the status of the capacitive
	 * Windows/Home button.
	 */
	input_report_key(ts->input_dev, KEY_LEFTMETA, point_data[0] & BIT(4));

	for (i = 0; i < touch_num; i++)
		if (ts->contact_size == 9)
			goodix_ts_report_touch_9b(ts,
				&point_data[1 + ts->contact_size * i]);
		else
			goodix_ts_report_touch_8b(ts,
				&point_data[1 + ts->contact_size * i]);

	input_mt_sync_frame(ts->input_dev);
	input_sync(ts->input_dev);
}

/**
 * goodix_ts_irq_handler - The IRQ handler
 *
 * @irq: interrupt number.
 * @dev_id: private data pointer.
 */
static irqreturn_t goodix_ts_irq_handler(int irq, void *dev_id)
{
	struct goodix_ts_data *ts = dev_id;

	goodix_process_events(ts);

	if (goodix_i2c_write_u8(ts->client, GOODIX_READ_COOR_ADDR, 0) < 0)
		dev_err(&ts->client->dev, "I2C write end_cmd error\n");

	return IRQ_HANDLED;
}

static void goodix_free_irq(struct goodix_ts_data *ts)
{
	devm_free_irq(&ts->client->dev, ts->client->irq, ts);
}

static int goodix_request_irq(struct goodix_ts_data *ts)
{
	return devm_request_threaded_irq(&ts->client->dev, ts->client->irq,
					 NULL, goodix_ts_irq_handler,
					 ts->irq_flags, ts->client->name, ts);
}

static int goodix_check_cfg_8(struct goodix_ts_data *ts, const u8 *cfg, int len)
{
	int i, raw_cfg_len = len - 2;
	u8 check_sum = 0;

	for (i = 0; i < raw_cfg_len; i++)
		check_sum += cfg[i];
	check_sum = (~check_sum) + 1;
	if (check_sum != cfg[raw_cfg_len]) {
		dev_err(&ts->client->dev,
			"The checksum of the config fw is not correct");
		return -EINVAL;
	}

	if (cfg[raw_cfg_len + 1] != 1) {
		dev_err(&ts->client->dev,
			"Config fw must have Config_Fresh register set");
		return -EINVAL;
	}

	return 0;
}

static void goodix_fix_cfg_8(struct goodix_ts_data *ts)
{
	int i, raw_cfg_len = ts->chip->config_len - 2;
	u8 check_sum = 0;

	for (i = 0; i < raw_cfg_len; i++)
		check_sum += ts->config[i];
	check_sum = (~check_sum) + 1;

	ts->config[raw_cfg_len] = check_sum;
	ts->config[raw_cfg_len + 1] = 1;
}

static int goodix_check_cfg_16(struct goodix_ts_data *ts, const u8 *cfg,
			       int len)
{
	int i, raw_cfg_len = len - 3;
	u16 check_sum = 0;

	for (i = 0; i < raw_cfg_len; i += 2)
		check_sum += get_unaligned_be16(&cfg[i]);
	check_sum = (~check_sum) + 1;
	if (check_sum != get_unaligned_be16(&cfg[raw_cfg_len])) {
		dev_err(&ts->client->dev,
			"The checksum of the config fw is not correct");
		return -EINVAL;
	}

	if (cfg[raw_cfg_len + 2] != 1) {
		dev_err(&ts->client->dev,
			"Config fw must have Config_Fresh register set");
		return -EINVAL;
	}

	return 0;
}

static void goodix_fix_cfg_16(struct goodix_ts_data *ts)
{
	int i, raw_cfg_len = ts->chip->config_len - 3;
	u16 check_sum = 0;

	for (i = 0; i < raw_cfg_len; i += 2)
		check_sum += get_unaligned_be16(&ts->config[i]);
	check_sum = (~check_sum) + 1;

	put_unaligned_be16(check_sum, &ts->config[raw_cfg_len]);
	ts->config[raw_cfg_len + 2] = 1;
}

/**
 * goodix_check_cfg - Checks if config fw is valid
 *
 * @ts: goodix_ts_data pointer
 * @cfg: firmware config data
 */
static int goodix_check_cfg(struct goodix_ts_data *ts, const u8 *cfg, int len)
{
	if (len > GOODIX_CONFIG_MAX_LENGTH) {
		dev_err(&ts->client->dev,
			"The length of the config fw is not correct");
		return -EINVAL;
	}

	return ts->chip->check_config(ts, cfg, len);
}

/**
 * goodix_send_cfg - Write fw config to device
 *
 * @ts: goodix_ts_data pointer
 * @cfg: config firmware to write to device
 */
static int goodix_send_cfg(struct goodix_ts_data *ts, const u8 *cfg, int len)
{
	int error;

	error = goodix_check_cfg(ts, cfg, len);
	if (error)
		return error;

	error = goodix_i2c_write(ts->client, ts->chip->config_addr, cfg, len);
	if (error) {
		dev_err(&ts->client->dev, "Failed to write config data: %d",
			error);
		return error;
	}
	dev_dbg(&ts->client->dev, "Config sent successfully.");

	/* Let the firmware reconfigure itself, so sleep for 10ms */
	usleep_range(10000, 11000);

	return 0;
}

static int goodix_irq_direction_output(struct goodix_ts_data *ts,
				       int value)
{
	struct device *dev = &ts->client->dev;
	acpi_status status;

	switch (ts->irq_pin_access_method) {
	case irq_pin_access_none:
		dev_err(&ts->client->dev,
			"%s called without an irq_pin_access_method set\n",
			__func__);
		return -EINVAL;
	case irq_pin_access_gpio:
		return gpiod_direction_output(ts->gpiod_int, value);
	case irq_pin_access_acpi_gpio:
		/*
		 * The IRQ pin triggers on a falling edge, so its gets marked
		 * as active-low, use output_raw to avoid the value inversion.
		 */
		return gpiod_direction_output_raw(ts->gpiod_int, value);
	case irq_pin_access_acpi_method:
		status = acpi_execute_simple_method(ACPI_HANDLE(dev),
						    "INTO", value);
		return ACPI_SUCCESS(status) ? 0 : -EIO;
	}

	return -EINVAL; /* Never reached */
}

static int goodix_irq_direction_input(struct goodix_ts_data *ts)
{
	struct device *dev = &ts->client->dev;
	acpi_status status;

	switch (ts->irq_pin_access_method) {
	case irq_pin_access_none:
		dev_err(&ts->client->dev,
			"%s called without an irq_pin_access_method set\n",
			__func__);
		return -EINVAL;
	case irq_pin_access_gpio:
	case irq_pin_access_acpi_gpio:
		return gpiod_direction_input(ts->gpiod_int);
	case irq_pin_access_acpi_method:
		status = acpi_evaluate_object(ACPI_HANDLE(dev), "INTI",
					      NULL, NULL);
		return ACPI_SUCCESS(status) ? 0 : -EIO;
	}

	return -EINVAL; /* Never reached */
}

static int goodix_int_sync(struct goodix_ts_data *ts)
{
	int error;

	error = goodix_irq_direction_output(ts, 0);
	if (error)
		return error;

	msleep(50);				/* T5: 50ms */

	error = goodix_irq_direction_input(ts);
	if (error)
		return error;

	return 0;
}

/**
 * goodix_reset - Reset device during power on
 *
 * @ts: goodix_ts_data pointer
 */
static int goodix_reset(struct goodix_ts_data *ts)
{
	int error;

	/* begin select I2C slave addr */
	error = gpiod_direction_output(ts->gpiod_rst, 0);
	if (error)
		return error;

	msleep(20);				/* T2: > 10ms */

	/* HIGH: 0x28/0x29, LOW: 0xBA/0xBB */
	error = goodix_irq_direction_output(ts, ts->client->addr == 0x14);
	if (error)
		return error;

	usleep_range(100, 2000);		/* T3: > 100us */

	error = gpiod_direction_output(ts->gpiod_rst, 1);
	if (error)
		return error;

	usleep_range(6000, 10000);		/* T4: > 5ms */

	/* end select I2C slave addr */
	error = gpiod_direction_input(ts->gpiod_rst);
	if (error)
		return error;

	error = goodix_int_sync(ts);
	if (error)
		return error;

	return 0;
}

#if defined CONFIG_X86 && defined CONFIG_ACPI
#include <asm/cpu_device_id.h>
#include <asm/intel-family.h>

static const struct x86_cpu_id baytrail_cpu_ids[] = {
	{ X86_VENDOR_INTEL, 6, INTEL_FAM6_ATOM_SILVERMONT, X86_FEATURE_ANY, },
	{}
};

static inline bool is_byt(void)
{
	const struct x86_cpu_id *id = x86_match_cpu(baytrail_cpu_ids);

	return !!id;
}

static const struct acpi_gpio_params first_gpio = { 0, 0, false };
static const struct acpi_gpio_params second_gpio = { 1, 0, false };

static const struct acpi_gpio_mapping acpi_goodix_int_first_gpios[] = {
	{ GOODIX_GPIO_INT_NAME "-gpios", &first_gpio, 1 },
	{ GOODIX_GPIO_RST_NAME "-gpios", &second_gpio, 1 },
	{ },
};

static const struct acpi_gpio_mapping acpi_goodix_int_last_gpios[] = {
	{ GOODIX_GPIO_RST_NAME "-gpios", &first_gpio, 1 },
	{ GOODIX_GPIO_INT_NAME "-gpios", &second_gpio, 1 },
	{ },
};

static const struct acpi_gpio_mapping acpi_goodix_reset_only_gpios[] = {
	{ GOODIX_GPIO_RST_NAME "-gpios", &first_gpio, 1 },
	{ },
};

static int goodix_resource(struct acpi_resource *ares, void *data)
{
	struct goodix_ts_data *ts = data;
	struct device *dev = &ts->client->dev;
	struct acpi_resource_gpio *gpio;

	switch (ares->type) {
	case ACPI_RESOURCE_TYPE_GPIO:
		gpio = &ares->data.gpio;
		if (gpio->connection_type == ACPI_RESOURCE_GPIO_TYPE_INT) {
			if (ts->gpio_int_idx == -1) {
				ts->gpio_int_idx = ts->gpio_count;
			} else {
				dev_err(dev, "More then one GpioInt resource, ignoring ACPI GPIO resources\n");
				ts->gpio_int_idx = -2;
			}
		}
		ts->gpio_count++;
		break;
	default:
		break;
	}

	return 0;
}

static int goodix_add_acpi_gpio_mappings(struct goodix_ts_data *ts)
{
	const struct acpi_gpio_mapping *gpio_mapping = NULL;
	struct device *dev = &ts->client->dev;
	LIST_HEAD(resources);
	int ret;

	ts->gpio_count = 0;
	ts->gpio_int_idx = -1;
	ret = acpi_dev_get_resources(ACPI_COMPANION(dev), &resources,
				     goodix_resource, ts);
	if (ret < 0) {
		dev_err(dev, "Error getting ACPI resources: %d\n", ret);
		return ret;
	}

	acpi_dev_free_resource_list(&resources);

	if (ts->gpio_count == 2 && ts->gpio_int_idx == 0) {
		ts->irq_pin_access_method = irq_pin_access_acpi_gpio;
		gpio_mapping = acpi_goodix_int_first_gpios;
	} else if (ts->gpio_count == 2 && ts->gpio_int_idx == 1) {
		ts->irq_pin_access_method = irq_pin_access_acpi_gpio;
		gpio_mapping = acpi_goodix_int_last_gpios;
	} else if (ts->gpio_count == 1 && ts->gpio_int_idx == -1 &&
		   acpi_has_method(ACPI_HANDLE(dev), "INTI") &&
		   acpi_has_method(ACPI_HANDLE(dev), "INTO")) {
		dev_info(dev, "Using ACPI INTI and INTO methods for IRQ pin access\n");
		ts->irq_pin_access_method = irq_pin_access_acpi_method;
		gpio_mapping = acpi_goodix_reset_only_gpios;
	} else if (is_byt() && ts->gpio_count == 2 && ts->gpio_int_idx == -1) {
		dev_info(dev, "No ACPI GpioInt resource, assuming that the GPIO order is reset, int\n");
		ts->irq_pin_access_method = irq_pin_access_acpi_gpio;
		gpio_mapping = acpi_goodix_int_last_gpios;
	} else {
		dev_warn(dev, "Unexpected ACPI resources: gpio_count %d, gpio_int_idx %d\n",
			 ts->gpio_count, ts->gpio_int_idx);
		return -EINVAL;
	}

	return devm_acpi_dev_add_driver_gpios(dev, gpio_mapping);
}
#else
static int goodix_add_acpi_gpio_mappings(struct goodix_ts_data *ts)
{
	return -EINVAL;
}
#endif /* CONFIG_X86 && CONFIG_ACPI */

/**
 * goodix_get_gpio_config - Get GPIO config from ACPI/DT
 *
 * @ts: goodix_ts_data pointer
 */
static int goodix_get_gpio_config(struct goodix_ts_data *ts)
{
	int error;
	struct device *dev;
	struct gpio_desc *gpiod;
	bool added_acpi_mappings = false;

	if (!ts->client)
		return -EINVAL;
	dev = &ts->client->dev;

	ts->avdd28 = devm_regulator_get(dev, "AVDD28");
	if (IS_ERR(ts->avdd28)) {
		error = PTR_ERR(ts->avdd28);
		if (error != -EPROBE_DEFER)
			dev_err(dev,
				"Failed to get AVDD28 regulator: %d\n", error);
		return error;
	}

	ts->vddio = devm_regulator_get(dev, "VDDIO");
	if (IS_ERR(ts->vddio)) {
		error = PTR_ERR(ts->vddio);
		if (error != -EPROBE_DEFER)
			dev_err(dev,
				"Failed to get VDDIO regulator: %d\n", error);
		return error;
	}

retry_get_irq_gpio:
	/* Get the interrupt GPIO pin number */
	gpiod = devm_gpiod_get_optional(dev, GOODIX_GPIO_INT_NAME, GPIOD_IN);
	if (IS_ERR(gpiod)) {
		error = PTR_ERR(gpiod);
		if (error != -EPROBE_DEFER)
			dev_dbg(dev, "Failed to get %s GPIO: %d\n",
				GOODIX_GPIO_INT_NAME, error);
		return error;
	}
	if (!gpiod && has_acpi_companion(dev) && !added_acpi_mappings) {
		added_acpi_mappings = true;
		if (goodix_add_acpi_gpio_mappings(ts) == 0)
			goto retry_get_irq_gpio;
	}

	ts->gpiod_int = gpiod;

	/* Get the reset line GPIO pin number */
	gpiod = devm_gpiod_get_optional(dev, GOODIX_GPIO_RST_NAME, GPIOD_IN);
	if (IS_ERR(gpiod)) {
		error = PTR_ERR(gpiod);
		if (error != -EPROBE_DEFER)
			dev_dbg(dev, "Failed to get %s GPIO: %d\n",
				GOODIX_GPIO_RST_NAME, error);
		return error;
	}

	ts->gpiod_rst = gpiod;

	switch (ts->irq_pin_access_method) {
	case irq_pin_access_acpi_gpio:
		if (!ts->gpiod_int || !ts->gpiod_rst)
			ts->irq_pin_access_method = irq_pin_access_none;
		break;
	case irq_pin_access_acpi_method:
		if (!ts->gpiod_rst)
			ts->irq_pin_access_method = irq_pin_access_none;
		break;
	default:
		if (ts->gpiod_int && ts->gpiod_rst) {
			ts->reset_controller_at_probe = true;
			ts->load_cfg_from_disk = true;
			ts->irq_pin_access_method = irq_pin_access_gpio;
		}
	}

	return 0;
}

/**
 * goodix_read_config - Read the embedded configuration of the panel
 *
 * @ts: our goodix_ts_data pointer
 *
 * Must be called during probe
 */
static void goodix_read_config(struct goodix_ts_data *ts)
{
	int x_max, y_max;
	int error;

	error = goodix_i2c_read(ts->client, ts->chip->config_addr,
				ts->config, ts->chip->config_len);
	if (error) {
		dev_warn(&ts->client->dev, "Error reading config: %d\n",
			 error);
		ts->int_trigger_type = GOODIX_INT_TRIGGER;
		ts->max_touch_num = GOODIX_MAX_CONTACTS;
		return;
	}

	ts->int_trigger_type = ts->config[TRIGGER_LOC] & 0x03;
	ts->max_touch_num = ts->config[MAX_CONTACTS_LOC] & 0x0f;

	x_max = get_unaligned_le16(&ts->config[RESOLUTION_LOC]);
	y_max = get_unaligned_le16(&ts->config[RESOLUTION_LOC + 2]);
	if (x_max && y_max) {
		input_abs_set_max(ts->input_dev, ABS_MT_POSITION_X, x_max - 1);
		input_abs_set_max(ts->input_dev, ABS_MT_POSITION_Y, y_max - 1);
	}

	/*
	 * Ensure valid checksum and config_fresh bit being set for possible
	 * re-upload of config after suspend/resume.
	 */
	ts->chip->fix_config(ts);
}

/**
 * goodix_read_version - Read goodix touchscreen version
 *
 * @ts: our goodix_ts_data pointer
 */
static int goodix_read_version(struct goodix_ts_data *ts)
{
	int error;
	u8 buf[6];
	char id_str[5];

	error = goodix_i2c_read(ts->client, GOODIX_REG_ID, buf, sizeof(buf));
	if (error) {
		dev_err(&ts->client->dev, "read version failed: %d\n", error);
		return error;
	}

	memcpy(id_str, buf, 4);
	id_str[4] = 0;
	if (kstrtou16(id_str, 10, &ts->id))
		ts->id = 0x1001;

	ts->version = get_unaligned_le16(&buf[4]);

	dev_info(&ts->client->dev, "ID %d, version: %04x\n", ts->id,
		 ts->version);

	return 0;
}

/**
 * goodix_i2c_test - I2C test function to check if the device answers.
 *
 * @client: the i2c client
 */
static int goodix_i2c_test(struct i2c_client *client)
{
	int retry = 0;
	int error;
	u8 test;

	while (retry++ < 2) {
		error = goodix_i2c_read(client, GOODIX_REG_ID,
					&test, 1);
		if (!error)
			return 0;

		dev_err(&client->dev, "i2c test failed attempt %d: %d\n",
			retry, error);
		msleep(20);
	}

	return error;
}

/**
 * goodix_configure_dev - Finish device initialization
 *
 * @ts: our goodix_ts_data pointer
 *
 * Must be called from probe to finish initialization of the device.
 * Contains the common initialization code for both devices that
 * declare gpio pins and devices that do not. It is either called
 * directly from probe or from request_firmware_wait callback.
 */
static int goodix_configure_dev(struct goodix_ts_data *ts)
{
	int error;

	ts->int_trigger_type = GOODIX_INT_TRIGGER;
	ts->max_touch_num = GOODIX_MAX_CONTACTS;

	ts->input_dev = devm_input_allocate_device(&ts->client->dev);
	if (!ts->input_dev) {
		dev_err(&ts->client->dev, "Failed to allocate input device.");
		return -ENOMEM;
	}

	ts->input_dev->name = "Goodix Capacitive TouchScreen";
	ts->input_dev->phys = "input/ts";
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->id.vendor = 0x0416;
	ts->input_dev->id.product = ts->id;
	ts->input_dev->id.version = ts->version;

	/* Capacitive Windows/Home button on some devices */
	input_set_capability(ts->input_dev, EV_KEY, KEY_LEFTMETA);

	input_set_capability(ts->input_dev, EV_ABS, ABS_MT_POSITION_X);
	input_set_capability(ts->input_dev, EV_ABS, ABS_MT_POSITION_Y);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);

	/* Read configuration and apply touchscreen parameters */
	goodix_read_config(ts);

	/* Try overriding touchscreen parameters via device properties */
	touchscreen_parse_properties(ts->input_dev, true, &ts->prop);

	if (!ts->prop.max_x || !ts->prop.max_y || !ts->max_touch_num) {
		dev_err(&ts->client->dev,
			"Invalid config (%d, %d, %d), using defaults\n",
			ts->prop.max_x, ts->prop.max_y, ts->max_touch_num);
		ts->prop.max_x = GOODIX_MAX_WIDTH - 1;
		ts->prop.max_y = GOODIX_MAX_HEIGHT - 1;
		ts->max_touch_num = GOODIX_MAX_CONTACTS;
		input_abs_set_max(ts->input_dev,
				  ABS_MT_POSITION_X, ts->prop.max_x);
		input_abs_set_max(ts->input_dev,
				  ABS_MT_POSITION_Y, ts->prop.max_y);
	}

	if (dmi_check_system(rotated_screen)) {
		ts->prop.invert_x = true;
		ts->prop.invert_y = true;
		dev_dbg(&ts->client->dev,
			"Applying '180 degrees rotated screen' quirk\n");
	}

	if (dmi_check_system(nine_bytes_report)) {
		ts->contact_size = 9;

		dev_dbg(&ts->client->dev,
			"Non-standard 9-bytes report format quirk\n");
	}

	error = input_mt_init_slots(ts->input_dev, ts->max_touch_num,
				    INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED);
	if (error) {
		dev_err(&ts->client->dev,
			"Failed to initialize MT slots: %d", error);
		return error;
	}

	error = input_register_device(ts->input_dev);
	if (error) {
		dev_err(&ts->client->dev,
			"Failed to register input device: %d", error);
		return error;
	}

	ts->irq_flags = goodix_irq_flags[ts->int_trigger_type] | IRQF_ONESHOT;
	error = goodix_request_irq(ts);
	if (error) {
		dev_err(&ts->client->dev, "request IRQ failed: %d\n", error);
		return error;
	}

	return 0;
}

/**
 * goodix_config_cb - Callback to finish device init
 *
 * @ts: our goodix_ts_data pointer
 *
 * request_firmware_wait callback that finishes
 * initialization of the device.
 */
static void goodix_config_cb(const struct firmware *cfg, void *ctx)
{
	struct goodix_ts_data *ts = ctx;
	int error;

	if (cfg) {
		/* send device configuration to the firmware */
		error = goodix_send_cfg(ts, cfg->data, cfg->size);
		if (error)
			goto err_release_cfg;
	}

	goodix_configure_dev(ts);

err_release_cfg:
	release_firmware(cfg);
	complete_all(&ts->firmware_loading_complete);
}

static void goodix_disable_regulators(void *arg)
{
	struct goodix_ts_data *ts = arg;

	regulator_disable(ts->vddio);
	regulator_disable(ts->avdd28);
}

static int goodix_ts_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct goodix_ts_data *ts;
	int error;

	dev_dbg(&client->dev, "I2C Address: 0x%02x\n", client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C check functionality failed.\n");
		return -ENXIO;
	}

	ts = devm_kzalloc(&client->dev, sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	ts->client = client;
	i2c_set_clientdata(client, ts);
	init_completion(&ts->firmware_loading_complete);
	ts->contact_size = GOODIX_CONTACT_SIZE;

	error = goodix_get_gpio_config(ts);
	if (error)
		return error;

	/* power up the controller */
	error = regulator_enable(ts->avdd28);
	if (error) {
		dev_err(&client->dev,
			"Failed to enable AVDD28 regulator: %d\n",
			error);
		return error;
	}

	error = regulator_enable(ts->vddio);
	if (error) {
		dev_err(&client->dev,
			"Failed to enable VDDIO regulator: %d\n",
			error);
		regulator_disable(ts->avdd28);
		return error;
	}

	error = devm_add_action_or_reset(&client->dev,
					 goodix_disable_regulators, ts);
	if (error)
		return error;

	if (ts->reset_controller_at_probe) {
		/* reset the controller */
		error = goodix_reset(ts);
		if (error) {
			dev_err(&client->dev, "Controller reset failed.\n");
			return error;
		}
	}

	error = goodix_i2c_test(client);
	if (error) {
		dev_err(&client->dev, "I2C communication failure: %d\n", error);
		return error;
	}

	error = goodix_read_version(ts);
	if (error) {
		dev_err(&client->dev, "Read version failed.\n");
		return error;
	}

	ts->chip = goodix_get_chip_data(ts->id);

	if (ts->load_cfg_from_disk) {
		/* update device config */
		ts->cfg_name = devm_kasprintf(&client->dev, GFP_KERNEL,
					      "goodix_%d_cfg.bin", ts->id);
		if (!ts->cfg_name)
			return -ENOMEM;

		error = request_firmware_nowait(THIS_MODULE, true, ts->cfg_name,
						&client->dev, GFP_KERNEL, ts,
						goodix_config_cb);
		if (error) {
			dev_err(&client->dev,
				"Failed to invoke firmware loader: %d\n",
				error);
			return error;
		}

		return 0;
	} else {
		error = goodix_configure_dev(ts);
		if (error)
			return error;
	}

	return 0;
}

static int goodix_ts_remove(struct i2c_client *client)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts->load_cfg_from_disk)
		wait_for_completion(&ts->firmware_loading_complete);

	return 0;
}

static int __maybe_unused goodix_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct goodix_ts_data *ts = i2c_get_clientdata(client);
	int error;

	/* We need gpio pins to suspend/resume */
	if (ts->irq_pin_access_method == irq_pin_access_none) {
		disable_irq(client->irq);
		return 0;
	}

	if (ts->load_cfg_from_disk)
		wait_for_completion(&ts->firmware_loading_complete);

	/* Free IRQ as IRQ pin is used as output in the suspend sequence */
	goodix_free_irq(ts);

	/* Output LOW on the INT pin for 5 ms */
	error = goodix_irq_direction_output(ts, 0);
	if (error) {
		goodix_request_irq(ts);
		return error;
	}

	usleep_range(5000, 6000);

	error = goodix_i2c_write_u8(ts->client, GOODIX_REG_COMMAND,
				    GOODIX_CMD_SCREEN_OFF);
	if (error) {
		dev_err(&ts->client->dev, "Screen off command failed\n");
		goodix_irq_direction_input(ts);
		goodix_request_irq(ts);
		return -EAGAIN;
	}

	/*
	 * The datasheet specifies that the interval between sending screen-off
	 * command and wake-up should be longer than 58 ms. To avoid waking up
	 * sooner, delay 58ms here.
	 */
	msleep(58);
	return 0;
}

static int __maybe_unused goodix_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct goodix_ts_data *ts = i2c_get_clientdata(client);
	u8 config_ver;
	int error;

	if (ts->irq_pin_access_method == irq_pin_access_none) {
		enable_irq(client->irq);
		return 0;
	}

	/*
	 * Exit sleep mode by outputting HIGH level to INT pin
	 * for 2ms~5ms.
	 */
	error = goodix_irq_direction_output(ts, 1);
	if (error)
		return error;

	usleep_range(2000, 5000);

	error = goodix_int_sync(ts);
	if (error)
		return error;

	error = goodix_i2c_read(ts->client, ts->chip->config_addr,
				&config_ver, 1);
	if (error)
		dev_warn(dev, "Error reading config version: %d, resetting controller\n",
			 error);
	else if (config_ver != ts->config[0])
		dev_warn(dev, "Config version mismatch %d != %d, resetting controller\n",
			 config_ver, ts->config[0]);

	if (error != 0 || config_ver != ts->config[0]) {
		error = goodix_reset(ts);
		if (error) {
			dev_err(dev, "Controller reset failed.\n");
			return error;
		}

		error = goodix_send_cfg(ts, ts->config, ts->chip->config_len);
		if (error)
			return error;
	}

	error = goodix_request_irq(ts);
	if (error)
		return error;

	return 0;
}

static SIMPLE_DEV_PM_OPS(goodix_pm_ops, goodix_suspend, goodix_resume);

static const struct i2c_device_id goodix_ts_id[] = {
	{ "GDIX1001:00", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, goodix_ts_id);

#ifdef CONFIG_ACPI
static const struct acpi_device_id goodix_acpi_match[] = {
	{ "GDIX1001", 0 },
	{ "GDIX1002", 0 },
	{ }
};
MODULE_DEVICE_TABLE(acpi, goodix_acpi_match);
#endif

#ifdef CONFIG_OF
static const struct of_device_id goodix_of_match[] = {
	{ .compatible = "goodix,gt1151" },
	{ .compatible = "goodix,gt5663" },
	{ .compatible = "goodix,gt5688" },
	{ .compatible = "goodix,gt911" },
	{ .compatible = "goodix,gt9110" },
	{ .compatible = "goodix,gt912" },
	{ .compatible = "goodix,gt927" },
	{ .compatible = "goodix,gt9271" },
	{ .compatible = "goodix,gt928" },
	{ .compatible = "goodix,gt967" },
	{ }
};
MODULE_DEVICE_TABLE(of, goodix_of_match);
#endif

static struct i2c_driver goodix_ts_driver = {
	.probe = goodix_ts_probe,
	.remove = goodix_ts_remove,
	.id_table = goodix_ts_id,
	.driver = {
		.name = "Goodix-TS",
		.acpi_match_table = ACPI_PTR(goodix_acpi_match),
		.of_match_table = of_match_ptr(goodix_of_match),
		.pm = &goodix_pm_ops,
	},
};
module_i2c_driver(goodix_ts_driver);

MODULE_AUTHOR("Benjamin Tissoires <benjamin.tissoires@gmail.com>");
MODULE_AUTHOR("Bastien Nocera <hadess@hadess.net>");
MODULE_DESCRIPTION("Goodix touchscreen driver");
MODULE_LICENSE("GPL v2");
