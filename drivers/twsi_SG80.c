#include "twsi.h"
#include <common.h>

int i2c_get(unsigned char * data,unsigned char len, unsigned char IC_ADDRESS, unsigned int reg)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_ADDRESS << 1) | TWSI_WRITE)},
                             {TWSI_ACTION_SEND_ADDR2,reg},
                             {TWSI_ACTION_SEND_STOP,0},
                             {TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_ADDRESS << 1) | TWSI_READ)},
                             {TWSI_ACTION_CONTINUE,0},
                             {TWSI_ACTION_RCV_DATA_STOP,0},
                             {0,0},
                            };

    return twsi_get(cmd,data,len);
}

int i2c_get_8bit(unsigned char IC_ADDRESS, unsigned int reg, unsigned char *data)
{
    return i2c_get(data, 1, IC_ADDRESS, reg);
}

int i2c_set_8bit(unsigned char IC_ADDRESS, unsigned int reg, unsigned char *data)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_ADDRESS << 1) | TWSI_WRITE)},
                             {TWSI_ACTION_SEND_ADDR2,reg},
                             {TWSI_ACTION_SEND_DATA,data[0]},
                             {TWSI_ACTION_SEND_STOP,0},
                             {0,0},
                            };

    return twsi_set(cmd);
}

int rtc_get_Byte(unsigned char IC_ADDRESS, int length, unsigned char *data)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_ADDRESS << 1) | TWSI_READ)},
                             {TWSI_ACTION_CONTINUE,0},
                             {TWSI_ACTION_RCV_DATA_STOP,0},
                             {0,0},
                            };

    return twsi_get(cmd, data, length);
}

int rtc_get_xByte(unsigned char IC_ADDRESS, int length, unsigned char *data)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_ADDRESS << 1) | TWSI_READ)},
                             {TWSI_ACTION_CONTINUE,0},
                             {TWSI_ACTION_RCV_DATA,0},
                             {TWSI_ACTION_RCV_DATA,0},
                             {TWSI_ACTION_RCV_DATA,0},
                             {TWSI_ACTION_RCV_DATA,0},
                             {TWSI_ACTION_RCV_DATA,0},
                             {TWSI_ACTION_RCV_DATA,0},
                             {TWSI_ACTION_RCV_DATA_STOP,0},
                             {0,0},
                            };

    return twsi_get(cmd, data, length);
}

int rtc_set_Byte(unsigned char IC_ADDRESS, int length, unsigned char data)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_ADDRESS << 1) | TWSI_WRITE)},
                             {TWSI_ACTION_SEND_DATA,data},
                             {TWSI_ACTION_SEND_STOP,0},
                             {0,0},
                            };

    return twsi_set(cmd);
}

int rtc_set_xByte(unsigned char IC_ADDRESS, int length, unsigned char *data)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_ADDRESS << 1) | TWSI_WRITE)},
                             {TWSI_ACTION_SEND_DATA,data[0]},
                             {TWSI_ACTION_SEND_DATA,data[1]},
                             {TWSI_ACTION_SEND_DATA,data[2]},
                             {TWSI_ACTION_SEND_DATA,data[3]},
                             {TWSI_ACTION_SEND_DATA,data[4]},
                             {TWSI_ACTION_SEND_DATA,data[5]},
                             {TWSI_ACTION_SEND_DATA,data[6]},
                             {TWSI_ACTION_SEND_STOP,0},
                             {0,0},
                            };

    return twsi_set(cmd);
}
/**********************************************************
 * sensor command
 **********************************************************/
#define SENSOR_ADDR IC_SG80_SENSOR_ADDR
#define GPIO_FUNC_ENABEL_CTRL 0x47
#define GPIO_1_5_MODE_CTRL 0x48
#define GPIO_8_14_MODE_CTRL 0x49
#define GPIO_1_7_OUT_DATA 0x4A
#define GPIO_8_14_OUT_DATA 0x4B
#define GPIO_1_5_OUT_MODE_CTRL 0x50
#define GPIO_8_14_OUT_MODE_CTRL 0x51
#define VCORE_OFFSET 0x20
#define VRAM_OFFSET 0x21
#define VCC_OFFSET 0x22
#define VIN1_OFFSET 0x23
#define VIN2_OFFSET 0x24
#define TEMP0_OFFSET 0x25
#define TEMP1_OFFSET 0x26
#define TEMP2_OFFSET 0x27

/**********************************************************
 * rtc time read/set S-35390
 **********************************************************/
static char DEC_To_BCD(unsigned char time)
{
        unsigned char temp1, temp2, temp;

        temp1 = time/10;
        temp2 = time%10;
        
        temp1 <<= 4;
        temp = temp1|temp2;
 
        return temp;    
}


/* BCD and DEC convert */
static char BCD_To_DEC (unsigned char reg)
{
        unsigned char temp1, temp2, temp;
        
        temp1 = reg&0xf0;
        temp2 = reg&0x0f;
        
        temp1 >>= 4;
        temp = temp1*10 + temp2;
        
        return temp;    
}

#define swapbit(x)  (((x&0x01) <<7) | \
                     ((x&0x02) <<5) | \
                     ((x&0x04) <<3) | \
                     ((x&0x08) <<1) | \
                     ((x&0x10) >>1) | \
                     ((x&0x20) >>3) | \
                     ((x&0x40) >>5) | \
                     ((x&0x80) >>7) )

void read_rtc(void)
{
    unsigned char rval[7];
    int i;

    memset(rval,0,7);
    rtc_get_xByte(0x32, 7, rval);

    for(i=0;i<7;i++)
        rval[i]=swapbit(rval[i]);

    rval[4]=rval[4]&0x3f;
	
    printf("\ndate:%d.%d.%d %d:%d:%d week :%d\n",
        BCD_To_DEC(rval[0])+2000,
        BCD_To_DEC(rval[1]),
        BCD_To_DEC(rval[2]),
        BCD_To_DEC(rval[4]),
        BCD_To_DEC(rval[5]),
        BCD_To_DEC(rval[6]),
        BCD_To_DEC(rval[3]));
}

void force_reset_rtc(void )
{
        rtc_set_Byte(0x30, 1, swapbit(0x0f));
}

                
void check_reset_rtc(void )
{
        unsigned char data=0;
        unsigned char cdata=0;

        rtc_get_Byte(0x30, 1, &data);
        cdata=data&0xff;
        if(cdata & (0x3))
        {
                printf("auto reset rtc ...\n");
                force_reset_rtc();
        }
}

void write_rtc(unsigned char* rtcTime)
{
        unsigned char value;
        unsigned char val[8];
        uint64_t dataw=0;
        unsigned char data=0;
        int i;
        
        check_reset_rtc();
 
        rtc_get_Byte(0x30, 1, &data);
        data |= 0x40;
        rtc_set_Byte(0x30, 1, data); /* for set to 24 hour mode */

        memset(val, 0x0, 8);
        memcpy(val, rtcTime, 8);

        for(i = 0; i < 7; i++) {
            value=DEC_To_BCD(val[i]);
            val[i]=swapbit(value);
        }

        if(rtc_set_xByte(0x32, 7, val)<0)
            printf("iic tx error\n");

        udelay(1000*100);       
}

/**********************************************************************
 * SX1505 -- GPIO expander
 **********************************************************************/
#define EXTENDER_REG_DATA 0
#define EXTENDER_REG_DIR 1

void extend_gpio_ctl(int extender_addr, int num_gpio, int status)
{
    unsigned char value;
	unsigned char mask;
    
    if(num_gpio >= 0 && num_gpio <= 7) {
		mask = (1 << (num_gpio));
		/* Get the gpio current modes */
        i2c_get_8bit(extender_addr, EXTENDER_REG_DIR, &value);
		/* Set gpio to output mode */
        value &= ~mask;		
        i2c_set_8bit(extender_addr, EXTENDER_REG_DIR, &value);
		/* Get gpio current data */
        i2c_get_8bit(extender_addr, EXTENDER_REG_DATA, &value);
		/* Set gpio new data */
        value = status?(value |= mask):(value & (~mask));
        i2c_set_8bit(extender_addr, EXTENDER_REG_DATA, &value);
    }
}

