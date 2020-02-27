/* 
   data is driving by the falling edge and sampling by the rising edge of clock
   CPHA=1
   CPOL=1
   发送高位先发送，接收高位先接(MSB-->>LSB)
   */
#include <common.h>
#define SPI_EEPROM_PAGE_SIZE 528
#define SPI_EEPROM_MAX_PAGES 4096

#define MPP4            *((unsigned int *) 0xf1010010)
#define MPP5            *((unsigned int *) 0xf1010014)
#define MPP6            *((unsigned int *) 0xf1010018)
#define GPIOOUT_EN_H    *((unsigned int *) 0xf1010144)
#define GPIOOUT_DATAH   *((unsigned int *) 0xf1010140)
#define GPIOIN_INVERT_H *((unsigned int *) 0xf101014C)
#define GPIOIN_DATA_H   *((unsigned int *) 0xf1010150)
#define GPIOOUT_EN_L    *((unsigned int *) 0xf1010104)
#define GPIOOUT_DATAL   *((unsigned int *) 0xf1010100)
#define GPIOIN_INVERT_L *((unsigned int *) 0xf101010C)
#define GPIOIN_DATA_L   *((unsigned int *) 0xf1010110)


#define	SPI_SCK_GPIO	40
#define	SPI_CHIPSEL		39
#define	SPI_MISO_GPIO	41
#define	SPI_MOSI_GPIO	42

#define out_put 1
#define in_put 0

#define HIGH 1
#define LOW  0

#define SPI_MASTER_TX 0
#define SPI_MASTER_RX 1

#define CMD_RD	(0x60)
#define CMD_WR	(0x61)

#define SPI_REG_PAGE	(0XFF)
#define SPI_REG_DATA	(0xF0)
#define SPI_REG_STAT	(0xFE)


void spi_init();
void spi_sel();
void spi_free();

void do_GPIO_output(unsigned int gpio,unsigned char status);
int do_GPIO_input(unsigned int gpio);

void do_GPIO_output(unsigned int gpio,unsigned char status)
{
    unsigned int data;

    switch(gpio / 8) {
    case 4:
        MPP4 &= (0xffffffff & (~(0xf << (4*(gpio % 8)))));
        break;
    case 5:
        MPP5 &= (0xffffffff & (~(0xf << (4*(gpio % 8)))));
        break;
    case 6:
        MPP6 &= (0xffffffff & (~(0xf << (4*(gpio % 8)))));
        break;
    default:
        printf("only support gpio 32 - 49\n");
        return 0;
        break;
    }

    if (gpio < 32) {
        data = GPIOOUT_EN_L;
        data &= ~(1<<gpio);
        GPIOOUT_EN_L = data;
        if (!status) {
            data = GPIOOUT_DATAL;
            data &= ~(1<<gpio);
            GPIOOUT_DATAL = data;
        } else {
            data = GPIOOUT_DATAL;
            data |= (1<<gpio);
            GPIOOUT_DATAL = data;
        }
    } else {
        gpio -= 32;
        data = GPIOOUT_EN_H;
        data &= ~(1<<gpio);
        GPIOOUT_EN_H = data;
//	    printf("GPIOOUT_EN_H: 0x%x\n", GPIOOUT_EN_H);
        if (!status) {
            data = GPIOOUT_DATAH;
            data &= ~(1<<gpio);
            GPIOOUT_DATAH = data;
//			printf("line: %d gpio: %d, GPIOOUT_DATA_H: 0x%x\n", __LINE__, gpio, GPIOOUT_DATAH);
        } else {
            data = GPIOOUT_DATAH;
            data |= (1<<gpio);
            GPIOOUT_DATAH = data;
//			printf("line: %d gpio: %d, GPIOOUT_DATA_H: 0x%x\n", __LINE__, gpio, GPIOOUT_DATAH);
        }
    }
}

int do_GPIO_input(unsigned int gpio)
{
    unsigned int data;

    switch(gpio / 8) {
    case 4:
        MPP4 &= (0xffffffff & (~(0xf << (4*(gpio % 8)))));
        break;
    case 5:
        MPP5 &= (0xffffffff & (~(0xf << (4*(gpio % 8)))));
        break;
    case 6:
        MPP6 &= (0xffffffff & (~(0xf << (4*(gpio % 8)))));
        break;
    default:
        printf("only support gpio 32 - 49\n");
        return 0;
        break;
    }

    if (gpio < 32) {
        data = GPIOOUT_EN_L;
        data |= (1<<gpio);
        GPIOOUT_EN_L = data;

        data = (GPIOIN_DATA_L ^ GPIOIN_INVERT_L);
        data |= (1<<gpio);
        data = (data >> gpio);
    } else {
        gpio -= 32;
        data = GPIOOUT_EN_H;
        data |= (1<<gpio);
        GPIOOUT_EN_H = data;

        data = (GPIOIN_DATA_H ^ GPIOIN_INVERT_H);
        data &= (1<<gpio);
        data = (data >> gpio);
    }

    return data;
}

void spi_sel()
{
	do_GPIO_output(SPI_SCK_GPIO, 1);
	udelay(1);
	do_GPIO_output(SPI_CHIPSEL, 0);
	udelay(1);
}

void spi_free()
{
	do_GPIO_output(SPI_SCK_GPIO, 1);
	udelay(1);
	do_GPIO_output(SPI_CHIPSEL, 1);
	udelay(1);
}


void spi_init()
{
//	cvmx_gpio_cfg(SPI_SCK_GPIO, out_put);
//	cvmx_gpio_cfg(SPI_CHIPSEL, out_put);
//	cvmx_gpio_cfg(SPI_MOSI_GPIO, out_put);
//	cvmx_gpio_cfg(SPI_MISO_GPIO, in_put);

	do_GPIO_output(SPI_CHIPSEL, 1);
	do_GPIO_output(SPI_SCK_GPIO, 1);
}


void spidelay()
{
	udelay(10);
}

void setsck(unsigned mask)
{
	if(mask)
	    do_GPIO_output(SPI_SCK_GPIO, 1);
	else
	    do_GPIO_output(SPI_SCK_GPIO, 0);

}

void setmosi(unsigned mask)
{
	if(mask)
	    do_GPIO_output(SPI_MOSI_GPIO, 1);
	else
	    do_GPIO_output(SPI_MOSI_GPIO, 0);
}

unsigned char getmiso()
{
	unsigned long long temp;

	temp = do_GPIO_input(SPI_MISO_GPIO);

	//printf("%s %ull\n", __func__, temp);

	if(temp) {
		return HIGH;
	} else {	
		return LOW;
	}
}

unsigned char txrx_cpha1(unsigned char byte)
{
	/** if (cpol == 0) this is SPI_MODE_1; else this is SPI_MODE_3 */

	/** clock starts at inactive polarity */
	unsigned char bit;
//    spi_sel();
	for(bit=0;bit<8;bit++)
	{
		/** setup MSB (to slave) on leading edge */
		setsck(LOW);
		
		if(byte & 0x80)
			setmosi(HIGH);
		else
			setmosi(LOW);

		byte <<= 1;
		udelay(1); /** T(setup) */
		setsck(HIGH);
		udelay(1);
		/** sample MSB (from slave) on trailing edge */
		byte |= getmiso();
	}

//	spi_free();
	return byte;
}



unsigned char rev_data()
{
	return txrx_cpha1(1);
}

void send_data(unsigned char spi_data)
{
	txrx_cpha1(spi_data);
}

/*******************************************************************/

static int waiting_spi_ready()
{
	int times = 100;
	unsigned char ret;

	while(1)
	{
		spi_sel();
		send_data(0xD7); //Status Register Read
		ret = rev_data();
		if( !(ret & 0x80) )
		{
			return 0;
		}
		else
		{
			udelay(1);
			if(times--<0)
			{
				printk("waiting ready error!!!\n");
				return -1;
			}
		}
		spi_free();
	}
	spi_free();
	return -1;
}


static int send_command(int command, int page_addr, int buf_addr, int additional_byte)
{
 	unsigned char addr_msb, addr_lsb, addr_middle;

 	addr_msb = ( (page_addr >> 6)& 0x3f );
 	addr_middle = ( (page_addr << 2) & 0xfc ) | ( (buf_addr >> 8) & 0x03 );
 	addr_lsb = ( buf_addr & 0xff );


 	send_data(command);
 	send_data(addr_msb);
 	send_data(addr_middle);
 	send_data(addr_lsb);
 	while(additional_byte) {//Don't care bytes
 		send_data(0);
 		additional_byte --;
 	}


 	return 0;
 }
 // Writing to EEProm (with BUILT-IN ERASE)
static int eeprom_write_bytes (int pg_num, int start_addr, int number, unsigned long buf)
{

	int i,n,buf_addr;
	printf("page=%x, start=%x, number=%x\n", pg_num, start_addr, number);


	if( (start_addr >= SPI_EEPROM_PAGE_SIZE) || (pg_num>= SPI_EEPROM_MAX_PAGES) )
		return -1;

	spi_sel();

	buf_addr = start_addr;
	n = 528 - start_addr;
	if(number < n)
		n = number;

	while(number > 0)
	{
		do_GPIO_output(SPI_CHIPSEL, 1);
		for(i=0; i < 0x500; i++)
			;
		/* write data to buf1 */
		send_command(0x84, 0, buf_addr, 0);
		for(i=0; i < 0x500; i++)
			;
		write_buf(n, buf);

		for(i=0; i < 0x500; i++)
			;
		do_GPIO_output(SPI_CHIPSEL, 0);

		/* waiting buf2 programming completed */
		if (waiting_spi_ready() != 0)
			return -1;

		do_GPIO_output(SPI_CHIPSEL, 1);
		for(i=0; i < 0x500; i++)
			;

		/* Send buf1 to flash */
		send_command(0x83, pg_num, 0, 0);

		for(i=0; i < 0x500; i++)
			;
		do_GPIO_output(SPI_CHIPSEL, 0);
		if (waiting_spi_ready() != 0)
			return -1;

		buf_addr = 0;
		buf += n;
		if ( (number -= n) <= 0)
			break;
		if (++pg_num>= 4096 )
			break;
		if (number < 528)
			n = number;
		else
			n = 528;
		do_GPIO_output(SPI_CHIPSEL, 1);
		for(i=0; i < 0x500; i++)
			;

		/* write data to buf2 */
		send_command(0x87, 0, buf_addr, 0);
		for(i=0; i < 0x500; i++)
			;
		write_buf(n, buf);
		for(i=0; i < 0x500; i++)
			;
		do_GPIO_output(SPI_CHIPSEL, 0);
		/* waiting buf1 programming completed */
		if (waiting_spi_ready() != 0)
			return -1;

		do_GPIO_output(SPI_CHIPSEL, 1);
		for(i=0; i < 0x500; i++)
			;
		/* send buf2 to flash */
		send_command(0x86, pg_num, 0, 0);

		for(i=0; i < 0x500; i++)
			;
		do_GPIO_output(SPI_CHIPSEL, 0);
		if (waiting_spi_ready() != 0)
			return -1;

		buf_addr = 0;
		buf += n;
		if ( (number -= n) <= 0)
			break;
		if (++pg_num>= 4096 )
			break;
		if(number < 528)
			n = number;
		else
			n = 528;

		if( (pg_num & 0x3f ) == 0 )
			printf("pg_num=%d\n",pg_num);
	}

	do_GPIO_output(SPI_CHIPSEL, 0);

	if (waiting_spi_ready() != 0)
		return -1;
	printf("\n");

	return 0;
}

static int spi_write_file(unsigned long from, unsigned long number)
{
	return eeprom_write_bytes(0,0,number,from);
}
/*******************************************************************/


void spi_write_page(unsigned short page_addr,unsigned short reg_addr,unsigned int data)
{
	unsigned char ret;
	unsigned char byte1,byte2,byte3,byte4;
	int times = 100;
	byte1 = data & 0xFF;
	byte2 = (data >> 8) & 0xFF;
	byte3 = (data >> 16) & 0xFF;
	byte4 = (data >> 24) & 0xFF;
	/* step 1 */
	spi_sel();
	do
	{
		send_data(CMD_RD);
		//udelay(10);
		send_data(SPI_REG_STAT);
		ret = rev_data();
		if(!(ret&0x80))
		{
			break;
		}
		if(times == 0)
		{
			printf("status register is %d,WRITE ERROR\n",ret);
			return -1;
		}
	}while(times--);
	spi_free();

	/* step 2 */
	spi_sel();

	send_data(CMD_WR);
	//udelay(10);
	send_data(SPI_REG_PAGE);
	//udelay(10);
	send_data(page_addr);//0x10 specify page register

	spi_free();
	/* step 3 */
	spi_sel();

	send_data(CMD_WR);
	//udelay(10);
	send_data(reg_addr);//0x12
	//udelay(10);
	
	send_data(byte1);
	//udelay(10);
	send_data(byte2);
	//udelay(10);
	send_data(byte3);
	//udelay(10);
	send_data(byte4);
	//udelay(10);
	
	spi_free();
}
#if 0
void spi_write_global(unsigned short reg_addr,unsigned int data)
{
	unsigned char ret;
	unsigned char byte1,byte2,byte3,byte4;
	byte1 = data & 0xFF;
	byte2 = (data >> 8) & 0xFF;
	byte3 = (data >> 16) & 0xFF;
	byte4 = (data >> 24) & 0xFF;
	/* step 1 */
	send_data(read_cmd);
	send_data(status_reg);
	ret = rev_data();
	if(ret&0x80)printf("status register is %d,WRITE ERROR\n",ret);
	/* step 3 */
	send_data(write_cmd);
	send_data(reg_addr);//0x12
	send_data(byte1);
	send_data(byte2);
	send_data(byte3);
	send_data(byte4);	
}
#endif

unsigned int spi_read_page(unsigned char page_addr,unsigned char reg_addr)
{
	unsigned char ret;
	unsigned char byte1,byte2,byte3,byte4;
	unsigned int value;
	int times = 100;
	/* step 1 */
	spi_sel();
	do
	{
		send_data(CMD_RD);
		//udelay(10);
		send_data(SPI_REG_STAT);
		ret = rev_data();
		if(!(ret&0x80)) 
		{	
		//	printf("status register and times is %d : %d\n",ret,times);
			break;
		}
		udelay(1);
		if(times == 0)
		{
		//	printf("status register is %d,read ERROR\n",ret);
			return -1;
		}
	}while(times--);
	spi_free();
	/* step 2 */
	spi_sel();
	send_data(CMD_WR);
	//udelay(10);
	send_data(SPI_REG_PAGE);
	//udelay(10);
	send_data(page_addr);//0x10
	spi_free();
	/* step 3 */
	spi_sel();
	send_data(CMD_RD);
	//udelay(10);
	send_data(reg_addr);//0x12
	//udelay(10);
	ret = rev_data();//dummy read
	spi_free();
	/* step 4 */
	static int times_ack = 100;
	spi_sel();
	do
	{
		send_data(CMD_RD);
	//	udelay(10);
		send_data(SPI_REG_STAT);
	//	udelay(10);
		ret = rev_data();
		if(ret&0x20)
		{	
	//		printf("status register and times_ack is %d  %d\n",ret,times_ack);
			break;
		}
		if(times_ack==0) 
		{
			printf("status register and times_ack is %d  %d\n",ret,times_ack);
			return -1;
		}
	}while(times_ack--);
	spi_free();

	/* step 5 */
	spi_sel();

	send_data(CMD_RD);
	//udelay(10);
	send_data(SPI_REG_DATA);
	//udelay(10);

	byte1 = rev_data();
	//udelay(10);
	byte2 = rev_data();
	//udelay(10);
	byte3 = rev_data();
	//udelay(10);
	byte4 = rev_data();
	//udelay(10);


	spi_free();

	printf("byte1 %x byte2 %x byte3 %x byte4 %x\n", \
			byte1,
			byte2,
			byte3,
			byte4);

	value = byte1 | (byte2 << 8) | (byte3 << 16) | (byte4 << 24) ;
	printf("value  %d\n",value);
	return value;		
}
#if 0
int spi_read_global(unsigned short reg_addr)
{
	unsigned char ret;
	unsigned char byte1,byte2,byte3,byte4;
	unsigned int value;
	/* step 1 */
	send_data(read_cmd);
	send_data(status_reg);
	ret = rev_data();
	if(ret&0x80)printf("status register is %d,read ERROR\n",ret);
	/* step 3 */
	send_data(read_cmd);
	send_data(reg_addr);//0x12
	ret = rev_data();
	/* step 4 */
	send_data(read_cmd);
	send_data(status_reg);
	do
	{
		ret = rev_data();
	}while(ret != 0x20);
	/* step 5 */
	send_data(read_cmd);
	send_data(data_reg);
	byte1 = rev_data();
	byte2 = rev_data();
	byte3 = rev_data();
	byte4 = rev_data();
	value = byte1 | (byte2 << 8) | (byte3 << 16) | (byte4 << 24);
	return value;		
}

unsigned int spi_read_page_fast(unsigned char page_addr,unsigned char reg_addr)
{
	unsigned char ret;
	unsigned char byte1,byte2,byte3,byte4;
	unsigned int value;
	/* step 1 */
	spi_sel();
	send_data(read_cmd);
	udelay(10);
	send_data(status_reg);
	ret = rev_data();
	if(!(ret&0x80)) 
	{	
		printf("status register is %d \n",ret);
	}
	else
	{
		printf("status register is %d,read ERROR\n",ret);
		return -1;
	}
	spi_free();
	/* step 2 */
	spi_sel();
	send_data(write_cmd);
	udelay(10);
	send_data(page_reg);
	udelay(10);
	send_data(page_addr);//0x10
	spi_free();
	/* step 3 */
	printf("sercomm debug line: %d\n", __LINE__);
	static int times_ack = 10000;
	spi_sel();
	do
	{
		send_data(0x10);
		udelay(10);
		send_data(reg_addr);
		udelay(10);
		ret = rev_data();
		if(ret&0x01)
		{	
			printf("status register and times_ack is %d  %d\n",ret,times_ack);
			break;
		}
		if(times_ack==0) 
		{
			printf("status register and times_ack is %d  %d\n",ret,times_ack);
			return -1;
		}
	}while(times_ack--);
	printf("RACK GOOD\n");
	udelay(10);
	byte1 = rev_data();
	udelay(10);
	byte2 = rev_data();
	udelay(10);
	byte3 = rev_data();
	udelay(10);
	byte4 = rev_data();
	spi_free();
	//	value = byte3 | (byte4 << 8);//byte1 | (byte2 << 8) ;
	value = byte1 | (byte2 << 8) ;//| (byte3 << 16) | (byte4 << 24) ;
	return value;		
}
#endif



