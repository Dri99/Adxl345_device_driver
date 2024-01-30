#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <stdbool.h>

#define MAX_DEVICES 128
typedef enum {
    DEVID = 0x00,
    BW_RATE = 0x2C,
    POWER_CTL = 0x2D,
    INT_ENABLE = 0x2E,
    DATA_FORMAT = 0x31,
    DATAX0 = 0x32,
    DATAX1 = 0x33,
    DATAY0 = 0x34,
    DATAY1 = 0x35,
    DATAZ0 = 0x36,
    DATAZ1 = 0x37,
    FIFO_CTL = 0x38,
} adxl345_register_t;

struct adxl345_device {
    int id;
    int signature;
    struct miscdevice miscdev;
};

static bool used_ids[MAX_DEVICES] = {false};
static unsigned int first_id_available = 0;

static struct attribute* adxl345_attrs [] = {
    NULL,
};
ATTRIBUTE_GROUPS(adxl345);

ssize_t adxl345_read(struct file *file, char __user *buf, size_t count, loff_t *f_pos);

static struct file_operations adxl345_fileopts = {
    .read = adxl345_read
};
static int read_register(struct i2c_client *client, adxl345_register_t address, unsigned char *data);
static int write_register(struct i2c_client *client, adxl345_register_t address, unsigned char *data);
static int setup_adxl345(struct i2c_client *client);

static int get_new_id(void);
static void free_id(int);

static int
adxl345_probe(struct i2c_client *client,
              const struct i2c_device_id *id) {
    int adxl345_id;
    struct adxl345_device *adxl345_device_;
    setup_adxl345(client);

    adxl345_device_ = NULL;
    adxl345_device_ = kmalloc(sizeof(struct adxl345_device), GFP_KERNEL);
    if (adxl345_device_ == NULL) {
        return -3;
    }
    adxl345_id = get_new_id();
    if(adxl345_id < 0){
        return -4;
    }

    adxl345_device_->id = adxl345_id;
    adxl345_device_->signature = 123456789;
    adxl345_device_->miscdev.minor = MISC_DYNAMIC_MINOR;
    adxl345_device_->miscdev.fops = &adxl345_fileopts;
    adxl345_device_->miscdev.this_device = NULL;
    adxl345_device_->miscdev.groups = adxl345_groups;
    adxl345_device_->miscdev.nodename = NULL;
    
    //kasprintf(GFP_KERNEL, adxl345_device_->miscdev.name,"adxl345-%d",adxl345_id);
    adxl345_device_->miscdev.name = kasprintf(GFP_KERNEL,"adxl345-%d",adxl345_id);
    pr_info("%s\n",adxl345_device_->miscdev.name);

    adxl345_device_->miscdev.parent = &(client->dev);
    if(adxl345_device_->miscdev.parent == NULL){
        pr_warn("error\n");
    }
    misc_register(&(adxl345_device_->miscdev));
    pr_info("the i2c_client struct*=%lx\n",(unsigned long)client);

    i2c_set_clientdata(client,(void*)adxl345_device_);
    return 0;
}
static int adxl345_remove(struct i2c_client *client) {
    // Setting Standby mode
    unsigned char buf;
    int transfered;
    struct adxl345_device* adxl345_device_ ;

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
    adxl345_device_ = (struct adxl345_device*) i2c_get_clientdata(client);
    free_id(adxl345_device_->id);
    misc_deregister(&(adxl345_device_->miscdev));
    
    kfree(adxl345_device_->miscdev.name);
    kfree(adxl345_device_);
    return 0;
}

/*
struct adxl345_device* adxl345_device_; 
    char buffer[256];
    adxl345_device_ = container_of(miscdev,struct adxl345_device,miscdev); 
    strncpy(buffer,miscdev->name,256);
    pr_info("miscdevice name: %s\n",buffer);
    pr_info("struct i2c_client*:%lx\n",(unsigned long)client);
    pr_info("adxl345_device_->signature:%d",adxl345_device_->signature);
*/
ssize_t adxl345_read(struct file *file, char __user *buf, size_t count, loff_t *f_pos){
    struct miscdevice* miscdev; 
    struct i2c_client* client;
    int retval;
    short k_buf = 0;

    pr_info("Read called!\n");
    miscdev = (struct miscdevice*)file->private_data;
    client = container_of(miscdev->parent,struct i2c_client,dev); 
    
    read_register(client,POWER_CTL, (char*)&k_buf);
    pr_info("POWERCTL : %x\n",(unsigned int)k_buf & 0xFF);

    if(count == 1){
        retval = read_register(client,DATAX0, (char*)&k_buf);
        if(retval < 0)
            return 0;
    }else{
        retval = read_register(client,DATAX0, &((char*)&k_buf)[0]);
        if(retval < 0)
            return 0;
        retval = read_register(client,DATAX1, &((char*)&k_buf)[1]);
        if(retval < 0)
            count = 1;
        count = 2;
    }

    put_user(k_buf, (short __user*)buf);
    return count;
}

static int setup_adxl345(struct i2c_client *client) {
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
    pr_info("POWER_CTL: %x\n", (unsigned int)buf & 0xFF);
    pr_info("Set up of the device completed\n");
    return 0;
}

static int read_multiple_registers(struct i2c_client *client, adxl345_register_t* addresses, unsigned char *data) {
    return 0;
}

static int read_register(struct i2c_client *client, adxl345_register_t address, unsigned char *data) {
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

static int write_register(struct i2c_client *client, adxl345_register_t address, unsigned char *data) {
    int transfered;
    char buf[2];
    buf[0] = address;
    buf[1] = &data;

    transfered = i2c_master_send(client, buf, 2);
    if (transfered != 2) {
        return -1;
    }
    return transfered;
}

int get_new_id(void) {
    // TODO: This function will have to be made atomic later
    int returned_id;
    if (first_id_available >= MAX_DEVICES){
        returned_id = -1;
    }
    else{
        returned_id = first_id_available;
        used_ids[first_id_available] = true;
    }
    
    while (used_ids[first_id_available] == true || first_id_available >= MAX_DEVICES)
        first_id_available++;
    return returned_id;
}

static void free_id(int id){
	// TODO: This function will have to be made atomic later
    used_ids[id]= false;
    if(id < first_id_available)
        first_id_available = id;    
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