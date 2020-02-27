#include "twsi.h"
#include <common.h>

//#define DEBUG_SENSOR

void twsi_hw_init(void)
{
    TWSI_SOFT_RESET = 0;
    TWSI_BAUD_RATE  = ((0x08 & 0xf)<<3) | (0x4 & 0x7);
    TWSI_SLAVE_ADDR = 0;
    TWSI_EXT_SLAVE_ADDR = 0;
    TWSI_CONTROL_REG = BIT_CONTROL_REG_STOP | BIT_CONTROL_REG_EN;
}

void twsi_do_action(unsigned int action,unsigned int * data)
{
    switch (action)
    {
        case TWSI_ACTION_SEND_START:
            TWSI_CONTROL_REG = CNTL_BITS_INT | BIT_CONTROL_REG_START;
            break;
        case TWSI_ACTION_SEND_ADDR1:
        case TWSI_ACTION_SEND_ADDR2:
        case TWSI_ACTION_SEND_DATA:
            TWSI_DATA_REG = *data & 0xff;
            TWSI_CONTROL_REG = CNTL_BITS_INT;
            break;
        case TWSI_ACTION_RCV_DATA:
            *data = TWSI_DATA_REG;
            TWSI_CONTROL_REG = CNTL_BITS_INT;
            break;
        case TWSI_ACTION_SEND_STOP:
            TWSI_CONTROL_REG = CNTL_BITS | BIT_CONTROL_REG_STOP;
            break;
        case TWSI_ACTION_RCV_DATA_STOP:
            *data = TWSI_DATA_REG;
            TWSI_CONTROL_REG = BIT_CONTROL_REG_EN | BIT_CONTROL_REG_STOP;
            break;
        case TWSI_ACTION_CONTINUE:
            TWSI_CONTROL_REG = BIT_CONTROL_REG_EN | BIT_CONTROL_REG_INT_EN | BIT_CONTROL_REG_ACK;
            break;
        case TWSI_ACTION_INVALID:
        default:
            printf("ERROR : invalid operation on TWSI\n");
            break;
    }
#ifdef DEBUG_SENSOR
    printf("action : %x\t\tdata : 0x%x\n",action,*data);
#endif
}

unsigned int twsi_get_status(void)
{
    unsigned int data;

    data = TWSI_STATUS_REG;
#ifdef DEBUG_SENSOR
    printf("status : 0x%x\n",data);
#endif
    return data;
}

#define REG_MPP1    *((unsigned int *) 0xf1010004)
void twsi_gpio_init(void)
{
    unsigned int data;

    data = REG_MPP1;
    data &= ~(0xff);
    /*DON'T SET GPIO MODE*/
    data |= (0x11);
    REG_MPP1 = data;
}

int twsi_get(struct twsi_cmd * cmd,unsigned char * data,unsigned char len)
{
    unsigned char *pdata;
    unsigned int  retry = 0x20;
    unsigned int  status;
    unsigned char idx = 0;
    int ret = ERR_TWSI_GET_FAILED;
    struct twsi_cmd *pcmd;

    pdata = data;
    pcmd  = cmd;

    while (retry > 0)
    {
        if (pcmd->cmd == 0)
        {
            ret = 0;
            break;
        }

        twsi_do_action(pcmd->cmd,&pcmd->value);
        udelay(50);
        status = twsi_get_status();

        if ((status == TWSI_STATUS_MAST_WR_ADDR_NO_ACK) |
            (status == TWSI_STATUS_MAST_WR_NO_ACK) |
            (status == TWSI_STATUS_MAST_RD_ADDR_NO_ACK)
           )
        {
            retry--;
            pcmd = cmd;
            idx  = 0;
            continue;
        }

        /*NOTE : Don't out of bound*/
        if ((pcmd->cmd == TWSI_ACTION_RCV_DATA) | (pcmd->cmd == TWSI_ACTION_RCV_DATA_STOP))
        {
            if (idx < len)
                pdata[idx++] = pcmd->value;
            else
            {
                ret = ERR_TWSI_OUT_BOUND;
                break;
            }
        }

        pcmd++;
        udelay(200);
    }
    
    return ret;
}

int twsi_set(struct twsi_cmd * cmd)
{
    unsigned int  retry = 0x20;
    unsigned int  status;
    int ret = ERR_TWSI_GET_FAILED;
    struct twsi_cmd *pcmd;

    pcmd  = cmd;

    while (retry > 0)
    {
        if (pcmd->cmd == 0)
        {
            ret = 0;
            break;
        }

        twsi_do_action(pcmd->cmd,&pcmd->value);
        udelay(50);
        status = twsi_get_status();

        if ((status == TWSI_STATUS_MAST_WR_ADDR_NO_ACK) |
            (status == TWSI_STATUS_MAST_WR_NO_ACK) |
            (status == TWSI_STATUS_MAST_RD_ADDR_NO_ACK)
           )
        {
            retry--;
            pcmd = cmd;
            continue;
        }

        pcmd++;
        udelay(200);
    }
    
    return ret;
}

void twsi_init(void)
{
    twsi_hw_init();
    twsi_gpio_init();
}

