// SPDX-License-Identifier: GPL-2.0
/*
 * ASAIR DHT20 I2C humidity and temperature sensor driver.
 *
 * Datasheet: DHT20 Data Sheet v1.0, May 2021 (www.aosong.com)
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include <linux/iio/iio.h>

#define DHT20_REG_INIT_1		0x1B
#define DHT20_REG_INIT_2		0x1C
#define DHT20_REG_INIT_3		0x1E
#define DHT20_REG_WRITE_OR		0xB0

#define DHT20_POWERON_DELAY_MS		100
#define DHT20_POLL_INTERVAL_MS		10
#define DHT20_STATUS_INIT_MASK		0x18

struct dht20_data {
	struct i2c_client	*client;
	struct mutex		lock;	/* serializes I2C transactions */
};

static const struct iio_info dht20_info = {};

static const struct iio_chan_spec dht20_channels[] = {};

static int dht20_read_status(struct i2c_client *client, u8 *status)
{
	int ret;

	ret = i2c_smbus_read_byte(client);
	if (ret < 0)
		return ret;

	*status = ret;
	return 0;
}

/*
 * Per ASAIR reference code, when the calibration/init status bits are
 * not set after power-on the three hidden calibration registers (0x1B,
 * 0x1C, 0x1E) must be reloaded with their factory values through the
 * "JH_Reset_REG" sequence.
 */
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
	/* buf[1], buf[2] carry the values just read back from the sensor */

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