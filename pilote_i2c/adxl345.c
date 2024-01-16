#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
typedef enum {
    DEVID = 0x00,
    BW_RATE = 0x2C,
    POWER_CTL = 0x2D,
    INT_ENABLE = 0x2E,
    DATA_FORMAT = 0x31,
    FIFO_CTL = 0x38,
} adxl345_register_t;

int read_register(struct i2c_client *client, adxl345_register_t address, unsigned char *data);
int write_register(struct i2c_client *client, adxl345_register_t address, unsigned char *data);

int read_register(struct i2c_client *client, adxl345_register_t address, unsigned char *data) {
    int transfered;
    unsigned char add_buf = address;
    transfered = i2c_master_send(client, &add_buf, 1);
    if (transfered != 1) {
        return -1;
    }
    transfered = i2c_master_recv(client, data, 1);
    if (transfered != 1) {
        return -2;
    }
    return transfered;
}

int write_register(struct i2c_client *client, adxl345_register_t address, unsigned char *data) {
    int transfered;
    unsigned char add_buf = address;
    transfered = i2c_master_send(client, &add_buf, 1);
    if (transfered != 1) {
        return -1;
    }
    transfered = i2c_master_send(client, data, 1);
    if (transfered != 1) {
        return -2;
    }
    return transfered;
}

static int adxl345_probe(struct i2c_client *client,
                         const struct i2c_device_id *id) {
    int transfered;
    unsigned char buf;
    printk("adxl345_probe function called\n");

    // Reading DEVID
    transfered = read_register(client, DEVID, &buf);
    if (transfered < 0) {
        pr_warn("Error reading adxl345\n");
        return -1;
    }
    pr_info("DEVID: %x\n", (unsigned int)buf & 0xFF);

    // Setting 100Hz communication speed
    transfered = read_register(client, BW_RATE, &buf);
    if (transfered < 0) {
        pr_warn("Error reading adxl345\n");
        return -2;
    }

    buf = (buf & 0xf0) | 0x0a;  // Writing code 0xa on lower bits of BW_RATE

    transfered = write_register(client, BW_RATE, &buf);
    if (transfered < 0) {
        pr_warn("Error writing adxl345\n");
        return -2;
    }

    // Disabling interrupts
    buf = (buf & 0x00);
    transfered = write_register(client, INT_ENABLE, &buf);
    if (transfered < 0) {
        pr_warn("Error writing adxl345\n");
        return -3;
    }

    // Defaulting DATA_FORMAT register
    buf = (buf & 0x00);
    transfered = write_register(client, DATA_FORMAT, &buf);
    if (transfered < 0) {
        pr_warn("Error writing adxl345\n");
        return -4;
    }

    // Setting FIFO bypass
    transfered = read_register(client, FIFO_CTL, &buf);
    if (transfered < 0) {
        pr_warn("Error reading adxl345\n");
        return -5;
    }

    buf = (buf & 0x3F);  // zeroing 2 MSb of FIFO_CTL

    transfered = write_register(client, FIFO_CTL, &buf);
    if (transfered < 0) {
        pr_warn("Error writing adxl345\n");
        return -5;
    }

    // Setting Measurement mode
    transfered = read_register(client, POWER_CTL, &buf);
    if (transfered < 0) {
        pr_warn("Error reading adxl345\n");
        return -6;
    }

    buf = (buf | (0x1 << 3));  // Setting bit 3 (Measurement)

    transfered = write_register(client, POWER_CTL, &buf);
    if (transfered < 0) {
        pr_warn("Error writing adxl345\n");
        return -6;
    }
    pr_info("Set up of the device completed\n");
    return 0;
}
static int adxl345_remove(struct i2c_client *client) {
    // Setting Standby mode
    unsigned char buf;
    int transfered;

    transfered = read_register(client, POWER_CTL, &buf);
    if (transfered < 0) {
        pr_warn("Error reading adxl345\n");
        return -7;
    }

    buf = (buf & !(0x1 << 3));  // Resetting bit 3 (Measurement)

    transfered = write_register(client, POWER_CTL, &buf);
    if (transfered < 0) {
        pr_warn("Error writing adxl345\n");
        return -7;
    }

    pr_info("adxl345_remove: device in standby\n");
    return 0;
}

/* The following list allows the association between a device and its driver
driver in the case of a static initialization without using
device tree.
Each entry contains a string used to make the association
association and an integer that can be used by the driver to
driver to perform different treatments depending on the physical
the physical device detected (case of a driver that can manage
different device models).*/
static struct i2c_device_id adxl345_idtable[] = {
    {"adxl345", 0},
    {}};
MODULE_DEVICE_TABLE(i2c, adxl345_idtable);
#ifdef CONFIG_OF
/* If device tree support is available, the following list
allows to make the association using the device tree.
Each entry contains a structure of type of_device_id. The field
compatible field is a string that is used to make the association
with the compatible fields in the device tree. The data field is
a void* pointer that can be used by the driver to perform different
perform different treatments depending on the physical device detected.
device detected.*/
static const struct of_device_id adxl345_of_match[] = {
    {.compatible = "qemu,adxl345",
     .data = NULL},
    {}};
MODULE_DEVICE_TABLE(of, adxl345_of_match);
#endif
static struct i2c_driver adxl345_driver = {
    .driver = {
        /* The name field must correspond to the name of the module
        and must not contain spaces. */
        .name = "adxl345",
        .of_match_table = of_match_ptr(adxl345_of_match),
    },
    .id_table = adxl345_idtable,
    .probe = adxl345_probe,
    .remove = adxl345_remove,
};
module_i2c_driver(adxl345_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("adxl345 driver");
MODULE_AUTHOR("Me");