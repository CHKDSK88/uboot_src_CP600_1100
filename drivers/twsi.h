#ifndef _TWSI_H_
#define _TWSI_H_

#define TWSI_BASE_ADDR          0xf1011000

#define TWSI_SLAVE_ADDR         *(volatile unsigned long*)(TWSI_BASE_ADDR + 0x00)
#define TWSI_DATA_REG           *(volatile unsigned long*)(TWSI_BASE_ADDR + 0x04)
#define TWSI_CONTROL_REG        *(volatile unsigned long*)(TWSI_BASE_ADDR + 0x08)
#define BIT_CONTROL_REG_ACK     0x00000004
#define BIT_CONTROL_REG_IFLG    0x00000008
#define BIT_CONTROL_REG_STOP    0x00000010
#define BIT_CONTROL_REG_START   0x00000020
#define BIT_CONTROL_REG_EN      0x00000040
#define BIT_CONTROL_REG_INT_EN  0x00000080
#define CNTL_BITS       (BIT_CONTROL_REG_ACK | BIT_CONTROL_REG_EN)
#define CNTL_BITS_INT   (BIT_CONTROL_REG_ACK | BIT_CONTROL_REG_EN | BIT_CONTROL_REG_INT_EN)

#define TWSI_BAUD_RATE          *(volatile unsigned long*)(TWSI_BASE_ADDR + 0x0c)
#define TWSI_STATUS_REG         *(volatile unsigned long*)(TWSI_BASE_ADDR + 0x0c)
#define TWSI_EXT_SLAVE_ADDR     *(volatile unsigned long*)(TWSI_BASE_ADDR + 0x10)
#define TWSI_SOFT_RESET         *(volatile unsigned long*)(TWSI_BASE_ADDR + 0x1c)

#define TWSI_WRITE              0
#define TWSI_READ               1

/*
 *  twsi status list
 */
#define	TWSI_STATUS_BUS_ERR                 0x00
#define	TWSI_STATUS_MAST_START			    0x08
#define	TWSI_STATUS_MAST_REPEAT_START		0x10
#define	TWSI_STATUS_MAST_WR_ADDR_ACK		0x18
#define	TWSI_STATUS_MAST_WR_ADDR_NO_ACK		0x20
#define	TWSI_STATUS_MAST_WR_ACK			    0x28
#define	TWSI_STATUS_MAST_WR_NO_ACK		    0x30
#define	TWSI_STATUS_MAST_LOST_ARB		    0x38
#define	TWSI_STATUS_MAST_RD_ADDR_ACK		0x40
#define	TWSI_STATUS_MAST_RD_ADDR_NO_ACK		0x48
#define	TWSI_STATUS_MAST_RD_DATA_ACK		0x50
#define	TWSI_STATUS_MAST_RD_DATA_NO_ACK		0x58
#define	TWSI_STATUS_MAST_WR_ADDR_2_ACK		0xd0
#define	TWSI_STATUS_MAST_WR_ADDR_2_NO_ACK	0xd8
#define	TWSI_STATUS_MAST_RD_ADDR_2_ACK		0xe0
#define	TWSI_STATUS_MAST_RD_ADDR_2_NO_ACK	0xe8
#define	TWSI_STATUS_NO_STATUS			    0xf8

/* twsi action list*/
enum
{
    TWSI_ACTION_INVALID,
    TWSI_ACTION_SEND_START,
    TWSI_ACTION_SEND_ADDR1,
    TWSI_ACTION_SEND_ADDR2,
    TWSI_ACTION_SEND_DATA,
    TWSI_ACTION_SEND_STOP,
    TWSI_ACTION_RCV_DATA,
    TWSI_ACTION_RCV_DATA_STOP,
    TWSI_ACTION_CONTINUE,
};

/*
 * sensor tmp100 registers list
 */


/*
 * RTC registers list
 */
#define RTC_REG1_READ               0x61
#define RTC_REG1_WRITE              0x60
#define RTC_REG2_READ               0x63
#define RTC_REG2_WRITE              0x62
#define RTC_TIME1_READ              0x65
#define RTC_TIME1_WRITE             0x64
#define RTC_TIME2_READ              0x67
#define RTC_TIME2_WRITE             0x66
#define RTC_FREEREG_READ            0x6F
#define RTC_FREEREG_WRITE           0x6E

struct twsi_cmd
{
    unsigned char cmd;
    unsigned int  value;
};

#define ERR_TWSI_GET_FAILED -1
#define ERR_TWSI_OUT_BOUND  -2

#define S3530_FMT_BCD2DEC   0
#define S3530_FMT_DEC2BCD   1

#define TYPE_TWSI_I2C   0
#define TYPE_GPIO_I2C   1

/*sensor address,fixed,from tmp100 datasheet*/
#define IC_RTC_ADDR                 0x30
/*ADS1000*/
#define IC_ADS_ADDR                 0x48

/* for SG80 */
#define IC_SG80_EXTEND1         	0x21
#define IC_SG80_EXTEND2         	0x20
#define IC_SG80_RTC_ADDR            0x06
#define IC_SG80_RTC_            0x06

extern int twsi_get(struct twsi_cmd * cmd,unsigned char * data,unsigned char len);
extern int twsi_set(struct twsi_cmd * cmd);

#endif
