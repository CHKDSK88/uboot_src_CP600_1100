#include "twsi.h"
#include <common.h>

int ads1000_get(unsigned char * data,unsigned char len)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_ADS_ADDR << 1) | TWSI_READ)},
                             {TWSI_ACTION_CONTINUE,0},
                             {TWSI_ACTION_RCV_DATA,0},
                             {TWSI_ACTION_RCV_DATA,0},
                             {TWSI_ACTION_RCV_DATA_STOP,0},
                             {0,0},
                            };

    return twsi_get(cmd,data,len);
}

int ads1000_init(void)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_ADS_ADDR << 1) | TWSI_WRITE)},
                             {TWSI_ACTION_SEND_DATA,0x80},
                             {TWSI_ACTION_SEND_STOP,0},
                             {0,0},
                            };

    return twsi_set(cmd);
}
