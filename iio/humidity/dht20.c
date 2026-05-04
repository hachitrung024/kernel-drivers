// SPDX-License-Identifier: GPL-2.0
/*
 * ASAIR DHT20 I2C humidity and temperature sensor driver.
 *
 * Datasheet: DHT20 Data Sheet v1.0, May 2021 (www.aosong.com)
 */

#include <linux/i2c.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include <linux/iio/iio.h>

struct dht20_data {
	struct i2c_client	*client;
	struct mutex		lock;	/* serializes I2C transactions */
};

static const struct iio_info dht20_info = {};

static const struct iio_chan_spec dht20_channels[] = {};

static int dht20_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct iio_dev *indio_dev;
	struct dht20_data *data;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -EOPNOTSUPP;

	indio_dev = devm_iio_device_alloc(dev, sizeof(*data));
	if (!indio_dev)
		return -ENOMEM;

	data = iio_priv(indio_dev);
	data->client = client;
	mutex_init(&data->lock);

	i2c_set_clientdata(client, indio_dev);

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