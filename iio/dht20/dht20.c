#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#define DHT20_ADDR 0x38

static int dht20_probe(struct i2c_client * client){
    dev_info(&client->dev, "dht20 installed");
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