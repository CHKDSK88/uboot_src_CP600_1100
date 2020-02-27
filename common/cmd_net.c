/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * Boot support
 */
#include <common.h>
#include <command.h>
#include <net.h>
#include "../drivers/twsi.h"
#if (CONFIG_COMMANDS & CFG_CMD_NET)

int voltage_item = 0;


extern int do_bootm (cmd_tbl_t *, int, int, char *[]);

extern void cp_disable_network_ports(void);
extern void cp_enable_network_ports(void);
extern unsigned int cp_is_network_enabled(void);

static int netboot_common (proto_t, cmd_tbl_t *, int , char *[]);

int do_bootp (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	return netboot_common (BOOTP, cmdtp, argc, argv);
}

U_BOOT_CMD(
	bootp,	3,	1,	do_bootp,
	"bootp\t- boot image via network using BootP/TFTP protocol\n",
	"[loadAddress] [bootfilename]\n"
);

int do_tftpb (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	return netboot_common (TFTP, cmdtp, argc, argv);
}

U_BOOT_CMD(
	tftpboot,	3,	1,	do_tftpb,
	"tftpboot- boot image via network using TFTP protocol\n",
	"[loadAddress] [bootfilename]\n"
);

#define SERCOMM_TOOL_OFFSET     0x96000
#define SERCOMM_TOOL_SIZE       0xa000
#define SERCOMM_DOWN_ENTRY      0x2200000

extern void mvEthSwitchRegWrite(unsigned int ethPortNum, unsigned int phyAddr, unsigned int regOffs, unsigned short int data);

extern void mvEthSwitchRegRead(unsigned int ethPortNum, unsigned int phyAddr, unsigned int regOffs, unsigned short int *data);
int do_switch_mii_w (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    int port;
    int phyAddr;
    int reg;
    unsigned short int data;
  
    if(argc < 5) {
        printf("switch_mii_w port phyAddr reg value\n");
        return 0;
    }

    port = simple_strtoul(argv[1],NULL,16);
    phyAddr = simple_strtoul(argv[2],NULL,16);
    reg = simple_strtoul(argv[3],NULL,16);
    data = simple_strtoul(argv[4],NULL,16);
    
    mvEthSwitchRegWrite(port, phyAddr, reg, data);

    return 0;
}

U_BOOT_CMD(
        switch_mii_w, 7,      1,      do_switch_mii_w,
        "switch_mii_w\n",
        "switch_mii_w\n"
);

int do_switch_mii_r (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    int port;
    int phyAddr;
    int reg;
    unsigned short int data;
  
    if(argc < 4) {
        printf("switch_mii_r port phyAddr reg\n");
        return 0;
    }

    port = simple_strtoul(argv[1],NULL,16);
    phyAddr = simple_strtoul(argv[2],NULL,16);
    reg = simple_strtoul(argv[3],NULL,16);
    
    mvEthSwitchRegRead(port, phyAddr, reg, &data);
    printf("read port: 0x%x, phyAddr 0x%x, reg 0x%x, data: 0x%x\n", port, phyAddr, reg, data);

    return 0;
}

U_BOOT_CMD(
        switch_mii_r, 7,      1,      do_switch_mii_r,
        "switch_mii_r\n",
        "switch_mii_r\n"
);


extern int lm63_get(unsigned char * data,unsigned char len);
extern void rtc_s3530_init(void);
extern void twsi_init(void);
extern int get_argv(char *argv[],unsigned char * set_time,unsigned int output_type);
extern void rtc_s3530_format(unsigned int fmt,unsigned char *data,unsigned char len);
int is_init_i2c_dev = 0;

extern int i2c_get(unsigned char * data,unsigned char len, unsigned char IC_ADDRESS, unsigned int reg);
extern int i2c_set_8bit(unsigned char IC_ADDRESS, unsigned int reg, unsigned char *data);
int do_sc_i2c (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    unsigned char data[8];
    int i;
	unsigned int reg;
	unsigned char addr;

    if (argc < 2) {
        printf("%s",cmdtp->usage);
        return 0;
    }

    twsi_init();
    memset(data,0,8*sizeof(unsigned char));

    if (strnicmp(argv[1], "test", sizeof("test")) == 0) {
		reg = simple_strtoul(argv[2], NULL, 16);
        for(i = 0; i < 0x7f; i++) {
            printf("addr: 0x%x reg: 0x%x ", i, reg);
            i2c_get(data, 1, i, reg);
            printf("0x%x\n", data[0]);
    	}
    } else if(strnicmp(argv[1], "r", sizeof("r")) == 0) {
		addr = simple_strtoul(argv[2], NULL, 16);
		reg = simple_strtoul(argv[3], NULL, 16);
        printf("addr: 0x%x reg: 0x%x ", addr, reg);
        i2c_get(data, 1, addr, reg);
        printf("0x%x\n", data[0]);
    } else if(strnicmp(argv[1], "w", sizeof("w")) == 0) {
		addr = simple_strtoul(argv[2], NULL, 16);
		reg = simple_strtoul(argv[3], NULL, 16);
		data[0] = simple_strtoul(argv[4], NULL, 16);
        printf("addr: 0x%x reg: 0x%x ", addr, reg);
        printf("0x%x\n", data[0]);
        i2c_set_8bit(addr, reg, data);
    }

    return 0;
}

U_BOOT_CMD(
	sc_i2c,	7,	1,	do_sc_i2c,
	"sc_i2c --- sc i2c control command\n",
	"sc_i2c\nsc_i2c test reg_addr ---- get reg_addr value from device addr 0x00-0x7f\n"
	"sc_i2c r device_addr reg_addr ---- get reg_addr value at device_addr\n"
);
#ifdef SEATTLE
#define SX1505_GPIO_DIR    0x01
#define SX1505_GPIO_VALUE  0x00
int do_sx1505 (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    unsigned char data[8];
    int i;
	unsigned int reg;
	unsigned char addr;
	unsigned char gpio_num;

    if (argc < 4) {
        printf("%s",cmdtp->usage);
        return 0;
    }

    twsi_init();
    memset(data,0,8*sizeof(unsigned char));

    addr = simple_strtoul(argv[1], NULL, 16);

    if(strnicmp(argv[2], "gpio", sizeof("gpio")) == 0) {
		gpio_num = simple_strtoul(argv[4], NULL, 16);

        i2c_get(data, 1, addr, SX1505_GPIO_DIR);
        data[0] &= ~(1 << gpio_num);
        i2c_set_8bit(addr, SX1505_GPIO_DIR, data);

		i2c_get(data, 1, addr, SX1505_GPIO_VALUE);
		if(memcmp(argv[3], "hi", 2) == 0){
            data[0] |= (1 << gpio_num);
        } else if(memcmp(argv[3], "low", 3) == 0){
            data[0] &= ~(1 << gpio_num);
        }
        i2c_set_8bit(addr, SX1505_GPIO_VALUE, data);

        printf("set device[0x%2.2x] gpio[%d] %s\n", addr, gpio_num, argv[3]);
    } else {
        printf("can not support this option, please type help %s\n", argv[0]);
    }

    return 0;
}

U_BOOT_CMD(
	sx1505,	7,	1,	do_sx1505,
	"sx1505 --- control sx1505 gpio \n",
	"sx1505\nsx1505 device_id gpio hi/low gpio_num  ---- let the sx1505 gpio hi or low\n"
	"\n"
);
int do_extend (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    int i;
    unsigned int gpio,state,extenderNo;
    static int extender_address[2]={IC_SG80_EXTEND1,IC_SG80_EXTEND2};


    if (argc < 2) {
        printf("%s",cmdtp->usage);
        return 0;
    }


    if(strnicmp(argv[1], "w", sizeof("w")) == 0) {
    	extenderNo = simple_strtoul(argv[2], NULL, 16);
    	gpio = simple_strtoul(argv[3], NULL, 16);
    	state = simple_strtoul(argv[5], NULL, 16);
		printf("gpio:%d ", gpio);
		extend_gpio_ctl(extender_address[extenderNo-1],gpio,state);
		printf("state=%d\n", state);
    }
    return 0;
}
U_BOOT_CMD(
	extend,	7,	1,	do_extend,
	"extender gpio\n",
	"extend extender_number[1/2] gpio_num [0/1]\n"
);
#endif
int do_gpio_ctl(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    int gpio_num;

    if(argc <= 1) {
        printf("argument num is error, type help %s\n", argv[0]);
        return 1;
    }
    gpio_num = argv[1];
    if (argv[2][0] =='i'){
    	if(memcmp(argv[3], "hi", 2) == 0){
    		do_GPIO_input(gpio_num, 1);
    	} else if(memcmp(argv[3], "low", 3) == 0){
    		do_GPIO_input(gpio_num, 0);
    	}
    }
    else{//out
    	if(memcmp(argv[3], "hi", 2) == 0){
    		do_GPIO_output(gpio_num, 1);
		} else if(memcmp(argv[3], "low", 3) == 0){
			do_GPIO_output(gpio_num, 0);
		}

    }

    return 0;
}

U_BOOT_CMD(
	gpio,	7,	1,	do_gpio_ctl,
	"gpio --- do gpio option",
	"gpio [i/o] [hi/low] gpio_num\n"
);

/* rtc command */
extern void read_rtc(void);
extern void write_rtc(unsigned char* rtcTime);
int do_rtc_read(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    read_rtc();
    return 0;
}

int do_rtc_write(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    unsigned char rtcTime[8];
    char *tmp, *cur;
    int i;

    if(argc <= 1) {
        printf("argument num is error, type help %s\n", argv[0]);
        return 1;
    }

    tmp = argv[1];

    cur = strsep(&tmp, "-:");
    rtcTime[0] = (simple_strtoul(cur, NULL, 10) % 100);

    for(i = 1; i < 3; i++) {
        cur = strsep(&tmp, "-:");
        rtcTime[i] = simple_strtoul(cur, NULL, 10);
    }
   
    for(i = 4; i < 7; i++) {
        cur = strsep(&tmp, "-:");
        rtcTime[i] = simple_strtoul(cur, NULL, 10);
    }

    cur = strsep(&tmp, "-:");
    rtcTime[3] = simple_strtoul(cur, NULL, 10);

    write_rtc(rtcTime);
    return 0;
}

U_BOOT_CMD(
        rtc_r,    4,    1,     do_rtc_read,
        "rtc_r    - read rtc time\n",
        "rtc_r\n    - will printf format as YY.MM.DD HH:mm:ss week\n"
);


U_BOOT_CMD(
        rtc_w,    4,    1,     do_rtc_write,
        "rtc_w    - set rtc time\n",
        "rtc_w YY-MM-DD-HH-mm-ss-week\n    - set rtc time, YY must > 2000\n"
);
/*
Temporarily disabled, reenable when cp_led_do works for both Seattle and London HW
int do_ledctl (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    unsigned int gpio;
    unsigned char status;

    if (argc < 3)
    {
        printf("%s",cmdtp->usage);
        return 0;
    }

    gpio = simple_strtoul(argv[1],NULL,10);
    status = simple_strtoul(argv[2],NULL,10);

	cp_do_led(gpio, status);
//    printf("set gpio %d to %d\n", gpio, status);
    return 0;
}

U_BOOT_CMD(
	ledctl,	7,	1,	do_ledctl,
	"ledctl gpionum 0/1\n",
	"ledctl gpionum 0/1\n"
);
*/
static int send_command1(int command, int page_addr, int buf_addr, int additional_byte)
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

extern unsigned char rev_data();
extern void send_data(unsigned char spi_data);
extern void spi_sel();
extern void spi_free();

void read_buf1(int number, char *buf)
{
	while(number)
	{
		*buf =  rev_data();
		number --;
		buf++;
	}
}

int do_sc_spi(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	unsigned char data;
	unsigned char buf[528];
	int i;
	int j=0;
	unsigned char addr_msb,	addr_middle, addr_lsb;

    if(argc <= 1) {
        printf("argument num is error, type help %s\n", argv[0]);
        return 1;
    }

    switch(argv[1][0]){
    case 'e':
    	spi_free();
    	spi_sel();
    	send_data(0xD4);
    	send_data(0x0);
    	send_data(0x0);
    	send_data(0x0);
    	for(i=0;i<528;i++)
		{
			data = rev_data();
			printf("0x%x\n", data);
		}
    	spi_free();
        break;
    case 'w':
        data= simple_strtoul(argv[2], NULL, 16);
        send_data(data);
        break;
    case 's':
        spi_sel();
        break;
    case 'f':
        spi_free();
        break;
    case 'i':
    	//printf("DSL OD?%d",is_primary_dsl_ok());
        break;
    case 'b':
       	spi_sel();
       	send_data(0x84);//buffer 1 write
       	send_data(0x0);
       	send_data(0x0);
       	send_data(0x0);
       	for(i=0;i<100;i++)
       		send_data(0x3);
       	//do_GPIO_output(39, 0);
       	//udelay(10);
       	do_GPIO_output(39, 1);
       	spi_free();
       	spi_sel();
       	send_data(0x83);//page earse+write
       	send_data(0x00);
       	send_data(0x0);
       	send_data(0x0);
    	do_GPIO_output(39, 1);
      /*send_data(0x81);//page earse
       	send_data(0x10);//1F /3e
       	send_data(0x0);
       	send_data(0x0);
       	do_GPIO_output(39, 1);
       	spi_free();
       	spi_sel();
       	send_data(0x88);//write buffer 1 to page
       	send_data(0x10);
       	send_data(0x0);
       	send_data(0x0);
       	do_GPIO_output(39, 1);
       	spi_free();*/
       	break;
	case 'n':		
		printf("*************%d 528***************\n", argv[2][0]);
		spi_sel();
		send_command1(0x52, argv[2][0], 0, 4);
		//do_GPIO_output(40, 1);
		spidelay();
		read_buf1(528, buf);
		for(j=0;j<528;j++)
		{
			printf("0x%x ",buf[j]);
		}
		spi_free();
		break;
    case 'r':
    	printf("*************0 528***************\n", data);
		spi_sel();
		send_command1(0x52, 0, 0, 4);
		//do_GPIO_output(40, 1);
		spidelay();
		read_buf1(528, buf);
		for(j=0;j<528;j++)
		{
			printf("0x%x ",buf[j]);
		}

		spi_free();
    	printf("*************1 528***************\n", data);
    	spi_sel();
    	send_command1(0x52, 1, 0, 4);
    	//do_GPIO_output(40, 1);
    	spidelay();
    	read_buf1(528, buf);
    	for(j=0;j<528;j++)
    	{
    		printf("0x%x ",buf[j]);
    	}

		spi_free();
		printf("*************10 528***************\n", data);

		spi_sel();
		send_command1(0xD2, 10, 0, 4);
		do_GPIO_output(40, 1);
		spidelay();
		read_buf1(528, buf);
		for(j=0;j<528;j++)
		{
			printf("0x%x ", buf[j]);
		}
		do_GPIO_output(39, 1);

		printf("*************100 528***************\n", data);
		spi_sel();
		send_command1(0xD2, 100, 0, 4);
		do_GPIO_output(40, 1);
		spidelay();
		read_buf1(528, buf);
		for(j=0;j<528;j++)
		{
			printf("0x%x ", buf[j]);
		}
		do_GPIO_output(39, 1);
		spi_free();
		printf("*************1000 528***************\n", data);
		spi_sel();
		send_command1(0xD2, 1000, 0, 4);
		do_GPIO_output(40, 1);
		spidelay();
		read_buf1(528, buf);
		for(j=0;j<528;j++)
		{
			printf("0x%x ", buf[j]);
		}
		do_GPIO_output(39, 1);
		spi_free();
		printf("**************************************\n", data);
		break;
    case 't':
	    spi_sel();
        send_data(0x9f);
        data = rev_data();
        printf("0x%x\n", data);
        data = rev_data();
        printf("0x%x\n", data);
        data = rev_data();
        printf("0x%x\n", data);
        data = rev_data();
        printf("0x%x\n", data);
        spi_free();
        break;
    default:
        break;
	}

    return 0;
}

U_BOOT_CMD(
        sc_spi,    4,    1,     do_sc_spi,
        "sc_spi    - do spi read/write\n",
        "sc_spi\n    - read/write spi\n"
);

int do_rarpb (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	return netboot_common (RARP, cmdtp, argc, argv);
}

U_BOOT_CMD(
	rarpboot,	3,	1,	do_rarpb,
	"rarpboot- boot image via network using RARP/TFTP protocol\n",
	"[loadAddress] [bootfilename]\n"
);

#if (CONFIG_COMMANDS & CFG_CMD_DHCP)
int do_dhcp (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	return netboot_common(DHCP, cmdtp, argc, argv);
}

U_BOOT_CMD(
	dhcp,	3,	1,	do_dhcp,
	"dhcp\t- invoke DHCP client to obtain IP/boot params\n",
	"\n"
);
#endif	/* CFG_CMD_DHCP */

#if (CONFIG_COMMANDS & CFG_CMD_NFS)
int do_nfs (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	return netboot_common(NFS, cmdtp, argc, argv);
}

U_BOOT_CMD(
	nfs,	3,	1,	do_nfs,
	"nfs\t- boot image via network using NFS protocol\n",
	"[loadAddress] [host ip addr:bootfilename]\n"
);
#endif	/* CFG_CMD_NFS */

static void netboot_update_env (void)
{
	char tmp[22];

	if (NetOurGatewayIP) {
		ip_to_string (NetOurGatewayIP, tmp);
		setenv ("gatewayip", tmp);
	}

	if (NetOurSubnetMask) {
		ip_to_string (NetOurSubnetMask, tmp);
		setenv ("netmask", tmp);
	}

	if (NetOurHostName[0])
		setenv ("hostname", NetOurHostName);

	if (NetOurRootPath[0])
		setenv ("rootpath", NetOurRootPath);

	if (NetOurIP) {
		ip_to_string (NetOurIP, tmp);
		setenv ("ipaddr", tmp);
	}

	if (NetServerIP) {
		ip_to_string (NetServerIP, tmp);
		setenv ("serverip", tmp);
	}

	if (NetOurDNSIP) {
		ip_to_string (NetOurDNSIP, tmp);
		setenv ("dnsip", tmp);
	}
#if (CONFIG_BOOTP_MASK & CONFIG_BOOTP_DNS2)
	if (NetOurDNS2IP) {
		ip_to_string (NetOurDNS2IP, tmp);
		setenv ("dnsip2", tmp);
	}
#endif
	if (NetOurNISDomain[0])
		setenv ("domain", NetOurNISDomain);

#if (CONFIG_COMMANDS & CFG_CMD_SNTP) && (CONFIG_BOOTP_MASK & CONFIG_BOOTP_TIMEOFFSET)
	if (NetTimeOffset) {
		sprintf (tmp, "%d", NetTimeOffset);
		setenv ("timeoffset", tmp);
	}
#endif
#if (CONFIG_COMMANDS & CFG_CMD_SNTP) && (CONFIG_BOOTP_MASK & CONFIG_BOOTP_NTPSERVER)
	if (NetNtpServerIP) {
		ip_to_string (NetNtpServerIP, tmp);
		setenv ("ntpserverip", tmp);
	}
#endif
}

static int
netboot_common (proto_t proto, cmd_tbl_t *cmdtp, int argc, char *argv[])
{
	char *s;
	int   rcode = 0;
	int   size;
	unsigned int net_was_enabled = 0;

	/* pre-set load_addr */
	if ((s = getenv("loadaddr")) != NULL) {
		load_addr = simple_strtoul(s, NULL, 16);
	}

    switch (argc) {
	case 1:
		break;

	case 2:	/* only one arg - accept two forms:
		 * just load address, or just boot file name.
		 * The latter form must be written "filename" here.
		 */
		if (argv[1][0] == '"') {	/* just boot filename */
			copy_filename (BootFile, argv[1], sizeof(BootFile));
		} else {			/* load address	*/
			load_addr = simple_strtoul(argv[1], NULL, 16);
		}
		break;

	case 3:	load_addr = simple_strtoul(argv[1], NULL, 16);
		copy_filename (BootFile, argv[2], sizeof(BootFile));

		break;

	default: printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}


	net_was_enabled = cp_is_network_enabled();
	if (!net_was_enabled)
	{
		cp_enable_network_ports();
	}

	size = NetLoop(proto);
	if (size < 0)
	{
		rcode = 1;
		goto CleanUp;
	}
	/* NetLoop ok, update environment */
	netboot_update_env();

	/* done if no file was loaded (no errors though) */
	if (size == 0)
	{
		rcode = 0;
		goto CleanUp;
	}

	/* flush cache */
	flush_cache(load_addr, size);

	/* Loading ok, check if we should attempt an auto-start */
	if (((s = getenv("autostart")) != NULL) && (strcmp(s,"yes") == 0)) {
		char *local_args[2];
		local_args[0] = argv[0];
		local_args[1] = NULL;

		printf ("Automatic boot of image at addr 0x%08lX ...\n",
			load_addr);
		rcode = do_bootm (cmdtp, 0, 1, local_args);
	}

#ifdef CONFIG_AUTOSCRIPT
	if (((s = getenv("autoscript")) != NULL) && (strcmp(s,"yes") == 0)) {
		printf("Running autoscript at addr 0x%08lX ...\n", load_addr);
		rcode = autoscript (load_addr);
	}
#endif
CleanUp:
	if (cp_is_network_enabled() && !net_was_enabled)
	{
		cp_disable_network_ports();
	}
	return rcode;
}

#if (CONFIG_COMMANDS & CFG_CMD_PING)
int do_ping (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	unsigned int net_was_enabled = 0;
	int rv = 0;
	
	if (argc < 2)
		return -1;

	NetPingIP = string_to_ip(argv[1]);
	if (NetPingIP == 0) {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return -1;
	}

	net_was_enabled = cp_is_network_enabled();
	if (!net_was_enabled)
	{
		cp_enable_network_ports();
	}
	if (NetLoop(PING) < 0) {
		printf("ping failed; host %s is not alive\n", argv[1]);
		rv = 1;
		goto CleanUp;
	}

	printf("host %s is alive\n", argv[1]);

CleanUp:
	if (cp_is_network_enabled() && !net_was_enabled)
	{
		cp_disable_network_ports();
	}
	return rv;
}

U_BOOT_CMD(
	ping,	2,	1,	do_ping,
	"ping\t- send ICMP ECHO_REQUEST to network host\n",
	"pingAddress\n"
);
#endif	/* CFG_CMD_PING */

#if (CONFIG_COMMANDS & CFG_CMD_CDP)

static void cdp_update_env(void)
{
	char tmp[16];

	if (CDPApplianceVLAN != htons(-1)) {
		printf("CDP offered appliance VLAN %d\n", ntohs(CDPApplianceVLAN));
		VLAN_to_string(CDPApplianceVLAN, tmp);
		setenv("vlan", tmp);
		NetOurVLAN = CDPApplianceVLAN;
	}

	if (CDPNativeVLAN != htons(-1)) {
		printf("CDP offered native VLAN %d\n", ntohs(CDPNativeVLAN));
		VLAN_to_string(CDPNativeVLAN, tmp);
		setenv("nvlan", tmp);
		NetOurNativeVLAN = CDPNativeVLAN;
	}

}

int do_cdp (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int r;

	r = NetLoop(CDP);
	if (r < 0) {
		printf("cdp failed; perhaps not a CISCO switch?\n");
		return 1;
	}

	cdp_update_env();

	return 0;
}

U_BOOT_CMD(
	cdp,	1,	1,	do_cdp,
	"cdp\t- Perform CDP network configuration\n",
);
#endif	/* CFG_CMD_CDP */

#if (CONFIG_COMMANDS & CFG_CMD_SNTP)
int do_sntp (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	char *toff;

	if (argc < 2) {
		NetNtpServerIP = getenv_IPaddr ("ntpserverip");
		if (NetNtpServerIP == 0) {
			printf ("ntpserverip not set\n");
			return (1);
		}
	} else {
		NetNtpServerIP = string_to_ip(argv[1]);
		if (NetNtpServerIP == 0) {
			printf ("Bad NTP server IP address\n");
			return (1);
		}
	}

	toff = getenv ("timeoffset");
	if (toff == NULL) NetTimeOffset = 0;
	else NetTimeOffset = simple_strtol (toff, NULL, 10);

	if (NetLoop(SNTP) < 0) {
		printf("SNTP failed: host %s not responding\n", argv[1]);
		return 1;
	}

	return 0;
}

U_BOOT_CMD(
	sntp,	2,	1,	do_sntp,
	"sntp\t- synchronize RTC via network\n",
	"[NTP server IP]\n"
);
#endif	/* CFG_CMD_SNTP */
#endif
