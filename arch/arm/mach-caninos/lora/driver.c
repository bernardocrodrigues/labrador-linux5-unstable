#include "driver.h"
#include "device.h"

extern int reset_lora_module(void); /* from board.c */
extern struct file_operations fops; /* from devops.c */

static int devmajor;
static struct class *devclass;

LIST_HEAD(device_list);
DEFINE_MUTEX(device_list_lock);

static void sx127x_tx_work_handler(struct work_struct *work)
{
	struct delayed_work *mywork = to_delayed_work(work);
	struct sx127x *data = container_of(mywork, struct sx127x, tx_work);
	u8 irqflags_reg;
	
	mutex_lock(&data->mutex);
	
	sx127x_reg_read(data->spidevice, REG_LORA_IRQFLAGS, &irqflags_reg);
	
	if(irqflags_reg & REG_LORA_IRQFLAGS_TXDONE)
	{
		sx127x_reg_write(data->spidevice, REG_LORA_IRQFLAGSMSK, 0xff);
		sx127x_reg_write(data->spidevice, REG_LORA_IRQFLAGS, 0xff);
		
		data->transmitted = true;
		wake_up(&data->writewq);
	}
	else {
		schedule_delayed_work(&data->tx_work, usecs_to_jiffies(POOLING_DELAY));
	}
	
	mutex_unlock(&data->mutex);
}

static void sx127x_rx_work_handler(struct work_struct *work)
{
	struct delayed_work *mywork = to_delayed_work(work);
	struct sx127x *data = container_of(mywork, struct sx127x, rx_work);
	u8 irqflags_reg, buf[128], len, snr, rssi;
	struct sx127x_pkt pkt;
	
	mutex_lock(&data->mutex);
	
	sx127x_reg_read(data->spidevice, REG_LORA_IRQFLAGS, &irqflags_reg);
	
	if((irqflags_reg & REG_LORA_IRQFLAGS_RXDONE) || (irqflags_reg & REG_LORA_IRQFLAGS_PAYLOADCRCERROR))
	{
		dev_info(data->chardevice, "rx interrupt received\n");
		
		sx127x_reg_write(data->spidevice, REG_LORA_IRQFLAGSMSK, 0xff);
		sx127x_reg_write(data->spidevice, REG_LORA_IRQFLAGS, 0xff);
		
		memset(&pkt, 0, sizeof(pkt));
		
		sx127x_fifo_readpkt(data->spidevice, buf, &len);
		
		sx127x_reg_read(data->spidevice, REG_LORA_PKTSNRVALUE, &snr);
		sx127x_reg_read(data->spidevice, REG_LORA_PKTRSSIVALUE, &rssi);
		
		pkt.hdrlen = sizeof(pkt);
		pkt.payloadlen = len;
		pkt.len = pkt.hdrlen + pkt.payloadlen;
		pkt.snr = (__s16)(snr << 2) / 4 ;
		pkt.rssi = -157 + rssi;
		
		if(irqflags_reg & REG_LORA_IRQFLAGS_PAYLOADCRCERROR) {
			pkt.crcfail = 1;
		}
		
		kfifo_in(&data->out, &pkt, sizeof(pkt));
		kfifo_in(&data->out, buf, len);
		wake_up(&data->readwq);
	}
	else {
		schedule_delayed_work(&data->rx_work, usecs_to_jiffies(POOLING_DELAY));
	}
	
	mutex_unlock(&data->mutex);
}

static void sx127x_cad_work_handler(struct work_struct *work)
{
	struct delayed_work *mywork = to_delayed_work(work);
	struct sx127x *data = container_of(mywork, struct sx127x, cad_work);
	u8 irqflags_reg;
	
	mutex_lock(&data->mutex);
	
	sx127x_reg_read(data->spidevice, REG_LORA_IRQFLAGS, &irqflags_reg);
	
	if(irqflags_reg & REG_LORA_IRQFLAGS_CADDONE)
	{
		sx127x_reg_write(data->spidevice, REG_LORA_IRQFLAGSMSK, 0xff);
		sx127x_reg_write(data->spidevice, REG_LORA_IRQFLAGS, 0xff);
		
		if(irqflags_reg & REG_LORA_IRQFLAGS_CADDETECTED){
			dev_info(data->chardevice, "CAD done, detected activity\n");
		}
		else {
			dev_info(data->chardevice, "CAD done, nothing detected\n");
		}
	}
	else {
		schedule_delayed_work(&data->cad_work, usecs_to_jiffies(POOLING_DELAY));
	}
	
	mutex_unlock(&data->mutex);
}

static int sx127x_probe(struct spi_device *spi)
{
	struct sx127x *data;
	unsigned minor;
	u8 version;
	
	data = devm_kzalloc(&spi->dev, sizeof(*data), GFP_KERNEL);
	
	if(!data)
	{
		dev_err(&spi->dev, "could not allocate driver private data.\n");
		return -ENOMEM;
	}
	
	spi_set_drvdata(spi, data);
	
	INIT_DELAYED_WORK(&data->tx_work, sx127x_tx_work_handler);
	INIT_DELAYED_WORK(&data->rx_work, sx127x_rx_work_handler);
	INIT_DELAYED_WORK(&data->cad_work, sx127x_cad_work_handler);
	
	INIT_LIST_HEAD(&data->device_entry);
	
	init_waitqueue_head(&data->readwq);
	init_waitqueue_head(&data->writewq);
	
	mutex_init(&data->mutex);
	
	data->fosc = 32000000; /* todo: parse fosc from DTS */
	data->spidevice = spi;
	data->open = false;
	
	if (kfifo_alloc(&data->out, PAGE_SIZE, GFP_KERNEL))
	{
		dev_err(&spi->dev, "could not allocate data out fifo.\n");
		return -ENOMEM;
	}
	
	if (reset_lora_module())
	{
		dev_err(&spi->dev, "could not reset lora module.\n");
		kfifo_free(&data->out);
		return -ENODEV;
	}
	
	if (sx127x_getchipversion(data, &version))
	{
		dev_err(&spi->dev, "could not read chip version.\n");
		kfifo_free(&data->out);
		return -ENODEV;
	}
	
	/* check chip version (must be 0x22) */
	if(version != 0x22)
	{
		dev_err(&spi->dev, "unknown chip version 0x%x\n", version);
		kfifo_free(&data->out);
		return -EINVAL;
	}
	
	if(sx127x_setopmode(data, SX127X_OPMODE_SLEEP))
	{
		dev_err(&spi->dev, "could not set opmode to sleep.\n");
		kfifo_free(&data->out);
		return -EINVAL;
	}
	
	data->opmode = SX127X_OPMODE_SLEEP;
	
	
	
	
	mutex_lock(&device_list_lock);
	minor = 0;
	
	data->devt = MKDEV(devmajor, minor);
	data->chardevice = device_create(devclass, &spi->dev, data->devt, 
	                                 data, SX127X_DEVICENAME, minor);
	
	if(IS_ERR(data->chardevice))
	{
		dev_err(&spi->dev, "could not create char device.\n");
		mutex_unlock(&device_list_lock);
		kfifo_free(&data->out);
		return -ENOMEM;
	}
	
	list_add(&data->device_entry, &device_list);
	mutex_unlock(&device_list_lock);
	
	// setup sysfs nodes
	//device_create_file(data->chardevice, &dev_attr_modulation);
	//device_create_file(data->chardevice, &dev_attr_opmode);
	//device_create_file(data->chardevice, &dev_attr_carrierfrequency);
	//device_create_file(data->chardevice, &dev_attr_rssi);
	//device_create_file(data->chardevice, &dev_attr_paoutput);
	//device_create_file(data->chardevice, &dev_attr_outputpower);
	
	// these are LoRa specifc
	//device_create_file(data->chardevice, &dev_attr_sf);
	//device_create_file(data->chardevice, &dev_attr_bw);
	//device_create_file(data->chardevice, &dev_attr_codingrate);
	//device_create_file(data->chardevice, &dev_attr_implicitheadermodeon);
	
	return 0;
}

static int sx127x_remove(struct spi_device *spi)
{
	struct sx127x *data = spi_get_drvdata(spi);
	
	//device_remove_file(data->chardevice, &dev_attr_modulation);
	//device_remove_file(data->chardevice, &dev_attr_opmode);
	//device_remove_file(data->chardevice, &dev_attr_carrierfrequency);
	//device_remove_file(data->chardevice, &dev_attr_rssi);
	//device_remove_file(data->chardevice, &dev_attr_paoutput);
	//device_remove_file(data->chardevice, &dev_attr_outputpower);
	
	//device_remove_file(data->chardevice, &dev_attr_sf);
	//device_remove_file(data->chardevice, &dev_attr_bw);
	//device_remove_file(data->chardevice, &dev_attr_codingrate);
	//device_remove_file(data->chardevice, &dev_attr_implicitheadermodeon);
	
	device_destroy(devclass, data->devt);
	
	kfifo_free(&data->out);
	
	return 0;
}

static const struct of_device_id sx127x_of_match[] = {
	{ .compatible = "semtech, sx127x", },
	{ },
};

MODULE_DEVICE_TABLE(of, sx127x_of_match);

static struct spi_driver sx127x_driver = {
	.probe  = sx127x_probe,
	.remove = sx127x_remove,
	.driver = {
		.name = SX127X_DRIVERNAME,
		.of_match_table = of_match_ptr(sx127x_of_match),
		.owner = THIS_MODULE,
	},
};

static int __init sx127x_init(void)
{
	int ret;
	ret = register_chrdev(0, SX127X_DRIVERNAME, &fops);
	
	if(ret < 0)
	{
		pr_err("failed to register char device\n");
		goto out;
	}
	
	devmajor = ret;
	devclass = class_create(THIS_MODULE, SX127X_CLASSNAME);
	
	if(!devclass)
	{
		pr_err("failed to register class\n");
		ret = -ENOMEM;
		goto out1;
	}
	
	ret = spi_register_driver(&sx127x_driver);
	
	if(ret)
	{
		pr_err("failed to register spi driver\n");
		goto out2;
	}
	
	goto out;
	
	out2:
	out1:
		class_destroy(devclass);
		devclass = NULL;
	out:
	return ret;
}

module_init(sx127x_init);

static void __exit sx127x_exit(void)
{
	spi_unregister_driver(&sx127x_driver);
	unregister_chrdev(devmajor, SX127X_DRIVERNAME);
	class_destroy(devclass);
	devclass = NULL;
}

module_exit(sx127x_exit);

MODULE_LICENSE("GPL");

