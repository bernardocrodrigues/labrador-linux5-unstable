#include "driver.h"
#include "device.h"

static const char* modstr[] = {
	[SX127X_MODULATION_FSK]     = "fsk",
	[SX127X_MODULATION_OOK]     = "ook",
	[SX127X_MODULATION_LORA]    = "lora",
	[SX127X_MODULATION_INVALID] = "invalid",
};

static const char* opmodestr[] = {
	[SX127X_OPMODE_SLEEP]        = "sleep",
	[SX127X_OPMODE_STANDBY]      = "standby",
	[SX127X_OPMODE_FSTX]         = "fstx",
	[SX127X_OPMODE_TX]           = "tx",
	[SX127X_OPMODE_FSRX]         = "fsrx",
	[SX127X_OPMODE_RX]           = "rx",
	[SX127X_OPMODE_RXCONTINUOUS] = "rxcontinuous",
	[SX127X_OPMODE_RXSINGLE]     = "rxsingle",
	[SX127X_OPMODE_CAD]          = "cad",
	[SX127X_OPMODE_INVALID]      = "invalid",
};

int sx127x_reg_read(struct spi_device *spi, u16 reg, u8 *result)
{
	u8 addr = reg & 0xff;
	return spi_write_then_read(spi, &addr, 1, result, 1);
}

int sx127x_reg_read16(struct spi_device *spi, u16 reg, u16* result)
{
	u8 addr = reg & 0xff;
	return spi_write_then_read(spi, &addr, 1, result, 2);
}

int sx127x_reg_read24(struct spi_device *spi, u16 reg, u32* result)
{
	u8 addr = reg & 0xff, buf[3];
	int ret = spi_write_then_read(spi, &addr, 1, buf, 3);
	
	if (ret) {
		*result = 0;
	}
	else {
		*result = (buf[0] << 16) | (buf[1] << 8) | buf[0];
	}
	
	return ret;
}

int sx127x_reg_write(struct spi_device *spi, u16 reg, u8 value)
{
	u8 addr = REGADDR(reg), buff[2];
	
	buff[0] = WRITEADDR(addr);
	buff[1] = value;
	
	return spi_write(spi, buff, 2);
}

int sx127x_reg_write24(struct spi_device *spi, u16 reg, u32 value)
{
	u8 addr = REGADDR(reg), buff[4];
	
	buff[0] = WRITEADDR(addr);
	buff[1] = (value >> 16) & 0xff;
	buff[2] = (value >> 8) & 0xff;
	buff[3] = value & 0xff;
	
	return spi_write(spi, buff, sizeof(buff));
}

int sx127x_getchipversion(struct sx127x *data, u8 *version)
{
	int ret;
	
	if (!data || !version) {
		return -EINVAL;
	}
	
	ret = sx127x_reg_read(data->spidevice, REG_VERSION, version);
	
	if (ret) {
		return ret;
	}
	
	return 0;
}

enum sx127x_modulation sx127x_getmodulation(struct sx127x *data)
{
	u8 val, opmode_reg;
	
	if (!data) {
		return SX127X_MODULATION_INVALID;
	}
	
	if (sx127x_reg_read(data->spidevice, REG_OPMODE, &opmode_reg)) {
		return SX127X_MODULATION_INVALID;
	}
	
	val = opmode_reg & REG_OPMODE_LONGRANGEMODE_MASK;
	
	if (val == REG_OPMODE_LONGRANGEMODE_LORA) {
		return SX127X_MODULATION_LORA;
	}
	
	val = opmode_reg & REG_OPMODE_MODULATIONTYPE_MASK;
	
	if (val == REG_OPMODE_MODULATIONTYPE_FSK) {
		return SX127X_MODULATION_FSK;
	}
	if (val == REG_OPMODE_MODULATIONTYPE_OOK) {
		return SX127X_MODULATION_OOK;
	}
	
	return SX127X_MODULATION_INVALID;
}

enum sx127x_opmode sx127x_getopmode(struct sx127x *data)
{
	enum sx127x_modulation curr_mod;
	u8 opmode_reg;
	
	if (!data) {
		return SX127X_OPMODE_INVALID;
	}
	
	curr_mod = sx127x_getmodulation(data);
	
	if (curr_mod == SX127X_MODULATION_INVALID) {
		return -EINVAL;
	} 
	
	if (sx127x_reg_read(data->spidevice, REG_OPMODE, &opmode_reg)) {
		return SX127X_OPMODE_INVALID;
	}
	
	opmode_reg &= REG_OPMODE_MODE_MASK;
	
	switch(opmode_reg)
	{
	case REG_OPMODE_MODE_SLEEP:
		return SX127X_OPMODE_SLEEP;
	case REG_OPMODE_MODE_STANDBY:
		return SX127X_OPMODE_STANDBY;
	case REG_OPMODE_MODE_FSTX:
		return SX127X_OPMODE_FSTX;
	case REG_OPMODE_MODE_TX:
		return SX127X_OPMODE_TX;
	case REG_OPMODE_MODE_FSRX:
		return SX127X_OPMODE_FSRX;
	}
	
	if (curr_mod == SX127X_MODULATION_LORA)
	{
		if (opmode_reg == REG_OPMODE_MODE_RXCONTINUOUS) {
			return SX127X_OPMODE_RXCONTINUOUS;
		}
		else if (opmode_reg == REG_OPMODE_MODE_RXSINGLE) {
			return SX127X_OPMODE_RXSINGLE;
		}
		else if (opmode_reg == REG_OPMODE_MODE_CAD) {
			return SX127X_OPMODE_CAD;
		}
	}
	else if (opmode_reg == REG_OPMODE_MODE_RX) {
		return SX127X_OPMODE_RX;
	}
	
	return SX127X_OPMODE_INVALID;
}

int sx127x_setopmode(struct sx127x *data, enum sx127x_opmode opmode)
{
	enum sx127x_modulation curr_mod;
	enum sx127x_opmode curr_opmode;
	u8 opmode_reg;
	int ret;
	
	if (!data) {
		return -EINVAL;
	}
	
	if ((opmode < SX127X_OPMODE_SLEEP) || (opmode >= SX127X_OPMODE_INVALID)) {
		return -EINVAL;
	}
	
	curr_opmode = sx127x_getopmode(data);
	
	if (curr_opmode == SX127X_OPMODE_INVALID) {
		return -EINVAL;
	}
	
	curr_mod = sx127x_getmodulation(data);
	
	if (curr_mod == SX127X_MODULATION_INVALID) {
		return -EINVAL;
	}
	
	/* nothing to do */
	if (curr_opmode == opmode) {
		return 0;
	}
	
	if (curr_mod != SX127X_MODULATION_LORA)
	{
		if (opmode >= SX127X_OPMODE_RXCONTINUOUS) {
			return -EINVAL;
		}
	}
	else if (opmode == SX127X_OPMODE_RX) {
		return -EINVAL;
	}
	
	dev_info(data->chardevice, "setting opmode to %s\n", opmodestr[opmode]);
	
	ret = sx127x_reg_read(data->spidevice, REG_OPMODE, &opmode_reg);
	
	if (ret) {
		return ret;
	}
	
	opmode_reg &= ~REG_OPMODE_MODE_MASK;
	
	switch(opmode)
	{
	case SX127X_OPMODE_SLEEP:
		opmode_reg |= REG_OPMODE_MODE_SLEEP;
		break;
		
	case SX127X_OPMODE_STANDBY:
		opmode_reg |= REG_OPMODE_MODE_STANDBY;
		break;
		
	case SX127X_OPMODE_FSTX:
		opmode_reg |= REG_OPMODE_MODE_FSTX;
		break;
		
	case SX127X_OPMODE_TX:
		opmode_reg |= REG_OPMODE_MODE_TX;
		break;
		
	case SX127X_OPMODE_FSRX:
		opmode_reg |= REG_OPMODE_MODE_FSRX;
		break;
		
	case SX127X_OPMODE_RX:
		opmode_reg |= REG_OPMODE_MODE_RX;
		break;
		
	case SX127X_OPMODE_RXSINGLE:
		opmode_reg |= REG_OPMODE_MODE_RXSINGLE;
		break;
		
	case SX127X_OPMODE_CAD:
		opmode_reg |= REG_OPMODE_MODE_CAD;
		break;
		
	default:
		return -EINVAL;
	}
	
	return sx127x_reg_write(data->spidevice, REG_OPMODE, opmode_reg);
}

int sx127x_setmodulation(struct sx127x *data, enum sx127x_modulation mod)
{
	enum sx127x_modulation curr_mod;
	enum sx127x_opmode curr_opmode;
	bool loramode;
	u8 opmode_reg;
	int ret;
	
	if (!data) {
		return -EINVAL;
	}
	
	if ((mod < SX127X_MODULATION_FSK) || (mod >= SX127X_MODULATION_INVALID)) {
		return -EINVAL;
	}
	
	curr_opmode = sx127x_getopmode(data);
	
	if (curr_opmode == SX127X_OPMODE_INVALID) {
		return -EINVAL;
	}
	
	curr_mod = sx127x_getmodulation(data);
	
	if (curr_mod == SX127X_MODULATION_INVALID) {
		return -EINVAL;
	}
	
	/* nothing to do */
	if (mod == curr_mod) {
		return 0;
	}
	
	loramode = (curr_mod == SX127X_MODULATION_LORA);
	loramode = loramode || (mod == SX127X_MODULATION_LORA);
	
	/* LoRa mode bit can only be changed in sleep mode */
	if (loramode && (curr_opmode != SX127X_OPMODE_SLEEP)) {
		return -EINVAL;
	}
	
	dev_info(data->chardevice, "setting modulation to %s\n", modstr[mod]);
	
	ret = sx127x_reg_read(data->spidevice, REG_OPMODE, &opmode_reg);
	
	if (ret) {
		return ret;
	}
	
	opmode_reg &= ~REG_OPMODE_MODULATIONTYPE_MASK;
	
	if (loramode) {
		opmode_reg &= ~REG_OPMODE_LONGRANGEMODE_MASK;
	}
	
	switch(mod)
	{
	case SX127X_MODULATION_FSK:
		if (loramode) {
			opmode_reg |= REG_OPMODE_LONGRANGEMODE_FSK_OOK;
		}
		opmode_reg |= REG_OPMODE_MODULATIONTYPE_FSK;
		data->loraregmap = false;
		break;
		
	case SX127X_MODULATION_OOK:
		if (loramode) {
			opmode_reg |= REG_OPMODE_LONGRANGEMODE_FSK_OOK;
		}
		opmode_reg |= REG_OPMODE_MODULATIONTYPE_OOK;
		data->loraregmap = false;
		break;
		
	case SX127X_MODULATION_LORA:
		opmode_reg |= REG_OPMODE_LONGRANGEMODE_LORA;
		data->loraregmap = true;
		break;
	
	default:
		return -EINVAL;
	}
	
	return sx127x_reg_write(data->spidevice, REG_OPMODE, opmode_reg);
}

int sx127x_setsyncword(struct sx127x *data, u8 syncword)
{
	if (!data) {
		return -EINVAL;
	}
	
	if (!data->loraregmap) {
		return -EINVAL;
	}
	
	dev_info(data->chardevice, "setting syncword to %d\n", syncword);
	
	return sx127x_reg_write(data->spidevice, REG_LORA_SYNCWORD, syncword);
}

int sx127x_setinvertiq(struct sx127x *data, bool invert)
{
	int ret;
	u8 reg;
	
	if (!data) {
		return -EINVAL;
	}
	
	if (!data->loraregmap) {
		return -EINVAL;
	}
	
	dev_info(data->chardevice, "setting invertiq to %s\n",
	         invert ? "true" : "false");
	
	ret = sx127x_reg_read(data->spidevice, REG_LORA_INVERTIQ, &reg);
	
	if (ret) {
		return ret;
	}
	
	if (reg & REG_LORA_INVERTIQ_INVERTIQ)
	{
		if (invert) {
			return 0;
		}
		reg &= ~REG_LORA_INVERTIQ_INVERTIQ;
	}
	else
	{
		if (!invert) {
			return 0;
		}
		reg |= REG_LORA_INVERTIQ_INVERTIQ;
	}
		
	return sx127x_reg_write(data->spidevice, REG_LORA_INVERTIQ, reg);
}

int sx127x_setcrc(struct sx127x *data, bool crc)
{
	int ret;
	u8 reg;
	
	if (!data) {
		return -EINVAL;
	}
	
	if (!data->loraregmap) {
		return -EINVAL;
	}
	
	dev_info(data->chardevice, "setting crc to %s\n", crc ? "true" : "false");
	
	ret = sx127x_reg_read(data->spidevice, REG_LORA_MODEMCONFIG1, &reg);
	
	if (ret) {
		return ret;
	}
	
	if (reg & REG_LORA_MODEMCONFIG1_RXPAYLOADCRCON)
	{
		if (crc) {
			return 0;
		}
		reg &= ~REG_LORA_MODEMCONFIG1_RXPAYLOADCRCON;
	}
	else 
	{
		if (!crc) {
			return 0;
		}
		reg |= REG_LORA_MODEMCONFIG1_RXPAYLOADCRCON;
	}
	
	return sx127x_reg_write(data->spidevice, REG_LORA_MODEMCONFIG1, reg);
}

int sx127x_setcarrierfrequency(struct sx127x *data, u64 freq)
{
	if (!data) {
		return -EINVAL;
	}
	
	dev_info(data->chardevice, "setting carrier frequency to %llu\n", freq);
	
	freq *= 524288; /* 2^19 */
	
	do_div(freq, data->fosc);
	
	return sx127x_reg_write24(data->spidevice, REG_FRFMSB, freq);
}

int sx127x_setpaoutput(struct sx127x *data, enum sx127x_pa pa)
{
	u8 paconfig_reg;
	int ret;
	
	if (!data) {
		return -EINVAL;
	}
	
	ret = sx127x_reg_read(data->spidevice, REG_PACONFIG, &paconfig_reg);
	
	if (ret) {
		return ret;
	}
	
	switch(pa)
	{
	case SX127X_PA_RFO:
		paconfig_reg &= ~REG_PACONFIG_PASELECT;
		break;
	case SX127X_PA_PABOOST:
		paconfig_reg |= REG_PACONFIG_PASELECT;
		break;
	}
	
	return sx127x_reg_write(data->spidevice, REG_PACONFIG, paconfig_reg);
}

int sx127x_setsf(struct sx127x *data, u8 sf)
{
	int ret;
	u8 r;
	
	if (!data) {
		return -EINVAL;
	}
	
	if ((sf < 6) || (sf > 12)) {
		return -EINVAL;
	}
	
	dev_info(data->chardevice, "setting spreading factor to %u\n", sf);
	
	ret = sx127x_reg_read(data->spidevice, REG_LORA_MODEMCONFIG2, &r);
	
	if (ret) {
		return ret;
	}
	
	r &= ~REG_LORA_MODEMCONFIG2_SPREADINGFACTOR_MASK;
	r |= sf << REG_LORA_MODEMCONFIG2_SPREADINGFACTOR_SHIFT;
	
	ret = sx127x_reg_write(data->spidevice, REG_LORA_MODEMCONFIG2, r);
	
	if (ret) {
		return ret;
	}
	
	ret = sx127x_reg_read(data->spidevice, REG_LORA_DETECTOPTIMIZATION, &r);
	
	if (ret) {
		return ret;
	}
	
	r &= ~REG_LORA_DETECTOPTIMIZATION_DETECTIONOPTIMIZE_MASK;
	
	if(sf == 6){
		r |= 0x5;
	}
	else{
		r |= 0x3;
	}
	
	return sx127x_reg_write(data->spidevice, REG_LORA_DETECTOPTIMIZATION, r);
}

int sx127x_fifo_readpkt(struct spi_device *spi, void *buffer, u8 *len)
{
	u8 addr = REG_FIFO, pktstart, rxbytes, off, fifoaddr;
	size_t maxtransfer = spi_max_transfer_size(spi);
	unsigned readlen;
	int ret;
	
	ret = sx127x_reg_read(spi, REG_LORA_RXCURRENTADDR, &pktstart);
	
	if (ret) {
		return ret;
	}
	
	ret = sx127x_reg_read(spi, REG_LORA_RXNBBYTES, &rxbytes);
	
	if (ret) {
		return ret;
	}
	
	for(off = 0; off < rxbytes; off += maxtransfer)
	{
		readlen = min(maxtransfer, (size_t) (rxbytes - off));
		fifoaddr = pktstart + off;
		
		ret = sx127x_reg_write(spi, REG_LORA_FIFOADDRPTR, fifoaddr);
		
		if (ret) {
			break;
		}
		
		ret = spi_write_then_read(spi, &addr, 1, buffer + off, readlen);
		
		if (ret) {
			break;
		}
	}
	*len = rxbytes;
	return ret;
}

int sx127x_fifo_writepkt(struct spi_device *spi, void *buffer, u8 len)
{
	u8 addr = WRITEADDR(REGADDR(REG_FIFO));
	int ret;
	
	struct spi_transfer fifotransfers[] = {
			{.tx_buf = &addr, .len = 1},
			{.tx_buf = buffer, .len = len},
	};
	
	u8 readbackaddr = REGADDR(REG_FIFO);
	u8 readbackbuff[256];
	
	struct spi_transfer readbacktransfers[] = {
			{.tx_buf = &readbackaddr, .len = 1},
			{.rx_buf = &readbackbuff, .len = len},
	};
	
	ret = sx127x_reg_write(spi, REG_LORA_FIFOTXBASEADDR, 0);
	
	if (ret) {
		return ret;
	}
	
	ret = sx127x_reg_write(spi, REG_LORA_FIFOADDRPTR, 0);
	
	if (ret) {
		return ret;
	}
	
	ret = sx127x_reg_write(spi, REG_LORA_PAYLOADLENGTH, len);
	
	if (ret) {
		return ret;
	}

	spi_sync_transfer(spi, fifotransfers, ARRAY_SIZE(fifotransfers));
	
	ret = sx127x_reg_write(spi, REG_LORA_FIFOADDRPTR, 0);
	
	spi_sync_transfer(spi, readbacktransfers, ARRAY_SIZE(readbacktransfers));
	
	if(memcmp(buffer, readbackbuff, len) != 0){
		dev_err(&spi->dev, "fifo readback doesn't match\n");
	}
	
	return ret;
}

