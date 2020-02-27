/*
 * (C) Copyright 2000-2002
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2001 Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Andreas Heppel <aheppel@sysgo.de>

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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <config.h>
#include <environment.h>
#include <linux/stddef.h>
#include <malloc.h>

#ifdef CONFIG_SHOW_BOOT_PROGRESS
# include <status_led.h>
# define SHOW_BOOT_PROGRESS(arg)	show_boot_progress(arg)
#else
# define SHOW_BOOT_PROGRESS(arg)
#endif

#ifdef CONFIG_AMIGAONEG3SE
	extern void enable_nvram(void);
	extern void disable_nvram(void);
#endif

#undef DEBUG_ENV
#ifdef DEBUG_ENV
#define DEBUGF(fmt,args...) printf(fmt ,##args)
#else
#define DEBUGF(fmt,args...)
#endif

extern env_t *env_ptr;

extern void env_relocate_spec (void);
extern uchar env_get_char_spec(int);

static uchar env_get_char_init (int index);
uchar (*env_get_char)(int) = env_get_char_init;

/************************************************************************
 * Default settings to be used when no valid environment is found
 */
#define XMK_STR(x)	#x
#define MK_STR(x)	XMK_STR(x)

uchar default_environment[] = {
#ifdef	CONFIG_BOOTARGS
	"bootargs="	CONFIG_BOOTARGS			"\0"
#endif
#ifdef	CONFIG_BOOTCOMMAND
	"bootcmd="	CONFIG_BOOTCOMMAND		"\0"
#endif
#ifdef	CONFIG_RAMBOOTCOMMAND
	"ramboot="	CONFIG_RAMBOOTCOMMAND		"\0"
#endif
#ifdef	CONFIG_NFSBOOTCOMMAND
	"nfsboot="	CONFIG_NFSBOOTCOMMAND		"\0"
#endif
#if defined(CONFIG_BOOTDELAY) && (CONFIG_BOOTDELAY >= 0)
	"bootdelay="	MK_STR(CONFIG_BOOTDELAY)	"\0"
#endif
#if defined(CONFIG_BAUDRATE) && (CONFIG_BAUDRATE >= 0)
	"baudrate="	MK_STR(CONFIG_BAUDRATE)		"\0"
#endif
#ifdef	CONFIG_LOADS_ECHO
	"loads_echo="	MK_STR(CONFIG_LOADS_ECHO)	"\0"
#endif
#ifdef	CONFIG_ETHADDR
	"ethaddr="	MK_STR(CONFIG_ETHADDR)		"\0"
#endif
#ifdef	CONFIG_ETH1ADDR
	"eth1addr="	MK_STR(CONFIG_ETH1ADDR)		"\0"
#endif
#ifdef	CONFIG_ETH2ADDR
	"eth2addr="	MK_STR(CONFIG_ETH2ADDR)		"\0"
#endif
#ifdef	CONFIG_ETH3ADDR
	"eth3addr="	MK_STR(CONFIG_ETH3ADDR)		"\0"
#endif
#ifdef	CONFIG_IPADDR
	"ipaddr="	MK_STR(CONFIG_IPADDR)		"\0"
#endif
#ifdef	CONFIG_SERVERIP
	"serverip="	MK_STR(CONFIG_SERVERIP)		"\0"
#endif
#ifdef	CFG_AUTOLOAD
	"autoload="	CFG_AUTOLOAD			"\0"
#endif
#ifdef	CONFIG_PREBOOT
	"preboot="	CONFIG_PREBOOT			"\0"
#endif
#ifdef	CONFIG_ROOTPATH
	"rootpath="	MK_STR(CONFIG_ROOTPATH)		"\0"
#endif
#ifdef	CONFIG_GATEWAYIP
	"gatewayip="	MK_STR(CONFIG_GATEWAYIP)	"\0"
#endif
#ifdef	CONFIG_NETMASK
	"netmask="	MK_STR(CONFIG_NETMASK)		"\0"
#endif
#ifdef	CONFIG_HOSTNAME
	"hostname="	MK_STR(CONFIG_HOSTNAME)		"\0"
#endif
#ifdef	CONFIG_BOOTFILE
	"bootfile="	MK_STR(CONFIG_BOOTFILE)		"\0"
#endif
#ifdef	CONFIG_LOADADDR
	"loadaddr="	MK_STR(CONFIG_LOADADDR)		"\0"
#endif
#ifdef  CONFIG_CLOCKS_IN_MHZ
	"clocks_in_mhz=1\0"
#endif
#if defined(CONFIG_PCI_BOOTDELAY) && (CONFIG_PCI_BOOTDELAY > 0)
	"pcidelay="	MK_STR(CONFIG_PCI_BOOTDELAY)	"\0"
#endif
#ifdef  CONFIG_EXTRA_ENV_SETTINGS
	CONFIG_EXTRA_ENV_SETTINGS
#endif
    "run_diag="     "yes"   "\0"

#ifdef CFG_CP_DEF_ENV
/* Check Point vars start here */
	"cp_quiet="			CP_BOOT_MODE_QUIET_STR		"\0"
	"cp_boot_mode="		MK_STR(CP_BOOT_MODE_QUIET)	"\0"
	

    "set_bootargs_L30="	"setenv bootargs mv_net_config=7,"		\
    					"($(lan1_mac_addr),1)"					\
    					"($(dmz_mac_addr),8)"					\
    					"($(lan2_mac_addr),2)"					\
    					"($(lan3_mac_addr),3)"					\
    					"($(lan4_mac_addr),5)"					\
    					"($(lan5_mac_addr),6)"					\
    					"($(lan6_mac_addr),7)"					\
                        ",mtu=1500 console=ttyS0,115200 "		\
    					"$(cp_quiet) "							\
    					"mtdparts=nand_mtd:640k(u-boot)ro,384k(bootldr-env),"\
    					"8m(kernel-1),86m(rootfs-1),"			\
    					"8m(kernel-2),86m(rootfs-2),"			\
    					"94m(default_sw),94m(images_back),"	\
    					"16m(config-back),"						\
    					"23m(logs),-(storage)"								"\0"

#ifdef SEATTLE
	"set_bootargs_L50="	"setenv bootargs mv_net_config=10,"		\
						"($(lan1_mac_addr),0)($(dmz_mac_addr),8)"	\
						"($(lan2_mac_addr),1)"					\
						"($(lan3_mac_addr),2)"					\
						"($(lan4_mac_addr),3)"					\
						"($(lan5_mac_addr),4)"					\
						"($(lan6_mac_addr),5)"					\
						"($(lan7_mac_addr),6)"					\
						"($(lan8_mac_addr),7)"					\
                        "($(dsl_mac_addr),9),"
						"mtu=1500 console=$(cp_console_str) "		\
						"$(cp_quiet) "							\
						"mtdparts=nand_mtd:640k(u-boot)ro,384k(bootldr-env),"\
						"8m(kernel-1),113m(rootfs-1),"			\
						"8m(kernel-2),113m(rootfs-2),"			\
						"121m(default_sw),"	\
						"24m(logs),"						\
						"1m(preset_cfg),"						\
						"1m(adsl),"						\
						"-(storage) " \
                                                "$(cp_quiet) " \
						"noExtPorts=${noExtPorts} " \
                        "boardFlavor=SEATTLE"	                "\0"
#else
"set_bootargs_L50="	"setenv bootargs mv_net_config=9,"		\
						"($(lan1_mac_addr),0)($(dmz_mac_addr),8)"	\
						"($(lan2_mac_addr),1)"					\
						"($(lan3_mac_addr),2)"					\
						"($(lan4_mac_addr),3)"					\
						"($(lan5_mac_addr),4)"					\
						"($(lan6_mac_addr),5)"					\
						"($(lan7_mac_addr),6)"					\
						"($(lan8_mac_addr),7),"					\
						"mtu=1500 console=ttyS0,115200"		\
						"$(cp_quiet) "							\
						"mtdparts=nand_mtd:640k(u-boot)ro,384k(bootldr-env),"\
						"8m(kernel-1),86m(rootfs-1),"			\
						"8m(kernel-2),86m(rootfs-2),"			\
						"94m(default_sw),94m(images_back),"	\
						"16m(config-back),"						\
						"23m(logs),"						\
						"-(storage)"				"\0"
#endif
	"set_bootargs="		"run set_bootargs_$(unitModel) ; saveenv"			"\0"

/* Partitions and memory addresses */
	"bootaddr="				MK_STR(CP_BOOT_ADDR)							"\0"
	
	"primary_offset="		MK_STR(CP_PRIMARY_OFFSET)						"\0"
	
	"secondary_offset="		MK_STR(CP_SECONDARY_OFFSET)						"\0"
	
	"default_image_offset="	MK_STR(CP_DEFAULT_IMAGE_OFFSET)					"\0"
#ifndef SEATTLE
	"images_back_offset="	MK_STR(CP_IMAGES_BACK_OFFSET)					"\0"
	
	"config_back_offset="	MK_STR(CP_CONFIG_BACK_OFFSET)					"\0"
#endif
	
	"logs_offset="			MK_STR(CP_LOGS_OFFSET)							"\0"
	
	"storage_offset="		MK_STR(CP_STORAGE_OFFSET)						"\0"
#ifndef SEATTLE
	"images_back_size="		MK_STR(CP_IMAGES_BACK_SIZE)						"\0"
	
	"config_back_size="		MK_STR(CP_CONFIG_BACK_SIZE)						"\0"
#endif
	
	"logs_size="			MK_STR(CP_LOG_SIZE)								"\0"
#ifdef SEATTLE	
	"adsl_offset="			MK_STR(CP_ADSL_OFFSET)						"\0"	
	"adsl_size="			MK_STR(CP_ADSL_SIZE)							"\0"
	"preset_cfg_offset="		MK_STR(CP_PRESET_CFG_OFFSET)						"\0"
	"preset_cfg_size="		MK_STR(CP_PRESET_CFG_SIZE)						"\0"
#endif
	"storage_size="			MK_STR(CP_STORAGE_SIZE)							"\0"
	
	"image_size="			MK_STR(CP_IMAGE_SIZE)							"\0"

	"default_image_size="	MK_STR(CP_IMAGE_SIZE)							"\0"

	"kernel_size="			MK_STR(CP_KERNEL_SIZE)							"\0"
	
	"flash_end="			MK_STR(CP_FLASH_END)							"\0"

	"flash_erase_size="		MK_STR(CP_FLASH_ERASE_SIZE)						"\0"

/* Safe NAND erasing */
	"primary_part_erase="	"nand erase $(primary_offset) $(image_size)" 	"\0"
	
	"secondary_part_erase="	"nand erase $(secondary_offset) $(image_size)"	"\0"
#ifndef SEATTLE
	"images_back_erase="	"nand erase $(images_back_offset) $(images_back_size)" "\0"
	"config_back_erase="	"nand erase $(config_back_offset) $(config_back_size)" "\0"
#else
	"images_back_erase="	"\0"	
	"config_back_erase="	"\0" 
#endif																			
	
	"logs_erase="			"nand erase $(logs_offset) $(logs_size)" 		"\0"
	
	"storage_erase="		"nand erase $(storage_offset) $(storage_size)"  "\0"
#ifdef SEATTLE		
	"flash_erase_except_default="
							"run primary_part_erase secondary_part_erase "  \
							"logs_erase storage_erase" "\0"
	
	"seattle_flash_erase="	"nand erase $(primary_offset) $(preset_cfg_offset)" "\0"
	"flash_erase="			"run seattle_flash_erase storage_erase" "\0"
#else
	"flash_erase_except_default="
							"run primary_part_erase secondary_part_erase "  \
							"images_back_erase config_back_erase logs_erase " \
							"storage_erase"									"\0"
	"flash_erase="			"nand erase $(primary_offset) $(flash_erase_size)" "\0"
#endif																		
														
	"enaAutoRecovery="		"yes"											"\0"

/* Active flags */
	"activePartition="		"1"												"\0"
	
	"activeConfig="			"1"												"\0"

/* upgradeFlag should be 1 only during an upgrade boot */
	"upgradeFlag="			"0"												"\0"

/* recoverFlag should be 1 only during a recovert boot */
	"recoverFlag="			"0"												"\0"

/* Write from memory to the primary parition */
	"burn_primary="			"nand write.e $(loadaddr) $(primary_offset) $(filesize)"
																			"\0"
/* LED manipulation */
	"sw_leds_progress="		"switch_leds progress"							"\0"

	"sw_leds_reset="		"switch_leds reset"					 			"\0"

/* Variables for resetting to the factory defaults image */
	"burn_default="			"nand write.e $(loadaddr) $(default_image_offset) $(filesize); "\
							"setenv default_image_size $(filesize) ; saveenv"
																			"\0"
							
	"read_default="			"nand read.e $(loadaddr) $(default_image_offset) $(default_image_size)"
																			"\0"
							
	"write_default_to_primary="
							"setenv activePartition 1 ; setenv activeConfig 1 ; "\
							"nand write.e $(loadaddr) $(primary_offset) $(default_image_size)"
																			"\0"
							
	"set_bootcmd_return_default="
							"setenv bootcmd run return_to_default bootImage ; saveenv"
																			"\0"

	"return_to_default="	"run sw_leds_progress set_bootcmd_return_default flash_erase_except_default"\
							" read_default write_default_to_primary "\
							"set_bootcmd_normal sw_leds_reset"							"\0"
				
/* Boot from memory, if _default - restore some variables to their default and then boot */															
	"bootImage="			"run set_bootargs ; bootm $(bootaddr)"			"\0"

/*Read the correct kernel partition from memory (according to activePartition) and then boot */
	
	"bootcmd="				"run tf1_image$(activePartition)"				"\0"


	"tf1_image1="			"nand read.e $(loadaddr) "	\
							"$(primary_offset) $(kernel_size); "			\
							"run bootImage"									"\0"

	"tf1_image2="			"nand read.e $(loadaddr) "	\
							"$(secondary_offset) $(kernel_size); "			\
							"run bootImage"									"\0"

/* Trick to switch the active partition */
	"switch_active_from_1="	"setenv activePartition 2 ; saveenv"			"\0"
	
	"switch_active_from_2="	"setenv activePartition 1 ; saveenv"			"\0"

	"switch_active="		"run switch_active_from_$(activePartition)"	"\0"

/* Trick to switch the active config */
	"switch_config_from_1="	"setenv activeConfig 2 ; saveenv"				"\0"
	
	"switch_config_from_2="	"setenv activeConfig 1 ; saveenv"				"\0"

	"switch_config="		"run switch_config_from_$(activeConfig)"		"\0"

/* Config change boot */
	"change_config="		"run switch_config set_bootcmd_normal tf1_image$(activePartition)"
																			"\0"

/* Upgrade-related variables */
	"set_bootcmd_normal="	"setenv bootcmd run tf1_image\$(activePartition) ; saveenv"		
																			"\0"

/* The user (from Linux) sets bootcmd to 'run upgrade_boot' after a new image is 
 * ready in the inactive partition. 
 */
	"set_bootcmd_recover="	"setenv bootcmd run recover_boot ; saveenv"		"\0"

/* In case the upgrade reboot failed, in the next reboot the bootcmd is 
 * 'run recover_boot', so we return to the (good) old image.
 */
 	"set_recover_flags="	"setenv recoverFlag 1 ; setenv upgradeFlag 0"	
 																			"\0"
 	
 	"recover_boot="			"run switch_active ; run set_recover_flags ; "\
 							"run set_bootcmd_normal ; boot"					"\0"

	"upgrade_boot="			"run switch_active ; run set_bootcmd_recover ;"\
							" setenv upgradeFlag 1 ; run tf1_image$(activePartition)"
																			"\0"
/* If the upgrade succeeded, the user confirms it by setting bootcmd to 
 * 'run confirm_boot' (can be done automatically) 
 */
 	"confirm_boot="			"run set_bootcmd_normal ; boot"					"\0"

/* The following 3 are read from flash and overwritten */
	"unitModel="			CP_MODEL_DEF_STR								"\0"
	
	"unitName=" 			CP_DEF_UNIT_NAME								"\0"	
		
	"unitVer="				MK_STR(CP_DEF_UNIT_VER)							"\0"

#endif /* CFG_CP_DEF_ENV */
	"\0"
};

#if defined(CFG_ENV_IS_IN_NAND)		/* Environment is in Nand Flash */
int default_environment_size = sizeof(default_environment);
#endif

void env_crc_update (void)
{
	env_ptr->crc = crc32(0, env_ptr->data, ENV_SIZE);
}

static uchar env_get_char_init (int index)
{
	DECLARE_GLOBAL_DATA_PTR;
	uchar c;

	/* if crc was bad, use the default environment */
	if (gd->env_valid)
	{
		c = env_get_char_spec(index);
	} else {
		c = default_environment[index];
	}

	return (c);
}

#ifdef CONFIG_AMIGAONEG3SE
uchar env_get_char_memory (int index)
{
	DECLARE_GLOBAL_DATA_PTR;
	uchar retval;
	enable_nvram();
	if (gd->env_valid) {
		retval = ( *((uchar *)(gd->env_addr + index)) );
	} else {
		retval = ( default_environment[index] );
	}
	disable_nvram();
	return retval;
}
#else
uchar env_get_char_memory (int index)
{
	DECLARE_GLOBAL_DATA_PTR;

	if (gd->env_valid) {
		return ( *((uchar *)(gd->env_addr + index)) );
	} else {
		return ( default_environment[index] );
	}
}
#endif

uchar *env_get_addr (int index)
{
	DECLARE_GLOBAL_DATA_PTR;

	if (gd->env_valid) {
		return ( ((uchar *)(gd->env_addr + index)) );
	} else {
		return (&default_environment[index]);
	}
}

void env_relocate (void)
{
	DECLARE_GLOBAL_DATA_PTR;
	
	DEBUGF ("%s[%d] offset = 0x%lx\n", __FUNCTION__,__LINE__,
		gd->reloc_off);

#ifdef CONFIG_AMIGAONEG3SE
	enable_nvram();
#endif

#ifdef ENV_IS_EMBEDDED
	/*
	 * The environment buffer is embedded with the text segment,
	 * just relocate the environment pointer
	 */
	env_ptr = (env_t *)((ulong)env_ptr + gd->reloc_off);
	DEBUGF ("%s[%d] embedded ENV at %p\n", __FUNCTION__,__LINE__,env_ptr);
#else
	/*
	 * We must allocate a buffer for the environment
	 */
	env_ptr = (env_t *)malloc (CFG_ENV_SIZE);
	DEBUGF ("%s[%d] malloced ENV at %p\n", __FUNCTION__,__LINE__,env_ptr);
#endif

	/*
	 * After relocation to RAM, we can always use the "memory" functions
	 */
	env_get_char = env_get_char_memory;

	if (gd->env_valid == 0) {
#if defined(CONFIG_GTH)	|| defined(CFG_ENV_IS_NOWHERE)	/* Environment not changable */
		puts ("Using default environment\n\n");
#else
		puts ("*** Warning - bad CRC, using default environment\n\n");
		SHOW_BOOT_PROGRESS (-1);
#endif

		if (sizeof(default_environment) > ENV_SIZE)
		{
			puts ("*** Error - default environment is too large\n\n");
			return;
		}

		memset (env_ptr, 0, sizeof(env_t));
		memcpy (env_ptr->data,
			default_environment,
			sizeof(default_environment));
#ifdef CFG_REDUNDAND_ENVIRONMENT
		env_ptr->flags = 0xFF;
#endif
		env_crc_update ();
		gd->env_valid = 1;
	}
	else {
#ifdef CONFIG_MARVELL
        mvMPPConfigToSPI();
		env_relocate_spec ();
        mvMPPConfigToDefault();
#else
        env_relocate_spec ();
#endif
	}
	gd->env_addr = (ulong)&(env_ptr->data);

#ifdef CONFIG_AMIGAONEG3SE
	disable_nvram();
#endif
}

#ifdef CONFIG_AUTO_COMPLETE
int env_complete(char *var, int maxv, char *cmdv[], int bufsz, char *buf)
{
	int i, nxt, len, vallen, found;
	const char *lval, *rval;

	found = 0;
	cmdv[0] = NULL;

	len = strlen(var);
	/* now iterate over the variables and select those that match */
	for (i=0; env_get_char(i) != '\0'; i=nxt+1) {

		for (nxt=i; env_get_char(nxt) != '\0'; ++nxt)
			;

		lval = (char *)env_get_addr(i);
		rval = strchr(lval, '=');
		if (rval != NULL) {
			vallen = rval - lval;
			rval++;
		} else
			vallen = strlen(lval);

		if (len > 0 && (vallen < len || memcmp(lval, var, len) != 0))
			continue;

		if (found >= maxv - 2 || bufsz < vallen + 1) {
			cmdv[found++] = "...";
			break;
		}
		cmdv[found++] = buf;
		memcpy(buf, lval, vallen); buf += vallen; bufsz -= vallen;
		*buf++ = '\0'; bufsz--;
	}

	cmdv[found] = NULL;
	return found;
}
#endif
