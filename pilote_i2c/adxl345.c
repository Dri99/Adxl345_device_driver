#include "adxl345.h"

static bool used_ids[MAX_DEVICES] = {false};
static unsigned int first_id_available = 0;

static struct attribute *adxl345_attrs[] = {
	NULL,
};
ATTRIBUTE_GROUPS(adxl345);

static struct file_operations adxl345_fileopts = {
	.owner = THIS_MODULE,
	.read = adxl345_read,
	.open = adxl345_open,
	.release = adxl345_release,
	.unlocked_ioctl = adxl345_unlocked_ioctl};

static int
adxl345_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	int adxl345_id;
	int err;
	struct adxl345_device *adxl345_device_;

	adxl345_device_ = NULL;
	adxl345_device_ = kmalloc(sizeof(struct adxl345_device), GFP_KERNEL);
	if (adxl345_device_ == NULL)
	{
		return -3;
	}
	adxl345_id = get_new_id();
	if (adxl345_id < 0)
	{
		return -4;
	}

	adxl345_device_->id = adxl345_id;
	INIT_KFIFO(adxl345_device_->samples_fifo);
	init_waitqueue_head(&adxl345_device_->queue);
	adxl345_device_->miscdev.minor = MISC_DYNAMIC_MINOR;
	adxl345_device_->miscdev.fops = &adxl345_fileopts;
	adxl345_device_->miscdev.this_device = NULL;
	adxl345_device_->miscdev.groups = adxl345_groups;
	adxl345_device_->miscdev.nodename = NULL;

	// kasprintf(GFP_KERNEL, adxl345_device_->miscdev.name,"adxl345-%d",adxl345_id);
	adxl345_device_->miscdev.name = kasprintf(GFP_KERNEL, "adxl345-%d", adxl345_id);
	pr_info("%s\n", adxl345_device_->miscdev.name);

	adxl345_device_->miscdev.parent = &(client->dev);
	if (adxl345_device_->miscdev.parent == NULL)
	{
		pr_warn("error\n");
	}
	misc_register(&(adxl345_device_->miscdev));

	err = request_threaded_irq(client->irq, NULL, adxl345_int,
							 IRQF_ONESHOT,
							 adxl345_device_->miscdev.name,
							 (void*)adxl345_device_);
	if(err <0 )
		return -1;
	i2c_set_clientdata(client, (void *)adxl345_device_);
	
	err = setup_adxl345(client);
	return err;
}
static int adxl345_remove(struct i2c_client *client)
{
	// Setting Standby mode
	unsigned char buf;
	int transfered;
	struct adxl345_device *adxl345_device_;

	transfered = read_register(client, POWER_CTL, &buf);
	if (transfered < 0)
	{
		pr_warn("Error reading adxl345\n");
		return -7;
	}

	buf = (buf & !(0x1 << 3)); // Resetting bit 3 (Measurement)

	transfered = write_register(client, POWER_CTL, &buf);
	if (transfered < 0)
	{
		pr_warn("Error writing adxl345\n");
		return -7;
	}

	pr_info("adxl345_remove: device in standby\n");
	adxl345_device_ = (struct adxl345_device *)i2c_get_clientdata(client);
	
	free_irq(client->irq, (void*)adxl345_device_);
	//disable_irq(client->irq);
	free_id(adxl345_device_->id);
	misc_deregister(&(adxl345_device_->miscdev));

	kfree(adxl345_device_->miscdev.name);
	kfree(adxl345_device_);
	return 0;
}

irqreturn_t adxl345_int(int irq, void *dev_id)
{
	struct adxl345_device *adxl345_device_;
	struct i2c_client* client;
	uint8_t h_fifo_elements = 0;
	int err;
	
	adxl345_device_ = (struct adxl345_device *)dev_id;

	client = container_of(adxl345_device_->miscdev.parent, struct i2c_client, dev);
	if (irq != client->irq)
		return IRQ_NONE;

	err = read_register(client,FIFO_STATUS,&h_fifo_elements);
	h_fifo_elements = h_fifo_elements & 0x1F;
	//pr_info("adxl345 called\n");
	//Empty HW FIFO
	while(h_fifo_elements > 0){
		struct adxl345_sample sample;
		struct adxl345_sample discarded;
		err = read_multiple_registers(client, DATAX0,(char*)&sample, 6);
		if(err != 6){
			pr_warn("Failed to read from adxl345 fifo\n");
			h_fifo_elements--;
			continue;
		}
		//pr_info("IRQ Test Read X:%d\tY:%d\tZ:%d\n",sample.X,sample.Y,sample.Z);
		while((err = kfifo_put(&adxl345_device_->samples_fifo, sample)) == 0){
			err = kfifo_get(&adxl345_device_->samples_fifo, &discarded);
			//pr_warn("discarding value X:%d\tY:%d\tZ:%d\n",discarded.X,discarded.Y,discarded.Z);
		}	
		h_fifo_elements--;	
	}

	wake_up(&adxl345_device_->queue);
	return IRQ_HANDLED;
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
int adxl345_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = (struct miscdevice *)file->private_data;
	struct adxl345_file_private *private_data = kmalloc(sizeof(struct adxl345_file_private), GFP_KERNEL);
	if (private_data == NULL)
		return ENOMEM;
	// private_data->axis_address = {DATAX0,DATAX1};
	private_data->axis_address[0] = DATAX0;
	private_data->axis_address[1] = DATAX1;
	private_data->miscdev = miscdev;
	file->private_data = (void *)private_data;
	return 0;
}

ssize_t adxl345_read(struct file *file, char __user *buf, size_t count, loff_t *f_pos)
{
	struct adxl345_file_private *private_data;
	struct miscdevice *miscdev;
	struct i2c_client *client;
	int retval;
	short k_buf = 0;
	// int16_t test_buf[3];
	// read_multiple_registers(client, DATAX0, (char *)&test_buf,6);

	private_data = (struct adxl345_file_private *)file->private_data;
	miscdev = private_data->miscdev;
	client = container_of(miscdev->parent, struct i2c_client, dev);

	if (count == 1)
	{
		retval = read_register(client, private_data->axis_address[0], (char *)&k_buf);
		put_user(((char *)(&k_buf))[0], buf);
	}
	else
	{
		retval = read_multiple_registers(client, private_data->axis_address[0], (char *)&k_buf, 2);
		put_user(k_buf, buf);
	}

	return retval;
}

long adxl345_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct adxl345_file_private *private_data;
	struct miscdevice *miscdev;

	private_data = (struct adxl345_file_private *)file->private_data;
	miscdev = private_data->miscdev;
	switch (cmd)
	{
	case READ_X:
		pr_info("setting x as axis\n");
		private_data->axis_address[0] = DATAX0;
		private_data->axis_address[1] = DATAX1;
		break;
	case READ_Y:
		pr_info("setting y as axis\n");
		private_data->axis_address[0] = DATAY0;
		private_data->axis_address[1] = DATAY1;
		break;
	case READ_Z:
		pr_info("setting z as axis\n");
		private_data->axis_address[0] = DATAZ0;
		private_data->axis_address[1] = DATAZ1;
		break;
	default:
		return -1;
	}
	return 0;
}

int adxl345_release(struct inode *inode, struct file *file)
{
	struct adxl345_file_private *private_data = (struct adxl345_file_private *)file->private_data;
	struct miscdevice *miscdev = private_data->miscdev;
	kfree(private_data);
	// Maybe this step is not needed as at the moment drivers/char/misc.c doesn't define it's own release function
	// However, we are kind guests in the kernel and we like to leave stuffs in same place we found them =)
	file->private_data = miscdev;

	return 0;
}

static int setup_adxl345(struct i2c_client *client)
{
	int transfered;
	unsigned char buf;
	printk("adxl345_probe function called\n");

	// Reading DEVID
	transfered = read_register(client, DEVID, &buf);
	if (transfered < 0)
	{
		pr_warn("Error reading adxl345\n");
		return -1;
	}
	pr_info("DEVID: %x\n", (unsigned int)buf & 0xFF);

	// Setting 100Hz communication speed
	transfered = read_register(client, BW_RATE, &buf);
	if (transfered < 0)
	{
		pr_warn("Error reading adxl345\n");
		return -2;
	}

	buf = (buf & 0xf0) | 0x0a; // Writing code 0xa on lower bits of BW_RATE

	transfered = write_register(client, BW_RATE, &buf);
	if (transfered < 0)
	{
		pr_warn("Error writing adxl345\n");
		return -2;
	}

	// Defaulting DATA_FORMAT register
	buf = (buf & 0x00);
	transfered = write_register(client, DATA_FORMAT, &buf);
	if (transfered < 0)
	{
		pr_warn("Error writing adxl345\n");
		return -4;
	}

	// Setting FIFO STream
	// 		1 0    |      0       	| 0x14
	// 		Stream | Trigger=INT1	| 20 Samples
	buf = (0x2 << 6) | (0 << 5) | (0x14);

	transfered = write_register(client, FIFO_CTL, &buf);
	if (transfered < 0)
	{
		pr_warn("Error writing adxl345\n");
		return -5;
	}

	// Setting Measurement mode
	transfered = read_register(client, POWER_CTL, &buf);
	if (transfered < 0)
	{
		pr_warn("Error reading adxl345\n");
		return -6;
	}

	// Enabling watermark interrupts
	buf = 0x1 << 1;

	transfered = write_register(client, INT_ENABLE, &buf);
	if (transfered < 0)
	{
		pr_warn("Error writing adxl345\n");
		return -3;
	}

	buf = (buf | (0x1 << 3)); // Setting bit 3 (Measurement)

	transfered = write_register(client, POWER_CTL, &buf);
	if (transfered < 0)
	{
		pr_warn("Error writing adxl345\n");
		return -6;
	}
	pr_info("POWER_CTL: %x\n", (unsigned int)buf & 0xFF);
	pr_info("Set up of the device completed\n");
	return 0;
}

static int read_multiple_registers(struct i2c_client *client, adxl345_register_t base_address, unsigned char *data, size_t num)
{
	int transfered;
	unsigned char add_buf = base_address;
	transfered = i2c_master_send(client, &add_buf, 1);
	if (transfered != 1)
		return -1;

	return i2c_master_recv(client, data, num);
}

static int read_register(struct i2c_client *client, adxl345_register_t address, unsigned char *data)
{
	int transfered;
	unsigned char add_buf = address;
	transfered = i2c_master_send(client, &add_buf, 1);
	if (transfered != 1)
	{
		return -1;
	}
	transfered = i2c_master_recv(client, data, 1);
	if (transfered != 1)
	{
		return -2;
	}
	return transfered;
}

static int write_register(struct i2c_client *client, adxl345_register_t address, unsigned char *data)
{
	int transfered;
	char buf[2];
	buf[0] = address;
	buf[1] = *data;

	transfered = i2c_master_send(client, buf, 2);
	if (transfered != 2)
	{
		return -1;
	}
	return transfered;
}

int get_new_id(void)
{
	// TODO: This function will have to be made atomic later
	int returned_id;
	if (first_id_available >= MAX_DEVICES)
	{
		returned_id = -1;
	}
	else
	{
		returned_id = first_id_available;
		used_ids[first_id_available] = true;
	}

	while (used_ids[first_id_available] == true || first_id_available >= MAX_DEVICES)
		first_id_available++;
	return returned_id;
}

static void free_id(int id)
{
	// TODO: This function will have to be made atomic later
	used_ids[id] = false;
	if (id < first_id_available)
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