// SPDX-License-Identifier: GPL-2.0
/*
 * ASAIR DHT20 I2C humidity and temperature sensor driver.
 *
 * Datasheet: DHT20 Data Sheet v1.0, May 2021 (www.aosong.com)
 */

#include <linux/i2c.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>

static int dht20_probe(struct i2c_client *client)
{
	return 0;
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