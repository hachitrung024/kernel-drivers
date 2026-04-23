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
#define DHT20_CRC_INIT		0xFF
#define DHT20_CRC_POLY		0x31	/* x^8 + x^5 + x^4 + 1, x^8 implicit */

static const u8 dht20_trigger_cmd[3] = { 0xAC, 0x33, 0x00 };

struct dht20_data {
	struct i2c_client	*client;
	struct mutex		lock;
};

static u8 dht20_crc8(const u8 *data, size_t len)
{
	u8 crc = DHT20_CRC_INIT;
	size_t i;
	int j;

	for (i = 0; i < len; i++) {
		crc ^= data[i];
		for (j = 0; j < 8; j++)
			crc = (crc & 0x80) ? (crc << 1) ^ DHT20_CRC_POLY
					   : (crc << 1);
	}
	return crc;
}

static int dht20_parse(const u8 *buf, s32 *t_c100, s32 *rh_c100)
{
	u32 s_rh, s_t;

	if (dht20_crc8(buf, 6) != buf[6])
		return -EIO;

	s_rh = ((u32)buf[1] << 12) | ((u32)buf[2] << 4) | (buf[3] >> 4);
	s_t  = (((u32)buf[3] & 0x0F) << 16) | ((u32)buf[4] << 8) | buf[5];

	*rh_c100 = (s32)(((u64)s_rh * 10000) >> 20);
	*t_c100  = (s32)(((u64)s_t  * 20000) >> 20) - 5000;
	return 0;
}

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

static int dht20_measure(struct dht20_data *data, s32 *t_c100, s32 *rh_c100)
{
	u8 buf[DHT20_RESP_LEN];
	int ret;

	ret = dht20_read_sensor(data, buf);
	if (ret)
		return ret;
	return dht20_parse(buf, t_c100, rh_c100);
}

static ssize_t temperature_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct dht20_data *data = dev_get_drvdata(dev);
	s32 t_c100, rh_c100;
	int ret;

	ret = dht20_measure(data, &t_c100, &rh_c100);
	if (ret)
		return ret;

	return sysfs_emit(buf, "%d.%02d\n",
			  t_c100 / 100, abs(t_c100 % 100));
}
static DEVICE_ATTR_RO(temperature);

static ssize_t humidity_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct dht20_data *data = dev_get_drvdata(dev);
	s32 t_c100, rh_c100;
	int ret;

	ret = dht20_measure(data, &t_c100, &rh_c100);
	if (ret)
		return ret;

	return sysfs_emit(buf, "%d.%02d\n",
			  rh_c100 / 100, rh_c100 % 100);
}
static DEVICE_ATTR_RO(humidity);

static struct attribute *dht20_attrs[] = {
	&dev_attr_temperature.attr,
	&dev_attr_humidity.attr,
	NULL,
};
ATTRIBUTE_GROUPS(dht20);

static int dht20_probe(struct i2c_client *client)
{
	struct dht20_data *data;

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;
	mutex_init(&data->lock);
	i2c_set_clientdata(client, data);

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
		.dev_groups	= dht20_groups,
	},
	.probe		= dht20_probe,
	.remove		= dht20_remove,
	.id_table	= dht20_id,
};
module_i2c_driver(dht20_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASAIR DHT20 I2C humidity and temperature sensor driver");
MODULE_AUTHOR("Trung Ha");
