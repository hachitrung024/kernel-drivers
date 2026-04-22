// SPDX-License-Identifier: GPL-2.0
/*
 * ASAIR DHT20 I2C humidity and temperature sensor driver.
 *
 * Task 2: minimal skeleton — probe/remove + module_i2c_driver.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>

static int dht20_probe(struct i2c_client *client)
{
	dev_info(&client->dev, "dht20 probed (addr=0x%02x)\n", client->addr);
	return 0;
}

static void dht20_remove(struct i2c_client *client)
{
	dev_info(&client->dev, "dht20 removed\n");
}

static const struct i2c_device_id dht20_id[] = {
	{ "dht20", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, dht20_id);

static const struct of_device_id dht20_of_match[] = {
	{ .compatible = "asair,dht20" },
	{ }
};
MODULE_DEVICE_TABLE(of, dht20_of_match);

static struct i2c_driver dht20_driver = {
	.driver = {
		.name		= "dht20",
		.of_match_table	= dht20_of_match,
	},
	.probe		= dht20_probe,
	.remove		= dht20_remove,
	.id_table	= dht20_id,
};
module_i2c_driver(dht20_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASAIR DHT20 I2C humidity and temperature sensor driver");
MODULE_AUTHOR("Trung Ha");
