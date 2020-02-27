/* 
   data is driving by the falling edge and sampling by the rising edge of clock
   CPHA=1
   CPOL=1
   发送高位先发送，接收高位先接(MSB-->>LSB)
   */
#include <common.h>
#include <spi_flash.h>
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

#define WRITE_DATA_TO_BUF1 0x84
#define WRITE_DATA_TO_BUF2 0x87
#define ERASE_AND_WRITE_BUF1_MAIN_MEMORY 0x83
#define ERASE_AND_WRITE_BUF2_MAIN_MEMORY 0x86
#define READ_MAIN_MEMORY 0x52
#define ERASE_PAGE 0x81

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


void do_spi_init()
{
//	cvmx_gpio_cfg(SPI_SCK_GPIO, out_put);
//	cvmx_gpio_cfg(SPI_CHIPSEL, out_put);
//	cvmx_gpio_cfg(SPI_MOSI_GPIO, out_put);
//	cvmx_gpio_cfg(SPI_MISO_GPIO, in_put);

	do_GPIO_output(SPI_CHIPSEL, 1);
	do_GPIO_output(SPI_SCK_GPIO, 1);
}
void spi_erase(void)
{
	int i;


	for (i=0; i<SPI_EEPROM_MAX_PAGES; i++)
	{
		if( (i % 32) == 0 )
			printf(".");
		if (earse_page(i) != 0)
		{
			printf("Error: Erase failed\n");
			return;
		}
	}
	printf("\nErase completed\n");
}
#if 0
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
	udelay(10);
	do_GPIO_output(SPI_CHIPSEL, 0);
	udelay(10);
}

void spi_free()
{
	do_GPIO_output(SPI_SCK_GPIO, 1);
	udelay(10);
	do_GPIO_output(SPI_CHIPSEL, 1);
	udelay(10);
}


void do_spi_init()
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
		spidelay(); /** T(setup) */
		setsck(HIGH);
		spidelay();
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
#endif
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
			spidelay();
			if(times--<0)
			{
				printf("waiting ready error!!!\n");
				return -1;
			}
		}
		spi_free();
	}
	spi_free();
	return -1;
}

static void write_buf(int number, unsigned long buf)
{
	char *p=(char *)buf;

	while(number)
	{

		send_data(*p);
		number --;
		p++;
	}


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
	int counter=0;
	if(number < n)
		n = number;

	while(number > 0)
	{
		/* write data to buf1 */
		send_command(WRITE_DATA_TO_BUF1, 0, buf_addr, 0);
		write_buf(n, buf);
		do_GPIO_output(SPI_CHIPSEL,1);

		spidelay();
		/* waiting buf2 programming completed */
		spidelay();

		/* Send buf1 to spi flash */
		spi_free();
		spi_sel();
		send_command(ERASE_AND_WRITE_BUF1_MAIN_MEMORY, pg_num, 0, 0);
		do_GPIO_output(SPI_CHIPSEL, 1);

		spidelay();

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

		spi_free();
		spi_sel();

		/* write data to buf2 */

		spidelay();

		send_command(WRITE_DATA_TO_BUF2, 0, buf_addr, 0);
		write_buf(n, buf);

		do_GPIO_output(SPI_CHIPSEL, 1);


		spi_free();
		spi_sel();
		/* send buf2 to flash */
		send_command(ERASE_AND_WRITE_BUF2_MAIN_MEMORY, pg_num, 0, 0);
		do_GPIO_output(SPI_CHIPSEL, 1);

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

		if( (pg_num % 32 ) == 0 )
			printf(".");
		spi_free();
		spi_sel();
	}

	do_GPIO_output(SPI_CHIPSEL, 0);
	spidelay();
	printf("\n");

	return 0;
}

int spi_write_file(unsigned long from, unsigned long number)
{
	return eeprom_write_bytes(0,0,number,from);
}
int spi_get_page_size(void)
{
	return SPI_EEPROM_PAGE_SIZE;
}

int earse_page(int page)
{
	int i;
	
	spi_free();
	spi_sel();
	spidelay();
	
	send_command(ERASE_PAGE, page, 0, 2); /* page earse */
	spidelay();
	
	do_GPIO_output(SPI_CHIPSEL, 1);
	
	// HS: Taken from AT45DB161B Spec -
	// The erase operation is internally self-timed and should take place in a maximum time of Tpe (==8ms). 
	// During this time, the status register will indicate that the part is busy.
	udelay(100);
	return 0;
}
void read_buf(int number, char *buf)
{
	while(number)
	{
		*buf =  rev_data();
		number --;
		buf++;
	}

}
int spi_read(int pg_num, unsigned char buf[SPI_EEPROM_PAGE_SIZE])
{
	int start_addr = 0;
	char  data;

	spi_free();
	spi_sel();
	spidelay();  

	send_command(READ_MAIN_MEMORY, pg_num, start_addr, 4);
	do_GPIO_output(SPI_SCK_GPIO, 1);
	spidelay();
	read_buf(SPI_EEPROM_PAGE_SIZE, buf);


	spi_free();
	return 0;

}

/*******************************************************************/
