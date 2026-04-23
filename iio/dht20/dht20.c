// SPDX-License-Identifier: GPL-2.0
/*
 * ASAIR DHT20 I2C humidity and temperature sensor driver.
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/of.h>

#define DHT20_TRIGGER_DELAY_MS	80
#define DHT20_RESP_LEN		7

static const u8 dht20_trigger_cmd[3] = { 0xAC, 0x33, 0x00 };

struct dht20_data {
	struct i2c_client	*client;
	struct mutex		lock;
};

static int dht20_read_sensor(struct dht20_data *data, u8 *buf)
{
	int ret;

	mutex_lock(&data->lock);

	ret = i2c_master_send(data->client, dht20_trigger_cmd,
			      sizeof(dht20_trigger_cmd));
	if (ret < 0)
		goto unlock;
	if (ret != sizeof(dht20_trigger_cmd)) {
		ret = -EIO;
		goto unlock;
	}

	msleep(DHT20_TRIGGER_DELAY_MS);

	ret = i2c_master_recv(data->client, buf, DHT20_RESP_LEN);
	if (ret < 0)
		goto unlock;
	if (ret != DHT20_RESP_LEN) {
		ret = -EIO;
		goto unlock;
	}

	ret = 0;
unlock:
	mutex_unlock(&data->lock);
	return ret;
}

static int dht20_probe(struct i2c_client *client)
{
	struct dht20_data *data;
	u8 buf[DHT20_RESP_LEN];
	int ret;

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;
	mutex_init(&data->lock);
	i2c_set_clientdata(client, data);

	dev_info(&client->dev, "dht20 probed (addr=0x%02x)\n", client->addr);

	ret = dht20_read_sensor(data, buf);
	if (ret)
		dev_warn(&client->dev, "initial read failed: %d\n", ret);
	else
		dev_info(&client->dev, "raw: %*ph\n", DHT20_RESP_LEN, buf);

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
