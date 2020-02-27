#include "twsi.h"
#include <common.h>

int tmp100_get_temperature(unsigned char * data,unsigned char len)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_SENSOR_ADDR << 1) | TWSI_WRITE)},
                             {TWSI_ACTION_SEND_ADDR2,SENSOR_TEMP_REG},
                             {TWSI_ACTION_SEND_STOP,0},
                             {TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_SENSOR_ADDR << 1) | TWSI_READ)},
                             {TWSI_ACTION_CONTINUE,0},
                             {TWSI_ACTION_RCV_DATA,0},
                             {TWSI_ACTION_RCV_DATA_STOP,0},
                             {0,0},
                            };

    return twsi_get(cmd,data,len);
}

int tmp100_get_config(unsigned char * data,unsigned char len)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_SENSOR_ADDR << 1) | TWSI_WRITE)},
                             {TWSI_ACTION_SEND_ADDR2,SENSOR_CONFIG_REG},
                             {TWSI_ACTION_SEND_STOP,0},
                             {TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_SENSOR_ADDR << 1) | TWSI_READ)},
                             {TWSI_ACTION_CONTINUE,0},
                             {TWSI_ACTION_RCV_DATA_STOP,0},
                             {0,0},
                            };

    return twsi_get(cmd,data,len);
}

int tmp100_set_config(void)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_SENSOR_ADDR << 1) | TWSI_WRITE)},
                             {TWSI_ACTION_SEND_ADDR2,SENSOR_CONFIG_REG},
                             {TWSI_ACTION_SEND_DATA,0x78},
                             {TWSI_ACTION_SEND_STOP,0},
                             {0,0},
                            };

    return twsi_set(cmd);
}

int tmp100_set_TH(void)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_SENSOR_ADDR << 1) | TWSI_WRITE)},
                             {TWSI_ACTION_SEND_ADDR2,SENSOR_THIGH_REG},
                             {TWSI_ACTION_SEND_DATA,0x64},
                             {TWSI_ACTION_SEND_DATA,0x00},
                             {TWSI_ACTION_SEND_STOP,0},
                             {0,0},
                            };

    return twsi_set(cmd);
}

int tmp100_get_TH(unsigned char * data,unsigned char len)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_SENSOR_ADDR << 1) | TWSI_WRITE)},
                             {TWSI_ACTION_SEND_ADDR2,SENSOR_THIGH_REG},
                             {TWSI_ACTION_SEND_STOP,0},
                             {TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_SENSOR_ADDR << 1) | TWSI_READ)},
                             {TWSI_ACTION_CONTINUE,0},
                             {TWSI_ACTION_RCV_DATA,0},
                             {TWSI_ACTION_RCV_DATA_STOP,0},
                             {0,0},
                            };

    return twsi_get(cmd,data,len);
}

int tmp100_set_TL(void)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_SENSOR_ADDR << 1) | TWSI_WRITE)},
                             {TWSI_ACTION_SEND_ADDR2,SENSOR_TLOW_REG},
                             {TWSI_ACTION_SEND_DATA,0x00},
                             {TWSI_ACTION_SEND_DATA,0x00},
                             {TWSI_ACTION_SEND_STOP,0},
                             {0,0},
                            };

    return twsi_set(cmd);
}

int tmp100_get_TL(unsigned char * data,unsigned char len)
{
    struct twsi_cmd cmd[] = {{TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_SENSOR_ADDR << 1) | TWSI_WRITE)},
                             {TWSI_ACTION_SEND_ADDR2,SENSOR_TLOW_REG},
                             {TWSI_ACTION_SEND_STOP,0},
                             {TWSI_ACTION_SEND_START,0},
                             {TWSI_ACTION_SEND_ADDR1,((IC_SENSOR_ADDR << 1) | TWSI_READ)},
                             {TWSI_ACTION_CONTINUE,0},
                             {TWSI_ACTION_RCV_DATA,0},
                             {TWSI_ACTION_RCV_DATA_STOP,0},
                             {0,0},
                            };

    return twsi_get(cmd,data,len);
}

void tmp100_init(void)
{
    tmp100_set_config();
    tmp100_set_TH();
    tmp100_set_TL();
}
