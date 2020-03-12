#ifndef _LORA_DEVICE_H_
#define _LORA_DEVICE_H_

#define REG_FIFO     (0x00)
#define REG_OPMODE   (0x01)
#define REG_FRFMSB   (0x06)
#define REG_FRFMLD   (0x07)
#define REG_FRFLSB   (0x08)
#define REG_PACONFIG (0x09)
#define REG_VERSION  (0x42)

#define REGADDR(x)   (x & 0x7f)
#define WRITEADDR(x) (x | (0x1 << 7))

#define LORAREG_MASK   (0x1 << 8)
#define FSKOOKREG_MASK (0x1 << 9)

#define LORAREG(x)   (x | LORAREG_MASK)
#define FSKOOKREG(x) (x | FSKOOK_MASK)

#define REG_LORA_SYNCWORD           LORAREG(0x39)
#define REG_LORA_INVERTIQ           LORAREG(0x33)
#define REG_LORA_MODEMCONFIG1       LORAREG(0x1d)
#define REG_LORA_MODEMCONFIG2       LORAREG(0x1e)
#define REG_LORA_DETECTOPTIMIZATION LORAREG(0x31)
#define REG_LORA_RXCURRENTADDR      LORAREG(0x10)
#define REG_LORA_IRQFLAGSMSK        LORAREG(0x11)
#define REG_LORA_IRQFLAGS           LORAREG(0x12)
#define REG_LORA_RXNBBYTES          LORAREG(0x13)
#define REG_LORA_FIFOADDRPTR        LORAREG(0x0d)
#define REG_LORA_FIFOTXBASEADDR     LORAREG(0x0e)
#define REG_LORA_PKTSNRVALUE        LORAREG(0x19)
#define REG_LORA_PKTRSSIVALUE       LORAREG(0x1A)
#define REG_LORA_PAYLOADLENGTH      LORAREG(0x22)

#define REG_LORA_IRQFLAGS_CADDETECTED     (0x1 << 0)
#define REG_LORA_IRQFLAGS_CADDONE         (0x1 << 2)
#define REG_LORA_IRQFLAGS_TXDONE          (0x1 << 3)
#define REG_LORA_IRQFLAGS_PAYLOADCRCERROR (0x1 << 5)
#define REG_LORA_IRQFLAGS_RXDONE          (0x1 << 6)

#define REG_LORA_DETECTOPTIMIZATION_DETECTIONOPTIMIZE_MASK (0x7 << 0)

#define REG_LORA_INVERTIQ_INVERTIQ (0x1 << 6)

#define REG_LORA_MODEMCONFIG1_RXPAYLOADCRCON (0x1 << 1)

#define REG_LORA_MODEMCONFIG2_SPREADINGFACTOR_MASK  (0xF << 4)
#define REG_LORA_MODEMCONFIG2_SPREADINGFACTOR_SHIFT (4)

#define REG_OPMODE_LONGRANGEMODE_MASK    (0x1 << 7)
#define REG_OPMODE_LONGRANGEMODE_FSK_OOK (0x0 << 7)
#define REG_OPMODE_LONGRANGEMODE_LORA    (0x1 << 7)

#define REG_OPMODE_MODULATIONTYPE_MASK   (0x3 << 5)
#define REG_OPMODE_MODULATIONTYPE_FSK    (0x0 << 5)
#define REG_OPMODE_MODULATIONTYPE_OOK    (0x1 << 5)

#define REG_OPMODE_MODE_MASK         (0x7 << 0)
#define REG_OPMODE_MODE_SLEEP        (0x0 << 0)
#define REG_OPMODE_MODE_STANDBY      (0x1 << 0)
#define REG_OPMODE_MODE_FSTX         (0x2 << 0)
#define REG_OPMODE_MODE_TX           (0x3 << 0)
#define REG_OPMODE_MODE_FSRX         (0x4 << 0)
#define REG_OPMODE_MODE_RX           (0x5 << 0)
#define REG_OPMODE_MODE_RXCONTINUOUS (0x5 << 0)
#define REG_OPMODE_MODE_RXSINGLE     (0x6 << 0)
#define REG_OPMODE_MODE_CAD          (0x7 << 0)

#define REG_PACONFIG_PASELECT (0x1 << 7)

int sx127x_reg_read(struct spi_device *spi, u16 reg, u8 *result);

int sx127x_reg_read16(struct spi_device *spi, u16 reg, u16* result);

int sx127x_reg_read24(struct spi_device *spi, u16 reg, u32* result);

int sx127x_reg_write(struct spi_device *spi, u16 reg, u8 value);

int sx127x_reg_write24(struct spi_device *spi, u16 reg, u32 value);

enum sx127x_modulation sx127x_getmodulation(struct sx127x *data);

enum sx127x_opmode sx127x_getopmode(struct sx127x *data);

int sx127x_getchipversion(struct sx127x *data, u8 *version);

int sx127x_setopmode(struct sx127x *data, enum sx127x_opmode opmode);

int sx127x_setmodulation(struct sx127x *data, enum sx127x_modulation mod);

int sx127x_setsyncword(struct sx127x *data, u8 syncword);

int sx127x_setinvertiq(struct sx127x *data, bool invert);

int sx127x_setcrc(struct sx127x *data, bool crc);

int sx127x_setcarrierfrequency(struct sx127x *data, u64 freq);

int sx127x_setpaoutput(struct sx127x *data, enum sx127x_pa pa);

int sx127x_setsf(struct sx127x *data, u8 sf);

int sx127x_fifo_readpkt(struct spi_device *spi, void *buffer, u8 *len);

int sx127x_fifo_writepkt(struct spi_device *spi, void *buffer, u8 len);

#endif

