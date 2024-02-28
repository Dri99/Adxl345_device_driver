#include <linux/i2c.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/kfifo.h>
#include <linux/wait.h>
#include <linux/ioctl.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <stdbool.h>

#define MAX_DEVICES 128
#define MAX_FIFO_EL 128
typedef enum
{
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
	FIFO_STATUS = 0x39,
} adxl345_register_t;

struct adxl345_sample{
	int16_t X;
	int16_t Y;
	int16_t Z;
};

struct adxl345_device
{
	int id;
	DECLARE_KFIFO(samples_fifo, struct adxl345_sample, 128);
	struct wait_queue_head queue;
	struct miscdevice miscdev;
};

struct adxl345_file_private
{
	adxl345_register_t axis_address[2];
	struct miscdevice *miscdev;
};

#define READ_X _IO(10, 1)
#define READ_Y _IO(10, 2)
#define READ_Z _IO(10, 3)

static int read_register(struct i2c_client *client, adxl345_register_t address, unsigned char *data);
static int write_register(struct i2c_client *client, adxl345_register_t address, unsigned char *data);
static int read_multiple_registers(struct i2c_client *client, adxl345_register_t base_address, unsigned char *data, size_t num);
static int setup_adxl345(struct i2c_client *client);

static int get_new_id(void);
static void free_id(int);

irqreturn_t adxl345_int(int irq, void *dev_id);

int adxl345_open(struct inode *inode, struct file *file);
long adxl345_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int adxl345_release(struct inode *, struct file *file);
ssize_t adxl345_read(struct file *file, char __user *buf, size_t count, loff_t *f_pos);