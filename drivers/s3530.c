#include "s3530.h"

/*
*
* rtc_Start (Control signal) - Initiate a transfer on the I2C bus. This
*		 should only be called from a master device 
*
* RETURNS: OK when the bus is free. ERROR if the bus is already in use by another
*	   task.
*/
unsigned char rtc_Start (void)
{
	if (rtc_BusFree() == OK) {
		RTC_GPIO_SETOUT(RTC_SDA|RTC_SCL);//SDA,SCL set as OUTPUT

		RTC_SCL_SET_HIGH; //SCL high and change SDA from high to low is mean start
		RTC_SDA_SET_HIGH;
		RTC_SDA_SET_LOW;
		RTC_SCL_SET_LOW;
		return (OK);
	}
	diag_printf("Failed to get free bus\n");
	return (ERROR);
}

/*
*
* rtc_Stop (Control signal) - End a transfer session. The I2C bus will be
*		left in a free state; i.e. SCL HIGH and SDA HIGH. This should
*		only be called from a master device 
*
* RETURNS: N/A
*/
void rtc_Stop (void)
{
	RTC_GPIO_SETOUT(RTC_SDA|RTC_SCL);//SDA,SCL set as OUTPUT

	RTC_SDA_SET_LOW; //SCL hight and change SDA from high to low is mean start
//	RTC_SCL_SET_LOW;
	RTC_SCL_SET_HIGH;
	RTC_SDA_SET_HIGH;
	RTC_SCL_SET_LOW;
//	RTC_SDA_SET_LOW;
}

/*
* rtc_AckSend (Control signal) - Send an acknowledgement
*
* This function sends an acknowledgement on the I2C bus.
*
* RETURNS: N/A
*/
void rtc_AckSend (void)
{
	RTC_GPIO_SETOUT(RTC_SDA|RTC_SCL);//SDA,SCL set as OUTPUT
	//maybe we should make sure the SCL is low now,the other function must take care
	RTC_SDA_SET_LOW; //data need change during the period that the SCL line is low

	RTC_SCL_SET_HIGH;
	RTC_SCL_SET_LOW;

//	RTC_SDA_SET_HIGH;//no need to set high?
}

/*
* rtc_AckReceive (Control Signal) - Get an acknowledgement from the I2C bus.
*
* RETURNS: OK if acknowledge received by slave; ERROR otherwise.
*/

unsigned char rtc_AckReceive (void)
{
	int retryCnt = 0;

	unsigned long sda = 0;

	RTC_GPIO_SETIN(RTC_SDA);
	RTC_SCL_SET_HIGH;

	do {
		RTC_SDA_GET(sda);
		retryCnt++;
//		diag_printf("Ack SDA:%x\n",sda);
	} while( sda && (retryCnt < RTC_ACK_RTY) );

	RTC_SCL_SET_LOW;

	if (!sda) //the ack should be zero
		return (OK);
	else
		return (ERROR);
}

void rtc_ByteTransmit (unsigned char dataByte)
{
	int bitCnt = 0;

	RTC_GPIO_SETOUT(RTC_SDA|RTC_SCL);//SDA,SCL set as OUTPUT

	RTC_SCL_SET_LOW;
	RTC_SDA_SET_LOW;

	for (bitCnt = 7; bitCnt >= 0; bitCnt--) {
		if (dataByte & BIT(bitCnt)) {
			RTC_SDA_SET_HIGH;
		} else {
			RTC_SDA_SET_LOW;
		}

		RTC_SCL_SET_HIGH; //when the SCL is low,we have change the SDA,now let SCL is high
		RTC_SCL_SET_LOW;
	}
}

void rtc_ByteReceive (unsigned char *dataByte)
{
	unsigned long sda = 0;
	unsigned char tmpByte = 0;
	int bitCnt = 0;

	RTC_GPIO_SETIN(RTC_SDA);
	RTC_GPIO_SETOUT(RTC_SCL);

	RTC_SCL_SET_LOW;

	for (bitCnt = 0; bitCnt <= 7; bitCnt++) { //only for LSB first
		RTC_SCL_SET_HIGH;

//		diag_printf("GPINR data:%p\n", *(unsigned long*)IXP425_GPINR);
		RTC_SDA_GET(sda);     /* Get the data bit */

		tmpByte |= (sda << bitCnt);//here should be some problem
		//        diag_printf("tmpByte:%p, sda:%p\n",tmpByte, sda);
		RTC_SCL_SET_LOW;
	}

	RTC_GPIO_SETOUT(RTC_SDA);
	RTC_SDA_SET_LOW;
	*dataByte = tmpByte;
}

int rtc_BusFree (void)
{
	unsigned long sda = 0;

	RTC_GPIO_SETIN(RTC_SDA);
    udelay(100*100);
	RTC_SDA_GET(sda);

	if (sda) //only check the SDA is high
		return (OK);
	else
		return (ERROR);
}

int RTCInit(int flag)
{
	unsigned char data = 0;

	if(rtc_Start() != OK) {
		diag_printf("bus not free\n");
		rtc_Stop();
		return ERROR;
	}

	rtc_ByteTransmit(RTC_REG1_READ); //read status register_1

	if(rtc_AckReceive() != OK) { //the board have no ack for our command
		diag_printf("No ACK received!\n");
		return ERROR;
	}

	rtc_ByteReceive(&data);
	rtc_AckSend();    
//	    diag_printf("Init data:%x\n", data);
	rtc_Stop();

	if(data & 0xC0 ) {// needs initialize,see datasheet,the POC and BLD bit in the status register_1
		diag_printf("Do a RTC reset\n");
		rtc_Start();
		rtc_ByteTransmit(RTC_REG1_WRITE);
//		rtc_AckReceive();
    	if(rtc_AckReceive() != OK) { //the board have no ack for our command
	    	diag_printf("No ACK received!\n");
		    return ERROR;
    	}

		rtc_ByteTransmit(1 | (flag <<1)); //Seas fix,i think this should be the bit 1,see datasheet
    	if(rtc_AckReceive() != OK) { //the board have no ack for our command
	    	diag_printf("No ACK received!\n");
		    return ERROR;
    	}
		rtc_Stop();
	}
        udelay(500);
//		rtc_Start();
        if(rtc_Start() != OK) {
	    	diag_printf("bus not free\n");
	    	rtc_Stop();
    		return ERROR;
    	}

		rtc_ByteTransmit(RTC_REG1_WRITE);
    	if(rtc_AckReceive() != OK) { //the board have no ack for our command
	    	diag_printf("No ACK received!\n");
		    return ERROR;
    	}

		rtc_ByteTransmit(1<<6); //Seas fix,i think this should be the bit 1,see datasheet
    	if(rtc_AckReceive() != OK) { //the board have no ack for our command
	    	diag_printf("No ACK received!\n");
		    return ERROR;
    	}
		rtc_Stop();  

	return OK;
}

/*****************************************************
* int RTCGetTime( unsigned char * buff)
*
* input : buffer to receive data
*
* output: OK or ERROR
*
* function: get FULL format time ( including y/m/d)
******************************************************/

int RTCGetTime( unsigned char * buff)
{
	int cnt = 7;
	unsigned char byte = 0;

	if( buff == NULL) {
		return ERROR;
	}

	if(rtc_Start() != OK) {
		diag_printf("bus not free\n");
		rtc_Stop();
		return ERROR;
	}

	rtc_ByteTransmit(RTC_TIME1_READ); //address 0110,cmd 010,read 1,so it 0x65
	if(rtc_AckReceive() != OK) {
		diag_printf("No ACK received!\n");
		return ERROR;
	}

	while(cnt >0) {
		rtc_ByteReceive(&byte);
		buff[cnt-1] = byte; //year,month,day,day of week,hour,minute,second
		//        diag_printf("cnt:%d,byte:%p\n", cnt, byte);
		if(cnt --) {
			rtc_AckSend();
		}//after the last byte,we should ack 1 follow the datasheet,but this can work,so i do not fix --Seas
	}
	rtc_Stop();
	return OK;
}

/*****************************************************
* int RTCSetTime( unsigned char * buff)
*
* input : buffer containing time data
*
* output: OK or ERROR
*
* function: set FULL format time ( including y/m/d)
*
******************************************************/

int RTCSetTime( unsigned char * buff)
{
	int cnt = 6;

	if( buff == NULL) {
		return ERROR;
	}

	if(rtc_Start() != OK) {
		diag_printf("bus not free\n");
		rtc_Stop();
		return ERROR;
	}

	rtc_ByteTransmit(RTC_TIME1_WRITE); //address 0110,cmd 010,write 0,so 0x64
	if(rtc_AckReceive() != OK) {
		diag_printf("No ACK received!\n");
		return ERROR;
	}

	while(cnt >=0) {
		diag_printf("%p ",buff[cnt]);
		rtc_ByteTransmit(buff[cnt]);
		//        buff[cnt-1] = byte;
		//        diag_printf("cnt:%d,byte:%p\n", cnt, byte);
		if(rtc_AckReceive()) //Seas fix,we should check the ACK
		{
			diag_printf("***Error*** No ack Received when set RTC time\n");
			return ERROR;
		}
		cnt--;
	}
	diag_printf("\n");
	rtc_Stop();
	return OK;
}

#define REG_MPP1    *((unsigned int *) 0xf1010004)
#define RGE_POLA    *((unsigned int *) 0xf101010c)

void s3530_RdID(void)
{
#if 0
	unsigned char cmd = 0x65;//0110 0101
	unsigned char cnt = 7;
	unsigned char byte = 0;
#endif

	unsigned char time[7];

    volatile unsigned int data; 

    data = REG_MPP1;
    data &= ~(0xff);
    REG_MPP1 = data;

    data = RGE_POLA;
    data &= ~(1<<8);
    data &= ~(1<<9);
    RGE_POLA = data;

	RTCInit(1); //1 is 24,0 is am/pm
//	RTCGetTime(time);
//	diag_printf("%d-%s %02d %s %02d:%02d:%02d\n",
//			2000+(BCD2DEC(time[6])), Month[BCD2DEC(time[5])], BCD2DEC(time[4]), Day[BCD2DEC(time[3])],
//			 BCD2DEC(time[2] & 0x3F), BCD2DEC(time[1]), BCD2DEC(time[0]));

}
/****************************************************
 * RTCS_35390A test function for MT test
 * Set time to 2008-08-08 08:08:08 and delay 2 second
 * and then check the time 
 * 
 ****************************************************/
int RTC_S_35390A_Test(void)
{
	unsigned char set_time[7],set_timeBCD[7],get_time[7];
	int i = 0;
	set_time[6] = 8; //year 2008
	set_time[5] = 8; //month 
	set_time[4] = 8; //day
	set_time[3] = 5; //day of week data,2008 -08 -08 is Fri
	set_time[2] = 8; //our
	set_time[1] = 8; //minute
	set_time[0] = 8; //second
/*because our RTCSetTime( unsigned char * buff) will transfer the date from MSB,but follow the data sheet,we should 
 *transmit the time from the LSB,so need do a byte reverse.
 */

	for(i = 0; i < 7; i++)
	{
		//though my test data is less than 10,the DEC and BCD is the same,but have a chang...
		//so if you change the set_time to another value it can work
		set_timeBCD[i] = DEC2BCD(set_time[i]);
//        if (i == 4)
//            set_timeBCD[i] |= (1<<6);
		set_timeBCD[i] = REVERSEBITS(set_timeBCD[i]);
	}

	diag_printf("***Will set time to 2008-08-08 08:08:08***\n");
	if(RTCSetTime(set_timeBCD))
	{
		diag_printf("***Set time to 2008-08-08 08:08:08 error***\n");
		return ERROR;
	}
	diag_printf("***Set time to 2008-08-08 08:08:08 success,will delay ...***\n");
	udelay(2000000);
	//hand the LSB byte send by the RTC,so do not have a reverse 
	if(RTCGetTime(get_time))
	{
		diag_printf("***After set time,read back error***");
		return ERROR;
	}
	get_time[2] &= 0x3f; //see datasheet
	for(i = 0 ;i < 7 ; i++)
	{
		get_time[i] = BCD2DEC(get_time[i]);
	}

	diag_printf("Read back time:%d-%d-%d %d(day of week) %d:%d:%d\n",2000+get_time[6],get_time[5]
	,get_time[4],get_time[3],get_time[2],get_time[1],get_time[0]);

	for(i = 0; i < 6; i++) //except the second,the other should not have a change
	{
		if(get_time[i] != set_time[i])
			return ERROR;
	
	}

	if(get_time[6] - set_time[6] < 2)
		diag_printf("***Error,time is less?***\n");

	return OK;	
}

void print_GetTime(void)
{
    unsigned char get_time[7];
    int i;

	if(RTCGetTime(get_time))
	{
		diag_printf("***After set time,read back error***");
		return ERROR;
	}
    
	get_time[2] &= 0x3f; //see datasheet
	for(i = 0 ;i < 7 ; i++)
	{
		get_time[i] = BCD2DEC(get_time[i]);
	}

	diag_printf("Read back time:%d-%d-%d %d:%d:%d %d(day of week)\n",2000+get_time[6],get_time[5]
	,get_time[4],get_time[2],get_time[1],get_time[0],get_time[3]);
}

void cmd_GetTime(unsigned char * get_time)
{
    int i;

	if(RTCGetTime(get_time))
	{
		diag_printf("***After set time,read back error***");
		return ERROR;
	}
    
	get_time[2] &= 0x3f; //see datasheet
	for(i = 0 ;i < 7 ; i++)
	{
		get_time[i] = BCD2DEC(get_time[i]);
	}
}

void cmd_SetTime(char * set_time)
{
    int i;
    unsigned char set_timeBCD[7];

    RTCInit(1);

	for(i = 0; i < 7; i++)
	{
		//though my test data is less than 10,the DEC and BCD is the same,but have a chang...
		//so if you change the set_time to another value it can work
		set_timeBCD[i] = DEC2BCD(set_time[i]);
		set_timeBCD[i] = REVERSEBITS(set_timeBCD[i]);
	}

	if(RTCSetTime(set_timeBCD))
	{
		diag_printf("***Set time error***\n");
		return ERROR;
	}
	diag_printf("***Set time success***\n");
}

void rtcreader(unsigned char * reg)
{
	unsigned char data = 0;
    unsigned char rr   = simple_strtoul(reg,NULL,10);

    printf("read register : 0x%x\n",rr);

	if(rtc_Start() != OK) {
		diag_printf("bus not free\n");
		rtc_Stop();
		return ERROR;
	}

	rtc_ByteTransmit(rr); //read status register_1

	if(rtc_AckReceive() != OK) { //the board have no ack for our command
		diag_printf("No ACK received!\n");
		return ERROR;
	}

	rtc_ByteReceive(&data);
	rtc_AckSend();    
    diag_printf("read data:%x\n", data);
	rtc_Stop();
}

void rtcset(unsigned char * reg,unsigned char * dat)
{
    unsigned char wr   = simple_strtoul(reg,NULL,10);
    unsigned char data = simple_strtoul(dat,NULL,10);

    printf("write register : 0x%x\t\tdata : 0x%x\n",wr,data);

    if(rtc_Start() != OK) {
	    diag_printf("bus not free\n");
	    rtc_Stop();
    	return ERROR;
    }

	rtc_ByteTransmit(wr);
    if(rtc_AckReceive() != OK) { //the board have no ack for our command
	    diag_printf("No ACK received!\n");
		return ERROR;
    }

	rtc_ByteTransmit(data); //Seas fix,i think this should be the bit 1,see datasheet
    if(rtc_AckReceive() != OK) { //the board have no ack for our command
	    diag_printf("No ACK received!\n");
		return ERROR;
    }
	rtc_Stop();  
}

