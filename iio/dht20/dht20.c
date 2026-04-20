#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/mutex.h>

#define DHT20_ADDR 0x38

#define DHT20_CMD_TRIGGER		0xAC
#define DHT20_CMD_TRIGGER_ARG0		0x33
#define DHT20_CMD_TRIGGER_ARG1		0x00

#define DHT20_RAW_BITS			20
#define DHT20_RAW_SCALE			(1U << DHT20_RAW_BITS)

#define DHT20_MEAS_DELAY_MS		80

#define DHT20_RESP_LEN			7

struct dht20_data {
	struct i2c_client	*client;
	struct mutex		lock;	/* serializes I2C transactions */
	unsigned long		last_read;
	bool			last_read_valid;
};
static int dht20_read_sensor(struct dht20_data *data, u32 *raw_humidity,
			     u32 *raw_temp)
{
	struct i2c_client *client = data->client;
	u8 cmd[3] = { DHT20_CMD_TRIGGER, DHT20_CMD_TRIGGER_ARG0,
		      DHT20_CMD_TRIGGER_ARG1 };
	u8 buf[DHT20_RESP_LEN];
	int ret;

	ret = i2c_master_send(client, cmd, sizeof(cmd));

	msleep(DHT20_MEAS_DELAY_MS);

	ret = i2c_master_recv(client, buf, sizeof(buf));

	*raw_humidity = ((u32)buf[1] << 12) | ((u32)buf[2] << 4) |
			((u32)buf[3] >> 4);
	*raw_temp = (((u32)buf[3] & 0x0F) << 16) | ((u32)buf[4] << 8) |
		    (u32)buf[5];

	data->last_read = jiffies;
	data->last_read_valid = true;

	dev_info(&client->dev, "raw hum=0x%05x temp=0x%05x\n",
		*raw_humidity, *raw_temp);
    int val;
	return 0;
}

static int dht20_probe(struct i2c_client * client){
    u32 temp, humi;
    struct dht20_data data;
    data.client = client;
    mutex_init(&data.lock);
    dev_info(&client->dev, "dht20 installed");
    dht20_read_sensor(&data, &temp,&humi);
    return 0;
}
static void dht20_remove(struct i2c_client * client){
    dev_info(&client->dev, "dht20 removed\n");
}
static const struct i2c_device_id dht20_id[] = {
    {"dht20",0},
    { }
};
MODULE_DEVICE_TABLE(i2c, dht20_id);
static const struct of_device_id dht20_of_match[] = {
    {.compatible = "asair,dht020"},
    { }
};
MODULE_DEVICE_TABLE(of, dht20_of_match);

static struct i2c_driver dht20_driver = {
    .driver = {
        .name = "dht20",
        .of_match_table = dht20_of_match,
    },
    .probe = dht20_probe,
    .remove = dht20_remove,
    .id_table = dht20_id,
};
module_i2c_driver(dht20_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DHT20 sensor driver");
MODULE_AUTHOR("Trung Ha");