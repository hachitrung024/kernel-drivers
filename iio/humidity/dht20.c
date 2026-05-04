// SPDX-License-Identifier: GPL-2.0
/*
 * ASAIR DHT20 I2C humidity and temperature sensor driver.
 *
 * Datasheet: DHT20 Data Sheet v1.0, May 2021 (www.aosong.com)
 */

#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/jiffies.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>

#include <linux/iio/iio.h>

#define DHT20_CMD_TRIGGER		0xAC
#define DHT20_CMD_TRIGGER_ARG0		0x33
#define DHT20_CMD_TRIGGER_ARG1		0x00

#define DHT20_REG_INIT_1		0x1B
#define DHT20_REG_INIT_2		0x1C
#define DHT20_REG_INIT_3		0x1E
#define DHT20_REG_WRITE_OR		0xB0

#define DHT20_STATUS_BUSY		BIT(7)
#define DHT20_STATUS_INIT_MASK		0x18

#define DHT20_POWERON_DELAY_MS		100
#define DHT20_MEAS_DELAY_MS		80
#define DHT20_POLL_RETRY		3
#define DHT20_POLL_INTERVAL_MS		10
#define DHT20_MIN_INTERVAL_MS		2000

#define DHT20_CRC_INIT			0xFF
#define DHT20_CRC_POLY			0x31

#define DHT20_RESP_LEN			7

struct dht20_data {
	struct i2c_client	*client;
	struct mutex		lock;	/* serializes I2C transactions */
	unsigned long		last_read;
	bool			last_read_valid;
};

static const struct iio_info dht20_info = {};

static const struct iio_chan_spec dht20_channels[] = {
	{
		.type = IIO_HUMIDITYRELATIVE,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
		.scan_index = 0,
		.scan_type = {
			.sign = 'u',
			.realbits = 20,
			.storagebits = 32,
			.endianness = IIO_CPU,
		},
	},
	{
		.type = IIO_TEMP,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
		.scan_index = 1,
		.scan_type = {
			.sign = 's',
			.realbits = 20,
			.storagebits = 32,
			.endianness = IIO_CPU,
		},
	},
	IIO_CHAN_SOFT_TIMESTAMP(2),
};

static u8 dht20_crc8(const u8 *data, int len)
{
	u8 crc = DHT20_CRC_INIT;
	int i, j;

	for (i = 0; i < len; i++) {
		crc ^= data[i];
		for (j = 0; j < 8; j++) {
			if (crc & 0x80)
				crc = (crc << 1) ^ DHT20_CRC_POLY;
			else
				crc <<= 1;
		}
	}

	return crc;
}

static int dht20_read_status(struct i2c_client *client, u8 *status)
{
	int ret;

	ret = i2c_smbus_read_byte(client);
	if (ret < 0)
		return ret;

	*status = ret;
	return 0;
}

static int dht20_reset_reg(struct dht20_data *data, u8 reg)
{
	struct i2c_client *client = data->client;
	u8 buf[3];
	int ret;

	buf[0] = reg;
	buf[1] = 0x00;
	buf[2] = 0x00;

	ret = i2c_master_send(client, buf, sizeof(buf));
	if (ret < 0) {
		dev_err(&client->dev, "reset reg 0x%02x write failed: %d\n",
			reg, ret);
		return ret;
	}

	usleep_range(5000, 6000);

	ret = i2c_master_recv(client, buf, sizeof(buf));
	if (ret < 0) {
		dev_err(&client->dev, "reset reg 0x%02x read failed: %d\n",
			reg, ret);
		return ret;
	}

	msleep(DHT20_POLL_INTERVAL_MS);

	buf[0] = DHT20_REG_WRITE_OR | reg;

	ret = i2c_master_send(client, buf, sizeof(buf));
	if (ret < 0) {
		dev_err(&client->dev, "reset reg 0x%02x restore failed: %d\n",
			reg, ret);
		return ret;
	}

	usleep_range(5000, 6000);
	return 0;
}

static int dht20_init_sensor(struct dht20_data *data)
{
	int ret;

	ret = dht20_reset_reg(data, DHT20_REG_INIT_1);
	if (ret)
		return ret;

	ret = dht20_reset_reg(data, DHT20_REG_INIT_2);
	if (ret)
		return ret;

	return dht20_reset_reg(data, DHT20_REG_INIT_3);
}

static int dht20_wait_not_busy(struct i2c_client *client)
{
	u8 status;
	int ret, i;

	for (i = 0; i < DHT20_POLL_RETRY; i++) {
		ret = dht20_read_status(client, &status);
		if (ret < 0)
			return ret;

		if (!(status & DHT20_STATUS_BUSY))
			return 0;

		msleep(DHT20_POLL_INTERVAL_MS);
	}

	return -ETIMEDOUT;
}

static void dht20_wait_min_interval(struct dht20_data *data)
{
	unsigned long elapsed, remaining;

	if (!data->last_read_valid)
		return;

	elapsed = jiffies_to_msecs(jiffies - data->last_read);
	if (elapsed >= DHT20_MIN_INTERVAL_MS)
		return;

	remaining = DHT20_MIN_INTERVAL_MS - elapsed;
	msleep(remaining);
}

static int dht20_read_sensor(struct dht20_data *data, u32 *raw_humidity,
			     u32 *raw_temp)
{
	struct i2c_client *client = data->client;
	u8 cmd[3] = { DHT20_CMD_TRIGGER, DHT20_CMD_TRIGGER_ARG0,
		      DHT20_CMD_TRIGGER_ARG1 };
	u8 buf[DHT20_RESP_LEN];
	int ret;

	dht20_wait_min_interval(data);

	ret = i2c_master_send(client, cmd, sizeof(cmd));
	if (ret < 0) {
		dev_err(&client->dev, "trigger measurement failed: %d\n", ret);
		return ret;
	}

	msleep(DHT20_MEAS_DELAY_MS);

	ret = dht20_wait_not_busy(client);
	if (ret < 0) {
		dev_err(&client->dev, "sensor busy after measurement: %d\n", ret);
		return ret;
	}

	ret = i2c_master_recv(client, buf, sizeof(buf));
	if (ret < 0) {
		dev_err(&client->dev, "data read failed: %d\n", ret);
		return ret;
	}
	if (ret != sizeof(buf)) {
		dev_err(&client->dev, "short read: %d of %zu bytes\n",
			ret, sizeof(buf));
		return -EIO;
	}

	if (dht20_crc8(buf, 6) != buf[6]) {
		dev_err(&client->dev, "CRC mismatch, discarding sample\n");
		return -EIO;
	}

	*raw_humidity = ((u32)buf[1] << 12) | ((u32)buf[2] << 4) |
			((u32)buf[3] >> 4);
	*raw_temp = (((u32)buf[3] & 0x0F) << 16) | ((u32)buf[4] << 8) |
		    (u32)buf[5];

	data->last_read = jiffies;
	data->last_read_valid = true;

	dev_dbg(&client->dev, "raw hum=0x%05x temp=0x%05x\n",
		*raw_humidity, *raw_temp);

	return 0;
}

static int dht20_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct iio_dev *indio_dev;
	struct dht20_data *data;
	u8 status;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -EOPNOTSUPP;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	data->client = client;
	mutex_init(&data->lock);

	i2c_set_clientdata(client, indio_dev);

	msleep(DHT20_POWERON_DELAY_MS);

	ret = dht20_read_status(client, &status);
	if (ret < 0) {
		dev_err(dev, "DHT20 not found at 0x%02x: %d\n",
			client->addr, ret);
		return -ENXIO;
	}

	if ((status & DHT20_STATUS_INIT_MASK) != DHT20_STATUS_INIT_MASK) {
		dev_dbg(dev, "calibration not loaded (status=0x%02x), initializing\n",
			status);
		ret = dht20_init_sensor(data);
		if (ret) {
			dev_err(dev, "sensor initialization failed: %d\n", ret);
			return ret;
		}
	}

	indio_dev->name = "dht20";
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &dht20_info;
	indio_dev->channels = dht20_channels;
	indio_dev->num_channels = ARRAY_SIZE(dht20_channels);

	return devm_iio_device_register(dev, indio_dev);
}

static const struct i2c_device_id dht20_id[] = {
	{ "dht20" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, dht20_id);

static const struct of_device_id dht20_of_match[] = {
	{ .compatible = "aosong,dht20" },
	{ }
};
MODULE_DEVICE_TABLE(of, dht20_of_match);

static struct i2c_driver dht20_driver = {
	.driver = {
		.name		= "dht20",
		.of_match_table	= dht20_of_match,
	},
	.probe		= dht20_probe,
	.id_table	= dht20_id,
};
module_i2c_driver(dht20_driver);

MODULE_AUTHOR("Trung Ha <hachitrung024@gmail.com>");
MODULE_DESCRIPTION("ASAIR DHT20 humidity and temperature sensor driver");
MODULE_LICENSE("GPL v2");