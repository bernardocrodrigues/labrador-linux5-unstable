#include "driver.h"
#include "device.h"

static ssize_t sx127x_modulation_show
	(struct device *child, struct device_attribute *attr, char *buf)
{
	struct sx127x *data = dev_get_drvdata(child);
	enum sx127x_modulation mod;
	int ret;
	
	mutex_lock(&data->mutex);
	
	mod = sx127x_getmodulation(data);
	ret = sprintf(buf, "%s\n", modstr[mod]);
	
	mutex_unlock(&data->mutex);
	return ret;
}

static int sx127x_indexofstring(const char* str, const char** options, unsigned noptions)
{
	int i;
	for (i = 0; i < noptions; i++) {
		if (sysfs_streq(str, options[i])) {
			return i;
		}
	}
	return -1;
}

static ssize_t sx127x_modulation_store
	(struct device *dev, struct device_attribute *attr, const char *buf, size_t count){
	struct sx127x *data = dev_get_drvdata(dev);
	int idx = sx127x_indexofstring(buf, modstr, ARRAY_SIZE(modstr));
	if(idx == -1){
		dev_warn(dev, "invalid modulation type\n");
		goto out;
	}
	mutex_lock(&data->mutex);
	sx127x_setmodulation(data, idx);
	mutex_unlock(&data->mutex);
out:
	return count;
}

static DEVICE_ATTR(modulation, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, sx127x_modulation_show, sx127x_modulation_store);

static ssize_t sx127x_opmode_show(struct device *child, struct device_attribute *attr, char *buf)
{
	struct sx127x *data = dev_get_drvdata(child);
	u8 opmode, mode;
	int ret;
	mutex_lock(&data->mutex);
	sx127x_reg_read(data->spidevice, SX127X_REG_OPMODE, &opmode);
	mode = opmode & SX127X_REG_OPMODE_MODE;
	if(mode > 4 && (opmode & SX127X_REG_OPMODE_LONGRANGEMODE)){
		ret = sprintf(buf, "%s\n", opmodestr[mode + 1]);
	}
	else {
		ret =sprintf(buf, "%s\n", opmodestr[mode]);
	}
	mutex_unlock(&data->mutex);
	return ret;
}

static ssize_t sx127x_opmode_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count){
	struct sx127x *data = dev_get_drvdata(dev);
	int idx;
	enum sx127x_opmode mode;

	idx = sx127x_indexofstring(buf, opmodestr, ARRAY_SIZE(opmodestr));
	if(idx == -1){
		dev_err(dev, "invalid opmode\n");
		goto out;
	}
	mutex_lock(&data->mutex);
	mode = idx;
	sx127x_setopmode(data, mode);
	mutex_unlock(&data->mutex);
out:
	return count;
}

static DEVICE_ATTR(opmode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, sx127x_opmode_show, sx127x_opmode_store);

static ssize_t sx127x_carrierfrequency_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sx127x *data = dev_get_drvdata(dev);
	u8 msb, mld, lsb;
	u32 frf;
	u32 freq;
	mutex_lock(&data->mutex);
	sx127x_reg_read(data->spidevice, SX127X_REG_FRFMSB, &msb);
	sx127x_reg_read(data->spidevice, SX127X_REG_FRFMLD, &mld);
	sx127x_reg_read(data->spidevice, SX127X_REG_FRFLSB, &lsb);
	frf = (msb << 16) | (mld << 8) | lsb;
	freq = ((u64)data->fosc * frf) / 524288;
	mutex_unlock(&data->mutex);
    return sprintf(buf, "%u\n", freq);
}

static ssize_t sx127x_carrierfrequency_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count){
	struct sx127x *data = dev_get_drvdata(dev);
	u64 freq;

	if(kstrtou64(buf, 10, &freq)){
		goto out;
	}
	mutex_lock(&data->mutex);
	sx127x_setcarrierfrequency(data, freq);
	mutex_unlock(&data->mutex);
	out:
	return count;
}

static DEVICE_ATTR(carrierfrequency, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, sx127x_carrierfrequency_show, sx127x_carrierfrequency_store);

static ssize_t sx127x_rssi_show(struct device *child, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", 0);
}

static DEVICE_ATTR(rssi, S_IRUSR | S_IRGRP | S_IROTH, sx127x_rssi_show, NULL);

static ssize_t sx127x_sf_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sx127x *data = dev_get_drvdata(dev);
	u8 config2;
	int sf;
	mutex_lock(&data->mutex);
	sx127x_reg_read(data->spidevice, SX127X_REG_LORA_MODEMCONFIG2, &config2);
	sf = config2 >> SX127X_REG_LORA_MODEMCONFIG2_SPREADINGFACTOR_SHIFT;
	mutex_unlock(&data->mutex);
    return sprintf(buf, "%d\n", sf);
}

static ssize_t sx127x_sf_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count){
	struct sx127x *data = dev_get_drvdata(dev);
	int sf;
	if(kstrtoint(buf, 10, &sf)){
		goto out;
	}
	mutex_lock(&data->mutex);
	sx127x_setsf(data, sf);
	mutex_unlock(&data->mutex);
	out:
	return count;
}

static DEVICE_ATTR(sf, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, sx127x_sf_show, sx127x_sf_store);

static ssize_t sx127x_bw_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sx127x *data = dev_get_drvdata(dev);
	u8 config1;
	int bw, ret;

	mutex_lock(&data->mutex);
	sx127x_reg_read(data->spidevice, SX127X_REG_LORA_MODEMCONFIG1, &config1);
	bw = config1 >> SX127X_REG_LORA_MODEMCONFIG1_BW_SHIFT;
	if(bw > SX127X_REG_LORA_MODEMCONFIG1_BW_MAX){
		ret = sprintf(buf, "invalid\n");
	}
	else {
		ret = sprintf(buf, "%d\n", bwmap[bw]);
	}
	mutex_unlock(&data->mutex);
	return ret;
}

static ssize_t sx127x_bw_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count){
	struct sx127x *data = dev_get_drvdata(dev);
	return count;
}

static DEVICE_ATTR(bw, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, sx127x_bw_show, sx127x_bw_store);

static char* crmap[] = { NULL, "4/5", "4/6", "4/7", "4/8" };

static ssize_t sx127x_codingrate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sx127x *data = dev_get_drvdata(dev);
	u8 config1;
	int cr, ret;

	mutex_lock(&data->mutex);
	sx127x_reg_read(data->spidevice, SX127X_REG_LORA_MODEMCONFIG1, &config1);
	cr = (config1 & SX127X_REG_LORA_MODEMCONFIG1_CODINGRATE) >> SX127X_REG_LORA_MODEMCONFIG1_CODINGRATE_SHIFT;
	if(cr < SX127X_REG_LORA_MODEMCONFIG1_CODINGRATE_MIN ||
			cr > SX127X_REG_LORA_MODEMCONFIG1_CODINGRATE_MAX){
		ret = sprintf(buf, "invalid\n");
	}
	else {
		ret = sprintf(buf, "%s\n", crmap[cr]);
	}
	mutex_unlock(&data->mutex);
    return ret;
}

static ssize_t sx127x_codingrate_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count){
	struct sx127x *data = dev_get_drvdata(dev);
	return count;
}

static DEVICE_ATTR(codingrate, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, sx127x_codingrate_show, sx127x_codingrate_store);

static ssize_t sx127x_implicitheadermodeon_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sx127x *data = dev_get_drvdata(dev);
	u8 config1;
	int hdrmodeon;

	mutex_lock(&data->mutex);
	sx127x_reg_read(data->spidevice, SX127X_REG_LORA_MODEMCONFIG1, &config1);
	hdrmodeon = config1 & SX127X_REG_LORA_MODEMCONFIG1_IMPLICITHEADERMODEON;
	mutex_unlock(&data->mutex);
    return sprintf(buf, "%d\n", hdrmodeon);
}

static ssize_t sx127x_implicitheadermodeon_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count){
	struct sx127x *data = dev_get_drvdata(dev);
	return count;
}

static DEVICE_ATTR(implicitheadermodeon, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, sx127x_implicitheadermodeon_show, sx127x_implicitheadermodeon_store);

static ssize_t sx127x_paoutput_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sx127x *data = dev_get_drvdata(dev);
	u8 paconfig;
	int idx;
	mutex_lock(&data->mutex);
	sx127x_reg_read(data->spidevice, SX127X_REG_PACONFIG, &paconfig);
	idx = (paconfig & SX127X_REG_PACONFIG_PASELECT) ? 1 : 0;
	mutex_unlock(&data->mutex);
    return sprintf(buf, "%s\n", paoutput[idx]);
}

static ssize_t sx127x_paoutput_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{ 
	//TODO this needs to take into account non-default values for the "padac".
	struct sx127x *data = dev_get_drvdata(dev);
	int idx = sx127x_indexofstring(buf, paoutput, ARRAY_SIZE(paoutput));
	if(idx == -1)
		goto out;
	mutex_lock(&data->mutex);
	sx127x_setpaoutput(data, idx);
	mutex_unlock(&data->mutex);
out:
	return count;
}

static DEVICE_ATTR(paoutput, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, sx127x_paoutput_show, sx127x_paoutput_store);

static ssize_t sx127x_outputpower_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sx127x *data = dev_get_drvdata(dev);
	u8 paconfig;
	int maxoutputpower = 17;
	int outputpower;
	mutex_lock(&data->mutex);
	sx127x_reg_read(data->spidevice, SX127X_REG_PACONFIG, &paconfig);
	if(!(paconfig & SX127X_REG_PACONFIG_PASELECT)){
		maxoutputpower = ((paconfig & SX127X_REG_PACONFIG_MAXPOWER) >> SX127X_REG_PACONFIG_MAXPOWER_SHIFT);
	}
	outputpower = maxoutputpower - (15 - (paconfig & SX127X_REG_PACONFIG_OUTPUTPOWER));
	mutex_unlock(&data->mutex);
    return sprintf(buf, "%d\n", outputpower);
}

static ssize_t sx127x_outputpower_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count){
	struct sx127x *data = dev_get_drvdata(dev);
	int idx = sx127x_indexofstring(buf, paoutput, ARRAY_SIZE(paoutput));
	u8 paconfig;
	sx127x_reg_read(data->spidevice, SX127X_REG_PACONFIG, &paconfig);
	sx127x_reg_write(data->spidevice, SX127X_REG_PACONFIG, paconfig);
	return count;
}

static DEVICE_ATTR(outputpower, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH,
		sx127x_outputpower_show, sx127x_outputpower_store);
		
