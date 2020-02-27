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

#include <config.h>
#include <common.h>
#include <command.h>
#include <pci.h>

#include "mvTypes.h"
#include "mvCtrlEnvLib.h"

#include <sofaware_crc.h>
#include <spi_flash.h>
#include "../../../drivers/twsi.h"
#if defined(MV_INC_BOARD_NOR_FLASH)
#include "norflash/mvFlash.h"
#endif

#if defined(MV_INCLUDE_UNM_ETH) || defined(MV_INCLUDE_GIG_ETH)
#include "eth-phy/mvEthPhy.h"
#include "../USP/ethSwitch/mvSwitch.h"
#endif

#if defined(MV_INCLUDE_PEX)
#include "pex/mvPex.h"
#endif

#if defined(MV_INCLUDE_IDMA)
#include "idma/mvIdma.h"
#include "sys/mvSysIdma.h"
#endif
static int write_preset_cfg_file(unsigned long img, unsigned long len);
#if defined(CFG_NAND_BOOT) || defined(CFG_CMD_NAND)
#include <nand.h>

/* references to names in cmd_nand.c */
//extern struct nand_chip nand_dev_desc[];
extern nand_info_t nand_info[];       /* info for NAND chips */
/* int nand_rw (struct nand_chip* nand, int cmd,
	    size_t start, size_t len,
	    size_t * retlen, u_char * buf);
 int nand_erase(struct nand_chip* nand, size_t ofs,
				size_t len, int clean);
*/
#define GPIO_DSL_POWER           0
#define GPIO_DSL_SW_RST          0x6

#define SWAP_LONG(x) \
	((__u32)( \
		(((__u32)(x) & (__u32)0x000000ffUL) << 24) | \
		(((__u32)(x) & (__u32)0x0000ff00UL) <<  8) | \
		(((__u32)(x) & (__u32)0x00ff0000UL) >>  8) | \
		(((__u32)(x) & (__u32)0xff000000UL) >> 24) ))
#ifdef SEATTLE		
#define CP_PART_DSL_PRI_HDR_OFFSET CP_ADSL_OFFSET
#define CP_PART_DSL_PRI_HDR_SIZE 0x00020000
//#define CP_PART_DSL_SEC_OFFSET CP_PART_DSL_PRI_HDR_OFFSET + CP_PART_DSL_PRI_HDR_SIZE
//#define CP_PART_DSL_SEC_SIZE 0x00200000
#define CP_PART_PRESET_CFG_OFFSET CP_PRESET_CFG_OFFSET
#define CP_PART_PRESET_CFG_SIZE CP_PRESET_CFG_SIZE
#define PRESET_HEADER "#Check point preset configuration file"
#define BLOB_HEADER "# SG80 Boot Loader script"
//#define CFG_BLOB_ADDR 0x2000000
#endif

extern int get_read_only_error();

typedef struct
{		
	ulong crc1;
	int magic;
	char unused[8];
	char wan_mac[0x10];
	char lan_mac_1[0x10];
	char lan_mac_2[0x10];
	char lan_mac_3[0x10];
	char lan_mac_4[0x10];
	char lan_mac_5[0x10];
	char lan_mac_6[0x10];
	char lan_mac_7[0x10];
	char lan_mac_8[0x10];
	char dmz_mac[0x10];
	char module[0x10]; 
	char unitVer[0x10];
	char unitName[0x40];
	char region[0x10];
	char dsl_mac[0x10];
	char dslAnnex[0x10];
	char marketingCapabilities[0x10];
	char hwCapabilities[0x20];
	char managment[0x80];	
	char mgmtSignature[0x200];
	char mgmtSignatureVer[0x80];	
} cp_blob_t;

typedef struct
{	
	cp_blob_t blob_param;
	char free_space[CP_BOARD_SETTINGS_NAND_SIZE-sizeof(cp_blob_t)];	
} cp_blob_full_t;

static cp_blob_full_t curr_blob;

#define CP_BLOB_MAGIC             0xA5A51235
#define CP_IS_BLOB_EXIST          (curr_blob.blob_param.magic == CP_BLOB_MAGIC)
#define __CP_VAR_GET_BLOB_MAC  getenv("hw_mac_addr") 
#define __CP_VAR_GET_BLOB_WLAN_REGION    getenv("wireless_region")
#define CP_VAR_GET_BLOB_WLAN_REGION      (__CP_VAR_GET_BLOB_WLAN_REGION?__CP_VAR_GET_BLOB_WLAN_REGION:"")

#define TMP_BLOB_MAC              "00:11:22:33:44:55"
#define CP_IS_TMP_BLOB            (CP_IS_BLOB_EXIST && (cp_macstr_cmp(__CP_VAR_GET_BLOB_MAC, curr_blob->wan_mac)==0))

#define NAND_PAGE_SIZE      (16*1024)
#define NAND_BLOCK_SIZE (128*1024)


extern int nand_erase_opts(nand_info_t *meminfo, const nand_erase_options_t *opts);
extern int nand_write_opts(nand_info_t *meminfo, const nand_write_options_t *opts);

extern unsigned int	cp_is_network_enabled(void);
extern void			cp_enable_network_ports(void);
extern void			cp_disable_network_ports(void);

extern void mvEthSwitchRegRead(MV_U32 ethPortNum, MV_U32 phyAddr, MV_U32 regOffs, MV_U16 *data);
extern void mvEthSwitchRegWrite(MV_U32 ethPortNum, MV_U32 phyAddr, MV_U32 regOffs, MV_U16 data);

#endif /* CFG_NAND_BOOT */

#if (CONFIG_COMMANDS & CFG_CMD_FLASH)
#if !defined(CFG_NAND_BOOT)
static unsigned int flash_in_which_sec(flash_info_t *fl,unsigned int offset)
{
	unsigned int sec_num;
	if(NULL == fl)
		return 0xFFFFFFFF;

	for( sec_num = 0; sec_num < fl->sector_count ; sec_num++){
		/* If last sector*/
		if (sec_num == fl->sector_count -1)
		{
			if((offset >= fl->start[sec_num]) && 
			   (offset <= (fl->size + fl->start[0] - 1)) )
			{
				return sec_num;
			}

		}
		else
		{
			if((offset >= fl->start[sec_num]) && 
			   (offset < fl->start[sec_num + 1]) )
			{
				return sec_num;
			}

		}
	}
	/* return illegal sector Number */
	return 0xFFFFFFFF;

}

#endif /* !defined(CFG_NAND_BOOT) */


/*******************************************************************************
burn a u-boot.bin on the Boot Flash
********************************************************************************/
extern flash_info_t flash_info[];       /* info for FLASH chips */
#include <net.h>
#include "bootstrap_def.h"
#if (CONFIG_COMMANDS & CFG_CMD_NET)
/* 
 * 8 bit checksum 
 */
static u8 checksum8(u32 start, u32 len,u8 csum)
{
    register u8 sum = csum;
	volatile u8* startp = (volatile u8*)start;

    if (len == 0)
	return csum;

    do{
	  	sum += *startp;
		startp++;
    }while(--len);

    return (sum);
}

/*
 * Check the extended header and execute the image
 */
static MV_U32 verify_extheader(ExtBHR_t *extBHR)
{
	MV_U8	chksum;


	/* Caclulate abd check the checksum to valid */
	chksum = checksum8((MV_U32)extBHR , EXT_HEADER_SIZE -1, 0);
	if (chksum != (*(MV_U8*)((MV_U32)extBHR + EXT_HEADER_SIZE - 1)))
	{
		printf("Error! invalid extende header checksum\n");
		return MV_FAIL;
	}
	
    return MV_OK;
}
/*
 * Check the CSUM8 on the main header
 */
static MV_U32 verify_main_header(BHR_t *pBHR, MV_U8 headerID)
{
	MV_U8	chksum;

	/* Verify Checksum */
	chksum = checksum8((MV_U32)pBHR, sizeof(BHR_t) -1, 0);

	if (chksum != pBHR->checkSum)
	{
		printf("Error! invalid image header checksum\n");
		return MV_FAIL;
	}

	/* Verify Header */
	if (pBHR->blockID != headerID)
	{
		printf("Error! invalid image header ID\n");
		return MV_FAIL;
	}
	
	/* Verify Alignment */
	if (pBHR->blockSize & 0x3)
	{
		printf("Error! invalid image header alignment\n");
		return MV_FAIL;
	}

	if ((cpu_to_le32(pBHR->destinationAddr) & 0x3) && (cpu_to_le32(pBHR->destinationAddr) != 0xffffffff))
	{
		printf("Error! invalid image header destination\n");
		return MV_FAIL;
	}

	if ((cpu_to_le32(pBHR->sourceAddr) & 0x3) && (pBHR->blockID != IBR_HDR_SATA_ID))
	{
		printf("Error! invalid image header source\n");
		return MV_FAIL;
	}

    return MV_OK;
}

int nandenvECC = 0;
#if defined(CFG_NAND_BOOT)
/* Boot from NAND flash */
/* Write u-boot image into the nand flash */
int nand_burn_uboot_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int filesize;
	MV_U32 ret = 0, rv = 1;
	extern char console_buffer[];
	nand_info_t *nand = &nand_info[0];
	nand_erase_options_t er_opts;
	nand_write_options_t wr_opts;
	unsigned int net_was_enabled = 0;

	load_addr = CFG_LOAD_ADDR; 
	if(argc == 2) {
		copy_filename (BootFile, argv[1], sizeof(BootFile));
	}
	else { 
		copy_filename (BootFile, "u-boot.bin", sizeof(BootFile));
		printf("using default file \"u-boot.bin\" \n");
	}

	if (flag != CP_BUBT_FLAG_FILE_IN_MEMORY)
	{
		net_was_enabled = cp_is_network_enabled();
		if (!net_was_enabled)
		{
			cp_enable_network_ports();
		}

		if ((filesize = NetLoop(TFTP)) < 0)
		{
			rv = 0;
			goto CleanUp;
		}
	}

#ifdef MV_BOOTROM
    	BHR_t*           	tmpBHR = (BHR_t*)load_addr;
	ExtBHR_t*   		extBHR = (ExtBHR_t*)(load_addr + BHR_HDR_SIZE);
	MV_U32			errCode=0;

	/* Verify Main header checksum */
	errCode = verify_main_header(tmpBHR, IBR_HDR_NAND_ID);
	if (errCode)
	{
		rv = 0;
		goto CleanUp;
	}

	/* Verify that the extended header is valid */
	errCode = verify_extheader(extBHR);
	if (errCode)
	{
		rv = 0;
		goto CleanUp;
	}
#endif
    /* The environment has Reed-Solomon ECC protection in the NAND */
    nandenvECC =  1;

#ifndef CP_BUBT_RESETS_ENV

	printf("\n**Warning**\n");
	printf("If U-Boot Endiannes is going to change (LE->BE or BE->LE), Then Env parameters should be overriden..\n");
	printf("Override Env parameters? (y/n)");
	readline(" ");
	if( strcmp(console_buffer,"Y") == 0 || 
	    strcmp(console_buffer,"yes") == 0 ||
	    strcmp(console_buffer,"y") == 0 ) {

		printf("Erase Env parameters sector %d... ",CFG_ENV_OFFSET);
		memset(&er_opts, 0, sizeof(er_opts));
		er_opts.offset = CFG_ENV_OFFSET;
		er_opts.length = CFG_ENV_SECT_SIZE;
		er_opts.quiet  = 1;

		nand_erase_opts(nand, &er_opts);
		//nand_erase(nand_dev_desc + 0, CFG_ENV_OFFSET, CFG_ENV_SECT_SIZE, 0);
		printf("\n");
	}
#else	/* CP_BUBT_RESETS_ENV */

	printf("\n## Resetting uboot environment variables ##\n\n");
	run_command("resetenv",0);

#endif	/* CP_BUBT_RESETS_ENV */

	printf("Erase %d - %d ... ",CFG_MONITOR_BASE, CFG_MONITOR_LEN);
	memset(&er_opts, 0, sizeof(er_opts));
	er_opts.offset = CFG_MONITOR_BASE;
	er_opts.length = CFG_MONITOR_LEN;
	er_opts.quiet  = 1;
	nand_erase_opts(nand, &er_opts);
	//nand_erase(nand_dev_desc + 0, CFG_MONITOR_BASE, CFG_MONITOR_LEN, 0);
	
	printf("\nCopy to Nand Flash... ");
	memset(&wr_opts, 0, sizeof(wr_opts));
	wr_opts.buffer	= (u_char*) load_addr;
	wr_opts.length	= CFG_MONITOR_LEN;
	wr_opts.offset	= CFG_MONITOR_BASE;
	/* opts.forcejffs2 = 1; */
	wr_opts.pad	= 1;
	wr_opts.blockalign = 1;
	wr_opts.quiet      = 1;
	ret = nand_write_opts(nand, &wr_opts);
	/* ret = nand_rw(nand_dev_desc + 0,
				  NANDRW_WRITE | NANDRW_JFFS2, CFG_MONITOR_BASE, CFG_MONITOR_LEN,
			      &total, (u_char*)0x100000 + CFG_MONITOR_IMAGE_OFFSET);
	*/
  	if (ret)
		printf("Error - NAND burn faild!\n");
	else
		printf("\ndone\n");	

    /* The environment has Reed-Solomon ECC protection in the NAND */
    nandenvECC =  0;

CleanUp:
	if (cp_is_network_enabled() && !net_was_enabled)
	{
		cp_disable_network_ports();
	}
    return rv;
}

U_BOOT_CMD(
        bubt,      2,     1,      nand_burn_uboot_cmd,
        "bubt	- Burn an image on the Boot Nand Flash.\n",
        " file-name \n"
        "\tBurn a binary image on the Boot Nand Flash, default file-name is u-boot.bin .\n"
);
static int write_preset_cfg_file(unsigned long img, unsigned long len)
{
#ifdef SEATTLE
	nand_erase_options_t opts;
	unsigned int size = CP_PART_PRESET_CFG_SIZE;
	nand_info_t *nand = &nand_info[0];
	printf("Writing preset configuration file\n");
	if (len > CP_PART_PRESET_CFG_SIZE)
	{
		printf("ERROR: Preset configuration file is too big (%d > %d)\n", len, CP_PART_PRESET_CFG_SIZE);
		return -1;
	}
	// erase NAND
	if (size % NAND_BLOCK_SIZE != 0) {
		printf("nand_write: WARNING: Increasing size to match block size\n");
		size += (NAND_BLOCK_SIZE - (size % NAND_BLOCK_SIZE));
	}
        memset(&opts, 0, sizeof(opts));
        opts.offset = CP_PART_PRESET_CFG_OFFSET;
        opts.length = size;
        opts.jffs2  = 0;
        opts.quiet  = 1;
	if (nand_erase_opts(nand, &opts) != 0) {
		printf("ERROR: Failed to erase %ld bytes from offset %ld\n", opts.length, opts.offset);
		return -1;
	}
	printf("Writing preset configuration file\n");

	if (write_to_nand(CP_PART_PRESET_CFG_OFFSET, len, (u_char *)img))
	{
		printf("ERROR: Write Preset configuration file failed\n");
		return -1;
	}

	// Check if write succeed
	if (read_from_nand(CP_PART_PRESET_CFG_OFFSET, len, (u_char *)img))		
	{
		printf("ERROR: Read Preset configuration file failed\n");
		return -1;
	}
#endif
	return 0;
}
/* Write nboot loader image into the nand flash */
int nand_burn_nboot_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int filesize;
	MV_U32 ret = 0;
	nand_info_t *nand = &nand_info[0];
	nand_erase_options_t er_opts;
	nand_write_options_t wr_opts;


	load_addr = CFG_LOAD_ADDR; 
	if(argc == 2) {
		copy_filename (BootFile, argv[1], sizeof(BootFile));
	}
	else { 
		copy_filename (BootFile, "nboot.bin", sizeof(BootFile));
		printf("using default file \"nboot.bin\" \n");
	}
 
	if ((filesize = NetLoop(TFTP)) < 0)
		return 0;
 
	printf("Erase %d - %d ... ",CFG_NBOOT_BASE, CFG_NBOOT_LEN);
	memset(&er_opts, 0, sizeof(er_opts));
	er_opts.offset = CFG_NBOOT_BASE;
	er_opts.length = CFG_NBOOT_LEN;
	er_opts.quiet  = 1;

	nand_erase_opts(nand, &er_opts);
	//nand_erase(nand_dev_desc + 0, CFG_NBOOT_BASE, CFG_NBOOT_LEN , 0);
	
	printf("\nCopy to Nand Flash... ");
	memset(&wr_opts, 0, sizeof(wr_opts));
	wr_opts.buffer	= (u_char*) load_addr;
	wr_opts.length	= CFG_NBOOT_LEN;
	wr_opts.offset	= CFG_NBOOT_BASE;
	/* opts.forcejffs2 = 1; */
	wr_opts.pad	= 1;
	wr_opts.blockalign = 1;
	wr_opts.quiet      = 1;
	ret = nand_write_opts(nand, &wr_opts);
	/* ret = nand_rw(nand_dev_desc + 0,
				  NANDRW_WRITE | NANDRW_JFFS2, CFG_NBOOT_BASE, CFG_NBOOT_LEN,
			      &total, (u_char*)0x100000);
	*/
  	if (ret)
		printf("Error - NAND burn faild!\n");
	else
		printf("\ndone\n");	

	return 1;
}

U_BOOT_CMD(
        nbubt,      2,     1,      nand_burn_nboot_cmd,
        "nbubt	- Burn a boot loader image on the Boot Nand Flash.\n",
        " file-name \n"
        "\tBurn a binary boot loader image on the Boot Nand Flash, default file-name is nboot.bin .\n"
);

#else
/* Boot from Nor flash */
int nor_burn_uboot_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int filesize;
	MV_U32 s_first,s_end,env_sec;
	extern char console_buffer[];


	s_first = flash_in_which_sec(&flash_info[BOOT_FLASH_INDEX], CFG_MONITOR_BASE);
	s_end = flash_in_which_sec(&flash_info[BOOT_FLASH_INDEX], CFG_MONITOR_BASE + CFG_MONITOR_LEN -1);

	env_sec = flash_in_which_sec(&flash_info[BOOT_FLASH_INDEX], CFG_ENV_ADDR);


	load_addr = CFG_LOAD_ADDR; 
	if(argc == 2) {
		copy_filename (BootFile, argv[1], sizeof(BootFile));
	}
	else { 
		copy_filename (BootFile, "u-boot.bin", sizeof(BootFile));
		printf("using default file \"u-boot.bin\" \n");
	}
 
	if ((filesize = NetLoop(TFTP)) < 0)
		return 0;
 
#ifdef MV_BOOTROM
    	BHR_t*           	tmpBHR = (BHR_t*)load_addr;
	ExtBHR_t*   		extBHR = (ExtBHR_t*)(load_addr + BHR_HDR_SIZE);
	MV_U32			errCode=0;

	/* Verify Main header checksum */
	errCode = verify_main_header(tmpBHR, IBR_HDR_SPI_ID);
	if (errCode) 
		return 0;

	/* Verify that the extended header is valid */
	errCode = verify_extheader(extBHR);
	if (errCode) 
		return 0;
#endif

	printf("Un-Protect Flash Monitor space\n");
	flash_protect (FLAG_PROTECT_CLEAR,
		       CFG_MONITOR_BASE,
		       CFG_MONITOR_BASE + CFG_MONITOR_LEN - 1,
		       &flash_info[BOOT_FLASH_INDEX]);

	printf("\n**Warning**\n");
	printf("If U-Boot Endiannes is going to change (LE->BE or BE->LE), Then Env parameters should be overriden..\n");
	printf("Override Env parameters? (y/n)");
	readline(" ");
	if( strcmp(console_buffer,"Y") == 0 || 
	    strcmp(console_buffer,"yes") == 0 ||
	    strcmp(console_buffer,"y") == 0 ) {

		flash_protect (FLAG_PROTECT_CLEAR,
				   flash_info[BOOT_FLASH_INDEX].start[env_sec],
				   flash_info[BOOT_FLASH_INDEX].start[env_sec] + CFG_ENV_SECT_SIZE - 1,
				   &flash_info[BOOT_FLASH_INDEX]);

		printf("Erase Env parameters sector %d... ",env_sec);
		flash_erase(&flash_info[BOOT_FLASH_INDEX], env_sec, env_sec);
	
		if ((mvCtrlModelGet() != MV_6082_DEV_ID) &&
		    (mvCtrlModelGet() != MV_6183_DEV_ID) &&
		    (mvCtrlModelGet() != MV_6183L_DEV_ID) &&
		    (mvCtrlModelGet() != MV_6281_DEV_ID) &&
		    (mvCtrlModelGet() != MV_6282_DEV_ID) &&
		    (mvCtrlModelGet() != MV_6280_DEV_ID) &&
		    (mvCtrlModelGet() != MV_6192_DEV_ID) &&
		    (mvCtrlModelGet() != MV_6190_DEV_ID) &&
		    (mvCtrlModelGet() != MV_6180_DEV_ID))
		flash_protect (FLAG_PROTECT_SET,
				   flash_info[BOOT_FLASH_INDEX].start[env_sec],
				   flash_info[BOOT_FLASH_INDEX].start[env_sec] + CFG_ENV_SECT_SIZE - 1,
				   &flash_info[BOOT_FLASH_INDEX]);

	}

	printf("Erase %d - %d sectors... ",s_first,s_end);
	flash_erase(&flash_info[BOOT_FLASH_INDEX], s_first, s_end);

	printf("Copy to Flash... ");

	flash_write ( (uchar *)(CFG_LOAD_ADDR + CFG_MONITOR_IMAGE_OFFSET),
				  (ulong)CFG_MONITOR_BASE,
				  (ulong)(filesize - CFG_MONITOR_IMAGE_OFFSET));

	printf("done\nProtect Flash Monitor space\n");
	flash_protect (FLAG_PROTECT_SET,
		       CFG_MONITOR_BASE,
		       CFG_MONITOR_BASE + CFG_MONITOR_LEN - 1,
		       &flash_info[BOOT_FLASH_INDEX]);

	return 1;
}

U_BOOT_CMD(
        bubt,      2,     1,      nor_burn_uboot_cmd,
        "bubt	- Burn an image on the Boot Flash.\n",
        " file-name \n"
        "\tBurn a binary image on the Boot Flash, default file-name is u-boot.bin .\n"
);
#endif /* defined(CFG_NAND_BOOT) */
#endif /* (CONFIG_COMMANDS & CFG_CMD_NET) */

/*******************************************************************************
burn a dsl image on the SPI Flash
********************************************************************************/
int write_to_nand(u_int32_t offset, unsigned int len, u_char *image) {
	nand_erase_options_t opts;
	nand_write_options_t wr_opts;
	unsigned int size = len;
	nand_info_t *nand = &nand_info[0];

	if (size % NAND_BLOCK_SIZE != 0) {
		printf("nand_write: WARNING: Increasing size to match block size\n");
		size += (NAND_BLOCK_SIZE - (size % NAND_BLOCK_SIZE));
	}

        memset(&opts, 0, sizeof(opts));
        opts.offset = offset;
        opts.length = size;
        opts.jffs2  = 0;
        opts.quiet  = 1;
	if (nand_erase_opts(nand, &opts) != 0) {
		printf("ERROR: Failed to erase %ld bytes from offset %ld\n", opts.length, opts.offset);
		return -1;
	}
	printf("\nCopy to Nand Flash... ");
	memset(&wr_opts, 0, sizeof(wr_opts));
	wr_opts.buffer	= (u_char*) image;
	wr_opts.length	= len;
	wr_opts.offset	= offset;
	/* opts.forcejffs2 = 1; */
	wr_opts.pad	= 1;
	wr_opts.blockalign = 1;
	wr_opts.quiet      = 1;
	if (nand_write_opts(nand, &wr_opts)){
		printf("ERROR: failed to write %ld bytes to offset %ld\n", opts.length, opts.offset); 
		return -1;
	}

	return 0;
}

extern flash_info_t flash_info[];       /* info for FLASH chips */
#if (CONFIG_COMMANDS & CFG_CMD_NET)

/* from sofaware.h */

#define ANNEX_A_ID    0x10000000
#define ANNEX_B_ID    0x20000000
#define ANNEX_C_ID    0x40000000
/* end of section from sofaware.h */

/* from sofaware_common.h */
#define BIN_DSL_MODEM_MAGIC     0x1234abcd
#define DSL_MODEM_VESION_LEN    65
#define BIN_MAGIC 0x550708AA
#define ACT_KERNEL_0	2 /* force writing to kernel 1 */
#define ACT_KERNEL_1	3 /* force writing to kernel 2 */





typedef struct {
	unsigned int xsum;							/* check sum include header */
	unsigned int magic;							/* 0x1234ABCD */
	unsigned int size;							/* bin image length */
	unsigned int annex;							/* annex */
	unsigned char version[DSL_MODEM_VESION_LEN];/* version */
	unsigned char __reserved[19];   			/* reserved */
} bin_dsl_modem_hdr_t;

/* end of section from sofaware_common.h */

char * sw_var_get_blob_adsl_annex(void)
{	
	return getenv("dsl_annex");
}

/* from sofaware-app-init.h */
typedef u_int32_t nand_img_offset_t;
/* end of section from sofaware-app-init.h */


 int read_from_nand(nand_img_offset_t offset, unsigned int len, u_char *image) {
	unsigned int size = len;
	nand_info_t *nand = &nand_info[0];
	printf("Trying to read nand: %ld bytes from offset %ld\n", len, offset);
	if (nand_read_skip_bad(nand, offset, &size, image,1) != 0) {
    	printf("ERROR: failed to read %ld bytes from offset %ld\n", len, offset);
        return -1;
    }
	printf("Read nand: %ld bytes from nand\n", size);
    return 0;
}

static int get_annex_from_file_name(unsigned int *annex, const char *filename)
{
	if ((strcmp("dsl0-b", filename)==0) || (strcmp("dsl1-b", filename)==0))
		*annex = ANNEX_B_ID;
	else if ( (strcmp("dsl0", filename)==0) || (strcmp("dsl1", filename)==0))
		*annex = ANNEX_A_ID;
	else
		return -1;

	return 0;
}

uint32_t sofaware_dsl_annex_2_int(const char* dsl_annex)
{
	int annex = 0;

	if (strcmp (dsl_annex, ANNEX_A) == 0)
		annex = ANNEX_A_ID;
	else if (strcmp (dsl_annex, ANNEX_B) == 0)
		annex = ANNEX_B_ID;
	else if (strcmp (dsl_annex, ANNEX_C) == 0)
		annex = ANNEX_C_ID;
	return annex;
}


int is_primary_dsl_ok(unsigned long heaptop)
{
#ifdef SEATTLE
	bin_dsl_modem_hdr_t *dsl_hdr;
	dsl_hdr = (bin_dsl_modem_hdr_t *)heaptop;

	printf("Read primary dsl header from flash\n");
	if (read_from_nand(CP_PART_DSL_PRI_HDR_OFFSET, sizeof(bin_dsl_modem_hdr_t), (u_char *)dsl_hdr))
	{
		printk("ERROR: Can not read dsl-header primary partition. Operation failed.\n");
		return -1;
	}

	if (dsl_hdr->magic == BIN_DSL_MODEM_MAGIC)
	{
		printf("dsl main firmware is OK\n");
		return 1;
	}

	printf("No dsl main firmware\n");
#endif
	return 0;
}
#if 0
unsigned int get_secondary_dsl_firm_annex(void)
{
#ifdef SEATTLE
	bin_dsl_modem_hdr_t dsl_hdr;

	printf("Get secondary dsl firm Annex\n");
	if (read_from_nand(CP_PART_DSL_SEC_OFFSET, sizeof(dsl_hdr), (u_char *)&dsl_hdr))
	{
		printk("ERROR: Failed to Read secondary dsl-frimware's header.\n");
		return 0;
	}

	if (SWAP_LONG(dsl_hdr.magic) != BIN_DSL_MODEM_MAGIC)
	{
		printf("ERROR: No dsl backup firmware found. Operation aborted.\n");
		return 0;
	}

	if ((SWAP_LONG(dsl_hdr.annex)!=ANNEX_A_ID) && (SWAP_LONG(dsl_hdr.annex)!=ANNEX_B_ID) && (SWAP_LONG(dsl_hdr.annex)!=ANNEX_C_ID))
	{
		printf("ERROR: Secondary-dsl-firmware's Annex is corrupted (%x)\n", dsl_hdr.annex);
		return 0;
	}
	return dsl_hdr.annex;
#endif
	return 0;
}

int restore_primary_dsl_firmware(unsigned long heaptop)
{
#ifdef SEATTLE
	bin_dsl_modem_hdr_t *dsl_hdr;

	printf("Read dsl backup firmware\n");
	if (read_from_nand(CP_PART_DSL_SEC_OFFSET, CP_PART_DSL_SEC_SIZE, (u_char *)heaptop))
	{
		printk("ERROR: Failed to Read secondary dsl-frimware.\n");
		return -1;
	}

	dsl_hdr = (bin_dsl_modem_hdr_t *)heaptop;
	if (dsl_hdr->magic != BIN_DSL_MODEM_MAGIC)
	{
		printf("No dsl backup firmware found. Operation aborted.\n");
		return -1;
	}

	printf("Check dsl backup firmware\n");
	if (dsl_hdr->xsum != sofaware_calc_crc((const char *)heaptop + 4, dsl_hdr->size + sizeof(bin_dsl_modem_hdr_t) - 4,0))
	// XSum should be calculated on all image beside the first 4 bytes (containing the xsum itself))
	{
		printf("Wrong CRC in dsl backup firmware. Operation aborted.\n");
		return -1;
	}
	// Write backup firmware to SPI
	return upgrade_dsl(heaptop, dsl_hdr->size + sizeof(bin_dsl_modem_hdr_t) ,1);
#endif
	return 0;
}
#endif
static void display_dsl_hdr(bin_dsl_modem_hdr_t *dsl_hdr)
{
	char tmp[DSL_MODEM_VESION_LEN + 1];

	printf("Dsl Header:\n");
	printf("xsum: 0x%x\n", SWAP_LONG(dsl_hdr->xsum));
	printf("magic: 0x%x\n", SWAP_LONG(dsl_hdr->magic));
	printf("size: 0x%x (%d)\n", SWAP_LONG(dsl_hdr->size), SWAP_LONG(dsl_hdr->size));
	printf("annex: 0x%x [blob's annex: 0x%x]\n", SWAP_LONG(dsl_hdr->annex), sofaware_dsl_annex_2_int(sw_var_get_blob_adsl_annex()));

	memcpy(tmp, dsl_hdr->version, DSL_MODEM_VESION_LEN);
	tmp[DSL_MODEM_VESION_LEN] = '\0';
	printf("version: %s\n", tmp);
}

int check_preset_header(unsigned long img)
{
#ifdef SEATTLE
	0;//strncmp(PRESET_HEADER,(char *)img,strlen(PRESET_HEADER)-1);
#endif
}
int check_blob_header(unsigned long img)
{
#ifdef SEATTLE
	return strncmp(BLOB_HEADER,(char *)img,strlen(BLOB_HEADER)-1);
#endif
}
int check_dsl_hdr(unsigned long img, unsigned long img_size, unsigned int annex, int is_primary)
{
	int hdr_size;
	bin_dsl_modem_hdr_t dsl_hdr;
	unsigned int dsl_firm_annex;

	// Validate img's size
	if (img_size < sizeof(bin_dsl_modem_hdr_t))
	{
		printf("Error: dsl firmware is too small - %d Bytes\n", img_size);
		return -1;
	}

	dsl_hdr = *(bin_dsl_modem_hdr_t *)img;
	display_dsl_hdr(&dsl_hdr);
	// Validate img's magic
	if (SWAP_LONG(dsl_hdr.magic) != BIN_DSL_MODEM_MAGIC)
	{
		printf("Error: firmware magic 0x%x is wrong\n", dsl_hdr.magic);
		return -1;
	}

	// Validate img's data size
	hdr_size = sizeof(bin_dsl_modem_hdr_t);
	if (img_size - hdr_size != SWAP_LONG(dsl_hdr.size))
	{
		printf("Error: Mismatch with img sizes. Image size=%d, Header part=%d, Data part=%d.\n"
			"Data part + Header part = %d, differ from Image size (%d).\n",
			img_size, hdr_size, dsl_hdr.size, dsl_hdr.size+hdr_size, img_size);
		return -1;
	}

	// Annex ABC - Check that header annex matches to the annex fatched from the file name
	if (annex != SWAP_LONG(dsl_hdr.annex))
	{
		printf("dsl-firmware header's annex: 0x%x, does not matches to the file name's annex: 0x%x.\n",
			dsl_hdr.annex, annex);
		return -1;
	}
#if 0
	dsl_firm_annex = get_secondary_dsl_firm_annex();
	if (is_primary && (dsl_firm_annex != 0) && (dsl_firm_annex != annex))
	{
		printf("New primary-dsl-firmware header's annex (%x) differ from\n", annex);
		printf("the installed secondary-dsl-firmware annex (%x)\n", dsl_firm_annex);
		return -1;
	}
#endif

	return 0;
}


static int validate_dsl_img_crc(unsigned long img, unsigned long img_size, unsigned int img_xsum)
{
	unsigned int calc_xsum;

	calc_xsum = sofaware_calc_crc((const char *)img + 4, img_size - 4,0); // XSum should be calculated on all image beside the
			// first 4 bytes (containing the xsum itself)

	// Validate calc_crc with header_crc
	if (calc_xsum != img_xsum)
	{
		printf("Error: Image is corrupted. Calculated CRC 0x%x, while image's header defines 0x%x\n", calc_xsum,
			img_xsum);
		return -1;
	}

	printf("DSL image CRC: 0x%x is valid\n", calc_xsum);
	return 0;
}

static int write_dsl_main_firm_hdr(bin_dsl_modem_hdr_t *dsl_hdr, unsigned int *calc_xsum)
{
#ifdef SEATTLE
	int len = sizeof(bin_dsl_modem_hdr_t);
	bin_dsl_modem_hdr_t wrote_hdr;
	
	printf("Writing primary dsl header to flash... \n");
	
	// erase is done inside write
	if (write_to_nand(CP_PART_DSL_PRI_HDR_OFFSET, len, (u_char *) dsl_hdr))
	{
		printk("ERROR: Write primary dsl header failed \n");
		return -1;
	}

	// Check if write succeed
	if (read_from_nand(CP_PART_DSL_PRI_HDR_OFFSET, len, (u_char *)&wrote_hdr))
	{
		printk("ERROR: Read primary dsl header failed \n");
		return -1;
	}

	if (memcmp(&wrote_hdr, dsl_hdr, len)!=0)
	{
		printk("ERROR: Input buffer differ from the buffer wrote to flash\n");
		return -1;
	}

	// XSum should be calculated on all image beside the first 4 bytes (containing the xsum itself)
	*calc_xsum = sofaware_calc_crc((const char *)dsl_hdr + 4, len - 4,0);
#endif
	return 0;
}
#if 0
static int write_dsl_backup_firm(unsigned long img, unsigned long len, unsigned int img_xsum)
{
	int ret;
#ifdef SEATTLE	
	printf("Writing backup DSL firmware to flash...\n");
	if (len > CP_PART_DSL_SEC_SIZE)
	{
		printk("ERROR: Backup dsl-firmware is too big (%d > %d)\n", len, CP_PART_DSL_SEC_SIZE);
		return -1;
	}
	// erase is done inside write
	if (write_to_nand(CP_PART_DSL_SEC_OFFSET, len, (u_char *)img))
	{
		printk("ERROR: Write secondary dsl-firmware failed\n");
		return -1;
	}

	// Check if write succeed
	if (read_from_nand(CP_PART_DSL_SEC_OFFSET, len, (u_char *)img))
	{
		printk("ERROR: Read secondary dsl-frimware failed\n");
		return -1;
	}

	ret = validate_dsl_img_crc(img, len, img_xsum);
	printf("Validate secondary dsl image on flash..\n %d",ret);
#endif
	return ret;
}
#endif
static unsigned int calculate_spi_crc(unsigned long len, unsigned int *calc_crc)
{
	int i, j; 
	unsigned char page_blk[SPI_EEPROM_PAGE_SIZE], *p;
	unsigned int blk_len;
	int num_pages, last_page;

	num_pages = (len - 1) / SPI_EEPROM_PAGE_SIZE + 1;
	last_page = num_pages - 1;

	printf("File length=%d. Going to read %d pages from SPI.\n", len, num_pages);
	if (num_pages > SPI_EEPROM_MAX_PAGES)
	{
		printf("Error: Too much pages to read - %d. Max pages is %d.\n", num_pages, SPI_EEPROM_MAX_PAGES);
		return -1;
	}
	if (spi_read(0, page_blk) != 0)
	{
		printf("Error: spi_read failed\n");
		return -1;
	}

	for (i=0; i<num_pages; i++)
	{
		// Get page from SPI
		if (spi_read(i, page_blk) != 0)
		{
			printf("Error: spi_read failed\n");
			return -1;
		}

		if( (i & 0x2f ) == 0 ) // print . for progress each 32 pages
		{
			printf(".");
		}

		// Calculate CRC
		blk_len = (i == last_page) ? len - i*SPI_EEPROM_PAGE_SIZE : SPI_EEPROM_PAGE_SIZE;
		p = page_blk;

		for (j=0; j<blk_len; j++) {
			SWcrc_char(*p, calc_crc);
			p++;
		}

	}
	printf("\n");

	return 0;
}

static int write_dsl_main_firm(unsigned long dsl_addr, unsigned long img_size, unsigned int i_calc_crc,
	unsigned int firm_crc)
{
	 unsigned int calc_crc = i_calc_crc;

	printf("Erasing SPI\n");
	
	//Dafna - leds??????

	// Write firmware to modem
	printf("Writing from flash 0x%x, size %d to SPI\n", dsl_addr, img_size);
	if (spi_write_file(dsl_addr, img_size) != 0)
	{
		printf("Error: Write operation failed\n");
		return -1;
	}

	printf("Read firmware from modem and calculate CRC\n");

	// Read firmware from modem and calculate CRC
	if (calculate_spi_crc(img_size, &calc_crc) != 0)
	{
		printf("Error: Read operation failed\n");
		return -1;
	}

    if (calc_crc != firm_crc)
	{
		printf("Error: spi crc (0x%x) differ from input buffer crc (0x%x) - EEProm Write/Read operation failed\n",
			calc_crc, firm_crc);
		return -1;
	}

	printf("dsl main firmware wrote successfully to modem [spi crc 0x%x]\n", calc_crc);
	return 0;
}

int upgrade_dsl(unsigned long img, unsigned long img_size, int is_backup)
{
	int ret, hdr_size;
	bin_dsl_modem_hdr_t *dsl_hdr;
	unsigned int calc_xsum;

	dsl_hdr = (bin_dsl_modem_hdr_t *)img;
	display_dsl_hdr(dsl_hdr);

	// Calculate buffer's CRC
	printf("Validate input buffer crc\n");
	ret = validate_dsl_img_crc(img, img_size, SWAP_LONG(dsl_hdr->xsum));
	if (ret != 0)
		return ret;

	// Turn adsl modem off
	//limor - Add clear of sensor 1 here

	do_spi_init();

	// Write dsl-firmware header to flash
	ret = write_dsl_main_firm_hdr(dsl_hdr, &calc_xsum);
	if (ret != 0)
		return ret;

	// Write dsl-firmware to dsl modem via SPI
	hdr_size = sizeof(bin_dsl_modem_hdr_t);
        reset_dsl_modem();
        extend_gpio_ctl(IC_SG80_EXTEND1,GPIO_DSL_SW_RST,0);
	ret = write_dsl_main_firm(img + hdr_size,  img_size - hdr_size, calc_xsum, SWAP_LONG(dsl_hdr->xsum));
	if (ret != 0)
		return ret;

	// restart adsl mode
	reset_dsl_modem();
#if 0	
	ret = write_dsl_backup_firm(img, img_size, SWAP_LONG(dsl_hdr->xsum));
#endif
	if (ret != 0)
		return ret;
	return 0;
}

void reset_dsl_modem(void)
{
	printk ("dsl_modem_reset\n");
	extend_gpio_ctl(IC_SG80_EXTEND1,GPIO_DSL_POWER,0);
	extend_gpio_ctl(IC_SG80_EXTEND1,GPIO_DSL_SW_RST,0);
	udelay (500);
	extend_gpio_ctl(IC_SG80_EXTEND1,GPIO_DSL_POWER,1);
	udelay (500);
	extend_gpio_ctl(IC_SG80_EXTEND1,GPIO_DSL_SW_RST,1);
	return;
}

int sofaware_check_dsl_image(unsigned long image)
{
        int check_len;
        unsigned int crc=0;
        bin_dsl_modem_hdr_t *hdr;

        hdr = (bin_dsl_modem_hdr_t *)image;

        if (offsetof(bin_dsl_modem_hdr_t, xsum)) {
                printf("ERROR: crc32 is not the first member of bin_dsl_modem_hdr_t");
        }
        check_len = SWAP_LONG(hdr->size)+(sizeof(bin_dsl_modem_hdr_t)-sizeof(hdr->xsum));

        crc = sofaware_calc_crc(image+sizeof(hdr->xsum) , check_len, crc);

        if (crc != SWAP_LONG(hdr->xsum)) {
                printf("ERROR: bad image xsum 0x%x (expected 0x%x).\n",
                                crc, hdr->xsum);
                return -1;
        }

        return 0;
}

/* Write dsl image into the SPI flash */
int nand_burn_dsl_cmd(cmd_tbl_t *cmdtp, unsigned int size, int argc, char *argv[])
{
	if(argc != 2){ 
		printf("command syntax: bdsl imageName\n");
		return 0;
	}
	return burn_dsl_firmware(argv[1]);
}

int burn_dsl_firmware(char* dslFileName)
{
	MV_U32 rv = 0;
	char * filename;
	int netEnabled=1;
	int size;
	unsigned int annex = sofaware_dsl_annex_2_int(sw_var_get_blob_adsl_annex());
	load_addr = CFG_LOAD_ADDR; 
	if (annex==0)
	{
		printf("ADSL is not supported\n");
		return 0;
	}
	copy_filename (BootFile, dslFileName, sizeof(BootFile));
		
	if (!cp_is_network_enabled())
	{
		netEnabled=0;
		cp_enable_network_ports();
	}

	if ((size = NetLoop(TFTP)) <= 0)
	{
		printf("Failed to load DSL image from FTP\n");
		if (!netEnabled)
			cp_disable_network_ports();
		return 0;
	}
	if (!netEnabled)
		cp_disable_network_ports();
	printf(" file size=%d \n",size);

	/* When adding support to .firm file use the following check
          filename = BootFile;
	  if ((strncmp(filename, "dsl0",4) == 0) || (strncmp(filename, "dsl1",4) == 0)){
		if (get_annex_from_file_name(&annex, filename)!=0)
		{
			printf("Error: Can not get annex from filename ('%s')\n", filename);
		}
		if (annex != sofaware_dsl_annex_2_int(sw_var_get_blob_adsl_annex()))
		{
			printf("DSL firmware's Annex (%x) differ from mine (%x). Skip.\n",
					annex, sofaware_dsl_annex_2_int(sw_var_get_blob_adsl_annex()));
		}*/
	//Annex should be taken from BLOB

	if(check_dsl_hdr(load_addr, size,sofaware_dsl_annex_2_int(sw_var_get_blob_adsl_annex()), 0))
	{
	     printf("Error: bad dsl firmware - wrong header!\n");
		 return 0;
	}
	if (sofaware_check_dsl_image(load_addr)==0)
	{
		if (upgrade_dsl(load_addr, size, 0) != 0)
		{
			printf("Error: Upgrade dsl failed\n");
		}
		else {
			rv = 1;
			printf("\ndone\n"); 
		}
	}
	else {
		printf("Error: Wrong file name\n");	
	}	

    return rv;
}

U_BOOT_CMD(
        bdsl,      2,     1,      nand_burn_dsl_cmd,
        "bdsl	- Burn a dsl image on the SPI Flash.\n",
        " file-name \n"
        "\tBurn a dsl binary image on the SPI Flash, default file-name is ???.??? .\n"
);

#endif /* (CONFIG_COMMANDS & CFG_CMD_NET) */
int global_sofaware_script_err = -1;
int parse_line (char *line, char *argv[])
{
	int nargs = 0;

#ifdef DEBUG_PARSER
	printf ("parse_line: \"%s\"\n", line);
#endif
	while (nargs < CFG_MAXARGS) {

		/* skip any white space */
		while ((*line == ' ') || (*line == '\t')) {
			++line;
		}

		if (*line == '\0') {	/* end of line, no more args	*/
			argv[nargs] = NULL;
#ifdef DEBUG_PARSER
		printf ("parse_line: nargs=%d\n", nargs);
#endif
			return (nargs);
		}

		argv[nargs++] = line;	/* begin of argument string	*/

		/* find end of string */
		while (*line && (*line != ' ') && (*line != '\t')) {
			++line;
		}

		if (*line == '\0') {	/* end of line, no more args	*/
			argv[nargs] = NULL;
#ifdef DEBUG_PARSER
		printf ("parse_line: nargs=%d\n", nargs);
#endif
			return (nargs);
		}

		*line++ = '\0';		/* terminate current arg	 */
	}

	printf ("** Too many args (max. %d) **\n", CFG_MAXARGS);

#ifdef DEBUG_PARSER
	printf ("parse_line: nargs=%d\n", nargs);
#endif
	return (nargs);
}

int burn_preset_cfg(char* cfgFileName)
{
#ifdef SEATTLE
	MV_U32 rv = 0;
	char * filename;
	int netEnabled=1;
	int size;
	load_addr = CFG_LOAD_ADDR; 
	
	copy_filename (BootFile, cfgFileName, sizeof(BootFile));
		
	if (!cp_is_network_enabled())
	{
		netEnabled=0;
		cp_enable_network_ports();
	}

	if ((size = NetLoop(TFTP)) <= 0)
	{
		printf("TFTP Error\n");
		if (!netEnabled)
			cp_disable_network_ports();
		return -1;
	}
	if (!netEnabled)
		cp_disable_network_ports();
	
	if(check_preset_header(load_addr) != 0)
	{
	    printf("Error: bad Configuration file !\n");
		return -1;
	}
	if (write_preset_cfg_file(load_addr,size) != 0)
	{
		printf("Error: Loading preset file\n");
		return -1;
	}
	setenv("preset_size",size);
#endif
    return 0;
}

int run_command_simple (const char *cmd, int flag) {
	char *argv[CFG_MAXARGS + 1];	/* NULL terminated	*/
	int argc;
	cmd_tbl_t *cmdtp;
	argc = parse_line (cmd, argv);

	if ((cmdtp = find_cmd(argv[0])) == NULL) {
		printf ("Unknown command '%s' - try 'help'\n", argv[0]);
		return -1;
	}

	/* found - check max args */
	if (argc > cmdtp->maxargs) {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return -1;
        }

	if ((cmdtp->cmd) (cmdtp, flag, argc, argv) != 0) {
		return -1;		
	}

	return 0;
}
void blobErrorIdication()
{
	cp_GPIO_LED_set(ALERT_LED_RED,	LED_CMD_ON);
	cp_GPIO_LED_set(ALERT_LED_RED,	LED_CMD_STATIC);
}
#define CFG_CBSIZE 200
int cp_run_script (char *script_buf, int script_len) 
{
	char cmd[CFG_CBSIZE+1];
	char *ptr = NULL;
	int cmd_len,ret;
	int line=1;

	global_sofaware_script_err = 0;
	if (CP_IS_BLOB_EXIST == 0)
		cp_disable_read_only();

	while (script_len > 0) 
	{
		/* skip empty lines and spaces before the command */
		while ((script_len > 0) && ((*script_buf)== 0x20)) {
			if (*script_buf == '\n')
				line++;
			script_buf++;
			script_len--;
		}

		if (script_len==0)
			break;
		//printf("script_buf=%s\n",script_buf);
		/* find the end of line and calc the command length */
		if (((ptr = memchr(script_buf, '\r', script_len)) != NULL) || ((ptr = memchr(script_buf, '\n', script_len)) != NULL) ) 
			cmd_len = ptr-script_buf;
		else 
			cmd_len = script_len;

		if (cmd_len > CFG_CBSIZE ) {
			printf ("ERROR: command at line %d is too long, skip it.", line);
			goto end_line;
		}

		memcpy (cmd, script_buf, cmd_len);
		cmd[cmd_len] = '\0';

		if (*cmd == '#')
			goto end_line;

	//	printf ("Run line %d, command \"%s\ cmd_len=%d \n", line, cmd, cmd_len);


		if (!strncmp(cmd, "setenv", 6) ||  !strncmp(cmd, "reset",5)) {
			run_command_simple (cmd, 0);
		} else if (!strncmp(cmd, "cp_blob_update",14) ){
			printf("update nand...\n");
			ret = cp_blob_update();
			if (ret != 0)
			{			
				blobErrorIdication();				
			}
			else if ( get_read_only_error() == 0)//success
			{
				cp_GPIO_LED_set(ALERT_LED_GREEN,LED_CMD_ON);				
				cp_GPIO_LED_set(ALERT_LED_GREEN,LED_CMD_BLINK);				
				
			}			
			else //Trying to rewrite read only parameters
			{			
				cp_GPIO_LED_set(ALERT_LED_RED,LED_CMD_ON);
				cp_GPIO_LED_set(ALERT_LED_GREEN,LED_CMD_ON);
				cp_GPIO_LED_set(ALERT_LED_RED,LED_CMD_STATIC);				
				cp_GPIO_LED_set(ALERT_LED_GREEN,LED_CMD_STATIC);				
			}
			
			
		}else{
			printf("Command \"%s\" unauthorized\n", cmd);
		}

end_line:
		script_buf+=cmd_len+1;
		script_len-=cmd_len+1;

		if (*ptr == '\r' && *script_buf == '\n') {
			script_buf++;
			script_len--;
		}

		line++;
	}
	cp_enable_read_only();
	return global_sofaware_script_err;
}
#define CP_BLOB_OFFSET 0xe0000
#define CP_BOARD_SETTINGS_NAND_SIZE	0x20000


unsigned int cp_hxtoui(const char* str)
{
	unsigned int num = 0;

	if (strncmp(str, "0x", 2)==0) 
		str+=2;

	while (*(str)){
		if (*str >= '0' && *str <= '9') 
			num = num * 16 + (*str - '0');
		else if (*str >= 'a' && *str <= 'f')
			num = num * 16 + (*str - 'a' + 10);
		else if (*str >= 'A' && *str <= 'F')
			num = num * 16 + (*str - 'A' + 10);
		else
			break;

		str++;
	}
	return num;
}
int cp_strmac_to_binmac(const char* strmac,  unsigned char *binmac)
{
	int i;

	for (i = 0 ; i<6 ; i++)
		binmac[i] = cp_hxtoui(strmac+(i*3));

	return 0;
}
int cp_macstr_cmp(const char* strmac1, const char* strmac2)
{
	unsigned char mac1[6], mac2[6];

	cp_strmac_to_binmac(strmac1, mac1);
	cp_strmac_to_binmac(strmac2, mac2);

	return memcmp (mac1, mac2, 6);
}

static void cp_set_wireless_region(char* region_var_name, const u_char region) {
	char region_str[4];
	sprintf(region_str, "%u%c", region, '\0');
	setenv(region_var_name, region_str);
}
	
int cp_blob_update(void)
{
	cp_blob_t new_blob; 
	char tmp_blob[CP_BOARD_SETTINGS_NAND_SIZE];
	//printf("enter cp_blob_update curr_blob=%s\n",curr_blob );
	
	memset(tmp_blob, 0, sizeof(tmp_blob));
	printf("Blob cp_blob_update %x\n",new_blob.magic);
/*	if (CP_IS_BLOB_EXIST) {
		//copy exist BLOB 
		printf("Blob exists\n");
		memcpy(&new_blob, &curr_blob, sizeof(cp_blob_t));
	}
	else*/
		memset(&new_blob, 0, sizeof(cp_blob_t));	
	mvMacStrToHex(getenv("hw_mac_addr"),new_blob.wan_mac);
	mvMacStrToHex(getenv("lan1_mac_addr"),new_blob.lan_mac_1);
	mvMacStrToHex(getenv("lan2_mac_addr"),new_blob.lan_mac_2);
	mvMacStrToHex(getenv("lan3_mac_addr"),new_blob.lan_mac_3);
	mvMacStrToHex(getenv("lan4_mac_addr"),new_blob.lan_mac_4);
	mvMacStrToHex(getenv("lan5_mac_addr"),new_blob.lan_mac_5);
	mvMacStrToHex(getenv("lan6_mac_addr"),new_blob.lan_mac_6);
	mvMacStrToHex(getenv("lan7_mac_addr"),new_blob.lan_mac_7);
	mvMacStrToHex(getenv("lan8_mac_addr"),new_blob.lan_mac_8);
	mvMacStrToHex(getenv("dmz_mac_addr"),new_blob.dmz_mac);
	strncpy (new_blob.module, getenv("unitModel"), 0x10);	//0xb0
	strncpy (new_blob.unitVer, getenv("unitVer"), 0x10);//0xc0
	strncpy (new_blob.unitName, getenv("unitName"), 0x40);//0xd0	
	mvMacStrToHex(getenv("dsl_mac_addr"),new_blob.dsl_mac);
	strncpy (new_blob.region, getenv("wireless_region"), 0x10);//0x120
	new_blob.region[0] = new_blob.region[0] -'0';

	if (getenv("dsl_annex"))
		strncpy (new_blob.dslAnnex, getenv("dsl_annex"), 0x10); //0x130
	else
		memset(new_blob.dslAnnex, 0, 0x10);
	if (getenv("marketing_capabilities"))
		strncpy (new_blob.marketingCapabilities, getenv("marketing_capabilities"), 0x10);//0x140
	if (getenv("hardware_capabilities"))
		strncpy (new_blob.hwCapabilities, getenv("hardware_capabilities"), 0x20);
	if (getenv("mgmt_opq"))
		strncpy (new_blob.managment, getenv("mgmt_opq"), 0x80);
	if (getenv("mgmt_signature"))
		strncpy (new_blob.mgmtSignature, getenv("mgmt_signature"), 0x200);
	if (getenv("mgmt_signature_ver"))
		strncpy (new_blob.mgmtSignatureVer, getenv("mgmt_signature_ver"), 0x80);
	new_blob.magic = CP_BLOB_MAGIC;
	memcpy(&tmp_blob,&new_blob,sizeof(new_blob));		
	new_blob.crc1 = crc32(0, &tmp_blob[0x10],sizeof(tmp_blob)-0x10);
	memcpy(&tmp_blob,&new_blob,sizeof(new_blob));
		
	return cp_write_blob (&tmp_blob, sizeof(tmp_blob));
}
U_BOOT_CMD(
	cp_blob_update,  1,   1,  cp_blob_update,
	"cp_blob_update	- set blob params from uboot enviroment.\n",
	" \n"
	"\ tset blob params from uboot enviroment.\n"
);
int cp_write_blob (void *blob, int size)
{

	ulong addr = 0xe0000;
	
	printf ("Update BLOB at address 0x%08x\n", addr);
	return cp_write_nand((char *)blob, addr, size);
}

int cp_write_nand(char *src, ulong addr, ulong cnt)
{
	int ret;
	nand_erase_options_t er_opts;
	nand_write_options_t wr_opts;
	nand_info_t *nand = &nand_info[0];
	printf("Erase %d - %d ... ",addr, cnt);
	memset(&er_opts, 0, sizeof(er_opts));
	er_opts.offset = addr;
	er_opts.length = cnt;
	er_opts.quiet  = 1;
	nand_erase_opts(nand, &er_opts);
	

	printf("\nCopy to Nand Flash...");
	memset(&wr_opts, 0, sizeof(wr_opts));
	wr_opts.buffer	= src;
	wr_opts.length	= cnt;
	wr_opts.offset	= addr;
	/* opts.forcejffs2 = 1; */
	wr_opts.pad	= 1;
	wr_opts.blockalign = 1;
	wr_opts.quiet      = 1;
	ret = nand_write_opts(nand, &wr_opts);
	return ret;
}
int burn_blob_cfg(char* blobFileName)
{
#ifdef SEATTLE
	MV_U32 rv = 0;
	char * filename;
	int netEnabled=1;
	int size;
	load_addr = 0x02000000; 

	//Read current config	
	if (read_from_nand(CP_BLOB_OFFSET, CP_BOARD_SETTINGS_NAND_SIZE, (u_char *)&curr_blob))		
	{
		blobErrorIdication();
		printf("ERROR: Reading Blob from NAND\n");
		return -1;
	}
	printf("%d magic=",curr_blob.blob_param.magic);
	curr_blob.blob_param.dslAnnex[sizeof(curr_blob.blob_param.dslAnnex)-1]='\0';
	printf("%s annex=",curr_blob.blob_param.dslAnnex);
	curr_blob.blob_param.unitName[sizeof(curr_blob.blob_param.unitName)-1]='\0';
	printf("%s unitName=",curr_blob.blob_param.unitName);
	curr_blob.blob_param.module[sizeof(curr_blob.blob_param.module)-1]='\0';
	printf("%s module=",curr_blob.blob_param.module);
	curr_blob.blob_param.unitVer[sizeof(curr_blob.blob_param.unitVer)-1]='\0';
	printf("%s unitVer=",curr_blob.blob_param.unitVer);
	
	copy_filename (BootFile, blobFileName, sizeof(BootFile));
		
	if (!cp_is_network_enabled())
	{
		netEnabled=0;
		cp_enable_network_ports();
	}

	if ((size = NetLoop(TFTP)) <= 0)
	{
		printf("Error: TFTP Error !\n");
		blobErrorIdication();		
		if (!netEnabled)
			cp_disable_network_ports();
		return -1;
	}
	if (!netEnabled)
		cp_disable_network_ports();
	
	if(check_blob_header(load_addr) != 0)
	{
		printf("Error: bad Blob header !\n");
		blobErrorIdication();
		return -1;
	}
	
	if (cp_run_script ((char*)load_addr, size)!=0)
	{
		printf("Error Loading blob file\n");
		return -1;
	}
#endif
    return 0;
}
/*******************************************************************************
Reset environment variables.
********************************************************************************/
extern flash_info_t flash_info[];       /* info for FLASH chips */
int resetenv_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
#if defined(CFG_NAND_BOOT)
	printf("Erase Env parameters offset 0x%x... ",CFG_ENV_OFFSET);
	nand_info_t *nand = &nand_info[0];
	nand_erase_options_t er_opts;
    int i = 0, goodBlockCounter = 0;

    while(i * nand_info[0].erasesize < nand_info[0].size)
    {
        if(nand_info[0].block_isbad(&nand_info[0], i * nand_info[0].erasesize) == 0)
            goodBlockCounter++;
        if((goodBlockCounter * nand_info[0].erasesize) >= CFG_ENV_OFFSET + CFG_ENV_SECT_SIZE)
            break;
        i++;
    }

	memset(&er_opts, 0, sizeof(er_opts));
	er_opts.offset = CFG_ENV_OFFSET + (1 + i - goodBlockCounter) * nand_info[0].erasesize;
	er_opts.length = CFG_ENV_SECT_SIZE;
	er_opts.quiet  = 1;

	nand_erase_opts(nand, &er_opts);
	//nand_erase(nand_dev_desc + 0, CFG_ENV_OFFSET, CFG_ENV_SECT_SIZE, 0);
	printf("done");
#else
	MV_U32 env_sec = flash_in_which_sec(&flash_info[0], CFG_ENV_ADDR);
	
	if (env_sec == -1)
	{
		printf("Could not find ENV Sector\n");
		return 0;
	}

	printf("Un-Protect ENV Sector\n");

	flash_protect(FLAG_PROTECT_CLEAR,
				  CFG_ENV_ADDR,CFG_ENV_ADDR + CFG_ENV_SECT_SIZE - 1,
				  &flash_info[0]);

	
	printf("Erase sector %d ... ",env_sec);
	flash_erase(&flash_info[0], env_sec, env_sec);
	printf("done\nProtect ENV Sector\n");
	
	flash_protect(FLAG_PROTECT_SET,
				  CFG_ENV_ADDR,CFG_ENV_ADDR + CFG_ENV_SECT_SIZE - 1,
				  &flash_info[0]);

#endif /* defined(CFG_NAND_BOOT) */
	printf("\nWarning: Default Environment Variables will take effect Only after RESET \n\n");
	return 1;
}

U_BOOT_CMD(
        resetenv,      1,     1,      resetenv_cmd,
        "resetenv	- Return all environment variable to default.\n",
        " \n"
        "\t Erase the environemnt variable sector.\n"
);

#endif /* #if (CONFIG_COMMANDS & CFG_CMD_FLASH) */
#if CONFIG_COMMANDS & CFG_CMD_BSP

/******************************************************************************
* Category     - General
* Functionality- The commands allows the user to view the contents of the MV
*                internal registers and modify them.
* Need modifications (Yes/No) - no
*****************************************************************************/
int ir_cmd( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[] )
{
	MV_U32 regNum = 0x0, regVal, regValTmp, res;
	MV_8 regValBin[40];
	MV_8 cmd[40];
	int i,j = 0, flagm = 0;
	extern MV_8 console_buffer[];

	if( argc == 2 ) {
		regNum = simple_strtoul( argv[1], NULL, 16 );
	}
	else { 
		printf( "Usage:\n%s\n", cmdtp->usage );
		return 0;
	}                                                                                                        

	regVal = MV_REG_READ( regNum );
	regValTmp = regVal;
	printf( "Internal register 0x%x value : 0x%x\n ",regNum, regVal );
	printf( "\n    31      24        16         8         0" );
	printf( "\n     |       |         |         |         |\nOLD: " );

	for( i = 31 ; i >= 0 ; i-- ) {
		if( regValTmp > 0 ) {
			res = regValTmp % 2;
			regValTmp = (regValTmp - res) / 2;
			if( res == 0 )
				regValBin[i] = '0';
			else
				regValBin[i] = '1';
		}
		else
			regValBin[i] = '0';
	}

	for( i = 0 ; i < 32 ; i++ ) {
		printf( "%c", regValBin[i] );
		if( (((i+1) % 4) == 0) && (i > 1) && (i < 31) )
			printf( "-" );
	}

	readline( "\nNEW: " );
	strcpy(cmd, console_buffer);
	if( (cmd[0] == '0') && (cmd[1] == 'x') ) {
		regVal = simple_strtoul( cmd, NULL, 16 );
		flagm=1;
	}
	else {
		for( i = 0 ; i < 40 ; i++ ) {
			if(cmd[i] == '\0')
				break;
			if( i == 4 || i == 9 || i == 14 || i == 19 || i == 24 || i == 29 || i == 34 )
				continue;
			if( cmd[i] == '1' ) {
				regVal = regVal | (0x80000000 >> j);
				flagm = 1;
			}
			else if( cmd[i] == '0' ) {
				regVal = regVal & (~(0x80000000 >> j));
				flagm = 1;
			}
			j++;
		}
	}

	if( flagm == 1 ) {
		MV_REG_WRITE( regNum, regVal );
		printf( "\nNew value = 0x%x\n\n", MV_REG_READ(regNum) );
	}
	return 1;
}

U_BOOT_CMD(
	ir,      2,     1,      ir_cmd,
	"ir	- reading and changing MV internal register values.\n",
	" address\n"
	"\tDisplays the contents of the internal register in 2 forms, hex and binary.\n"
	"\tIt's possible to change the value by writing a hex value beginning with \n"
	"\t0x or by writing 0 or 1 in the required place. \n"
    	"\tPressing enter without any value keeps the value unchanged.\n"
);

/******************************************************************************
* Category     - General
* Functionality- Display the auto detect values of the TCLK and SYSCLK.
* Need modifications (Yes/No) - no
*****************************************************************************/
int clk_cmd( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    	printf( "TCLK %dMhz, SYSCLK %dMhz (UART baudrate %d)\n",
		mvTclkGet()/1000000, mvSysClkGet()/1000000, CONFIG_BAUDRATE);
	return 1;
}

U_BOOT_CMD(
	dclk,      1,     1,      clk_cmd,
	"dclk	- Display the MV device CLKs.\n",
	" \n"
	"\tDisplay the auto detect values of the TCLK and SYSCLK.\n"
);

/******************************************************************************
* Functional only when using Lauterbach to load image into DRAM
* Category     - DEBUG
* Functionality- Display the array of registers the u-boot write to.
*
*****************************************************************************/
#if defined(REG_DEBUG)
int reg_arry[REG_ARRAY_SIZE][2];
int reg_arry_index = 0;
int print_registers( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int i;
	printf("Register display\n");

	for (i=0; i < reg_arry_index; i++)
		printf("Index %d 0x%x=0x%08x\n", i, (reg_arry[i][0] & 0x000fffff), reg_arry[i][1]);
	
	/* Print DRAM registers */	
	printf("Index %d 0x%x=0x%08x\n", i++, 0x1500, MV_REG_READ(0x1500));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x1504, MV_REG_READ(0x1504));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x1508, MV_REG_READ(0x1508));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x150c, MV_REG_READ(0x150c));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x1510, MV_REG_READ(0x1510));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x1514, MV_REG_READ(0x1514));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x1518, MV_REG_READ(0x1518));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x151c, MV_REG_READ(0x151c));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x1400, MV_REG_READ(0x1400));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x1404, MV_REG_READ(0x1404));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x1408, MV_REG_READ(0x1408));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x140c, MV_REG_READ(0x140c));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x1410, MV_REG_READ(0x1410));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x141c, MV_REG_READ(0x141c));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x1420, MV_REG_READ(0x1420));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x1424, MV_REG_READ(0x1424));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x1428, MV_REG_READ(0x1428));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x147c, MV_REG_READ(0x147c));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x1494, MV_REG_READ(0x1494));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x1498, MV_REG_READ(0x1498));
	printf("Index %d 0x%x=0x%08x\n", i++, 0x149c, MV_REG_READ(0x149c));

	printf("Number of Reg %d \n", i);

	return 1;
}

U_BOOT_CMD(
	printreg,      1,     1,      print_registers,
	"printreg	- Display the register array the u-boot write to.\n",
	" \n"
	"\tDisplay the register array the u-boot write to.\n"
);
#endif

#if defined(MV_INCLUDE_UNM_ETH) || defined(MV_INCLUDE_GIG_ETH)
/******************************************************************************
* Category     - Etherent
* Functionality- Display PHY ports status (using SMI access).
* Need modifications (Yes/No) - No
*****************************************************************************/
int sg_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
#if defined(MV_INC_BOARD_QD_SWITCH)
		printf( "Switch status not supported\n");
#else
	MV_U32 port;
	for( port = 0 ; port < mvCtrlEthMaxPortGet(); port++ ) {

		printf( "PHY %d :\n", port );
		printf( "---------\n" );

		mvEthPhyPrintStatus( mvBoardPhyAddrGet(port) );

		printf("\n");
	}
#endif
	return 1;
}

U_BOOT_CMD(
	sg,      1,     1,      sg_cmd,
	"sg	- scanning the PHYs status\n",
	" \n"
	"\tScan all the Gig port PHYs and display their Duplex, Link, Speed and AN status.\n"
);
#endif /* #if defined(MV_INCLUDE_UNM_ETH) || defined(MV_INCLUDE_GIG_ETH) */

#if defined(MV_INCLUDE_IDMA)

/******************************************************************************
* Category     - DMA
* Functionality- Perform a DMA transaction
* Need modifications (Yes/No) - No
*****************************************************************************/
int mvDma_cmd( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[] )
{
	MV_8 cmd[20], c;
	extern MV_8 console_buffer[];
	MV_U32 chan, src, dst, byteCount, ctrlLo;
	MV_DMA_DEC_WIN win;
	MV_BOOL err;

	/* IDMA channel */
	if( argc == 2 ) 
		chan = simple_strtoul( argv[1], NULL, 16 );
	else
		chan = 0;

	/* source address */
	while(1) {
		readline( "Source Address: " );
		strcpy( cmd, console_buffer );
		src = simple_strtoul( cmd, NULL, 16 );
		if( src == 0xffffffff ) printf( "Bad address !!!\n" );
		else break;
	}

	/* desctination address */
	while(1) {
		readline( "Destination Address: " );
		strcpy(cmd, console_buffer);
		dst = simple_strtoul( cmd, NULL, 16 );
		if( dst == 0xffffffff ) printf("Bad address !!!\n");
		else break;
	}

	/* byte count */
	while(1) {
		readline( "Byte Count (up to 16M (0xffffff-1)): " );
		strcpy( cmd, console_buffer );
		byteCount = simple_strtoul( cmd, NULL, 16 );
		if( (byteCount > 0xffffff) || (byteCount == 0) ) printf("Bad value !!!\n");
		else break;
	}

	/* compose the command */
	ctrlLo = ICCLR_BLOCK_MODE | ICCLR_NON_CHAIN_MODE | ICCLR_SRC_INC | ICCLR_DST_INC;


	if (byteCount > _64K)
	{
		ctrlLo |= ICCLR_DESC_MODE_16M;
	}

	/* set data transfer limit */
	while(1) {
		printf( "Data transfer limit:\n" );
		printf( "(1) 8   bytes at a time.\n" );
		printf( "(2) 16  bytes at a time.\n" );
		printf( "(3) 32  bytes at a time.\n" );
		printf( "(4) 64  bytes at a time.\n" );
		printf( "(5) 128 bytes at a time.\n" );

		c = getc(); 
		printf( "%c\n", c );

		err = MV_FALSE;

		switch( c ) {
			case 13: /* Enter */
				ctrlLo |= (ICCLR_DST_BURST_LIM_32BYTE | ICCLR_SRC_BURST_LIM_32BYTE);
				printf( "32 bytes at a time.\n" );
				break;
			case '1':
				ctrlLo |= (ICCLR_DST_BURST_LIM_8BYTE | ICCLR_SRC_BURST_LIM_8BYTE);
				break;
			case '2':
				ctrlLo |= (ICCLR_DST_BURST_LIM_16BYTE | ICCLR_SRC_BURST_LIM_16BYTE);
				break;
			case '3':
				ctrlLo |= (ICCLR_DST_BURST_LIM_32BYTE | ICCLR_SRC_BURST_LIM_32BYTE);
				break;
			case '4':
				ctrlLo |= (ICCLR_DST_BURST_LIM_64BYTE | ICCLR_SRC_BURST_LIM_64BYTE);
				break;
			case '5':
				ctrlLo |= (ICCLR_DST_BURST_LIM_128BYTE | ICCLR_SRC_BURST_LIM_128BYTE);
				break;
			default:
				printf( "Bad value !!!\n" );
				err = MV_TRUE;
		}

		if( !err ) break;
	}

	/* set ovveride source option */
	while(1) {
		printf( "Override Source:\n" ); 
		printf( "(0) - no override\n" );
		mvDmaWinGet( 1, &win );
		printf( "(1) - use Win1 (%s)\n",mvCtrlTargetNameGet(win.target));
		mvDmaWinGet( 2, &win );
		printf( "(2) - use Win2 (%s)\n",mvCtrlTargetNameGet(win.target));
		mvDmaWinGet( 3, &win );
		printf( "(3) - use Win3 (%s)\n",mvCtrlTargetNameGet(win.target));

		c = getc(); 
		printf( "%c\n", c );

		err = MV_FALSE;

		switch( c ) {
			case 13: /* Enter */
			case '0':
				printf( "No override\n" );
				break;
			case '1':
				ctrlLo |= ICCLR_OVRRD_SRC_BAR(1);
				break;
			case '2':
				ctrlLo |= ICCLR_OVRRD_SRC_BAR(2);
				break;
			case '3':
				ctrlLo |= ICCLR_OVRRD_SRC_BAR(3);
				break;
			default:
				printf("Bad value !!!\n");
				err = MV_TRUE;
		}

		if( !err ) break;
	}

	/* set override destination option */
	while(1) {
		printf( "Override Destination:\n" ); 
		printf( "(0) - no override\n" );
		mvDmaWinGet( 1, &win );
		printf( "(1) - use Win1 (%s)\n",mvCtrlTargetNameGet(win.target));
		mvDmaWinGet( 2, &win );
		printf( "(2) - use Win2 (%s)\n",mvCtrlTargetNameGet(win.target));
		mvDmaWinGet( 3, &win );
		printf( "(3) - use Win3 (%s)\n",mvCtrlTargetNameGet(win.target));

		c = getc(); 
		printf( "%c\n", c );

		err = MV_FALSE;

	        switch( c ) {
			case 13: /* Enter */
			case '0':
				printf( "No override\n" );
				break;
			case '1':
				ctrlLo |= ICCLR_OVRRD_DST_BAR(1);
				break;
			case '2':
				ctrlLo |= ICCLR_OVRRD_DST_BAR(2);
				break;
			case '3':
				ctrlLo |= ICCLR_OVRRD_DST_BAR(3);
				break;
			default:
				printf("Bad value !!!\n");
				err = MV_TRUE;
		}

		if( !err ) break;
	}

	/* wait for previous transfer completion */
	while( mvDmaStateGet(chan) != MV_IDLE );

	/* issue the transfer */
	mvDmaCtrlLowSet( chan, ctrlLo ); 
	mvDmaTransfer( chan, src, dst, byteCount, 0 );

	/* wait for completion */
	while( mvDmaStateGet(chan) != MV_IDLE );

	printf( "Done...\n" );
	return 1;
}

U_BOOT_CMD(
	dma,      2,     1,      mvDma_cmd,
	"dma	- Perform DMA\n",
	" \n"
	"\tPerform DMA transaction with the parameters given by the user.\n"
);

#endif /* #if defined(MV_INCLUDE_IDMA) */

/******************************************************************************
* Category     - Memory
* Functionality- Displays the MV's Memory map
* Need modifications (Yes/No) - Yes
*****************************************************************************/
int displayMemoryMap_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	mvCtrlAddrDecShow();
	return 1;
}

U_BOOT_CMD(
	map,      1,     1,      displayMemoryMap_cmd,
	"map	- Diasplay address decode windows\n",
	" \n"
	"\tDisplay controller address decode windows: CPU, PCI, Gig, DMA, XOR and COMM\n"
);



#include "ddr2/spd/mvSpd.h"
#if defined(MV_INC_BOARD_DDIM)

/******************************************************************************
* Category     - Memory
* Functionality- Displays the SPD information for a givven dimm
* Need modifications (Yes/No) - 
*****************************************************************************/
              
int dimminfo_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
        int num = 0;
 
        if (argc > 1) {
                num = simple_strtoul (argv[1], NULL, 10);
        }
 
        printf("*********************** DIMM%d *****************************\n",num);
 
        dimmSpdPrint(num);
 
        printf("************************************************************\n");
         
        return 1;
}
 
U_BOOT_CMD(
        ddimm,      2,     1,      dimminfo_cmd,
        "ddimm  - Display SPD Dimm Info\n",
        " [0/1]\n"
        "\tDisplay Dimm 0/1 SPD information.\n"
);

/******************************************************************************
* Category     - Memory
* Functionality- Copy the SPD information of dimm 0 to dimm 1
* Need modifications (Yes/No) - 
*****************************************************************************/
              
int spdcpy_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
 
        printf("Copy DIMM 0 SPD data into DIMM 1 SPD...");
 
        if (MV_OK != dimmSpdCpy())
        	printf("\nDIMM SPD copy fail!\n");
 	else
        	printf("Done\n");
         
        return 1;
}
 
U_BOOT_CMD(
        spdcpy,      2,     1,      spdcpy_cmd,
        "spdcpy  - Copy Dimm 0 SPD to Dimm 1 SPD \n",
        ""
        ""
);
#endif /* #if defined(MV_INC_BOARD_DDIM) */

/******************************************************************************
* Functionality- Go to an address and execute the code there and return,
*    defualt address is 0x40004
*****************************************************************************/
extern void cpu_dcache_flush_all(void);
extern void cpu_icache_flush_invalidate_all(void);

void mv_go(unsigned long addr,int argc, char *argv[])
{
	int rc;
	addr = MV_CACHEABLE(addr);
	char* envCacheMode = getenv("cacheMode");
 
	/*
	 * pass address parameter as argv[0] (aka command name),
	 * and all remaining args
	 */

    if(envCacheMode && (strcmp(envCacheMode,"write-through") == 0))
	{	
		int i=0;

		/* Flush Invalidate I-Cache */
		cpu_icache_flush_invalidate_all();

		/* Drain write buffer */
		asm ("mcr p15, 0, %0, c7, c10, 4": :"r" (i));
		

	}
	else /*"write-back"*/
	{
		int i=0;

		/* Flush Invalidate I-Cache */
		cpu_icache_flush_invalidate_all();

		/* Drain write buffer */
		asm ("mcr p15, 0, %0, c7, c10, 4": :"r" (i));
		

		/* Flush invalidate D-cache */
		cpu_dcache_flush_all();


    }


	rc = ((ulong (*)(int, char *[]))addr) (--argc, &argv[1]);
 
        return;
}

int g_cmd (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
        ulong   addr;

	if(!enaMonExt()){
		printf("This command can be used only if enaMonExt is set!\n");
		return 0;
	}

	addr = 0x40000;

        if (argc > 1) {
		addr = simple_strtoul(argv[1], NULL, 16);
        }
	mv_go(addr,argc,&argv[0]);
	return 1;
}                                                                                                                     

U_BOOT_CMD(
	g,      CFG_MAXARGS,     1,      g_cmd,
        "g	- start application at cached address 'addr'(default addr 0x40000)\n",
        " addr [arg ...] \n"
	"\tStart application at address 'addr'cachable!!!(default addr 0x40004/0x240004)\n"
	"\tpassing 'arg' as arguments\n"
	"\t(This command can be used only if enaMonExt is set!)\n"
);

/******************************************************************************
* Functionality- Searches for a value
*****************************************************************************/
int fi_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    MV_U32 s_address,e_address,value,i,tempData;
    MV_BOOL  error = MV_FALSE;

    if(!enaMonExt()){
	printf("This command can be used only if enaMonExt is set!\n");
	return 0;}

    if(argc == 4){
	value = simple_strtoul(argv[1], NULL, 16);
	s_address = simple_strtoul(argv[2], NULL, 16);
	e_address = simple_strtoul(argv[3], NULL, 16);
    }else{ printf ("Usage:\n%s\n", cmdtp->usage);
    	return 0;
    }     

    if(s_address == 0xffffffff || e_address == 0xffffffff) error = MV_TRUE;
    if(s_address%4 != 0 || e_address%4 != 0) error = MV_TRUE;
    if(s_address > e_address) error = MV_TRUE;
    if(error)
    {
	printf ("Usage:\n%s\n", cmdtp->usage);
        return 0;
    }
    for(i = s_address; i < e_address ; i+=4)
    {
        tempData = (*((volatile unsigned int *)i));
        if(tempData == value)
        {
            printf("Value: %x found at ",value);
            printf("address: %x\n",i);
            return 1;
        }
    }
    printf("Value not found!!\n");
    return 1;
}

U_BOOT_CMD(
	fi,      4,     1,      fi_cmd,
	"fi	- Find value in the memory.\n",
	" value start_address end_address\n"
	"\tSearch for a value 'value' in the memory from address 'start_address to\n"
	"\taddress 'end_address'.\n"
	"\t(This command can be used only if enaMonExt is set!)\n"
);

/******************************************************************************
* Functionality- Compare the memory with Value.
*****************************************************************************/
int cmpm_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    MV_U32 s_address,e_address,value,i,tempData;
    MV_BOOL  error = MV_FALSE;

    if(!enaMonExt()){
	printf("This command can be used only if enaMonExt is set!\n");
	return 0;}

    if(argc == 4){
	value = simple_strtoul(argv[1], NULL, 16);
	s_address = simple_strtoul(argv[2], NULL, 16);
	e_address = simple_strtoul(argv[3], NULL, 16);
    }else{ printf ("Usage:\n%s\n", cmdtp->usage);
    	return 0;
    }     

    if(s_address == 0xffffffff || e_address == 0xffffffff) error = MV_TRUE;
    if(s_address%4 != 0 || e_address%4 != 0) error = MV_TRUE;
    if(s_address > e_address) error = MV_TRUE;
    if(error)
    {
	printf ("Usage:\n%s\n", cmdtp->usage);
        return 0;
    }
    for(i = s_address; i < e_address ; i+=4)
    {
        tempData = (*((volatile unsigned int *)i));
        if(tempData != value)
        {
            printf("Value: %x found at address: %x\n",tempData,i);
        }
    }
    return 1;
}

U_BOOT_CMD(
	cmpm,      4,     1,      cmpm_cmd,
	"cmpm	- Compare Memory\n",
	" value start_address end_address.\n"
	"\tCompare the memory from address 'start_address to address 'end_address'.\n"
	"\twith value 'value'\n"
	"\t(This command can be used only if enaMonExt is set!)\n"
);



#if 0
/******************************************************************************
* Category     - Etherent
* Functionality- Display PHY ports status (using SMI access).
* Need modifications (Yes/No) - No
*****************************************************************************/
int eth_show_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	ethRegs(argv[1]);
	ethPortRegs(argv[1]);
	ethPortStatus(argv[1]);
	ethPortQueues(argv[1],0,0,1);
	return 1;
}

U_BOOT_CMD(
	ethShow,      2,    2,      eth_show_cmd,
	"ethShow	- scanning the PHYs status\n",
	" \n"
	"\tScan all the Gig port PHYs and display their Duplex, Link, Speed and AN status.\n"
);
#endif

#if defined(MV_INCLUDE_PEX)

#include "pci/mvPci.h"

int pcie_phy_read_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    	MV_U16 phyReg;

    	mvPexPhyRegRead(simple_strtoul( argv[1], NULL, 16 ),
	                simple_strtoul( argv[2], NULL, 16), &phyReg);

	printf ("0x%x\n", phyReg);

	return 1;
}

U_BOOT_CMD(
	pciePhyRead,      3,     3,      pcie_phy_read_cmd,
	"phyRead	- Read PCI-E Phy register\n",
	" PCI-E_interface Phy_offset. \n"
	"\tRead the PCI-E Phy register. \n"
);


int pcie_phy_write_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	mvPexPhyRegWrite(simple_strtoul( argv[1], NULL, 16 ),
					 simple_strtoul( argv[2], NULL, 16 ),
					 simple_strtoul( argv[3], NULL, 16 ));

	return 1;
}

U_BOOT_CMD(
	pciePhyWrite,      4,     4,      pcie_phy_write_cmd,
	"pciePhyWrite	- Write PCI-E Phy register\n",
	" PCI-E_interface Phy_offset value.\n"
	"\tWrite to the PCI-E Phy register.\n"
);

#endif /* #if defined(MV_INCLUDE_UNM_ETH) || defined(MV_INCLUDE_GIG_ETH) */
#if defined(MV_INCLUDE_UNM_ETH) || defined(MV_INCLUDE_GIG_ETH)

#include "eth-phy/mvEthPhy.h"

int phy_read_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    	MV_U16 phyReg;

    	mvEthPhyRegRead(simple_strtoul( argv[1], NULL, 16 ),
	                simple_strtoul( argv[2], NULL, 16), &phyReg);

	printf ("0x%x\n", phyReg);

	return 1;
}

U_BOOT_CMD(
	phyRead,      3,     3,      phy_read_cmd,
	"phyRead	- Read Phy register\n",
	" Phy_address Phy_offset. \n"
	"\tRead the Phy register. \n"
);


int phy_write_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	mvEthPhyRegWrite(simple_strtoul( argv[1], NULL, 16 ),
					 simple_strtoul( argv[2], NULL, 16 ),
					 simple_strtoul( argv[3], NULL, 16 ));

	return 1;
}

U_BOOT_CMD(
	phyWrite,      4,     4,      phy_write_cmd,
	"phyWrite	- Write Phy register\n",
	" Phy_address Phy_offset value.\n"
	"\tWrite to the Phy register.\n"
);


int switch_read_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    	MV_U16 phyReg;

    	mvEthSwitchRegRead(simple_strtoul( argv[1], NULL, 16 ),
	                simple_strtoul( argv[2], NULL, 16), simple_strtoul( argv[3], NULL, 16 ), &phyReg);

	printf ("0x%x\n", phyReg);

	return 1;
}

U_BOOT_CMD(
	switchRegRead,      4,     4,      switch_read_cmd,
	"switchRegRead	- Read switch register\n",
	" Port_number Phy_address Phy_offset. \n"
	"\tRead the switch register. \n"
);


int switch_write_cmd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	mvEthSwitchRegWrite(simple_strtoul( argv[1], NULL, 16 ),
                        simple_strtoul( argv[2], NULL, 16 ),
                        simple_strtoul( argv[3], NULL, 16 ),
                        simple_strtoul( argv[4], NULL, 16 ));

	return 1;
}

U_BOOT_CMD(
	switchRegWrite,      4,     4,      switch_write_cmd,
	"switchRegWrite	- Write switch register\n",
	" Port_number Phy_address Phy_offset value.\n"
	"\tWrite to the switch register.\n"
);
#endif /* #if defined(MV_INCLUDE_UNM_ETH) || defined(MV_INCLUDE_GIG_ETH) */

#endif /* MV_TINY */

int _4BitSwapArry[] = {0,8,4,0xc,2,0xa,6,0xe,1,9,5,0xd,3,0xb,7,0xf};
int _3BitSwapArry[] = {0,4,2,6,1,5,3,7};

int do_satr(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	char *cmd, *s;
	MV_U8 data0=0, data1=0, devNum0=0, devNum1=0;
	MV_U8 moreThenOneDev=0, regNum = 0;
	MV_U8 mask0=0, mask1=0, shift0=0, shift1=0;
	MV_U8 val=0, width=0;

	/* at least two arguments please */
	if (argc < 2)
		goto usage;

	cmd = argv[1];

	if (strncmp(cmd, "read", 4) != 0 && strncmp(cmd, "write", 5) != 0)
		goto usage;

	/* read write */
	if (strncmp(cmd, "read", 4) == 0 || strncmp(cmd, "write", 5) == 0) {
		int read;


		read = strncmp(cmd, "read", 4) == 0; /* 1 = read, 0 = write */

		/* In write mode we have additional value */
		if (!read)
		{
			if (argc < 3)
				goto usage;
			else
				/* Value for write */
				val = (ulong)simple_strtoul(argv[2], NULL, 16);
		}

		printf("\nS@R %s: ", read ? "read" : "write");
		s = strchr(cmd, '.');
		if ((s != NULL) && (strcmp(s, ".cpu") == 0))
		{
#if defined(DB_88F6180A) || defined(DB_88F6280A)
			moreThenOneDev = 0;
			regNum = 0;
			devNum0 = 0;
			mask0 = 0x7;
			shift0 = 0;
			mask1 = 0x0;
			shift1 = 0;
			width = 3;
#else
			moreThenOneDev = 0;
			regNum = 0;
			devNum0 = 0;
			mask0 = 0xf;
			shift0 = 0;
			mask1 = 0x0;
			shift1 = 0;
			width = 4;
#endif
		}

		if ((s != NULL) && (strcmp(s, ".cpu2ddr") == 0))
		{
			moreThenOneDev = 1;
			regNum = 0;
			devNum0 = 0;
			devNum1 = 1;
			mask0 = 0x10;
			shift0 = 4;
			mask1 = 0x7;
			shift1 = 1;
			width = 4;
		}

		if ((s != NULL) && (strcmp(s, ".cpu2L2") == 0))
		{
			moreThenOneDev = 1;
			regNum = 0;
			devNum0 = 1;
			devNum1 = 2;
			mask0 = 0x18;
			shift0 = 3;
			mask1 = 0x1;
			shift1 = 2;
			width = 3;
		}

		if ((s != NULL) && (strcmp(s, ".SSCG") == 0)) 
		{
#ifdef DB_88F6180A
			moreThenOneDev = 0;
			regNum = 0;
			devNum0 = 0;
			mask0 = 0x8;
			shift0 = 3;
			mask1 = 0x0;
			shift1 = 0;
#else
			moreThenOneDev = 0;
			regNum = 0;
			devNum0 = 2;
			mask0 = 0x4;
			shift0 = 2;
			mask1 = 0x0;
			shift1 = 0;
#endif
		}

		if ((s != NULL) && (strcmp(s, ".PEXCLK") == 0)) 
		{
			moreThenOneDev = 0;
			regNum = 0;
			devNum0 = 2;
			mask0 = 0x10;
			shift0 = 4;
			mask1 = 0x0;
			shift1 = 0;
		}

		if ((s != NULL) && ((strcmp(s, ".MPP18") == 0) || (strcmp(s, ".TCLK") == 0)) )
		{

#ifdef DB_88F6180A
			moreThenOneDev = 0;
			regNum = 0;
			devNum0 = 0;
			mask0 = 0x10;
			shift0 = 4;
			mask1 = 0x0;
			shift1 = 0;
#else
			moreThenOneDev = 0;
			regNum = 0;
			devNum0 = 2;
			mask0 = 0x8;
			shift0 = 3;
			mask1 = 0x0;
			shift1 = 0;
#endif
		}
			
		if (read) {
			/* read */
			data0 = mvBoarTwsiSatRGet(devNum0, regNum);
			if (moreThenOneDev)
				data1 = mvBoarTwsiSatRGet(devNum1, regNum);

			data0 = ((data0 & mask0) >> shift0);

			if (moreThenOneDev)
			{
				data1 = ((data1 & mask1) << shift1);
				data0 |= data1;
			}
			
			/* Swap value */
			switch(width)
			{
				case 4:
					data0 = _4BitSwapArry[data0];
					break;
				case 3:
					data0 = _3BitSwapArry[data0];
					break;
				case 2:
					data0 = (((data0 & 0x1) << 0x1) | ((data0 & 0x2) >> 0x1));
					break;
			}

			printf("Read S@R val %x\n", data0);

		} else {

			/* Swap value */
			switch(width)
			{
				case 4:
					val = _4BitSwapArry[val];
					break;
				case 3:
					val = _3BitSwapArry[val];
					break;
				case 2:
					val = (((val & 0x1) << 0x1) | ((val & 0x2) >> 0x1));
					break;
			}

			/* read modify write */
			data0 = mvBoarTwsiSatRGet(devNum0, regNum);
			data0 = (data0 & ~mask0);
			data0 |= ((val << shift0) & mask0);
			if (mvBoarTwsiSatRSet(devNum0, regNum, data0) != MV_OK)
			{
				printf("Write S@R first device val %x fail\n", data0);
				return 1;
			}
			printf("Write S@R first device val %x succeded\n", data0);

			if (moreThenOneDev)
			{
				data1 = mvBoarTwsiSatRGet(devNum1, regNum);
				data1 = (data1 & ~mask1);
				data1 |= ((val >> shift1) & mask1);
				if (mvBoarTwsiSatRSet(devNum1, regNum, data1) != MV_OK)
				{
					printf("Write S@R second device val %x fail\n", data1);
					return 1;
				}
				printf("Write S@R second device val %x succeded\n", data1);
			}
		}

		return 0;
	}

usage:
	printf("Usage:\n%s\n", cmdtp->usage);
	return 1;
}

#ifdef DB_88F6180A
U_BOOT_CMD(SatR, 5, 1, do_satr,
	"SatR - sample at reset sub-system, relevent for DB only\n",
	"SatR read.cpu 		- read cpu/L2/DDR clock from S@R devices\n"
    "SatR read.SSCG		- read SSCG state from S@R devices [0 ~ en]\n"
    "SatR read.MPP18	- reserved\n"
	"SatR write.cpu val	- write cpu/L2/DDR clock val to S@R devices [0,1,..,7]\n"
    "SatR write.SSCG val	- write SSCG state val to S@R devices [0 ~ en]\n"
    "SatR write.MPP18	- reserved\n"                       
);
#elif defined(DB_88F6280A)
U_BOOT_CMD(SatR, 5, 1, do_satr,
	"SatR - sample at reset sub-system, relevent for DB only\n",
	"SatR read.cpu 		- read cpu/L2/DDR clock from S@R devices\n"
);

#elif defined(DB_88F6192A) || defined(DB_88F6281A) || defined(DB_88F6282A)
U_BOOT_CMD(SatR, 5, 1, do_satr,
	"SatR - sample at reset sub-system, relevent for DB only\n",
	"SatR read.cpu 		- read cpu clock from S@R devices\n"
	"SatR read.cpu2ddr	- read cpu2ddr clock ratio from S@R devices\n"
	"SatR read.cpu2L2	- read cpu2L2 clock ratio from S@R devices\n"
	"SatR read.SSCG		- read SSCG state from S@R devices [0 ~ en]\n"
    "SatR read.PEXCLK   - read PCI-E clock state from S@R devices [0 ~ input]\n"
#if defined(DB_88F6281A) || defined(DB_88F6282A)
   "SatR read.TCLK	    - read TCLK value (0 = 200MHz, 1 = 166MHz)\n"
#else
   "SatR read.MPP18	    - reserved\n"
#endif
	"SatR write.cpu val	- write cpu clock val to S@R devices [0,1,..,F]\n"
	"SatR write.cpu2ddr val	- write cpu2ddr clock ratio val to S@R devices [0,1,..,F]\n"
	"SatR write.cpu2L2 val	- write cpu2L2 clock ratio val to S@R devices [0,1,..,7]\n"
	"SatR write.SSCG val	- write SSCG state val to S@R devices [0 ~ en]\n"
    "SatR write.PEXCLK	- write PCI-E clock state from S@R devices [0 ~ input]\n"
#if defined(DB_88F6281A) || defined(DB_88F6282A)
   "SatR write.TCLK	    - write TCLK value (0 = 200MHz, 1 = 166MHz)\n"
#else
   "SatR write.MPP18	    - reserved\n"
#endif
);
#endif

#if (CONFIG_COMMANDS & CFG_CMD_RCVR)
extern void recoveryHandle(void);
int do_rcvr (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	recoveryHandle();
	return 1;
}

U_BOOT_CMD(
	rcvr,	3,	1,	do_rcvr,
	"rcvr\t- Satrt recovery process (Distress Beacon with TFTP server)\n",
	"\n"
);
#endif	/* CFG_CMD_RCVR */

#ifdef CFG_DIAG

#include "../diag/diag.h"
extern diag_func_t *diag_sequence[];

int mv_diag (cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
        int test_no = 0, no_of_tests = 0; 
        diag_func_t **diag_func_ptr;


        for (diag_func_ptr = diag_sequence; *diag_func_ptr; ++diag_func_ptr)
                no_of_tests++;

        if (argc > 1) 
        {
                test_no = simple_strtoul(argv[1], NULL, 10); 
                if (test_no > no_of_tests)
                {
                        printf("There are only %d tests\n", no_of_tests);
                        printf("Usage: %s\n", cmdtp->help);
                        return 0;
                }

                test_no--;
                (*diag_sequence[test_no])();
                return 0;
        }

        for (diag_func_ptr = diag_sequence; *diag_func_ptr; ++diag_func_ptr)
        {
                printf("\n");
                if((*diag_func_ptr)())
                        break;
        }

        if(*diag_func_ptr == NULL)
                printf("\nDiag completed\n");
        else
                printf("\nDiag FAILED\n");

        return 0;
}

U_BOOT_CMD(
        mv_diag, 2, 0, mv_diag,
        "mv_diag - perform board diagnostics\n"
        "mv_diag - run all available tests\n"
        "mv_diag [1|2|...]\n"
        "        - run specified test number\n",
        "mv_diag - perform board diagnostics\n"
        "mv_diag - run all available tests\n"
        "mv_diag [1|2|...]\n"
        "        - run specified test number\n"
);

#endif /*CFG_DIAG*/
