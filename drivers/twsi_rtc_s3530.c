#include "twsi.h"
#include <common.h>

int rtc_s3530_get_status_reg1(unsigned char * data,unsigned char len)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_RTC_ADDR << 1) | TWSI_WRITE)},
                             {TWSI_ACTION_SEND_ADDR2,RTC_REG1_READ},
                             {TWSI_ACTION_SEND_STOP,0},
                             {TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_RTC_ADDR << 1) | TWSI_READ)},
                             {TWSI_ACTION_CONTINUE,0},
                             {TWSI_ACTION_RCV_DATA_STOP,0},
                             {0,0},
                            };

    return twsi_get(cmd,data,len);
}

int rtc_s3530_reset(void)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_RTC_ADDR << 1) | TWSI_WRITE)},
                             {TWSI_ACTION_SEND_ADDR2,RTC_REG1_WRITE},
                             {TWSI_ACTION_SEND_STOP,0},
                             {TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,RTC_REG1_WRITE},
                             {TWSI_ACTION_SEND_DATA,0xC0},/*1<<6 12/24   1<<7 reset*/
                             {TWSI_ACTION_SEND_STOP,0},
                             {0,0},
                            };

    return twsi_set(cmd);
}

int rtc_s3530_get_time(unsigned char * data,unsigned char len)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_RTC_ADDR << 1) | TWSI_WRITE)},
                             {TWSI_ACTION_SEND_ADDR2,RTC_TIME1_READ},
                             {TWSI_ACTION_SEND_STOP,0},
                             {TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,RTC_TIME1_READ},
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

    return twsi_get(cmd,data,len);
}

/*
 *      FORMAT
 *time[0] ------------ year
 *time[1] ------------ month
 *time[2] ------------ day
 *time[3] ------------ day of week
 *time[4] ------------ hour
 *time[5] ------------ minute
 *time[6] ------------ second
 */
int rtc_s3530_set_time(unsigned char * time)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_RTC_ADDR << 1) | TWSI_WRITE)},
                             {TWSI_ACTION_SEND_ADDR2,RTC_TIME1_WRITE},
                             {TWSI_ACTION_SEND_STOP,0},
                             {TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,RTC_TIME1_WRITE},
                             {TWSI_ACTION_SEND_DATA,time[0]},
                             {TWSI_ACTION_SEND_DATA,time[1]},
                             {TWSI_ACTION_SEND_DATA,time[2]},
                             {TWSI_ACTION_SEND_DATA,time[3]},
                             {TWSI_ACTION_SEND_DATA,time[4]},
                             {TWSI_ACTION_SEND_DATA,time[5]},
                             {TWSI_ACTION_SEND_DATA,time[6]},
                             {TWSI_ACTION_SEND_STOP,0},
                             {0,0},
                            };

    return twsi_set(cmd);
}

#define BCD2DEC(val) ((val>>4)*10 + (val & 0xF))
#define DEC2BCD(val) (((val/10)<<4) + val%10)
void rtc_s3530_swap(unsigned char *data,unsigned char len)
{
    unsigned char tmp,value;
    unsigned int  i,j;

    for (i=0;i<len;i++)
    {
        if (data[i] == 0)
            continue;

        value = 0;

        for (j=0;j<8;j++)
        {
            value |= (((data[i]>>j) & 0x01) << (7-j));
        }

        data[i] = value;
    }
}

void rtc_s3530_format(unsigned int fmt,unsigned char *data,unsigned char len)
{
    unsigned int i;

    switch (fmt)
    {
        case S3530_FMT_BCD2DEC:
            rtc_s3530_swap(data,len);
            data[4] &= 0x3F;
            for(i=0;i<len;i++)
                data[i] = BCD2DEC(data[i]);
            break;
        case S3530_FMT_DEC2BCD:
            for(i=0;i<len;i++)
                data[i] = DEC2BCD(data[i]);
            rtc_s3530_swap(data,len);
            break;
    }
}

void rtc_s3530_init(void)
{
    unsigned char data[8];
    unsigned char count;
    int ret;

    ret = rtc_s3530_get_status_reg1(data,8*sizeof(unsigned char));

    if (!ret)
    {
        if (data[0] & 0x03)
        {
            printf("S3530 do reset\n");
            rtc_s3530_reset();
        }
    }
}
