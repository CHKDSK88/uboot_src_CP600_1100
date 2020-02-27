/*
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
/*******************************************************************************
Copyright (C) Marvell International Ltd. and its affiliates

********************************************************************************
Marvell GPL License Option

If you received this File from Marvell, you may opt to use, redistribute and/or 
modify this File in accordance with the terms and conditions of the General 
Public License Version 2, June 1991 (the "GPL License"), a copy of which is 
available along with the File in the license.txt file or by writing to the Free 
Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or 
on the worldwide web at http://www.gnu.org/licenses/gpl.txt. 

THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED 
WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY 
DISCLAIMED.  The GPL License provides additional details about this warranty 
disclaimer.

*******************************************************************************/

#include <common.h>
#include "mvTypes.h"
#include <configs/mv_kw.h>
#include "mvBoardEnvLib.h"
#include "mvCpuIf.h"
#include "mvCtrlEnvLib.h"
#include "mv_mon_init.h"
#include "mvDebug.h"
#include "device/mvDevice.h"
#include "twsi/mvTwsi.h"
#include "eth/mvEth.h"
#include "ethSwitch/mvSwitchRegs.h"
#include "pex/mvPex.h"
#include "gpp/mvGpp.h"
#include "sys/mvSysUsb.h"
#include "mv_service.h"
#include "../../../drivers/twsi.h"

#ifdef MV_INCLUDE_RTC
#include "rtc/integ_rtc/mvRtc.h"
#include "rtc.h"
#elif CONFIG_RTC_DS1338_DS1339
#include "rtc/ext_rtc/mvDS133x.h"
#endif

#if defined(MV_INCLUDE_XOR)
#include "xor/mvXor.h"
#endif
#if defined(MV_INCLUDE_IDMA)
#include "sys/mvSysIdma.h"
#include "idma/mvIdma.h"
#endif
#if defined(MV_INCLUDE_USB)
#include "usb/mvUsb.h"
#endif

#include "cpu/mvCpu.h"
#include "nand.h"
#ifdef CONFIG_PCI
#   include <pci.h>
#endif
#include "pci/mvPciRegs.h"

#include <asm/arch/vfpinstr.h>
#include <asm/arch/vfp.h>

#include "net.h"
#include <command.h>

/* #define MV_DEBUG */
#ifdef MV_DEBUG
#define DB(x) x
#else
#define DB(x)
#endif

#ifndef BIT
#define BIT(x)	(1<<(x))
#endif

/* Macros to convert int macro to string by preprocessor */
#ifndef CP_TOSTRING
#define CP_STRINGIFY(x) #x
#define CP_TOSTRING(x) CP_STRINGIFY(x) 
#endif // CP_TOSTRING
#define MPP0            *((unsigned int *) 0xf1010000)
/* CPU address decode table. */
MV_CPU_DEC_WIN mvCpuAddrWinMap[] = MV_CPU_IF_ADDR_WIN_MAP_TBL;

static void mvHddPowerCtrl(void);

#if (CONFIG_COMMANDS & CFG_CMD_RCVR)
static void recoveryDetection(void);
void recoveryHandle(void);
static u32 rcvrflag = 0;
static s32 g_cp_abort_boot = 0;
static s32 g_cp_usb_attempted = 0;
static u32 g_cp_network_enabled = 0;

#endif
void mv_cpu_init(void);
#if defined(MV_INCLUDE_CLK_PWR_CNTRL)
int mv_set_power_scheme(void);
#endif

#ifdef	CFG_FLASH_CFI_DRIVER
MV_VOID mvUpdateNorFlashBaseAddrBank(MV_VOID);
int mv_board_num_flash_banks;
extern flash_info_t	flash_info[]; /* info for FLASH chips */
extern unsigned long flash_add_base_addr (uint flash_index, ulong flash_base_addr);
#endif	/* CFG_FLASH_CFI_DRIVER */

#if defined(MV_INCLUDE_UNM_ETH) || defined(MV_INCLUDE_GIG_ETH)
extern MV_VOID mvBoardEgigaPhySwitchInit(void);
#endif 

#if (CONFIG_COMMANDS & CFG_CMD_NAND)
/* Define for SDK 2.0 */
int __aeabi_unwind_cpp_pr0(int a,int b,int c) {return 0;}
int __aeabi_unwind_cpp_pr1(int a,int b,int c) {return 0;}
#endif

extern nand_info_t nand_info[];       /* info for NAND chips */
MV_VOID mvMppModuleTypePrint(MV_VOID);

#ifdef MV_NAND_BOOT
extern MV_U32 nandEnvBase;
#endif

extern void cp_disable_read_only(void);
extern void cp_enable_read_only(void);

extern int do_fat_fsload (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
extern int do_ext2load (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
extern int do_bootd (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);

extern void restart_pci(void);


/* Define for SDK 2.0 */
int raise(void) {return 0;}
void print_mvBanner(void)
{
#ifdef CONFIG_SILENT_CONSOLE
	DECLARE_GLOBAL_DATA_PTR;
	gd->flags |= GD_FLG_SILENT;
#endif
	printf("\n");
	printf("         __  __                      _ _\n");
	printf("        |  \\/  | __ _ _ ____   _____| | |\n");
	printf("        | |\\/| |/ _` | '__\\ \\ / / _ \\ | |\n");
	printf("        | |  | | (_| | |   \\ V /  __/ | |\n");
	printf("        |_|  |_|\\__,_|_|    \\_/ \\___|_|_|\n");
	printf(" _   _     ____              _\n");
	printf("| | | |   | __ )  ___   ___ | |_ \n");
	printf("| | | |___|  _ \\ / _ \\ / _ \\| __| \n");
	printf("| |_| |___| |_) | (_) | (_) | |_ \n");
	printf(" \\___/    |____/ \\___/ \\___/ \\__| ");
#if !defined(MV_NAND_BOOT)
#if defined(MV_INCLUDE_MONT_EXT)
    mvMPPConfigToSPI();
	if(!enaMonExt())
		printf(" ** LOADER **"); 
	else
		printf(" ** MONITOR **");
    mvMPPConfigToDefault();
#else
	printf(" ** Forcing LOADER mode only **"); 
#endif /* MV_INCLUDE_MONT_EXT */
#endif
	return;
}

void print_cpBanner(void)
{
#ifdef CONFIG_SILENT_CONSOLE
	DECLARE_GLOBAL_DATA_PTR;
	gd->flags |= GD_FLG_SILENT;
#endif

printf("\n\n");
printf("   ______  __                    __        _______           _            _    \n");
printf(" .' ___  |[  |                  [  |  _   |_   __ \\         (_)          / |_  \n");
printf("/ .'   \\_| | |--.  .---.  .---.  | | / ]    | |__) | .--.   __   _ .--. `| |-' \n");
printf("| |        | .-. |/ /__\\\\/ /'`\\] | '' <     |  ___// .'`\\ \\[  | [ `.-. | | |   \n");
printf("\\ `.___.'\\ | | | || \\__.,| \\__.  | |`\\ \\   _| |_   | \\__. | | |  | | | | | |,  \n");
printf(" `.____ .'[___]|__]'.__.''.___.'[__|  \\_] |_____|   '.__.' [___][___||__]\\__/  \n");

#if !defined(MV_NAND_BOOT)
#if defined(MV_INCLUDE_MONT_EXT)
    mvMPPConfigToSPI();
	if(!enaMonExt())
		printf(" ** LOADER **"); 
	else
		printf(" ** MONITOR **");
    mvMPPConfigToDefault();
#else
	printf(" ** Forcing LOADER mode only **"); 
#endif /* MV_INCLUDE_MONT_EXT */
#endif
	return;
}

void print_dev_id(void){
	static char boardName[30];

	mvBoardNameGet(boardName);
	
#if defined(MV_CPU_BE)
	printf("\n ** MARVELL BOARD: %s BE ",boardName);
#else
	printf("\n ** MARVELL BOARD: %s LE ",boardName);
#endif
    return;
}


void maskAllInt(void)
{
        /* mask all external interrupt sources */
        MV_REG_WRITE(CPU_MAIN_IRQ_MASK_REG, 0);
        MV_REG_WRITE(CPU_MAIN_FIQ_MASK_REG, 0);
        MV_REG_WRITE(CPU_ENPOINT_MASK_REG, 0);
        MV_REG_WRITE(CPU_MAIN_IRQ_MASK_HIGH_REG, 0);
        MV_REG_WRITE(CPU_MAIN_FIQ_MASK_HIGH_REG, 0);
        MV_REG_WRITE(CPU_ENPOINT_MASK_HIGH_REG, 0);
}

/* init for the Master*/
void misc_init_r_dec_win(void)
{
#if defined(MV_INCLUDE_USB)
	{
		char *env;

		env = getenv("usb0Mode");
		if((!env) || (strcmp(env,"device") == 0) || (strcmp(env,"Device") == 0) )
		{
			printf("USB 0: device mode\n");	
			mvUsbInit(0, MV_FALSE);
		}
		else
		{
			printf("USB 0: host mode\n");	
			mvUsbInit(0, MV_TRUE);
		}
	}
#endif/* #if defined(MV_INCLUDE_USB) */

#if defined(MV_INCLUDE_XOR)
	mvXorInit();
#endif

#if defined(MV_INCLUDE_CLK_PWR_CNTRL)
	mv_set_power_scheme();
#endif

    return;
}


/*
 * Miscellaneous platform dependent initialisations
 */

extern	MV_STATUS	mvEthPhyRegRead(MV_U32 phyAddr, MV_U32 regOffs, MV_U16 *data);
extern	MV_STATUS	mvEthPhyRegWrite(MV_U32 phyAddr, MV_U32 regOffs, MV_U16 data);

// NIC LED manipulation for CUT2
extern  MV_VOID		mvEthE6161SwitchLEDInit(MV_U32 ethPortNum, MV_U32 LEDnum, MV_U32 mode);
// NIC LED manipulation for CUT3 / CUT4
extern  MV_VOID		mvEthE6171SwitchLEDInit(MV_U32 LEDnum, MV_U32 mode);
extern	MV_VOID		mvEthE6171SwitchLED_Progress_Bar(void);
// Eth Switch Shutdown / Init fucntions for CUT3 / CUT4
extern  MV_VOID		mvEthE6171SwitchBasicShutdown(MV_U32 ethPortNum);
extern  MV_VOID		mvEthE6171SwitchBasicInit(MV_U32 ethPortNum);
// Wan port shutdown / init funtions for CUT3 / CUT4
extern  MV_VOID		mvEthE1116PhyBasicShutdown(MV_U32 ethPortNum);
extern  MV_VOID		mvEthE1116PhyBasicInit(MV_U32 ethPortNum);


/* golabal mac address for yukon EC */
unsigned char yuk_enetaddr[6];
extern int interrupt_init (void);
extern void i2c_init(int speed, int slaveaddr);


int board_init (void)
{
	DECLARE_GLOBAL_DATA_PTR;
#if defined(MV_INCLUDE_TWSI)
	MV_TWSI_ADDR slave;
#endif
	unsigned int i;
#ifdef SEATTLE
	MPP0 &= (0xffffffff & (~(0xf << (4*(7 % 8)))));
#endif
	maskAllInt();

	/* must initialize the int in order for udelay to work */
	interrupt_init();

#if defined(MV_INCLUDE_TWSI)
	slave.type = ADDR7_BIT;
	slave.address = 0;
	mvTwsiInit(0, CFG_I2C_SPEED, CFG_TCLK, &slave, 0);
#endif
 
	/* Init the Board environment module (device bank params init) */
	mvBoardEnvInit();
   	
	/* Init the Controlloer environment module (MPP init) */
	mvCtrlEnvInit();

	mvBoardDebugLed(3);

        /* Init the Controller CPU interface */
	mvCpuIfInit(mvCpuAddrWinMap);

        /* arch number of Integrator Board */
        gd->bd->bi_arch_number = 527;
 
        /* adress of boot parameters */
        gd->bd->bi_boot_params = 0x00000100;

	/* relocate the exception vectors */
	/* U-Boot is running from DRAM at this stage */
	for(i = 0; i < 0x100; i+=4)
	{
		*(unsigned int *)(0x0 + i) = *(unsigned int*)(TEXT_BASE + i);
	}
	
	/* Update NOR flash base address bank for CFI driver */
#ifdef	CFG_FLASH_CFI_DRIVER
	mvUpdateNorFlashBaseAddrBank();
#endif	/* CFG_FLASH_CFI_DRIVER */

#if defined(MV_INCLUDE_UNM_ETH) || defined(MV_INCLUDE_GIG_ETH)
	/* Init the PHY or Switch of the board */
	//mvBoardEgigaPhySwitchInit(); - Not done unless requested
	/* Shut down WAN phy */ 
	mvEthE1116PhyBasicShutdown(0);

#endif /* #if defined(MV_INCLUDE_UNM_ETH) || defined(MV_INCLUDE_GIG_ETH) */

	mvBoardDebugLed(4);
	
	return 0;
}


// mike
void misc_init_r_env(void){
        char *env;
        char tmp_buf[10];
        unsigned int malloc_len;
        DECLARE_GLOBAL_DATA_PTR;
	
        unsigned int flashSize =0 , secSize =0, ubootSize =0;
	char buff[256];

#if defined(MV_BOOTSIZE_4M)
	flashSize = _4M;
#elif defined(MV_BOOTSIZE_8M)
	flashSize = _8M;
#elif defined(MV_BOOTSIZE_16M)
	flashSize = _16M;
#elif defined(MV_BOOTSIZE_32M)
	flashSize = _32M;
#elif defined(MV_BOOTSIZE_64M)
	flashSize = _64M;
#endif

#if defined(MV_SEC_64K)
	secSize = _64K;
#if defined(MV_TINY_IMAGE)
	ubootSize = _256K;
#else
	ubootSize = _512K;
#endif
#elif defined(MV_SEC_128K)
	secSize = _128K;
#if defined(MV_TINY_IMAGE)
	ubootSize = _128K * 3;
#else
	ubootSize = _128K * 5;
#endif
#elif defined(MV_SEC_256K)
	secSize = _256K;
#if defined(MV_TINY_IMAGE)
	ubootSize = _256K * 3;
#else
	ubootSize = _256K * 3;
#endif
#endif
	if ((0 == flashSize) || (0 == secSize) || (0 == ubootSize))
	{
		env = getenv("console");
		if(!env)
		{
			sprintf(env,"console=ttyS0,%s",getenv("baudrate"));
			setenv("console",env);
		}
	}
	else
#if defined(MV_SPI_BOOT)
	{
		sprintf(buff,"console=ttyS0,%s mtdparts=spi_flash:0x%x@0(uboot)ro,0x%x@0x%x(root)",getenv("baudrate"),
			 ubootSize, flashSize - 0x100000, 0x100000);
		env = getenv("console");
		if(!env)
			setenv("console",buff);
	}
#elif defined(MV_NAND_BOOT)
	{
		
		sprintf(buff,"console=ttyS0,%s mtdparts=nand_mtd:0x%x@0(uboot)ro,0x%x@0x%x(root)",
				getenv("baudrate"), ubootSize, nand_info[0].size - 0x100000, 0x100000);
		//env = getenv("console");
		//if(!env)
			setenv("console",buff);

		env = getenv("nandEnvBase");
        strcpy(env, "");
        sprintf(env, "%x", nandEnvBase);
        setenv("nandEnvBase", env);
	}
#endif
		
        /* Linux open port support */
	env = getenv("mainlineLinux");
	if(env && ((strcmp(env,"yes") == 0) ||  (strcmp(env,"Yes") == 0)))
		setenv("mainlineLinux","yes");
	else
		setenv("mainlineLinux","no");

	env = getenv("mainlineLinux");
	if(env && ((strcmp(env,"yes") == 0) ||  (strcmp(env,"Yes") == 0)))
	{
	    /* arch number for open port Linux */
	    env = getenv("arcNumber");
	    if(!env )
	    {
		/* arch number according to board ID */
		int board_id = mvBoardIdGet();	
		switch(board_id){
		    case(DB_88F6281A_BP_ID):
			sprintf(tmp_buf,"%d", DB_88F6281_BP_MLL_ID);
			board_id = DB_88F6281_BP_MLL_ID; 
			break;
		    case(RD_88F6192A_ID):
			sprintf(tmp_buf,"%d", RD_88F6192_MLL_ID);
			board_id = RD_88F6192_MLL_ID; 
			break;
		    case(RD_88F6281A_ID):
			sprintf(tmp_buf,"%d", RD_88F6281_MLL_ID);
			board_id = RD_88F6281_MLL_ID; 
			break;
            case(DB_CUSTOMER_ID):
            break;
		    default:
			sprintf(tmp_buf,"%d", board_id);
			board_id = board_id; 
			break;
		}
		gd->bd->bi_arch_number = board_id;
		setenv("arcNumber", tmp_buf);
	    }
	    else
	    {
		gd->bd->bi_arch_number = simple_strtoul(env, NULL, 10);
	    }
	}

        /* update the CASset env parameter */
        env = getenv("CASset");
        if(!env )
        {
#ifdef MV_MIN_CAL
                setenv("CASset","min");
#else
                setenv("CASset","max");
#endif
        }
        /* Monitor extension */
#ifdef MV_INCLUDE_MONT_EXT
        env = getenv("enaMonExt");
        if(/* !env || */ ( (strcmp(env,"yes") == 0) || (strcmp(env,"Yes") == 0) ) )
                setenv("enaMonExt","yes");
        else
#endif
                setenv("enaMonExt","no");
#if defined (MV_INC_BOARD_NOR_FLASH)
        env = getenv("enaFlashBuf");
        if( ( (strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) ) )
                setenv("enaFlashBuf","no");
        else
                setenv("enaFlashBuf","yes");
#endif

	/* CPU streaming */
        env = getenv("enaCpuStream");
        if(!env || ( (strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) ) )
                setenv("enaCpuStream","no");
        else
                setenv("enaCpuStream","yes");

	/* Write allocation */
	env = getenv("enaWrAllo");
	if( !env || ( ((strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) )))
		setenv("enaWrAllo","no");
	else
		setenv("enaWrAllo","yes");

	/* Pex mode */
	env = getenv("pexMode");
	if( env && ( ((strcmp(env,"EP") == 0) || (strcmp(env,"ep") == 0) )))
		setenv("pexMode","EP");
	else
		setenv("pexMode","RC");

    	env = getenv("disL2Cache");
    	if(!env || ( (strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) ) )
        	setenv("disL2Cache","no"); 
    	else
        	setenv("disL2Cache","yes");

    	env = getenv("setL2CacheWT");
    	if(!env || ( (strcmp(env,"yes") == 0) || (strcmp(env,"Yes") == 0) ) )
        	setenv("setL2CacheWT","yes"); 
    	else
        	setenv("setL2CacheWT","no");

    	env = getenv("disL2Prefetch");
    	if(!env || ( (strcmp(env,"yes") == 0) || (strcmp(env,"Yes") == 0) ) )
	{
        	setenv("disL2Prefetch","yes"); 
	
		/* ICache Prefetch */
		env = getenv("enaICPref");
		if( env && ( ((strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) )))
			setenv("enaICPref","no");
		else
			setenv("enaICPref","yes");
		
		/* DCache Prefetch */
		env = getenv("enaDCPref");
		if( env && ( ((strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) )))
			setenv("enaDCPref","no");
		else
			setenv("enaDCPref","yes");
	}
    	else
	{
        	setenv("disL2Prefetch","no");
		setenv("enaICPref","no");
		setenv("enaDCPref","no");
	}


        env = getenv("sata_dma_mode");
        if( env && ((strcmp(env,"No") == 0) || (strcmp(env,"no") == 0) ) )
                setenv("sata_dma_mode","no");
        else
                setenv("sata_dma_mode","yes");


        /* Malloc length */
        env = getenv("MALLOC_len");
        malloc_len =  simple_strtoul(env, NULL, 10) << 20;
        if(malloc_len == 0){
                sprintf(tmp_buf,"%d",CFG_MALLOC_LEN>>20);
                setenv("MALLOC_len",tmp_buf);
	}
         
        /* primary network interface */
        env = getenv("ethprime");
	if(!env)
    {
        if(mvBoardIdGet() == RD_88F6281A_ID)
            setenv("ethprime","egiga1");
        else
            setenv("ethprime",ENV_ETH_PRIME);
    }
 
	/* netbsd boot arguments */
        env = getenv("netbsd_en");
	if( !env || ( ((strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) )))
		setenv("netbsd_en","no");
	else
	{
	    setenv("netbsd_en","yes");
	    env = getenv("netbsd_gw");
	    if(!env)
                setenv("netbsd_gw","192.168.0.254");

	    env = getenv("netbsd_mask");
	    if(!env)
                setenv("netbsd_mask","255.255.255.0");

	    env = getenv("netbsd_fs");
	    if(!env)
                setenv("netbsd_fs","nfs");

	    env = getenv("netbsd_server");
	    if(!env)
                setenv("netbsd_server","192.168.0.1");

	    env = getenv("netbsd_ip");
	    if(!env)
	    {
		env = getenv("ipaddr");
               	setenv("netbsd_ip",env);
	    }

	    env = getenv("netbsd_rootdev");
	    if(!env)
                setenv("netbsd_rootdev","mgi0");

	    env = getenv("netbsd_add");
	    if(!env)
                setenv("netbsd_add","0x800000");

	    env = getenv("netbsd_get");
	    if(!env)
                setenv("netbsd_get","tftpboot $(netbsd_add) $(image_name)");

#if defined(MV_INC_BOARD_QD_SWITCH)
	    env = getenv("netbsd_netconfig");
	    if(!env)
	    setenv("netbsd_netconfig","mv_net_config=<((mgi0,00:00:11:22:33:44,0)(mgi1,00:00:11:22:33:55,1:2:3:4)),mtu=1500>");
#endif
	    env = getenv("netbsd_set_args");
	    if(!env)
	    	setenv("netbsd_set_args","setenv bootargs nfsroot=$(netbsd_server):$(rootpath) fs=$(netbsd_fs) \
ip=$(netbsd_ip) serverip=$(netbsd_server) mask=$(netbsd_mask) gw=$(netbsd_gw) rootdev=$(netbsd_rootdev) \
ethaddr=$(ethaddr) eth1addr=$(eth1addr) ethmtu=$(ethmtu) eth1mtu=$(eth1mtu) $(netbsd_netconfig)");

	    env = getenv("netbsd_boot");
	    if(!env)
	    	setenv("netbsd_boot","bootm $(netbsd_add) $(bootargs)");

	    env = getenv("netbsd_bootcmd");
	    if(!env)
	    	setenv("netbsd_bootcmd","run netbsd_get ; run netbsd_set_args ; run netbsd_boot");
	}

	/* vxWorks boot arguments */
        env = getenv("vxworks_en");
	if( !env || ( ((strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) )))
		setenv("vxworks_en","no");
	else
	{
	    char* buff = (char*) 0x1100;
	    setenv("vxworks_en","yes");
		
	    sprintf(buff,"mgi(0,0) host:vxWorks.st");
	    env = getenv("serverip");
	    strcat(buff, " h=");
	    strcat(buff,env);
	    env = getenv("ipaddr");
	    strcat(buff, " e=");
	    strcat(buff,env);
	    strcat(buff, ":ffff0000 u=anonymous pw=target ");

	    setenv("vxWorks_bootargs",buff);
	}

        /* linux boot arguments */
        env = getenv("bootargs_root");
        if(!env)
                setenv("bootargs_root","root=/dev/nfs rw");
 
	/* For open Linux we set boot args differently */
	env = getenv("mainlineLinux");
	if(env && ((strcmp(env,"yes") == 0) ||  (strcmp(env,"Yes") == 0)))
	{
        	env = getenv("bootargs_end");
        	if(!env)
                setenv("bootargs_end",":::orion:eth0:none");
	}
	else
	{
        	env = getenv("bootargs_end");
        	if(!env)
#if defined(MV_INC_BOARD_QD_SWITCH)
                setenv("bootargs_end",CFG_BOOTARGS_END_SWITCH);
#else
                setenv("bootargs_end",CFG_BOOTARGS_END);
#endif
	}
	
        env = getenv("image_name");
        if(!env)
                setenv("image_name","uImage");
 

#if (CONFIG_BOOTDELAY >= 0)
        env = getenv("bootcmd");
        if(!env)
#if defined(MV_INCLUDE_TDM) && defined(MV_INC_BOARD_QD_SWITCH)
	    setenv("bootcmd","tftpboot 0x2000000 $(image_name); \
setenv bootargs $(console) $(bootargs_root) nfsroot=$(serverip):$(rootpath) \
ip=$(ipaddr):$(serverip)$(bootargs_end) $(mvNetConfig) $(mvPhoneConfig);  bootm 0x2000000; ");
#elif defined(MV_INC_BOARD_QD_SWITCH)
            setenv("bootcmd","tftpboot 0x2000000 $(image_name); \
setenv bootargs $(console) $(bootargs_root) nfsroot=$(serverip):$(rootpath) \
ip=$(ipaddr):$(serverip)$(bootargs_end) $(mvNetConfig);  bootm 0x2000000; ");
#elif defined(MV_INCLUDE_TDM)
            setenv("bootcmd","tftpboot 0x2000000 $(image_name); \
setenv bootargs $(console) $(bootargs_root) nfsroot=$(serverip):$(rootpath) \
ip=$(ipaddr):$(serverip)$(bootargs_end) $(mvNetConfig) $(mvPhoneConfig);  bootm 0x2000000; ");
#else

            setenv("bootcmd","tftpboot 0x2000000 $(image_name); \
setenv bootargs $(console) $(bootargs_root) nfsroot=$(serverip):$(rootpath) \
ip=$(ipaddr):$(serverip)$(bootargs_end);  bootm 0x2000000; ");
#endif
#endif /* (CONFIG_BOOTDELAY >= 0) */

        env = getenv("standalone");
        if(!env)
#if defined(MV_INCLUDE_TDM) && defined(MV_INC_BOARD_QD_SWITCH)
	    setenv("standalone","fsload 0x2000000 $(image_name);setenv bootargs $(console) root=/dev/mtdblock0 rw \
ip=$(ipaddr):$(serverip)$(bootargs_end) $(mvNetConfig) $(mvPhoneConfig); bootm 0x2000000;");
#elif defined(MV_INC_BOARD_QD_SWITCH)
            setenv("standalone","fsload 0x2000000 $(image_name);setenv bootargs $(console) root=/dev/mtdblock0 rw \
ip=$(ipaddr):$(serverip)$(bootargs_end) $(mvNetConfig); bootm 0x2000000;");
#elif defined(MV_INCLUDE_TDM)
            setenv("standalone","fsload 0x2000000 $(image_name);setenv bootargs $(console) root=/dev/mtdblock0 rw \
ip=$(ipaddr):$(serverip)$(bootargs_end) $(mvPhoneConfig); bootm 0x2000000;");
#else
            setenv("standalone","fsload 0x2000000 $(image_name);setenv bootargs $(console) root=/dev/mtdblock0 rw \
ip=$(ipaddr):$(serverip)$(bootargs_end); bootm 0x2000000;");
#endif
             
       /* Set boodelay to CONFIG_BOOTDELAY (in seconds), if Monitor extension are disabled */
        if(!enaMonExt()){
                setenv("bootdelay",CP_TOSTRING(CONFIG_BOOTDELAY)); 
		setenv("disaMvPnp","no");
	}

	/* Disable PNP config of Marvel memory controller devices. */
        env = getenv("disaMvPnp");
        if(!env)
                setenv("disaMvPnp","no");

#if (defined(MV_INCLUDE_GIG_ETH) || defined(MV_INCLUDE_UNM_ETH))
	/* Generate random ip and mac address */
	/* Read RTC to create pseudo-random data for enc */
    struct rtc_time tm;
	unsigned int xi, xj, xk, xl;
	char ethaddr_0[30];
	char ethaddr_1[30];

    rtc_get(&tm);
	xi = ((tm.tm_yday + tm.tm_sec)% 254);
	/* No valid ip with one of the fileds has the value 0 */
	if (xi == 0)
		xi+=2;

	xj = ((tm.tm_yday + tm.tm_min)%254);
	/* No valid ip with one of the fileds has the value 0 */
	if (xj == 0)
		xj+=2;

	/* Check if the ip address is the same as the server ip */
	if ((xj == 1) && (xi == 11))
		xi+=2;

	xk = (tm.tm_min * tm.tm_sec)%254;
	xl = (tm.tm_hour * tm.tm_sec)%254;

	sprintf(ethaddr_0,"00:50:43:%02x:%02x:%02x",xk,xi,xj);
	sprintf(ethaddr_1,"00:50:43:%02x:%02x:%02x",xl,xi,xj);

	/* MAC addresses */
        env = getenv("ethaddr");
        if(!env)
                setenv("ethaddr",ethaddr_0);

        env = getenv("ethmtu");
        if(!env)
                setenv("ethmtu","1500");
        
#if !defined(MV_INC_BOARD_QD_SWITCH)
/* ETH1ADDR not define in GWAP boards */
	if ((mvBoardMppGroupTypeGet(MV_BOARD_MPP_GROUP_1) == MV_BOARD_RGMII) || 
			(mvBoardMppGroupTypeGet(MV_BOARD_MPP_GROUP_2) == MV_BOARD_RGMII) || 
			(mvBoardMppGroupTypeGet(MV_BOARD_MPP_GROUP_1) == MV_BOARD_MII))
	{
        	env = getenv("eth1addr");
        	if(!env)
               		setenv("eth1addr",ethaddr_1);

            env = getenv("eth1mtu");
            if(!env)
                    setenv("eth1mtu","1500");
	}
#elif defined(MV_INC_BOARD_QD_SWITCH) && (defined(RD_88F6190A) || defined(RD_88F6192A))
    env = getenv("eth1addr");
    if(!env)
            setenv("eth1addr",ethaddr_1);

    env = getenv("eth1mtu");
    if(!env)
            setenv("eth1mtu","1500");
#endif
#if defined(MV_INCLUDE_TDM)
        /* Set mvPhoneConfig env parameter */
        env = getenv("mvPhoneConfig");
        if(!env )
            setenv("mvPhoneConfig","mv_phone_config=dev0:fxs,dev1:fxs");
#endif
        /* Set mvNetConfig env parameter */
        env = getenv("mvNetConfig");
        if(!env )
		    setenv("mvNetConfig","mv_net_config=(00:11:88:0f:62:81,0:1:2:3),mtu=1500");
#endif /*  (MV_INCLUDE_GIG_ETH) || defined(MV_INCLUDE_UNM_ETH) */

#if defined(MV_INCLUDE_USB)
	/* USB Host */
	env = getenv("usb0Mode");
	if(!env)
		setenv("usb0Mode",ENV_USB0_MODE);
#endif  /* (MV_INCLUDE_USB) */

#if defined(YUK_ETHADDR)
	env = getenv("yuk_ethaddr");
	if(!env)
		setenv("yuk_ethaddr",YUK_ETHADDR);

	{
		int i;
		char *tmp = getenv ("yuk_ethaddr");
		char *end;

		for (i=0; i<6; i++) {
			yuk_enetaddr[i] = tmp ? simple_strtoul(tmp, &end, 16) : 0;
			if (tmp)
				tmp = (*end) ? end+1 : end;
		}
	}
#endif /* defined(YUK_ETHADDR) */

#if defined(MV_NAND)
	env = getenv("nandEcc");
	if(!env)
    {
        setenv("nandEcc", "1bit");
    }
#endif

#if defined(RD_88F6281A) || defined(RD_88F6192A) || defined(RD_88F6190A)
	mvHddPowerCtrl();
#endif
#if (CONFIG_COMMANDS & CFG_CMD_RCVR)
	env = getenv("netretry");
	if (!env)
		setenv("netretry","no");

	env = getenv("rcvrip");
	if (!env)
		setenv("rcvrip",RCVR_IP_ADDR);

	env = getenv("loadaddr");
	if (!env)
		setenv("loadaddr",RCVR_LOAD_ADDR);

	env = getenv("autoload");
	if (!env)
		setenv("autoload","no");

	/* Check the recovery trigger */
	recoveryDetection();
#endif
        return;
}

#ifdef BOARD_LATE_INIT
int board_late_init (void)
{
	/* Check if to use the LED's for debug or to use single led for init and Linux heartbeat */
	mvBoardDebugLed(0);
	return 0;
}
#endif

void pcie_tune(void)
{

  MV_REG_WRITE(0xF1041AB0, 0x100);
  MV_REG_WRITE(0xF1041A20, 0x78000801);
  MV_REG_WRITE(0xF1041A00, 0x4014022F);
  MV_REG_WRITE(0xF1040070, 0x18110008);

  return;

}


int misc_init_r (void)
{
	char name[128], *env;
	
	mvBoardDebugLed(5);

	mvCpuNameGet(name);
	printf("\nCPU : %s\n",  name);

	/* init special env variables */
	misc_init_r_env();

	mv_cpu_init();

#if defined(MV_INCLUDE_MONT_EXT)
	if(enaMonExt()){
			printf("\n      Marvell monitor extension:\n");
			mon_extension_after_relloc();
	}
    	printf("\n");		
#endif /* MV_INCLUDE_MONT_EXT */

	/* print detected modules */
	mvMppModuleTypePrint();

    	printf("\n");		
	/* init the units decode windows */
	misc_init_r_dec_win();

#ifdef CONFIG_PCI
#if !defined(MV_MEM_OVER_PCI_WA) && !defined(MV_MEM_OVER_PEX_WA)
       pci_init();
#endif
#endif

	mvBoardDebugLed(6);

	mvBoardDebugLed(7);

	env = getenv("pcieTune");
	if(env && ((strcmp(env,"yes") == 0) || (strcmp(env,"yes") == 0)))
        pcie_tune();
    else
        setenv("pcieTune","no");
	
	return 0;
}

MV_U32 mvTclkGet(void)
{
        DECLARE_GLOBAL_DATA_PTR;
        /* get it only on first time */
        if(gd->tclk == 0)
                gd->tclk = mvBoardTclkGet();

        return gd->tclk;
}

MV_U32 mvSysClkGet(void)
{
        DECLARE_GLOBAL_DATA_PTR;
        /* get it only on first time */
        if(gd->bus_clk == 0)
                gd->bus_clk = mvBoardSysClkGet();

        return gd->bus_clk;
}
 
#ifndef MV_TINY_IMAGE
/* exported for EEMBC */
MV_U32 mvGetRtcSec(void)
{
        MV_RTC_TIME time;
#ifdef MV_INCLUDE_RTC
        mvRtcTimeGet(&time);
#elif CONFIG_RTC_DS1338_DS1339
        mvRtcDS133xTimeGet(&time);
#endif
	return (time.minutes * 60) + time.seconds;
}
#endif

void reset_cpu(void)
{
	mvBoardReset();
}

void mv_cpu_init(void)
{
	char *env;
	volatile unsigned int temp;

	/*CPU streaming & write allocate */
	env = getenv("enaWrAllo");
	if(env && ((strcmp(env,"yes") == 0) ||  (strcmp(env,"Yes") == 0)))  
	{
		__asm__ __volatile__("mrc    p15, 1, %0, c15, c1, 0" : "=r" (temp));
		temp |= BIT28;
		__asm__ __volatile__("mcr    p15, 1, %0, c15, c1, 0" :: "r" (temp));
		
	}
	else
	{
		__asm__ __volatile__("mrc    p15, 1, %0, c15, c1, 0" : "=r" (temp));
		temp &= ~BIT28;
		__asm__ __volatile__("mcr    p15, 1, %0, c15, c1, 0" :: "r" (temp));
	}

	env = getenv("enaCpuStream");
	if(!env || (strcmp(env,"no") == 0) ||  (strcmp(env,"No") == 0) )
	{
		__asm__ __volatile__("mrc    p15, 1, %0, c15, c1, 0" : "=r" (temp));
		temp &= ~BIT29;
		__asm__ __volatile__("mcr    p15, 1, %0, c15, c1, 0" :: "r" (temp));
	}
	else
	{
		__asm__ __volatile__("mrc    p15, 1, %0, c15, c1, 0" : "=r" (temp));
		temp |= BIT29;
		__asm__ __volatile__("mcr    p15, 1, %0, c15, c1, 0" :: "r" (temp));
	}
		
	/* Verifay write allocate and streaming */
	printf("\n");
	__asm__ __volatile__("mrc    p15, 1, %0, c15, c1, 0" : "=r" (temp));
	if (temp & BIT29)
		printf("Streaming enabled \n");
	else
		printf("Streaming disabled \n");
	if (temp & BIT28)
		printf("Write allocate enabled\n");
	else
		printf("Write allocate disabled\n");

	/* DCache Pref  */
	env = getenv("enaDCPref");
	if(env && ((strcmp(env,"yes") == 0) ||  (strcmp(env,"Yes") == 0)))
	{
		MV_REG_BIT_SET( CPU_CONFIG_REG , CCR_DCACH_PREF_BUF_ENABLE);
	}

	if(env && ((strcmp(env,"no") == 0) ||  (strcmp(env,"No") == 0)))
        {
		MV_REG_BIT_RESET( CPU_CONFIG_REG , CCR_DCACH_PREF_BUF_ENABLE);
	}

	/* ICache Pref  */
	env = getenv("enaICPref");
        if(env && ((strcmp(env,"yes") == 0) ||  (strcmp(env,"Yes") == 0)))
	{
		MV_REG_BIT_SET( CPU_CONFIG_REG , CCR_ICACH_PREF_BUF_ENABLE);
	}
	
	if(env && ((strcmp(env,"no") == 0) ||  (strcmp(env,"No") == 0)))
        {
		MV_REG_BIT_RESET( CPU_CONFIG_REG , CCR_ICACH_PREF_BUF_ENABLE);
	}

	/* Set L2C WT mode - Set bit 4 */
	temp = MV_REG_READ(CPU_L2_CONFIG_REG);
    	env = getenv("setL2CacheWT");
    	if(!env || ( (strcmp(env,"yes") == 0) || (strcmp(env,"Yes") == 0) ) )
	{
		temp |= BIT4;
	}
	else
		temp &= ~BIT4;
	MV_REG_WRITE(CPU_L2_CONFIG_REG, temp);


	/* L2Cache settings */
	asm ("mrc p15, 1, %0, c15, c1, 0":"=r" (temp));

	/* Disable L2C pre fetch - Set bit 24 */
    	env = getenv("disL2Prefetch");
    	if(env && ( (strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) ) )
		temp &= ~BIT24;
	else
		temp |= BIT24;

	/* enable L2C - Set bit 22 */
    	env = getenv("disL2Cache");
    	if(!env || ( (strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) ) )
		temp |= BIT22;
	else
		temp &= ~BIT22;
	
	asm ("mcr p15, 1, %0, c15, c1, 0": :"r" (temp));


	/* Enable i cache */
	asm ("mrc p15, 0, %0, c1, c0, 0":"=r" (temp));
	temp |= BIT12;
	asm ("mcr p15, 0, %0, c1, c0, 0": :"r" (temp));

	/* Change reset vector to address 0x0 */
	asm ("mrc p15, 0, %0, c1, c0, 0":"=r" (temp));
	temp &= ~BIT13;
	asm ("mcr p15, 0, %0, c1, c0, 0": :"r" (temp));
}
/*******************************************************************************
* mvBoardMppModuleTypePrint - print module detect
*
* DESCRIPTION:
*
* INPUT:
*
* OUTPUT:
*       None.
*
* RETURN:
*
*******************************************************************************/
MV_VOID mvMppModuleTypePrint(MV_VOID)
{

	MV_BOARD_MPP_GROUP_CLASS devClass;
	MV_BOARD_MPP_TYPE_CLASS mppGroupType;
	MV_U32 devId;
	MV_U32 maxMppGrp = 1;
	
	devId = mvCtrlModelGet();

	switch(devId){
		case MV_6281_DEV_ID:
			maxMppGrp = MV_6281_MPP_MAX_MODULE;
			break;
        case MV_6282_DEV_ID:
            maxMppGrp = MV_6282_MPP_MAX_MODULE;
            break;
        case MV_6280_DEV_ID:
            maxMppGrp = MV_6280_MPP_MAX_MODULE;
            break;
		case MV_6192_DEV_ID:
			maxMppGrp = MV_6192_MPP_MAX_MODULE;
			break;
        case MV_6190_DEV_ID:
            maxMppGrp = MV_6190_MPP_MAX_MODULE;
            break;
		case MV_6180_DEV_ID:
			maxMppGrp = MV_6180_MPP_MAX_MODULE;
			break;		
	}

	for (devClass = 0; devClass < maxMppGrp; devClass++)
	{
		mppGroupType = mvBoardMppGroupTypeGet(devClass);

		switch(mppGroupType)
		{
			case MV_BOARD_TDM:
                if(devId != MV_6190_DEV_ID)
                    printf("Module %d is TDM\n", devClass);
				break;
			case MV_BOARD_AUDIO:
                if(devId != MV_6190_DEV_ID)
                    printf("Module %d is AUDIO\n", devClass);
				break;
			case MV_BOARD_RGMII:
                if(devId != MV_6190_DEV_ID)
                    printf("Module %d is RGMII\n", devClass);
				break;
			case MV_BOARD_GMII:
                if(devId != MV_6190_DEV_ID)
                    printf("Module %d is GMII\n", devClass);
				break;
			case MV_BOARD_TS:
                if(devId != MV_6190_DEV_ID)
                    printf("Module %d is TS\n", devClass);
				break;
			case MV_BOARD_MII:
                if(devId != MV_6190_DEV_ID)
                    printf("Module %d is MII\n", devClass);
				break;
			default:
				break;
		}
	}
}

/* Set unit in power off mode acording to the detection of MPP */
#if defined(MV_INCLUDE_CLK_PWR_CNTRL)
int mv_set_power_scheme(void)
{
	int mppGroupType1 = mvBoardMppGroupTypeGet(MV_BOARD_MPP_GROUP_1);
	int mppGroupType2 = mvBoardMppGroupTypeGet(MV_BOARD_MPP_GROUP_2);
	MV_U32 devId = mvCtrlModelGet();
	MV_U32 boardId = mvBoardIdGet();

    if (devId == MV_6180_DEV_ID || boardId == RD_88F6281A_PCAC_ID || devId == MV_6280_DEV_ID || boardId == SHEEVA_PLUG_ID)
	{
		/* Sata power down */
		mvCtrlPwrMemSet(SATA_UNIT_ID, 1, MV_FALSE);
		mvCtrlPwrMemSet(SATA_UNIT_ID, 0, MV_FALSE);
		mvCtrlPwrClckSet(SATA_UNIT_ID, 1, MV_FALSE);
		mvCtrlPwrClckSet(SATA_UNIT_ID, 0, MV_FALSE);
		/* Sdio power down */
		mvCtrlPwrMemSet(SDIO_UNIT_ID, 0, MV_FALSE);
		mvCtrlPwrClckSet(SDIO_UNIT_ID, 0, MV_FALSE);
	}

    if (boardId == RD_88F6281A_ID || boardId == SHEEVA_PLUG_ID || devId == MV_6280_DEV_ID)
    {
		DB(printf("Warning: TS is Powered Off\n"));
		mvCtrlPwrClckSet(TS_UNIT_ID, 0, MV_FALSE);
    }

    if (devId == MV_6280_DEV_ID)
    {
		DB(printf("Warning: PCI-E is Powered Off\n"));
		mvCtrlPwrClckSet(PEX_UNIT_ID, 0, MV_FALSE);
    }
	
	/* Close egiga 1 */
	if ((mppGroupType1 != MV_BOARD_GMII) && (mppGroupType1 != MV_BOARD_RGMII) && (mppGroupType2 != MV_BOARD_RGMII)
		 && (mppGroupType1 != MV_BOARD_MII))
	{
		DB(printf("Warning: Giga1 is Powered Off\n"));
		mvCtrlPwrMemSet(ETH_GIG_UNIT_ID, 1, MV_FALSE);
		mvCtrlPwrClckSet(ETH_GIG_UNIT_ID, 1, MV_FALSE);
	}

	/* Close TDM */
	if ((mppGroupType1 != MV_BOARD_TDM) && (mppGroupType2 != MV_BOARD_TDM))
	{
		DB(printf("Warning: TDM is Powered Off\n"));
		mvCtrlPwrClckSet(TDM_UNIT_ID, 0, MV_FALSE);
	}

	/* Close AUDIO */
	if ((mppGroupType1 != MV_BOARD_AUDIO) && (mppGroupType2 != MV_BOARD_AUDIO) && boardId != RD_88F6281A_ID)
	{
		DB(printf("Warning: AUDIO is Powered Off\n"));
		mvCtrlPwrMemSet(AUDIO_UNIT_ID, 0, MV_FALSE);
		mvCtrlPwrClckSet(AUDIO_UNIT_ID, 0, MV_FALSE);
	}

	/* Close TS */
	if ((mppGroupType1 != MV_BOARD_TS) && (mppGroupType2 != MV_BOARD_TS))
	{
		DB(printf("Warning: TS is Powered Off\n"));
		mvCtrlPwrClckSet(TS_UNIT_ID, 0, MV_FALSE);
	}

	return MV_OK;
}

#endif /* defined(MV_INCLUDE_CLK_PWR_CNTRL) */

/*******************************************************************************
* mvUpdateNorFlashBaseAddrBank - 
*
* DESCRIPTION:
*       This function update the CFI driver base address bank with on board NOR
*       devices base address.
*
* INPUT:
*
* OUTPUT:
*
* RETURN:
*       None
*
*******************************************************************************/
#ifdef	CFG_FLASH_CFI_DRIVER
MV_VOID mvUpdateNorFlashBaseAddrBank(MV_VOID)
{
    
    MV_U32 devBaseAddr;
    MV_U32 devNum = 0;
    int i;

    /* Update NOR flash base address bank for CFI flash init driver */
    for (i = 0 ; i < CFG_MAX_FLASH_BANKS_DETECT; i++)
    {
	devBaseAddr = mvBoardGetDeviceBaseAddr(i,BOARD_DEV_NOR_FLASH);
	if (devBaseAddr != 0xFFFFFFFF)
	{
	    flash_add_base_addr (devNum, devBaseAddr);
	    devNum++;
	}
    }
    mv_board_num_flash_banks = devNum;

    /* Update SPI flash count for CFI flash init driver */
    /* Assumption only 1 SPI flash on board */
    for (i = 0 ; i < CFG_MAX_FLASH_BANKS_DETECT; i++)
    {
    	devBaseAddr = mvBoardGetDeviceBaseAddr(i,BOARD_DEV_SPI_FLASH);
    	if (devBaseAddr != 0xFFFFFFFF)
		mv_board_num_flash_banks += 1;
    }
}
#endif	/* CFG_FLASH_CFI_DRIVER */


/*******************************************************************************
* mvHddPowerCtrl - 
*
* DESCRIPTION:
*       This function set HDD power on/off acording to env or wait for button push
* INPUT:
*	None
* OUTPUT:
*	None
* RETURN:
*       None
*
*******************************************************************************/
static void mvHddPowerCtrl(void)
{

    MV_32 hddPowerBit = 0;
    MV_32 fanPowerBit = 0;
	MV_32 hddHigh = 0;
	MV_32 fanHigh = 0;
	char* env;
	
    if(RD_88F6281A_ID == mvBoardIdGet())
    {
        hddPowerBit = mvBoarGpioPinNumGet(BOARD_GPP_HDD_POWER, 0);
        fanPowerBit = mvBoarGpioPinNumGet(BOARD_GPP_FAN_POWER, 0);
        if (hddPowerBit > 31)
    	{
    		hddPowerBit = hddPowerBit % 32;
    		hddHigh = 1;
    	}
    
    	if (fanPowerBit > 31)
    	{
    		fanPowerBit = fanPowerBit % 32;
    		fanHigh = 1;
    	}
    }

	if ((RD_88F6281A_ID == mvBoardIdGet()) || (RD_88F6192A_ID == mvBoardIdGet()) || 
        (RD_88F6190A_ID == mvBoardIdGet()))
	{
		env = getenv("hddPowerCtrl");
 		if(!env || ( (strcmp(env,"no") == 0) || (strcmp(env,"No") == 0) ) )
                	setenv("hddPowerCtrl","no");
        	else
                	setenv("hddPowerCtrl","yes");

        if(RD_88F6281A_ID == mvBoardIdGet())
        {
            mvBoardFanPowerControl(MV_TRUE);
            mvBoardHDDPowerControl(MV_TRUE);
        }
        else
        {
            /* FAN power on */
    		MV_REG_BIT_SET(GPP_DATA_OUT_REG(fanHigh),(1<<fanPowerBit));
    		MV_REG_BIT_RESET(GPP_DATA_OUT_EN_REG(fanHigh),(1<<fanPowerBit));
            /* HDD power on */
            MV_REG_BIT_SET(GPP_DATA_OUT_REG(hddHigh),(1<<hddPowerBit));
            MV_REG_BIT_RESET(GPP_DATA_OUT_EN_REG(hddHigh),(1<<hddPowerBit));
        }
        
	}
}


#if (CONFIG_COMMANDS & CFG_CMD_RCVR)

/* Disable WAN and LAN Switch ports */
void
cp_disable_network_ports(void)
{
	if (g_cp_network_enabled)
	{
		printf("Disabling network ports...\n");
		mvEthE6171SwitchBasicShutdown(1);
		mvEthE1116PhyBasicShutdown(0);
		g_cp_network_enabled = 0;
	}
	else
	{
		printf("Network ports disabled\n");
	}
	printf("Done.\n");
}

/* Enable WAN and LAN Switch ports */
void
cp_enable_network_ports(void)
{
	if (!g_cp_network_enabled)
	{
		printf("Enabling network ports...\n");
		mvEthE6171SwitchBasicInit(1);
		mvEthE1116PhyBasicInit(0);
		g_cp_network_enabled = 1;
	}
	else
	{
		printf("Network ports enabled\n");
	}
	printf("Done.\n");
}

/* Test if WAN and LAN Switch ports are enabled */
unsigned int
cp_is_network_enabled(void)
{
	return g_cp_network_enabled;
}

void
cp_GPIO_LED_set(int num, int state)
{
	MV_32 LEDBit			= 0;
	MV_32 bitInHigh			= 0;
	MV_32 registerNumber	= 0;

	if (num == UNSUPPORTED_LED )
		return;
	if ((num >= LED_CPU_BASE_NUM) && (num <= LED_CPU_HIGH_NUM)) //CPU
	{
		LEDBit = num;

		bitInHigh = LEDBit / 32;
		LEDBit = LEDBit % 32;
	
		switch (state){
		case LED_CMD_OFF:
			registerNumber = GPP_DATA_OUT_REG(bitInHigh);
			MV_REG_BIT_SET(registerNumber, BIT(LEDBit));
			break;

		case LED_CMD_ON:
			registerNumber = GPP_DATA_OUT_REG(bitInHigh);
			MV_REG_BIT_RESET(registerNumber, BIT(LEDBit));
			break;

		case LED_CMD_BLINK:
			registerNumber = GPP_BLINK_EN_REG(bitInHigh);
			MV_REG_BIT_SET(registerNumber, BIT(LEDBit));
			break;

		case LED_CMD_STATIC:
			registerNumber = GPP_BLINK_EN_REG(bitInHigh);
			MV_REG_BIT_RESET(registerNumber, BIT(LEDBit));
			break;
	
		default:
			printf("Illegal LED state: %d\n", state);
			return;
                }
	}
	if (num & GPIO_EXTENDER1) { // extra bit indicates seattle board I2C
		if (state == LED_CMD_OFF)	{
			extend_gpio_ctl(IC_SG80_EXTEND1,num^GPIO_EXTENDER1,1);
		}
		else if (state != LED_CMD_STATIC){
			extend_gpio_ctl(IC_SG80_EXTEND1,num^GPIO_EXTENDER1,0);
		}
	}
	else if (num & GPIO_EXTENDER2) {
		if (state == LED_CMD_OFF)	{
			extend_gpio_ctl(IC_SG80_EXTEND2,num^GPIO_EXTENDER2,1);
		}
		else if (state != LED_CMD_STATIC){
			extend_gpio_ctl(IC_SG80_EXTEND2,num^GPIO_EXTENDER2,0);
		}
	}
}

int
do_GPIO_LED_set (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int led_num	= 0;
	int led_cmd	= 0;
	
	if (argc != 3) 
	{ 
		goto Usage; 
	} 

	led_num = simple_strtoul(argv[1], NULL, 10);
	led_cmd = simple_strtoul(argv[2], NULL, 10);

	cp_GPIO_LED_set(led_num, led_cmd);
	
	return 0;

Usage:
	printf("%s",cmdtp->usage);
	return -1;	
}

U_BOOT_CMD(
		ledCtl,      3,     1,      do_GPIO_LED_set,
		"ledCtl <LED GPIO num> <cmd>\n",
		"\n\tSet GPIO LED state. "
		"\n Commands:\n"
			"\t"CP_TOSTRING(LED_CMD_OFF)	" - Off\n"
			"\t"CP_TOSTRING(LED_CMD_ON)		" - On\n"
			"\t"CP_TOSTRING(LED_CMD_BLINK)	" - Blink\n"
			"\t"CP_TOSTRING(LED_CMD_OFF)	" - Stop blinking\n"
);

void
cp_USB_LED_set(int portnr, int on, int blink)
{
	static int last_portnr = USB_PORTNR_USB1; /* The last port number used */
	int cmd = 0;
	int usb_led_g, usb_led_r;

	/* If portnr is not known - use last... */
	switch (portnr)
	{
	case USB_PORTNR_UNKNOWN:
		cp_USB_LED_set(last_portnr, on, blink);
		return;
		break;

	case USB_PORTNR_USB1:
		last_portnr = USB_PORTNR_USB1;
		usb_led_g = USB1_LED_GREEN;
		usb_led_r = USB1_LED_RED;
		break;

	case USB_PORTNR_USB2:
		last_portnr = USB_PORTNR_USB2;
		usb_led_g = USB2_LED_GREEN;
		usb_led_r = USB2_LED_RED;
		break;

	default:
		printf("Illegal USB port number option: %d\n", portnr);
		return;
	}

	/* Take care of on/off */
	cmd = on ? LED_CMD_ON : LED_CMD_OFF;
	cp_GPIO_LED_set(usb_led_g, cmd);
	cp_GPIO_LED_set(usb_led_r, cmd);

	/* Take care of blink/static */
	cmd = blink ? LED_CMD_BLINK : LED_CMD_STATIC;
	cp_GPIO_LED_set(usb_led_g, cmd);
	cp_GPIO_LED_set(usb_led_r, cmd);
}

void
cp_switch_LEDs_reset(void)
{
	mvEthE6171SwitchLEDInit_reset_blink();
	mvEthE6171SwitchLEDInit(0,3);
	mvEthE6171SwitchLEDInit(1,3);
	mvEthE6171SwitchLEDInit(2,3);
	mvEthE6171SwitchLEDInit(3,3);
}

void
cp_switch_LEDs_blink(void)
{
	cp_switch_LEDs_off();
	mvEthE6171SwitchLEDInit(0,E6171_LED_FORCE_BLINK);
	mvEthE6171SwitchLEDInit(1,E6171_LED_FORCE_BLINK);
	//mvEthE6171SwitchLEDInit(2,E6171_LED_FORCE_BLINK);
	//mvEthE6171SwitchLEDInit(3,E6171_LED_FORCE_BLINK);
}

/* Light switch LEDs in "progress-bar" mode */
void
cp_switch_LEDs_Progress_Bar(void)
{ 
	cp_switch_LEDs_off();
	mvEthE6171SwitchLED_Progress_Bar();
}


void
cp_switch_LEDs_green_orange(void)
{
	cp_switch_LEDs_off();
	mvEthE6171SwitchLEDInit(0,E6171_LED_FORCE_ON);
	mvEthE6171SwitchLEDInit(1,E6171_LED_FORCE_ON);
}

void
cp_switch_LEDs_off(void)
{
	cp_switch_LEDs_reset();
	mvEthE6171SwitchLEDInit(0,E6171_LED_FORCE_OFF);
	mvEthE6171SwitchLEDInit(1,E6171_LED_FORCE_OFF);
	mvEthE6171SwitchLEDInit(2,E6171_LED_FORCE_OFF);
	mvEthE6171SwitchLEDInit(3,E6171_LED_FORCE_OFF);
}

void
cp_switch_LEDs_green(void)
{
	cp_switch_LEDs_off();
	mvEthE6171SwitchLEDInit(0,E6171_LED_FORCE_ON);
	mvEthE6171SwitchLEDInit(2,E6171_LED_FORCE_ON);
}


static void recoveryDetection(void)
{
    	//MV_32 stateButtonBit = mvBoarGpioPinNumGet(BOARD_GPP_WPS_BUTTON,0); 
	MV_32 stateButtonBit = 29;        
	MV_32 buttonHigh = 0;
	char* env;

        //printf ("%s:%d:%s():  start: board_id = %d \n", __FILE__, __LINE__, __FUNCTION__, mvBoardIdGet()); 

	/* Check if auto recovery is en */	
	env = getenv("enaAutoRecovery");
 	if(!env || ( (strcmp(env,"yes") == 0) || (strcmp(env,"Yes") == 0) ) )
               	setenv("enaAutoRecovery","yes");
        else
	{
        	//printf ("%s:%d:%s():  auto recovery env is set to NO .. \n",__FILE__,__LINE__,__FUNCTION__); 
               	setenv("enaAutoRecovery","no");
		rcvrflag = 0;
		return;
	}

	if (stateButtonBit == MV_ERROR)
	{	
        	printf ("%s:%d:%s():  stateButtonBit == %d .. \n",__FILE__,__LINE__,__FUNCTION__,stateButtonBit); 
		rcvrflag = 0;
		return;
	}

	if (stateButtonBit > 31)
	{
		stateButtonBit = stateButtonBit % 32;
		buttonHigh = 1;
	}

	/* Set state input indication pin as input */
	MV_REG_BIT_SET(GPP_DATA_OUT_EN_REG(buttonHigh),(1<<stateButtonBit));

	/* configuring MPP 29 to be GPIO !!! */
        MV_REG_WRITE(0x1000C, ( MV_REG_READ(0x1000C) & ~(BIT23|BIT22|BIT21|BIT20) ) );       

	/* check if recovery triggered - button is pressed */
	if (!(MV_REG_READ(GPP_DATA_IN_REG(buttonHigh)) & (1 << stateButtonBit)))
	{
		printf ("\n\n\n\tRESET TO FACTORY DEFAULTS BUTTON IS PRESSED\n\n\n");
		/* LEDs will be on in this scenario which will always end with factory defaults (new or existing) */
		/* Turn on red power & alert to indicate FD button is handled */
		cp_GPIO_LED_set(POWER_LED_GREEN,	LED_CMD_OFF);
		cp_GPIO_LED_set(POWER_LED_GREEN,	LED_CMD_STATIC);

		cp_GPIO_LED_set(POWER_LED_RED,		LED_CMD_ON);
		cp_GPIO_LED_set(POWER_LED_RED,		LED_CMD_STATIC);
				
		cp_GPIO_LED_set(ALERT_LED_GREEN,	LED_CMD_OFF);
		cp_GPIO_LED_set(ALERT_LED_GREEN,	LED_CMD_STATIC);

		cp_GPIO_LED_set(ALERT_LED_RED,		LED_CMD_ON);
		cp_GPIO_LED_set(ALERT_LED_RED,		LED_CMD_STATIC);
		
		rcvrflag = 1; // change it to 1 to enable recoveryHandle function 
	}
}

extern int do_bootm (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);


int
cp_filename_like(const char* filename, const char* prefix, const char* suffix)
{
	char *suff = NULL;

	if (filename == NULL)
	{
		return 0;
	}

	if ( strlen(filename) < strlen(prefix) + strlen(suffix))
	{
		return 0; /* String too short */
	}

	suff = filename + strlen(filename) - strlen(suffix);
	
	if ( !strncmp(filename, prefix, strlen(prefix))	&&
		 !strncmp(suff, suffix, strlen(suffix)))	
	{
		return 1;
	}
	
	return 0;
}

void cp_indicate_boot_error(void) 
{
	cp_switch_LEDs_blink();
	printf("Press any key to continue...\n");
	getc(); /* Wait for any key */
	cp_switch_LEDs_reset();
}

void cp_indicate_install_success(int should_pause) 
{
	cp_switch_LEDs_green();
	
	if (should_pause) {
		printf("Press any key to continue...\n");
		getc(); /* Wait for any key */
	} else {
		udelay(5000000);
	}
	cp_switch_LEDs_reset();
}	


int cp_verify_crc(MV_U32 image_addr)
{
	MV_U32 crc				= 0;
	MV_U32 hdr_crc			= 0;
	MV_U32 img_len			= 0;
	MV_U32 hdr_len 			= 0;
	MV_U32 crc_start_addr	= 0;
	MV_U32 crc_calc_size	= 0;

	hdr_crc = *( (MV_U32*)(image_addr + CP_CRC_OFFSET) );
	img_len = *( (MV_U32*)(image_addr + CP_IMG_LEN_OFFSET) );
	hdr_len = CP_IMG_HEADER_LEN;

	printf("Verifying image CRC\n");
	
	/* compute CRC of firmware */
	crc_calc_size = img_len - hdr_len;
	crc_start_addr = image_addr + hdr_len;

	crc = crc32(0, (u_char*) crc_start_addr, crc_calc_size);
	printf("\nHeader CRC....... : 0x%08X\n", hdr_crc);
	printf(  "Calculated CRC... : 0x%08X\n", crc);

	return (crc == hdr_crc);
}



extern int nand_curr_device;
extern nand_info_t nand_info[];


/* taken from firmCommon.h in sfwos_tools */
#define FIRM_VERSION_LEN    (32)

void get_image_name(MV_U32 image_addr, char *image_name, int max_len) {
	memcpy(image_name, ((((void *)image_addr) + CP_IMG_NAME_OFFSET)), max_len);
	image_name[max_len - 1] = '\0';
}




int cp_check_if_image_changed(MV_U32 image_addr) 
{
	
	MV_U32 hdr_offset_arr[CP_IMG_HEADER_LEN/sizeof(MV_U32) + 1];
	u_char *hdr_offset = ((u_char *) hdr_offset_arr);
	
	
	MV_U32 hdr_crc = *( (MV_U32*)(image_addr + CP_CRC_OFFSET) );
	MV_U32 hdr_len = *( (MV_U32*)(image_addr + CP_IMG_LEN_OFFSET) );

	MV_U32 new_hdr_crc;
	MV_U32 new_hdr_len;
	size_t read_len;

	char new_image_name[FIRM_VERSION_LEN];
	char cur_image_name[FIRM_VERSION_LEN];


	get_image_name(image_addr, new_image_name, sizeof(new_image_name));

	read_len = CP_IMG_HEADER_LEN;
	if (nand_read_skip_bad(&(nand_info[nand_curr_device]), CP_PRIMARY_OFFSET, &read_len ,hdr_offset, 1) || read_len < CP_IMG_HEADER_LEN) {
		printf("WARNING: Could check CRC of primary image\n");
		return 1;
	}


	new_hdr_crc = *( (MV_U32*)(hdr_offset + CP_CRC_OFFSET) );

	if (new_hdr_crc != hdr_crc) {
		printf("Header CRC primary %x != %x\n", new_hdr_crc, hdr_crc);
		return 1;
	}
	new_hdr_len = *( (MV_U32*)(hdr_offset + CP_IMG_LEN_OFFSET) );
	if (new_hdr_len != hdr_len) {
		printf("Header LEN primary %x != %x\n", new_hdr_len, hdr_len);
		return 1;
	}

	get_image_name((MV_U32) hdr_offset, cur_image_name, sizeof(cur_image_name));

	printf("Current primary image name: '%s'\n", cur_image_name);
	printf("New primary image name: '%s'\n", new_image_name);

	if (strcmp(cur_image_name, new_image_name)) {

		return 1;
	}

	read_len = CP_IMG_HEADER_LEN;
	if (nand_read_skip_bad(&(nand_info[nand_curr_device]), CP_DEFAULT_IMAGE_OFFSET, &read_len,hdr_offset, 1) || read_len < CP_IMG_HEADER_LEN) {
		printf("WARNING: Could check CRC of default image\n");
		return 1;
	}

	new_hdr_crc = *( (MV_U32*)(hdr_offset + CP_CRC_OFFSET) );
	if (new_hdr_crc != hdr_crc) {
		printf("Header CRC default %x != %x\n", new_hdr_crc, hdr_crc);
		return 1;
	}

	new_hdr_len = *( (MV_U32*)(hdr_offset + CP_IMG_LEN_OFFSET) );
	if (new_hdr_len != hdr_len) {
		printf("Header LEN default %x != %x\n", new_hdr_len, hdr_len);
		return 1;
	}

	get_image_name((MV_U32) hdr_offset, cur_image_name, sizeof(cur_image_name));
	
	printf("Current default image name: '%s'\n", cur_image_name);
	printf("New default image name: '%s'\n", new_image_name);

	if (strcmp(cur_image_name, new_image_name)) {

		return 1;
	}

	return 0;	
}


int
do_cp_boot_menu (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	g_cp_abort_boot = 1;
	main_loop();
	return -1;
}

U_BOOT_CMD(
        menu,      1,     1,      do_cp_boot_menu,
        "menu\n",
        " \n\tReturn to boot menu and wait for user input\n"
);

#define SW_LEDS_RESET_STR		"reset"
#define SW_LEDS_PROGRESS_STR	"progress"
#define SW_LEDS_GREEN_STR		"green"
#define SW_LEDS_BLINK_STR		"blink"
#define SW_LEDS_ON_STR			"on"
#define SW_LEDS_OFF_STR			"off"

int
do_cp_switch_leds(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	if (argc < 2) 
	{ 
		goto Usage; 
	}

	if (!strcmp(argv[1], SW_LEDS_RESET_STR))
	{
		cp_switch_LEDs_reset();
		return 0;
	}
	else if (!strcmp(argv[1], SW_LEDS_PROGRESS_STR))
	{
		cp_switch_LEDs_Progress_Bar();
		return 0;
	}
	else if (!strcmp(argv[1], SW_LEDS_GREEN_STR))
	{
		cp_switch_LEDs_green();
		return 0;
	}
	else if (!strcmp(argv[1], SW_LEDS_BLINK_STR))
	{
		cp_switch_LEDs_blink();
		return 0;
	}
	else if (!strcmp(argv[1], SW_LEDS_ON_STR))
	{
		cp_switch_LEDs_green_orange();
		return 0;
	}
	else if (!strcmp(argv[1], SW_LEDS_OFF_STR))
	{
		cp_switch_LEDs_off();
		return 0;
	}

	printf("Illegal option: %s\n", argv[1]);		

Usage:
	printf("%s",cmdtp->usage);
	return -1;

}


U_BOOT_CMD(
        switch_leds,      2,     1,      do_cp_switch_leds,
        "switch_leds ["SW_LEDS_RESET_STR"|"SW_LEDS_PROGRESS_STR \
        			 "|"SW_LEDS_GREEN_STR"|"SW_LEDS_BLINK_STR \
        			 "|"SW_LEDS_ON_STR"|"SW_LEDS_OFF_STR"]\n",
        " \n\tSet switch LEDs state\n"
);

int
do_cp_network (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	
	if (argc < 2) 
	{ 
		goto Usage; 
	} 

	/* Start/Stop ? */
	if (!strcmp(argv[1], "enable"))
	{
		cp_enable_network_ports();
	}
	else if (!strcmp(argv[1], "disable"))
	{
		cp_disable_network_ports();
	}
	else if (!strcmp(argv[1], "status"))
	{
		printf("Network interfaces are %s\n", 
			cp_is_network_enabled() ? "Enabled" : "Disabled");
	}
	else
	{
		goto Usage;
	}

	return 0;

Usage:
	printf("%s",cmdtp->usage);
	return -1;	
}

U_BOOT_CMD(
        network,      3,     1,      do_cp_network,
        "network [enable|disable|status]\n",
        " \n\tEnable/Disable network interfaces\n"
);


int cp_burn_dsl(char *DSLFile, unsigned int size)
{
	char* bdsl_argv[2];
	int rc = 0;
	
	printf("\n\n\nWARNING:\t\tBURNING OF NEW DSL STARTED");
	printf("\n\n\n\t\t\tPLEASE DO NOT PULL OUT THE POWER CORD \n\n\n");

	cp_switch_LEDs_Progress_Bar();

	bdsl_argv[0] = NULL;
	bdsl_argv[1] = DSLFile;
	if (nand_burn_dsl_cmd(NULL, size, 2, bdsl_argv) == 0)
	{
		printf("Burning new dsl image failed\n");
		rc = -1;
		goto CleanUp;
	}
	else
	{
		printf("Burning new dsl image succeeded\n");
		rc = 1; /* Meaning success - you can reset */
	}
	
	printf("Done.\n");

CleanUp:

	cp_switch_LEDs_reset();
	return rc;
}

int cp_burn_uboot(char *BootFile)
{
	char* bubt_argv[2];
	int rc = 0;
	
	printf("\n\n\nWARNING:\t\tBURNING OF NEW UBOOT STARTED");
	printf("\n\n\n\t\t\tPLEASE DO NOT PULL OUT THE POWER CORD \n\n\n");

	cp_switch_LEDs_Progress_Bar();

	bubt_argv[0] = NULL;
	bubt_argv[1] = BootFile;
	if (nand_burn_uboot_cmd(NULL, CP_BUBT_FLAG_FILE_IN_MEMORY, 2, bubt_argv) == 0)
	{
		printf("Burning new uboot failed\n");
		rc = -1;
		goto CleanUp;
	}
	else
	{
		printf("Burning new uboot succeeded\n");
		rc = 1; /* Meaning success - you can reset */
	}
	
	printf("Done.\n");

CleanUp:

	cp_switch_LEDs_reset();
	return rc;
}


int cp_burn_images(void)
{
	int rc = 0;
	
	printf("\n\n\nWARNING:\t\tBURNING OF NEW IMAGE STARTED");
	printf("\n\n\n\t\t\tPLEASE DO NOT PULL OUT THE POWER CORD \n\n\n");

	cp_switch_LEDs_Progress_Bar();
	
	printf("Running flash_erase: %s\n", getenv("flash_erase"));
	if (run_command ("run flash_erase", 0) < 0)
	{
		printf("flash_erase failed\n");
		rc = -1;
		goto CleanUp;
	}
	printf("Done.\n");

	printf("\nBurning default image\n");
	if (run_command ("run burn_default",0) < 0)
	{
		printf("Burning default image failed\n");
		rc = -1;
		goto CleanUp;
	}
	printf("Done.\n");

	printf("\nBurning primary image\n");
	if (run_command ("run burn_primary",0) < 0)
	{
		printf("Burning primary image failed\n");
		rc = -1;
		goto CleanUp;
	}

	/* Restore some uboot variables to their default state */
	setenv ("activePartition",	"1");
	setenv ("activeConfig",		"1");
	setenv ("bootcmd",			"run tf1_image$(activePartition)");
	setenv ("upgradeFlag",		"0");
	setenv ("recoverFlag",		"0");
	setenv ("bootImage",		"run set_bootargs ; bootm $(bootaddr)");
	
	printf("Done.\n");

CleanUp:

	cp_switch_LEDs_reset();
	return rc;
}

/* Clean CP_IMG_HEADER_LEN bytes from "loadaddr" */
int cp_clear_img_header(void)
{
	char cmd[256];
	
	sprintf(cmd, "mw.b %s 0 %s", getenv("loadaddr"), CP_TOSTRING(CP_IMG_HEADER_LEN));
	if (run_command(cmd, 0) < 0)
	{
		return -1;
	}
	return 0;
}

/* Verify CRC of default image and primary image.
 * This function is used to test whether the images on flash were burned 
 * correctly.
 * NOTE: they are done in-memory (at loadaddr)
 * If passed successfully, image in primary partition is loaded to loadaddr
 * Otherwise - you cannot boot safely without loading the desired image to
 * memory 
 * Also, this function should be called after loading an image from usb/network,
 * so that "filesize" reflects the image's size
 */
int cp_verify_images(void)
{
	int rc = 0;

	cp_switch_LEDs_Progress_Bar();

	if (getenv(CP_NO_CRC_CHECK_VAR))
	{
		printf("Skipping CRC check - %s is set\n", CP_NO_CRC_CHECK_VAR);
		goto CleanUp;
	}
	/* Clear memory before reading from flash, to make sure no "leftover" 
	 * image header is there 
	 */
	cp_clear_img_header();

	/* Read default image from flash to memory */
	run_command("nand read.e $(loadaddr) $(default_image_offset) $(filesize)", 0);
	/* Verify default image CRC */
	if ( !cp_verify_crc(simple_strtoul(getenv("loadaddr"), NULL, 16)))
	{
		printf("\nDefault image CRC verification failed!\n");
		rc = -1;
		goto CleanUp;
	}
	printf("\nDefault image CRC verification passed\n");

	/* Clear memory before reading from flash, to make sure no "leftover" 
	 * image header is there 
	 */
	cp_clear_img_header();
	
	/* Read primary image from flash to memory */
	run_command("nand read.e $(loadaddr) $(primary_offset) $(filesize)", 0);

	/* Verify primary image CRC */
	if ( !cp_verify_crc(simple_strtoul(getenv("loadaddr"), NULL, 16)))
	{
		printf("\nPrimary image CRC verification failed!\n");
		rc = -1;
		goto CleanUp;
	}
	printf("\nPrimary image CRC verification passed\n");

CleanUp:
	cp_switch_LEDs_reset();
	return rc;
}

static inline void cp_restore_net_vars(void)
{
	char *cp_old_autoload = NULL;

	cp_old_autoload = getenv("cp_old_autoload");

	if (cp_old_autoload != NULL)
	{
		setenv("autoload", cp_old_autoload);
		setenv("cp_old_autoload", NULL);
	}
}

/* Handle the received file after it was placed in memory
 * Parameters:
 *		filename	- The name of file received
 * 		type		- Outputs the type of file according to its name
 *
 * Returns:
 *		< 0	on failure
 *		>=0	otherwise
 */
int cp_reset_file_in_memory(char* filename, int* type)
{
	int rc = 0;
	int imagSize = 0;
	unsigned int annex=0;
	
	/* See which type of file we received, by its name */
	if (cp_filename_like(filename, CP_IMAGE_PREFIX, CP_IMAGE_SUFFIX))
	{
		*type = CP_GET_TYPE_FIRMWARE;
	}
	else if (cp_filename_like(filename, CP_UBOOT_PREFIX, CP_UBOOT_SUFFIX))
	{
		*type = CP_GET_TYPE_UBOOT;
	}
	else if (cp_filename_like(filename, CP_HW_TEST_PREFIX, CP_HW_TEST_SUFFIX))
	{
		*type = CP_GET_TYPE_HW_TEST;
	}
	else
	{
		*type = CP_GET_TYPE_UNKNOWN;
		printf("Illegal file type found: %s\n", filename);
		rc = -1;
		goto CleanUp;
	}
	
	switch (*type)
	{
	case CP_GET_TYPE_FIRMWARE:
	
		/* filesize is set by NetLoop when it succeeds */
		imagSize = simple_strtoul(getenv("filesize"), NULL, 16);
		
		if ( imagSize < CP_IMG_HEADER_LEN)
		{
			printf("\nFile size is smaller than expected header: ( %u < %u )\n",
				     								imagSize, CP_IMG_HEADER_LEN);
			printf("File cannot be verified!\n");
			rc = -1;
			goto CleanUp;
		}
		if ( !cp_verify_crc(simple_strtoul(getenv("loadaddr"), NULL, 16)))
		{
			printf("\nImage CRC verification failed!\n");
			rc = -1;
			goto CleanUp;
		}
		printf("\nImage CRC verification passed\n");

		if (cp_burn_images() < 0)
		{
			rc = -1;
			goto CleanUp;
		}
		if (cp_verify_images() < 0)
		{
			rc = -1;
			goto CleanUp;
		}
		cp_restore_net_vars();	
		rc = 1;
		goto CleanUp;
		break;
		
	case CP_GET_TYPE_UBOOT:
		if (cp_burn_uboot(filename) < 0)
		{
			rc = -1;
			goto CleanUp;
		}
		rc = 1;
		goto CleanUp;
		break;
		
	case CP_GET_TYPE_DSL:
		annex = sofaware_dsl_annex_2_int(sw_var_get_blob_adsl_annex());
		if (annex==0)
		{
			printf("ADSL is not supported\n");
			return 0;
		}
		if (cp_burn_dsl(filename, imagSize) < 0)
		{
			rc = -1;
			goto CleanUp;
		}
		rc = 1;
		goto CleanUp;
		break;
		
	case CP_GET_TYPE_HW_TEST:
		/* Network should be enabled, since HW test tool needs it on */
		cp_enable_network_ports();
		printf("Running HW test tool (\"go %s\")\n", getenv("loadaddr"));
		run_command("go $(loadaddr)", 0);
		/* We should never reach here - HW test tool is another uboot binary */
		rc = -1;
		goto CleanUp;
		break;
	}
CleanUp:
	return rc;
}
/* 		
 * Returns:
 * 		< 0  - Error
 *		== 0 - not error, but process not complete
 *		> 0	 - Success. After this you can boot
 *
 *		type - uboot or firmware (CP_GET_TYPE_FIRMWARE/CP_GET_TYPE_UBOOT)
 */
int cp_reset_from_net(int* type, proto_t protocol)
{
	MV_32 netRetry = 0, netSuccess = -1;
	int	rc = 0;
	unsigned int net_was_enabled = 0;

	if (!type)
	{
		rc = -1;
		goto CleanUp;
	}
	/* Clear memory before tftp, to make sure no "leftover" image header is there */
	cp_clear_img_header();

	/* Set autoload to "yes", since it we are running network boot */
	setenv("cp_old_autoload", getenv("autoload"));
	setenv("autoload", "yes");

	net_was_enabled = cp_is_network_enabled();
	if (!net_was_enabled)
	{
		cp_enable_network_ports();
	}

	switch (protocol)
	{
	case DHCP:
		/* Perform the DHCP */
		printf("Aquiring an IP address using DHCP...\n");

		while (netRetry < CP_NET_RETRY )
		{
			netSuccess = NetLoop(DHCP);
			if (netSuccess >= 0)
			{
				break;
			}
			netRetry++;
			printf("BOOTP attempt #%d ( out of %d ) failed...\n", netRetry, CP_NET_RETRY);
			mvOsDelay(1000);
		}
		break;
	case TFTP:
		netSuccess = NetLoop(TFTP);
	}

	if ( netSuccess < 0 )
	{
		printf("Network installation failed\n");
		rc = -1;
		goto CleanUp;
	}

	rc = cp_reset_file_in_memory(BootFile, type);
	if (rc < 0)
	{
		rc = -1;
		goto CleanUp;
	}
		
CleanUp:
	cp_restore_net_vars();
	if (cp_is_network_enabled() && !net_was_enabled)
	{
		cp_disable_network_ports();
	}
	return rc;
}

/* 
 * Params:
 *		type - uboot or firmware (CP_GET_TYPE_FIRMWARE/CP_GET_TYPE_UBOOT)
 * Returns:
 * 		< 0  - Error
 *		== 0 - not error, but process not complete
 *		> 0	 - Success. After this you can boot
 */
int cp_reset_from_usb(int* type)
{
   MV_32 imagSize = 0, netflag = 1;
   char* usbload[5];
   char *loadaddr;
   int	ret = 0;
   int	rc = 0;
  
#ifdef CONFIG_USB_STORAGE

    /* try to recognize storage devices immediately */
    if (usb_init() >= 0)
    {
        if(usb_stor_scan(1) >= 0)
        {
            netflag = 0;
			loadaddr = getenv("loadaddr");
			
			/* Clear memory before tftp, to make sure no "leftover" image header is there */
			cp_clear_img_header();
			
            usbload[0] = "fatload";
            usbload[1] = "usb";
            usbload[2] = "0:1";
            usbload[3] = loadaddr;
TryBoth:
			if ((*type == CP_GET_TYPE_BOTH) || (*type == CP_GET_TYPE_FIRMWARE)) 
			{
				/* If type == both - try firmware first */
            			usbload[4] = CP_IMAGE_PREFIX"*"CP_IMAGE_SUFFIX;
			}			
			else if (*type == CP_GET_TYPE_UBOOT)
			{
            			usbload[4] = CP_UBOOT_PREFIX"*"CP_UBOOT_SUFFIX;
			}
			else if (*type == CP_GET_TYPE_DSL)
			{
				printf("(*type == CP_GET_TYPE_DSL)\n");
            			usbload[4] = CP_DSL_IMAGE_PREFIX"*"CP_IMAGE_SUFFIX;
			}			
			else
			{
				printf("Illegal type requested: %d\n", *type);
				rc = -1;
				goto CleanUp;
			}
			
            printf("\nTrying to load image (%s) from USB flash drive using FAT FS\n", usbload[4]);
            if(do_fat_fsload(0, 0, 5, usbload) > 0)
            {
				//printf("\nError: Could not load recovery image from USB flash drive \n");
				/* Return 0 and not -1, so that in default boot, if USB is found but no file
				 * is on it, boot will continue w/o error 
				 */
				rc = 0;
				
				if (*type == CP_GET_TYPE_BOTH)
				{
					/* We want to try both types, so after failure to find  
				 	 * firmware, try to find uboot
				 	 */
					*type = CP_GET_TYPE_UBOOT;
					goto TryBoth;
				}
				else if (*type == CP_GET_TYPE_UBOOT)
				{
					/* We want to try both types, so after failure to find  
				 	 * firmware, try to find dsl image
				 	 */
				
					*type = CP_GET_TYPE_DSL;
					goto TryBoth;
				}
				goto CleanUp;
            }
            else
            {
				if (*type == CP_GET_TYPE_BOTH)
				{
					/* If we get here and type is still "both" it means firmware
					 * load succeeded. So let's change type back to "firmware"
					 * to avoid ambiguity
					 */
					*type = CP_GET_TYPE_FIRMWARE;
				}
				switch (*type)
				{
				case CP_GET_TYPE_FIRMWARE:

					imagSize = simple_strtoul(getenv("filesize"), NULL, 16); /* get the filesize env var */

					if ( imagSize < CP_IMG_HEADER_LEN)
					{
						printf("\nFile size is smaller than expected header: ( %u < %u )\n",
							     								imagSize, CP_IMG_HEADER_LEN);
						printf("File cannot be verified!\n");
						rc = -1;
						goto CleanUp;
					}
					cp_USB_LED_set(USB_PORTNR_UNKNOWN,1,1);
					ret = cp_verify_crc(simple_strtoul(loadaddr, NULL, 16));
					cp_USB_LED_set(USB_PORTNR_UNKNOWN,0,0);
					if (!ret)
					{
						printf("\nImage CRC verification failed!\n");
						rc = -1;
						goto CleanUp;
					}
					printf("\nImage CRC verification passed\n");

					if (!cp_check_if_image_changed(simple_strtoul(loadaddr, NULL, 16))) {
						printf("Image has not changed. Not installing.");
						rc = 0;
						goto CleanUp;
					}


					if (cp_burn_images() < 0)
					{
						rc = -1;
						goto CleanUp;
					}
					if (cp_verify_images() < 0)
					{
						rc = -1;
						goto CleanUp;
					}
					rc = 1; /* Meaning success - you can boot */
					goto CleanUp;

				case CP_GET_TYPE_UBOOT:
					if (cp_burn_uboot(CP_UBOOT_PREFIX"*"CP_UBOOT_SUFFIX) < 0)
					{
						rc = -1;
						goto CleanUp;
					}
					rc = 1; /* Meaning success - you can reset */
					goto CleanUp;
					break;
				case CP_GET_TYPE_DSL: 				
					imagSize = simple_strtoul(getenv("filesize"), NULL, 16); /* get the filesize env var */
					if (sofaware_check_dsl_image(load_addr)==0)
					{
						if (upgrade_dsl(load_addr, imagSize, 1) != 0)
						{
							printf("Error: Upgrade dsl failed\n");
						}
						else {						
							printf("\nDone\n"); 
						}
					}
					rc = 1;
					goto CleanUp;
					break;					
			
				default:
					printf("Illegal type requested: %d\n", *type);
					rc = -1;
					goto CleanUp;
				}
            }
        }
		rc = 0;
		goto CleanUp;
    }
    printf("\nError loading USB subsystem - please reset the device and try again .. \n");
    rc = -1;
	goto CleanUp;
#endif
CleanUp:
	return rc;
}



// mike: original recovery handling function 
void recoveryHandle(void)
{
	char cmd[256];
	char img[10];
	char * argv[3];
	char *env, *env1;
	MV_32 imagAddr = 0x400000;	
	MV_32 imagSize = 0, netflag = 1;	
	char ip[16]= {"dhcp"};
	char tmp[15];
    char* usbload[5];

    /* First try to perform recovery from USB DOK*/
#ifdef CONFIG_USB_STORAGE
    /* try to recognize storage devices immediately */
    if (usb_init() >= 0)
    {
        if(usb_stor_scan(1) >= 0)
        {
            netflag = 0;
            usbload[0] = "fatload";
            usbload[1] = "usb";
            usbload[2] = "0:1";
            usbload[3] = getenv("loadaddr");
            usbload[4] = "/flashware.img";
            printf("Trying to load image from USB flash drive using FAT FS\n");
            if(do_fat_fsload(0, 0, 5, usbload) == 1)
            {
                printf("Trying to load image from USB flash drive using ext2 FS partition 0\n");
                usbload[2] = "0:0";
                if(do_ext2load(0, 0, 5, usbload) == 1)
                {
                    printf("Trying to load image from USB flash drive using ext2 FS partition 1\n");
                    usbload[2] = "0:1";
                    if(do_ext2load(0, 0, 5, usbload) == 1)
                    {
                        printf("Couldn't load recovery image from USB flash drive, Trying network interface\n");
                        netflag = 1;
                    }
                    else
                    {
                        env = getenv("filesize");
                        imagSize = simple_strtoul(env, NULL, 16); /* get the filesize env var */
                        sprintf(ip, "usb");
                    }
                }
                else
                {
                    env = getenv("filesize");
                    imagSize = simple_strtoul(env, NULL, 16); /* get the filesize env var */
                    sprintf(ip, "usb");
                }
            }
            else
            {
                env = getenv("filesize");
                imagSize = simple_strtoul(env, NULL, 16); /* get the filesize env var */
                sprintf(ip, "usb");
            }
        }
    }
#endif

    if(netflag == 1)
    {
        /* Perform the DHCP */
    	printf("Aquiring an IP address using DHCP...\n");
    	if (NetLoop(DHCP) == -1)
    	{
    		mvOsDelay(1000);
    		if (NetLoop(DHCP) == -1)
    		{
    			mvOsDelay(1000);
    			if (NetLoop(DHCP) == -1)
    			{
    				ulong tmpip;
    				printf("Failed to retreive an IP address assuming default (%s)!\n", getenv("rcvrip"));
    				tmpip = getenv_IPaddr ("rcvrip");
    				NetCopyIP(&NetOurIP, &tmpip);			
    				sprintf(ip, "static");
    			}
    		}
    	}
    
    	/* Perform the recovery */
    	printf("Starting the Recovery process to retreive the file...\n");
        if ((imagSize = NetLoop(RCVR)) == -1)
    	{
    		printf("Failed\n");
    		return;
    	}
    }

	/* Boot the downloaded image */	
	env = getenv("loadaddr");
	if (!env)
		printf("Missing loadaddr environment variable assuming default (0x400000)!\n");
	else
		imagAddr = simple_strtoul(env, NULL, 16); /* get the loadaddr env var */

    /* This assignment to cmd should execute prior to the RD setenv and saveenv below*/
    printf("Update bootcmd\n");
    env = getenv("ethaddr");
    env1 = getenv("eth1addr");
    sprintf(cmd,"setenv bootargs $(console) root=/dev/ram0 $(mvNetConfig) rootfstype=squashfs initrd=0x%x,0x%x ramdisk_size=%d recovery=%s serverip=%d.%d.%d.%d  ethact=$(ethact) ethaddr=%s eth1addr=%s; bootm 0x%x;", 
            imagAddr + 0x200000, (imagSize - 0x300000), (imagSize - 0x300000)/1024, ip, (NetServerIP & 0xFF), ((NetServerIP >> 8) & 0xFF), ((NetServerIP >> 16) & 0xFF), ((NetServerIP >> 24) & 0xFF),env, env1, imagAddr);

    if(RD_88F6281A_ID == mvBoardIdGet() || SHEEVA_PLUG_ID == mvBoardIdGet())
    {
        setenv("bootcmd","setenv bootargs $(console) rootfstype=squashfs root=/dev/mtdblock2 $(mvNetConfig) $(mvPhoneConfig; nand read.e $(loadaddr) 0x100000 0x200000; bootm $(loadaddr);");
    	sprintf(tmp,"console=ttyS0,%s",getenv("baudrate"));
		setenv("console",tmp);
        saveenv();
    }
    else if(RD_88F6192A_ID == mvBoardIdGet())
    {
        setenv("console","console=ttyS0,115200 mtdparts=spi_flash:0x100000@0x0(uboot)ro,0x200000@0x100000(uimage),0xb80000@0x300000(rootfs),0x180000@0xe80000(varfs),0xf00000@0x100000(flash) varfs=/dev/mtdblock3");
    	setenv("bootcmd","setenv bootargs $(console) rootfstype=squashfs root=/dev/mtdblock2; bootm 0xf8100000;");
        saveenv();
    }

	printf("\nbootcmd: %s\n", cmd);
	setenv("bootcmd", cmd);

	printf("Booting the image (@ 0x%x)...\n", imagAddr);

	sprintf(cmd, "boot");
	sprintf(img, "0x%x", imagAddr);
	argv[0] = cmd;
	argv[1] = img;

	do_bootd(NULL, 0, 2, argv);
}

int cp_recoveryHandle(void)
{
	printf("\n\n\nWARNING:  RESET TO FACTORY DEFAULTS PROCESS STARTED:  WARNING: \n\n\n");
	printf("\t\t\t PLEASE DO NOT PULL OUT THE POWER CORD \n\n\n");

	cp_switch_LEDs_Progress_Bar();
	run_command("run return_to_default",0);
	cp_switch_LEDs_reset();
	restart_pci();
	run_command("run bootImage",0);	// this should reboot the system upon completion 
	return -1;
}

int cp_abort_boot_flag(void)
{
	return g_cp_abort_boot;
}

int cp_usb_attempted(void)
{
	return g_cp_usb_attempted;
}

void cp_usb_set_attempted(void)
{
	g_cp_usb_attempted = 1;
}



static __inline__ int recover_abortboot(int bootdelay)
{
	int abort = 0;
    char keyCmb;

#ifdef CONFIG_SILENT_CONSOLE
	{
		DECLARE_GLOBAL_DATA_PTR;

		if (gd->flags & GD_FLG_SILENT) {
			/* Restore serial console */
			console_assign (stdout, "serial");
			console_assign (stderr, "serial");
		}
	}
#endif

#ifdef CONFIG_MENUPROMPT
	printf(CONFIG_MENUPROMPT, bootdelay);
#endif

#if defined CONFIG_ZERO_BOOTDELAY_CHECK
	/*
	 * Check if key already pressed
	 * Don't check if bootdelay < 0
	 */
	if (bootdelay >= 0) {
		if (tstc()) {	/* we got a key press	*/
            keyCmb = getc();
            if ( keyCmb == 0x03 ) {
                abort = 1; 	/* Ctrl + C , don't auto boot */
				g_cp_abort_boot = 1;
            }
		}
	}
#endif

	putc ('\n');

#ifdef CONFIG_SILENT_CONSOLE
	{
		DECLARE_GLOBAL_DATA_PTR;

		if (abort) {
			/* permanently enable normal console output */
			gd->flags &= ~(GD_FLG_SILENT);
		} else if (gd->flags & GD_FLG_SILENT) {
			/* Restore silent console */
			console_assign (stdout, "nulldev");
			console_assign (stderr, "nulldev");
		}
	}
#endif

	return abort;
}

/* Returns number of LAN ports according to unitModel environment variable 
 * NOTE: 
 *   The correct unitModel variable MUST(!) be set prior to calling this function
 */
static int cp_get_num_lans(void)
{
	if ( !strcmp(getenv("unitModel"), CP_L50_STR) )
	{
		return CP_L50_LANS_NUM;
	}
	if ( !strcmp(getenv("unitModel"), CP_L30_STR) )
	{
		return CP_L30_LANS_NUM;
	}	
	return CP_DEF_LANS_NUM;
}

static unsigned int cp_get_lan_mac_offset(int index)
{
	switch (index)
	{
	case 1:
		return CP_NAND_OFFSET_LAN1_MAC;
	case 2:
		return CP_NAND_OFFSET_LAN2_MAC;
	case 3:
		return CP_NAND_OFFSET_LAN3_MAC;
	case 4:
		return CP_NAND_OFFSET_LAN4_MAC;
	case 5:
		return CP_NAND_OFFSET_LAN5_MAC;
	case 6:
		return CP_NAND_OFFSET_LAN6_MAC;
	case 7:
		return CP_NAND_OFFSET_LAN7_MAC;
	case 8:
		return CP_NAND_OFFSET_LAN8_MAC;
	}
	return CP_NAND_OFFSET_LAN1_MAC;
}

static void cp_set_mac_var(char* if_mac_name, const u_char* buf)
{
	char mac_str[18];
		
 	sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X%c", buf[0], buf[1],buf[2],
														buf[3], buf[4],buf[5], '\0');
	setenv(if_mac_name, mac_str);
}

static void cp_set_wireless_region(char* region_var_name, const u_char region) {
	char region_str[4];
	if ((region >= 0) && (region < 7))
	{
		sprintf(region_str, "%u%c", region, '\0');	
		setenv(region_var_name, region_str);
	}
	else
		setenv(region_var_name, 0);
}

static int cp_set_board_vars(void)
{
	u_char settings_buf[CP_BOARD_SETTINGS_NAND_SIZE];
	char lan_str[32];
	char unit_name_str[CP_UNIT_NAME_LEN];
	char setting_str[CP_SETTING_LEN];
	unsigned char wifi_region;
	nand_read_options_t rd_opts;
	nand_read_options_t rd_opts_dsl;
	int ret, _i, i, rc=0;
	int	magic1 = 0;
	int crc = 0;
	int dslSupported=1;
	u_char dslHeader[100];
	u_char dslFwName[65];
	char capabilities[0x201];
	
#ifdef SEATTLE
        setenv("sub_hw_ver", "SEATTLE\0");
#else
        setenv("sub_hw_ver", "LONDON\0");
#endif
	/* Allow writing of read-only variables */
	cp_disable_read_only();

	/* Read buffer from flash */
	memset(&rd_opts, 0, sizeof(rd_opts));
	rd_opts.buffer = settings_buf;
	rd_opts.length = sizeof(settings_buf);
	rd_opts.offset = CP_BOARD_SETTINGS_NAND_ADDR;
	rd_opts.quiet = 0;

	ret = nand_read_opts(&nand_info[0], &rd_opts);
	if (ret < 0)
	{
		printf("\tError: cp_set_board_vars: NAND read failed!\n");
		rc = -1;
		goto CleanUp;
	}
	
	/* Verify magic number on flash is correct */
	memcpy(&magic1, &settings_buf[CP_NAND_OFFSET_CRC], sizeof(magic1));

	crc = crc32(0, &settings_buf[CP_NAND_OFFSET_START], 
					CP_BOARD_SETTINGS_NAND_SIZE - CP_NAND_OFFSET_START);

	printf("Verifying CRC for settings area... ");
	
	if ( magic1 != crc )
	{
		printf("\nSettings CRC verification failed\n");
		printf("Expected crc:....0x%08x\n", crc);
		printf("Found crc:.......0x%08x\n", magic1);
		rc = -1;
		goto CleanUp;
	}
	printf("Done\n");

	/* No snprintf, unfortunately */
	settings_buf[CP_NAND_OFFSET_UNIT_MODEL + sizeof(setting_str)-1] = '\0';
	sprintf(setting_str, "%s", &settings_buf[CP_NAND_OFFSET_UNIT_MODEL]);
	setting_str[sizeof(setting_str)-1] = '\0';
	setenv("unitModel", setting_str);

	/* No snprintf, unfortunately */
	settings_buf[CP_NAND_OFFSET_UNIT_NAME + CP_UNIT_NAME_LEN - 1] = '\0';
	sprintf(unit_name_str, "%s", &settings_buf[CP_NAND_OFFSET_UNIT_NAME]);
	unit_name_str[sizeof(unit_name_str)-1] = '\0';
	setenv("unitName", unit_name_str);

	/* No snprintf, unfortunately */
	settings_buf[CP_NAND_OFFSET_UNIT_VER + sizeof(setting_str)-1] = '\0';
	sprintf(setting_str, "%s", &settings_buf[CP_NAND_OFFSET_UNIT_VER]);
	setting_str[sizeof(setting_str)-1] = '\0';
	setenv("unitVer", setting_str);

	cp_set_mac_var("hw_mac_addr", &settings_buf[CP_NAND_OFFSET_HW_MAC]);
	/* Change ethaddr as well - for DHCP to have unique MAC */
	cp_set_mac_var("ethaddr",  &settings_buf[CP_NAND_OFFSET_HW_MAC]);
	cp_set_mac_var("eth1addr", &settings_buf[CP_NAND_OFFSET_LAN1_MAC]);

	/* Set LAN MACs */
	for (_i=1 ; _i <= cp_get_num_lans() ; _i++)
	{
		sprintf(lan_str, "lan%d_mac_addr", _i);
		cp_set_mac_var(lan_str, &settings_buf[cp_get_lan_mac_offset(_i)]);
	}

	/* Set DMZ MACs */
	cp_set_mac_var("dmz_mac_addr", &settings_buf[CP_NAND_OFFSET_DMZ_MAC]);

	/* Set DSL MAC */
#ifdef SEATTLE
	cp_set_mac_var("dsl_mac_addr", &settings_buf[CP_NAND_OFFSET_DSL_MAC]);


	/* Set DSL ANNEX */
	/* ANNEX should be taken from blob if not exists should not be set.
	 * This is an indication if ADSL supported in current board
	 */
	settings_buf[CP_NAND_OFFSET_DSL_ANNEX + ANNEX_STR_LEN] = '\0';
	sprintf(setting_str, "%s", &settings_buf[CP_NAND_OFFSET_DSL_ANNEX]);
	setting_str[ANNEX_STR_LEN] = '\0';
	if (!strncmp(setting_str,ANNEX_A,ANNEX_STR_LEN)|| !strncmp(setting_str,"annxA",6))	
		setenv("dsl_annex", ANNEX_A);//ADSL is supported
	else if (!strncmp(setting_str,ANNEX_B,ANNEX_STR_LEN)|| !strncmp(setting_str,"annxB",6))	
		setenv("dsl_annex", ANNEX_B);//ADSL is supported
	else if (!strncmp(setting_str,ANNEX_C,ANNEX_STR_LEN)|| !strncmp(setting_str,"annxC",6))	
		setenv("dsl_annex", ANNEX_C);//ADSL is supported
	else
		dslSupported = 0;
 
	if (dslSupported)
	{
		/* Read buffer from flash */
		memset(&rd_opts_dsl, 0, sizeof(rd_opts_dsl));
		rd_opts_dsl.buffer = dslHeader;
		rd_opts_dsl.length = sizeof(dslHeader);
		rd_opts_dsl.offset = CP_ADSL_OFFSET;
		rd_opts_dsl.quiet = 0;
		ret = nand_read_opts(&nand_info[0], &rd_opts_dsl);
		
		if (ret < 0)
		{
			printf("\tError: cp_set_board_vars: NAND read failed (dslHeader)!\n");
			rc = -1;	
		}
		else
		{
			memset(dslFwName,0,65);
			memcpy(dslFwName,&dslHeader[0x10],64);
			setenv("dsl_firmware",dslFwName);			
		}
	}
	
#endif
	printf("/* Wireless region code */");
	wifi_region=settings_buf[CP_NAND_OFFSET_WIRELESS_REGION];
	cp_set_wireless_region("wireless_region", wifi_region);

	/* Marketing Capabilities */
	if (settings_buf[CP_NAND_OFFSET_LICENSE_TYPE] != '\0')
	{
		for(i=0;i<8;i++)
			capabilities[i] = settings_buf[CP_NAND_OFFSET_LICENSE_TYPE+i];
		capabilities[i]='\0';
		setenv("marketing_capabilities", capabilities);
	}
	if (settings_buf[CP_NAND_OFFSET_HW_CAPABILITIES_TYPE] != '\0')
	{
		for(i=0;i<8;i++)
			capabilities[i] = settings_buf[CP_NAND_OFFSET_HW_CAPABILITIES_TYPE+i];
		capabilities[i]='\0';
		setenv("hardware_capabilities", capabilities);
	}
	if (settings_buf[CP_NAND_OFFSET_MGMT_OPQ_TYPE] != '\0')
	{
		for(i=0;i<0x80;i++)
			capabilities[i] = settings_buf[CP_NAND_OFFSET_MGMT_OPQ_TYPE+i];
		capabilities[i]='\0';
		setenv("mgmt_opq", capabilities);
	}
	
	
	if (settings_buf[CP_NAND_OFFSET_MGMT_SIGNATURE] != '\0')
	{
		for(i=0;i<0x200;i++)
			capabilities[i] = settings_buf[CP_NAND_OFFSET_MGMT_SIGNATURE+i];
		capabilities[i]='\0';
		setenv("mgmt_signature", capabilities);
	}

	if (settings_buf[CP_NAND_OFFSET_MGMT_SIGNATURE_VER] != '\0')
	{
		for(i=0;i<0x80;i++)
			capabilities[i] = settings_buf[CP_NAND_OFFSET_MGMT_SIGNATURE_VER+i];
		capabilities[i]='\0';
		setenv("mgmt_signature_ver", capabilities);
	}

CleanUp:
	cp_enable_read_only();
	return rc;
}

int
cp_abort_factory_default(void)
{
	int bootdelay = CP_NET_FD_DELAY;
	int leds_on = 1;
	int abort = 0;

	/* Blink for CP_NET_FD_DELAY seconds
	 * in case anybody is watching...
	 */
	printf ("Press any key to abort: %2d", bootdelay);

	while ((bootdelay > 0) && (!abort))
	{
		int i;

		if (leds_on)
		{
			cp_switch_LEDs_off();
			leds_on = 0;
		}
		else
		{
			cp_switch_LEDs_green_orange();
			leds_on = 1;
		}
		
		--bootdelay;
		/* delay 100 * 10ms */
		for (i=0; !abort && i<100; ++i)
		{
			if (tstc()) /* we got a key press	*/
			{
				abort  = 1;	/* don't auto boot	*/
                bootdelay = 0;	/* no more delay	*/
				(void) getc();  /* consume input	*/
				break;
			}
			udelay (10000);
		}
		printf ("\b\b%2d", bootdelay);				
	}
	cp_switch_LEDs_reset();
	printf("\n");
	if (abort)
	{
		printf("Aborted...\n");
	}
	return abort;
}

int g_should_pause_after_usb_install_firm = 1;
#define stringify(s)	tostring(s)
#define tostring(s)	#s
void recoveryCheck(void)
{
	int rv = 0;
	int install_type = CP_GET_TYPE_FIRMWARE;
	char *install_from = CP_IMAGE_UBOOT_INSTALL_FROM;

	if (cp_set_board_vars() < 0)
	{
		g_cp_abort_boot = 1;
		cp_indicate_boot_error();
		return;
	}
	
	if (recover_abortboot(0))
	{
		printf("\n\'Ctrl + C\' Detected. Proceeding to boot menu.\n");
		return;
	}

	if (rcvrflag)
	{
		/* Power - ORANGE Static. Alert - ORANGE Blinking */
		cp_GPIO_LED_set(POWER_LED_GREEN,	LED_CMD_ON);
		cp_GPIO_LED_set(POWER_LED_GREEN,	LED_CMD_STATIC);

		cp_GPIO_LED_set(POWER_LED_RED,		LED_CMD_ON);
		cp_GPIO_LED_set(POWER_LED_RED,		LED_CMD_STATIC);
	
		cp_GPIO_LED_set(ALERT_LED_GREEN,	LED_CMD_ON);
		cp_GPIO_LED_set(ALERT_LED_GREEN,	LED_CMD_BLINK);
				
		cp_GPIO_LED_set(ALERT_LED_RED,		LED_CMD_ON);
		cp_GPIO_LED_set(ALERT_LED_RED,		LED_CMD_BLINK);
	}
	
	/* Automatic reset from usb is both for firmware and uboot */
	install_type = CP_GET_TYPE_BOTH;
 	rv = cp_reset_from_usb(&install_type);

	/* Set indication that USB was attempted, and was unsuccessful (Otherwise 
	 * system would have booted, and this line is not reached)
	 *
	 * Due to USB limitation, this will later mean that in order to load USB 
	 * again from uboot, a reset is needed
	 */
	cp_usb_set_attempted();
	
    if ( rv < 0 )
    {
        printf("\nERROR: " CP_IMAGE_INSTALL_FROM " USB failed.\n");
		cp_indicate_boot_error();
    }
	else if ( rv > 0)
	{
		printf("\n" CP_IMAGE_INSTALL_FROM " USB succeeded.\n");

		if (g_should_pause_after_usb_install_firm 
			|| install_type != CP_GET_TYPE_FIRMWARE) {
			printf("\nPlease remove USB device.\n");
		}
		setenv ("cp_quiet", CP_BOOT_MODE_QUIET_STR);
		setenv ("cp_boot_mode", stringify(CP_BOOT_MODE_QUIET));

		cp_indicate_install_success((g_should_pause_after_usb_install_firm 
									|| install_type != CP_GET_TYPE_FIRMWARE) ? 1 : 0);
		if (install_type == CP_GET_TYPE_FIRMWARE)
		{
			restart_pci();
			run_command ("run bootImage",0);
		}
		else if (install_type == CP_GET_TYPE_UBOOT)
		{
			run_command ("reset",0);
		}
		else
		{
			printf("Unknown file type received\n");
		}
	}

	/* Start the recovery process if indicated by user via reset button press */
	if (rcvrflag)
	{		
		install_from = CP_IMAGE_UBOOT_INSTALL_FROM;
		install_type = CP_GET_TYPE_FIRMWARE;
		
		rv = cp_reset_from_net(&install_type, DHCP);
		
		if (install_type == CP_GET_TYPE_UBOOT)
		{
			install_from = CP_UBOOT_INSTALL_FROM;								
		}
		else if (install_type == CP_GET_TYPE_FIRMWARE)
		{
			install_from = CP_IMAGE_INSTALL_FROM;
		}

		if ( rv < 0 )
		{
			printf("\nERROR: %s Network failed.\n", install_from);
		}
		else if ( rv > 0 )
		{
			printf("\n%s Network succeeded.\n", install_from);
			cp_indicate_install_success(1);
			setenv ("cp_quiet", CP_BOOT_MODE_QUIET_STR);
			setenv ("cp_boot_mode", stringify(CP_BOOT_MODE_QUIET));

			if (install_type == CP_GET_TYPE_FIRMWARE)
			{
				restart_pci();
				run_command ("run bootImage",0);
			}
			else if (install_type == CP_GET_TYPE_UBOOT)
			{
				run_command ("reset",0);
			}
			else
			{
				printf("Unknown file type received\n");
			}
		}
		/* FACTORY DEFAULTS button was pressed, but no image from net - resetting to 
		 * factory defaults from device.
		 *
		 * However, if a uboot image was found, and did not succeed - the user probably 
		 * did not mean to reset to factory defaults...
		 */
		if (install_type != CP_GET_TYPE_UBOOT)
		{
			printf ("\nProceeding to local recovery of factory defaults image\n");
#ifdef CP_DELAY_BEFORE_FD
			if (cp_abort_factory_default())
			{
				return;
			}
#endif
			cp_recoveryHandle(); 
		}
		return;
	}
}
#endif

#ifdef MV_INC_BOARD_SPI_FLASH
#include <environment.h>
#include "norflash/mvFlash.h"

void memcpyFlash(env_t *env_ptr, void* buffer, MV_U32 size)
{
    MV_FLASH_INFO *pFlash;
    pFlash = getMvFlashInfo(BOOT_FLASH_INDEX);

    mvFlashBlockRd(pFlash,(MV_U32 *)env_ptr - mvFlashBaseAddrGet(pFlash),
                    size, (MV_U8 *)buffer);
}
#endif

