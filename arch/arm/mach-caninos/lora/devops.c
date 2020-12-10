#include "driver.h"
#include "device.h"

extern struct list_head device_list;
extern struct mutex device_list_lock;

static int sx127x_dev_open(struct inode *inode, struct file *file)
{
	struct sx127x *data;
	int status = -ENXIO;
	int minor = iminor(inode);
	
	mutex_lock(&device_list_lock);
	
	list_for_each_entry(data, &device_list, device_entry)
	{
		if (data->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}
	
	if(status)
	{
		dev_err(data->chardevice, "no device file for minor %d.\n", minor);
		mutex_unlock(&device_list_lock);
		return status;
	}
	
	mutex_lock(&data->mutex);
	
	if(data->open)
	{
		dev_err(data->chardevice, "device file already open.\n");
		mutex_unlock(&data->mutex);
		mutex_unlock(&device_list_lock);
		return -EBUSY;
	}
	
	data->open = true;
	mutex_unlock(&data->mutex);
	mutex_unlock(&device_list_lock);
	file->private_data = data;
	return 0;
}

static ssize_t sx127x_dev_read
	(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct sx127x *data = filp->private_data;
	unsigned copied;
	ssize_t ret = 0;
	bool receive;
	
	mutex_lock(&data->mutex);
	receive = (data->opmode == SX127X_OPMODE_RXCONTINUOUS);
	receive = receive || (data->opmode == SX127X_OPMODE_RXSINGLE);
	
	if (!receive)
	{
		dev_err(data->chardevice, "device is not in receive mode.\n");
		mutex_unlock(&data->mutex);
		return -EINVAL;
	}
	
	sx127x_reg_write(data->spidevice, REG_LORA_IRQFLAGS, 0xff);
	
	sx127x_reg_write(data->spidevice, REG_LORA_IRQFLAGSMSK, 
		~(REG_LORA_IRQFLAGS_RXDONE | REG_LORA_IRQFLAGS_PAYLOADCRCERROR));
	
	mutex_unlock(&data->mutex);
	
	schedule_delayed_work(&data->rx_work, usecs_to_jiffies(POOLING_DELAY));
	wait_event_interruptible(data->readwq, kfifo_len(&data->out));
	
	ret = kfifo_to_user(&data->out, buf, count, &copied);
	
	if(!ret && copied > 0){
		ret = copied;
	}
	mutex_lock(&data->mutex);
	data->opmode = sx127x_getopmode(data);
	mutex_unlock(&data->mutex);
	return ret;
}

static ssize_t sx127x_dev_write
	(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	struct sx127x *data = filp->private_data;
	size_t packetsz, offset, maxpkt = 256;
	unsigned long n;
	bool transmit;
	u8 kbuf[256];
	
	mutex_lock(&data->mutex);
	transmit = (data->opmode == SX127X_OPMODE_TX);
	mutex_unlock(&data->mutex);
	
	if (!transmit)
	{
		dev_err(data->chardevice, "device is not in transmit mode.\n");
		return -EINVAL;
	}
	
	for(offset = 0; offset < count; offset += maxpkt)
	{
		packetsz = min((count - offset), maxpkt);
		
		mutex_lock(&data->mutex);
		n = copy_from_user(kbuf, buf + offset, packetsz);
		
		sx127x_setopmode(data, SX127X_OPMODE_STANDBY);
		sx127x_fifo_writepkt(data->spidevice, kbuf, packetsz);
		
		data->transmitted = false;
		sx127x_setopmode(data, data->opmode);
		
		sx127x_reg_write(data->spidevice, REG_LORA_IRQFLAGS, 0xff);
		
		sx127x_reg_write(data->spidevice, REG_LORA_IRQFLAGSMSK, 
			~(REG_LORA_IRQFLAGS_TXDONE));
		
		mutex_unlock(&data->mutex);
		
		schedule_delayed_work(&data->tx_work, usecs_to_jiffies(POOLING_DELAY));
		wait_event_interruptible_timeout(data->writewq, data->transmitted, 60 * HZ);
	}
	mutex_lock(&data->mutex);
	data->opmode = sx127x_getopmode(data);
	mutex_unlock(&data->mutex);
	return count;
}

static int sx127x_dev_release(struct inode *inode, struct file *filp)
{
	struct sx127x *data = filp->private_data;
	
	mutex_lock(&data->mutex);
	
	sx127x_setopmode(data, SX127X_OPMODE_SLEEP);
	data->opmode = SX127X_OPMODE_SLEEP;
	
	data->open = false;
	kfifo_reset(&data->out);
	
	mutex_unlock(&data->mutex);
	return 0;
}

static long sx127x_dev_ioctl
	(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct sx127x *data = filp->private_data;
	enum sx127x_ioctl_cmd ioctlcmd = cmd;
	int ret;
	
	mutex_lock(&data->mutex);
	
	switch(ioctlcmd)
	{
		case SX127X_IOCTL_CMD_SETMODULATION:
			ret = sx127x_setmodulation(data, arg);
			break;
		case SX127X_IOCTL_CMD_GETMODULATION:
			ret = 0;
			break;
		case SX127X_IOCTL_CMD_SETCARRIERFREQUENCY:
			ret = sx127x_setcarrierfrequency(data, arg);
			break;
		case SX127X_IOCTL_CMD_GETCARRIERFREQUENCY:
			ret = 0;
			break;
		case SX127X_IOCTL_CMD_SETSF:
			ret = sx127x_setsf(data, arg);
			break;
		case SX127X_IOCTL_CMD_GETSF:
			ret = 0;
			break;
		case SX127X_IOCTL_CMD_SETOPMODE:
			ret = sx127x_setopmode(data, arg);
			if (!ret) {
				data->opmode = arg;
			}
			break;
		case SX127X_IOCTL_CMD_GETOPMODE:
			ret = 0;
			break;
		case SX127X_IOCTL_CMD_SETPAOUTPUT:
			ret = sx127x_setpaoutput(data, arg);
			break;
		case SX127X_IOCTL_CMD_GETPAOUTPUT:
			ret = 0;
			break;
		case SX127X_IOCTL_CMD_SETSYNCWORD:
			ret = sx127x_setsyncword(data, arg & 0xff);
			break;
		case SX127X_IOCTL_CMD_GETSYNCWORD:
			ret = 0;
			break;
		case SX127X_IOCTL_CMD_SETCRC:
			ret = sx127x_setcrc(data, arg & 0x1);
			break;
		case SX127X_IOCTL_CMD_GETCRC:
			ret = 0;
			break;
		case SX127X_IOCTL_CMD_SETINVERTIQ:
			ret = sx127x_setinvertiq(data, arg & 0x1);
			break;
		case SX127X_IOCTL_CMD_GETINVERTIQ:
			ret = 0;
			break;
		default:
			ret = -EINVAL;
			break;
	}
	mutex_unlock(&data->mutex);
	return ret;
}

struct file_operations fops = {
	.open = sx127x_dev_open,
	.read = sx127x_dev_read,
	.write = sx127x_dev_write,
	.release = sx127x_dev_release,
	.unlocked_ioctl = sx127x_dev_ioctl
};

