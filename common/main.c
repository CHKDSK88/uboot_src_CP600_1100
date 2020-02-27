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

/* #define	DEBUG	*/

#include <common.h>
#include <watchdog.h>
#include <command.h>
#ifdef CONFIG_MODEM_SUPPORT
#include <malloc.h>		/* for free() prototype */
#endif

#ifdef CFG_HUSH_PARSER
#include <hush.h>
#endif

#include <post.h>
#include <configs/CP_LONDON.h>
#include <linux/ctype.h>
#include "net.h"

#ifdef CP_LONDON_UBOOT_MENU
	extern int  cp_recoveryHandle(void); 
	extern int  cp_reset_from_usb(int* type);
	extern int  cp_reset_from_net(int* type, proto_t protocol);
	extern int  cp_abort_boot_flag(void);
	extern int  cp_usb_attempted(void);
	extern void cp_usb_set_attempted(void);
	extern int	cp_reset_file_in_memory(char* filename, int* type);
	extern int	cp_filename_like(const char* filename, const char* prefix, const char* suffix);
	// CUT-3 LED Manipulation
	extern void cp_switch_LEDs_blink(void);
	extern void cp_switch_LEDs_reset(void);
	extern void cp_switch_LEDs_green_orange(void);
	extern void cp_switch_LEDs_off(void);
	extern void cp_indicate_boot_error(void);
	extern void cp_indicate_install_success(int should_pause);
#endif

#if defined(CONFIG_BOOT_RETRY_TIME) && defined(CONFIG_RESET_TO_RETRY)
extern int do_reset (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);		/* for do_reset() prototype */
#endif

extern int do_bootd (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
#if defined(CONFIG_MARVELL)
extern unsigned int whoAmI(void);
#endif

#define MAX_DELAY_STOP_STR 32

static char * delete_char (char *buffer, char *p, int *colp, int *np, int plen);
static int parse_line (char *, char *[]);
#if defined(CONFIG_BOOTDELAY) && (CONFIG_BOOTDELAY >= 0)
static int abortboot(int);
#endif

#undef DEBUG_PARSER

char        console_buffer[CFG_CBSIZE];		/* console I/O buffer	*/

static char erase_seq[] = "\b \b";		/* erase sequence	*/
static char   tab_seq[] = "        ";		/* used to expand TABs	*/

#ifdef CONFIG_BOOT_RETRY_TIME
static uint64_t endtime = 0;  /* must be set, default is instant timeout */
static int      retry_time = -1; /* -1 so can call readline before main_loop */
#endif

#define	endtick(seconds) (get_ticks() + (uint64_t)(seconds) * get_tbclk())

#ifndef CONFIG_BOOT_RETRY_MIN
#define CONFIG_BOOT_RETRY_MIN CONFIG_BOOT_RETRY_TIME
#endif

#ifdef CONFIG_MODEM_SUPPORT
int do_mdm_init = 0;
extern void mdm_init(void); /* defined in board.c */
#endif
extern int g_stack_cachable;

extern void set_read_only_error(int i);


/***************************************************************************
 * Watch for 'delay' seconds for autoboot stop or autoboot delay string.
 * returns: 0 -  no key string, allow autoboot
 *          1 - got key string, abort
 */
#if defined(CONFIG_BOOTDELAY) && (CONFIG_BOOTDELAY >= 0)
# if defined(CONFIG_AUTOBOOT_KEYED)
static __inline__ int abortboot(int bootdelay)
{
	int abort = 0;
	uint64_t etime = endtick(bootdelay);
	struct
	{
		char* str;
		u_int len;
		int retry;
	}
	delaykey [] =
	{
		{ str: getenv ("bootdelaykey"),  retry: 1 },
		{ str: getenv ("bootdelaykey2"), retry: 1 },
		{ str: getenv ("bootstopkey"),   retry: 0 },
		{ str: getenv ("bootstopkey2"),  retry: 0 },
	};

	char presskey [MAX_DELAY_STOP_STR];
	u_int presskey_len = 0;
	u_int presskey_max = 0;
	u_int i;

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

#  ifdef CONFIG_AUTOBOOT_PROMPT
	printf (CONFIG_AUTOBOOT_PROMPT, bootdelay);
#  endif

#  ifdef CONFIG_AUTOBOOT_DELAY_STR
	if (delaykey[0].str == NULL)
		delaykey[0].str = CONFIG_AUTOBOOT_DELAY_STR;
#  endif
#  ifdef CONFIG_AUTOBOOT_DELAY_STR2
	if (delaykey[1].str == NULL)
		delaykey[1].str = CONFIG_AUTOBOOT_DELAY_STR2;
#  endif
#  ifdef CONFIG_AUTOBOOT_STOP_STR
	if (delaykey[2].str == NULL)
		delaykey[2].str = CONFIG_AUTOBOOT_STOP_STR;
#  endif
#  ifdef CONFIG_AUTOBOOT_STOP_STR2
	if (delaykey[3].str == NULL)
		delaykey[3].str = CONFIG_AUTOBOOT_STOP_STR2;
#  endif

	for (i = 0; i < sizeof(delaykey) / sizeof(delaykey[0]); i ++) {
		delaykey[i].len = delaykey[i].str == NULL ?
				    0 : strlen (delaykey[i].str);
		delaykey[i].len = delaykey[i].len > MAX_DELAY_STOP_STR ?
				    MAX_DELAY_STOP_STR : delaykey[i].len;

		presskey_max = presskey_max > delaykey[i].len ?
				    presskey_max : delaykey[i].len;

#  if DEBUG_BOOTKEYS
		printf("%s key:<%s>\n",
		       delaykey[i].retry ? "delay" : "stop",
		       delaykey[i].str ? delaykey[i].str : "NULL");
#  endif
	}

	/* In order to keep up with incoming data, check timeout only
	 * when catch up.
	 */
	while (!abort && get_ticks() <= etime) {
		for (i = 0; i < sizeof(delaykey) / sizeof(delaykey[0]); i ++) {
			if (delaykey[i].len > 0 &&
			    presskey_len >= delaykey[i].len &&
			    memcmp (presskey + presskey_len - delaykey[i].len,
				    delaykey[i].str,
				    delaykey[i].len) == 0) {
#  if DEBUG_BOOTKEYS
				printf("got %skey\n",
				       delaykey[i].retry ? "delay" : "stop");
#  endif

#  ifdef CONFIG_BOOT_RETRY_TIME
				/* don't retry auto boot */
				if (! delaykey[i].retry)
					retry_time = -1;
#  endif
				abort = 1;
			}
		}

		if (tstc()) {
			if (presskey_len < presskey_max) {
				presskey [presskey_len ++] = getc();
			}
			else {
				for (i = 0; i < presskey_max - 1; i ++)
					presskey [i] = presskey [i + 1];

				presskey [i] = getc();
			}
		}
	}
#  if DEBUG_BOOTKEYS
	if (!abort)
		puts ("key timeout\n");
#  endif

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

# else	/* !defined(CONFIG_AUTOBOOT_KEYED) */

#ifdef CONFIG_MENUKEY
static int menukey = 0;
#endif

#define KEYCOMB /* Ctrl + C Interrupt*/

static __inline__ int abortboot(int bootdelay)
{
	int abort = 0;
	int has_delay = 0;
#ifdef KEYCOMB
    char keyCmb;
#endif

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
#else

	if (cp_abort_boot_flag())
	{
		abort = 1;
		has_delay = 0;
	}
	else
	{
		has_delay = (bootdelay > 0) ? 1 : 0;
	}
	
	if (has_delay)
	{
		printf("Hit \'Ctrl + C\' key to stop autoboot: %2d ", bootdelay); /* Comment out*/
	}
#endif

#if defined CONFIG_ZERO_BOOTDELAY_CHECK
	/*
	 * Check if key already pressed
	 * Don't check if bootdelay < 0
	 */
	if (bootdelay >= 0) {
		if (tstc()) {	/* we got a key press	*/
#ifdef KEYCOMB
            keyCmb = getc();
            if ( keyCmb == 0x03 ) {
				puts ("\b\b\b 0");
				abort = 1; 	/* Ctrl + C , don't auto boot */
            }
#else
			(void) getc();  /* consume input	*/
			puts ("\b\b\b 0");
			abort = 1; 	/* don't auto boot	*/
#endif
		}
	}
#endif

	while ((bootdelay > 0) && (!abort)) {
		int i;

		--bootdelay;
		/* delay 100 * 10ms */
		for (i=0; !abort && i<100; ++i) {
			if (tstc()) {	/* we got a key press	*/
# ifdef CONFIG_MENUKEY
				abort  = 1;	/* don't auto boot	*/
				bootdelay = 0;	/* no more delay	*/
				menukey = getc();
# else
#ifdef KEYCOMB
                keyCmb = getc();
                if ( keyCmb == 0x03 ) {
                    abort  = 1;	/* don't auto boot	*/
                    bootdelay = 0;	/* no more delay	*/
                    break;
                }
#else
                abort  = 1;	/* don't auto boot	*/
                bootdelay = 0;	/* no more delay	*/
				(void) getc();  /* consume input	*/
				break;
#endif
#endif

			}
			udelay (10000);
		}

		if (has_delay)
		{
			printf ("\b\b\b%2d ", bootdelay);
		}
	}

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

# endif	/* CONFIG_AUTOBOOT_KEYED */
#endif	/* CONFIG_BOOTDELAY >= 0  */

/****************************************************************************/

#ifdef CP_REQUIRE_ACCESS_CODE

int cp_verify_access_code(int access_code_retries)
{
	char access_code[UBOOT_ACCESS_CODE_MAX_LEN];
	char _c = '\0';
	int	_i = 0;
	long int code_crc = 0;

	printf("\n\nPlease enter access code: (press ENTER to finish)\n\n\tAccess Code: ");
	memset(access_code, '\0', UBOOT_ACCESS_CODE_MAX_LEN);

	while (( _c = getc()) != ASCII_enter)
	{
		if (_i < UBOOT_ACCESS_CODE_MAX_LEN - 1)
		{
			access_code[_i] = _c;
			putc('*');
		}		
		_i++;
	}

	code_crc = crc32(0, (u_char*)access_code,	strlen(access_code));
	if (code_crc != UBOOT_ACCESS_CODE_CRC)
	{
		printf("\n\nAccess Code incorrect\n");
	}
	if (access_code_retries >= UBOOT_ACCESS_CODE_REBOOT)
	{
		printf("\nRebooting the device\n\n");
		run_command("reset", 0);
	}
	if ((access_code_retries % UBOOT_ACCESS_CODE_MAX_FAILURE) == 0)
	{
		printf("\nIncorrect Access Code entered. Please wait %d seconds\n", UBOOT_ACCESS_CODE_DELAY);
		udelay(UBOOT_ACCESS_CODE_DELAY * 1000000);
	}
	return (code_crc == UBOOT_ACCESS_CODE_CRC);
}
#endif /* CP_REQUIRE_ACCESS_CODE */	

#define stringify(s)	tostring(s)
#define tostring(s)	#s

static int cp_confirm_menu(void)
{
	char cp_rfd_yn=0;
	while (1)
	{
		printf ("\nAre you sure? (y/n) ");
		cp_rfd_yn=getc();
		switch(cp_rfd_yn)
		{
			case 'y':
				printf("\n\n");
				return 1;
				break;
			case 'n':
				printf("\n\n");
				return 0;
				break;
			default :  continue;
		}
	} 
}

#define is_digit(c)	((c) >= '0' && (c) <= '9')
#define is_dot(c)	((c) == '.')


/*
 * This function gets a string with an IP and verifies it is a valid IP address
 */

static int cp_is_valid_ip_string(const char* ip_string)
{
	static const char rname[] = "cp_is_valid_ip_string";
	ulong  a[4];
	char*  _p[3];
	int	ip_str_len = 0;
	const char* _s;
	char* _e;
	int _i, _j;

	a[0] = a[1] = a[2] = a[3] = -1;
	
	if ((ip_str_len = strlen(ip_string)) < sizeof("1.1.1.1")-1)
	{
		debug("%s: String is too short to be an IP string\n", rname);
		return 0;
	}

	/* Parse string */
	
	/* Must start with gidit */
	if (!is_digit(ip_string[0]))
	{
		debug("%s: First character not digit: %c\n", rname, ip_string[0]);
		return 0;
	}

	/* Must end with gidit */
	if (!is_digit(ip_string[ip_str_len-1]))
	{
		debug("%s: Last character not digit: %c\n", rname, ip_string[ip_str_len]);
		return 0;
	}

	/* Character must be digits or dots */
	for (_i=0, _j=0; _i < ip_str_len; _i++)
	{
		if (is_dot(ip_string[_i]))
		{
			if (_j > 2)
			{
				debug("%s: Found more than 3 dots\n", rname);
				return 0;
			}
			_p[_j] = &ip_string[_i];
			if ((_j > 0) && (_p[_j] == _p[_j-1]+1))
			{
				debug("%s: Found consecutive dots\n", rname);
				return 0;
			}
			_j++;
		} else if (!is_digit(ip_string[_i]))
		{
			debug("%s: Illegal character at index %d: %c\n", rname, _i, ip_string[_i]);
			return 0;
		}
	}
	if (_j != 3)
	{
		debug("%s: Less than 3 dots\n", rname);
		return 0;
	}
	
	_s = ip_string;
	
	for (_i=0; _i<4; _i++) {
		a[_i] = _s ? simple_strtoul(_s, &_e, 10) : 0;
		debug("%s: Found val: %u (%d)\n", rname, a[_i], _i);

		if (_s) {
			_s = (*_e) ? _e+1 : _e;
		}
	}

	if (a[0] > 255 || a[1] > 255 || a[2] > 255 || a[3] > 255)
	{
		debug("%s: Invalid IP string '%s' %u.%u.%u.%u\n", rname, ip_string, 
													a[0], a[1], a[2], a[3]);
		return 0;
	}
	return 1;
}

#define CP_LOCAL_BUF_SIZE 256
#define TRY_AGAIN printf("\n\tPlease try again (Ctrl+C to quit)"); goto TryAgain;

int cp_read_ip_addr(char *ip_str, u_int ip_str_len,const char* default_ip)
{
	int bytes_read = 0, rc = 0;
	char local_buff[CP_LOCAL_BUF_SIZE];

TryAgain:
	memset(local_buff, '\0', CP_LOCAL_BUF_SIZE);
	sprintf(local_buff, " (default: %s): ", default_ip ? default_ip : "No Default" );

	bytes_read = readline(local_buff);

	/* No input */
	if (bytes_read < 0)
	{
		rc = -1;
		goto CleanUp;
	}
	else if(bytes_read == 0)
	{
		/* Set the default ip */
		if (strlen(default_ip) < ip_str_len)
		{
			printf("\tUsing default IP (%s)\n", default_ip);
			sprintf(ip_str, default_ip);
			bytes_read = strlen(ip_str);
		}
		else
		{
			printf("\n\tERROR: Illegal IP (%s) - string too long\n", default_ip);
			TRY_AGAIN
		}
	}
	else
	{
		/* Set the read ip */
		if (bytes_read < ip_str_len)
		{
			sprintf(ip_str, "%s",console_buffer);
		}
		else
		{
			printf("\n\tERROR: Illegal IP (%s) - string too long\n", console_buffer);
			TRY_AGAIN
		}		
	}

	if (bytes_read >= 0)
	{
		if (!cp_is_valid_ip_string(ip_str))
		{
			printf("\tError: %s is not a valid address\n", ip_str);
			TRY_AGAIN
		}
	}
	
CleanUp:
	return rc;
}
int cp_read_filename(char* buf, u_int buf_len)
{
	int bytes_read = 0;
	int rc = 0;

TryAgain:
	bytes_read = readline(": ");
	debug("Bytes read: %u\n", bytes_read);
	
	/* No input */
	if (bytes_read < 0)
	{
		return -1;
	}
	else if (bytes_read == 0)
	{
		TRY_AGAIN;
	}
	else
	{
		/* Set the filename */
		if (bytes_read < buf_len)
		{
			sprintf(buf, "%s",console_buffer);
		}
		else
		{
			printf("\n\tERROR: File name string too long (max: %u)\n", buf_len-1);
			TRY_AGAIN
		}		
	}

	if ( ! cp_filename_like(buf, "dsl", CP_IMAGE_SUFFIX) &&
		 ! cp_filename_like(buf, "", ".cfg") &&
		 ! cp_filename_like(buf, "", ".bls") )	 
	{
		printf("\n\tERROR: Illegal file name: %s\n", buf);
		printf("\tFile name format should be: dsl*.img OR *.cfg \n");
		TRY_AGAIN
	}
	return rc;
}

int cp_read_boot_filename(char* buf, u_int buf_len)
{
	int bytes_read = 0;
	int rc = 0;

TryAgain:
	bytes_read = readline(": ");
	debug("Bytes read: %u\n", bytes_read);
	
	/* No input */
	if (bytes_read < 0)
	{
		return -1;
	}
	else if (bytes_read == 0)
	{
		TRY_AGAIN;
	}
	else
	{
		/* Set the filename */
		if (bytes_read < buf_len)
		{
			sprintf(buf, "%s",console_buffer);
		}
		else
		{
			printf("\n\tERROR: File name string too long (max: %u)\n", buf_len-1);
			TRY_AGAIN
		}		
	}

	if ( ! cp_filename_like(buf, CP_IMAGE_PREFIX, CP_IMAGE_SUFFIX) &&
		 ! cp_filename_like(buf, CP_UBOOT_PREFIX, CP_UBOOT_SUFFIX) &&
		 ! cp_filename_like(buf, CP_HW_TEST_PREFIX, CP_HW_TEST_SUFFIX))
	{
		printf("\n\tERROR: Illegal file name: %s\n", buf);
		printf("\tFile name format should be: \""	CP_IMAGE_PREFIX"*"CP_IMAGE_SUFFIX"\" OR "
										"\""CP_UBOOT_PREFIX"*"CP_UBOOT_SUFFIX"\"\n");
		TRY_AGAIN
	}
	return rc;
}


int cp_network_install_finalize(int install_type)
{
	char *install_from = CP_IMAGE_UBOOT_INSTALL_FROM;
	
	if (install_type == CP_GET_TYPE_UBOOT)
	{
		install_from = CP_UBOOT_INSTALL_FROM;								
	}
	else if (install_type == CP_GET_TYPE_FIRMWARE)
	{
		install_from = CP_IMAGE_INSTALL_FROM;
	}
	printf("\n%s Network succeeded.\n", install_from);
	cp_indicate_install_success(1);
	
	if (install_type == CP_GET_TYPE_FIRMWARE)
	{
		restart_pci();
		run_command ("run bootImage",0);
	}
	else if (install_type == CP_GET_TYPE_UBOOT)
	{
		run_command ("reset",0);
	}

	printf("Installation type unknown: %d\n", install_type);
	return -1; /* Never reached */
}

int cp_network_install_handler(void)
{
	//char local_buff[CP_LOCAL_BUF_SIZE];
	char ipaddr[CP_IP_STR_LEN];
	char serverip[CP_IP_STR_LEN];
	char filename[CP_MAX_FILENAME_LEN];
	char *install_from = CP_IMAGE_UBOOT_INSTALL_FROM;
	int ret = 0, rc = 0;
	int stop_menu = 0;
	int menu_option= 0;
	int install_type = 0;
	char _c;
	
	memset(ipaddr,	'\0', CP_IP_STR_LEN);
	memset(serverip,'\0', CP_IP_STR_LEN);
	memset(filename,'\0', CP_MAX_FILENAME_LEN);	

//	if (! cp_is_network_enabled()) {
//		cp_enable_network_ports();
//	}
	while (!stop_menu)
	{
		printf("\n\nPlease select installation type:\n\n");
		printf("\t"stringify(CP_NET_INSTALL_AUTO)
				". Automatic\t- "CP_IMAGE_UBOOT_INSTALL_FROM" BOOTP server\n");
		printf("\t"stringify(CP_NET_INSTALL_MAN)
				". Manual\t- "CP_IMAGE_UBOOT_INSTALL_FROM" tftp server\n");
		printf(CP_ENTER_SELECTION_STR);
		
		_c = getc();

		switch (_c)
		{
		case (ASCII_0+CP_NET_INSTALL_AUTO):
			menu_option = CP_NET_INSTALL_AUTO;
			stop_menu = 1;
			break;
			
		case (ASCII_0+CP_NET_INSTALL_MAN):
			menu_option = CP_NET_INSTALL_MAN;
			stop_menu = 1;
			break;

		case ASCII_ctrlC:
			stop_menu = 1;
			break;

		default:
			continue;
		}
	}

	switch (menu_option)
	{
	case CP_NET_INSTALL_AUTO:
		printf ("\n\nGoing to %s Network ... \n\n", install_from);
	
		if (cp_confirm_menu())
		{
			ret = cp_reset_from_net(&install_type, DHCP);
			
			if ( ret <= 0 ) /* <=, not just < */
			{
				printf("\n\tERROR: %s Network failed.\n\n", install_from);
				cp_indicate_boot_error();
//				if (cp_is_network_enabled()) {
//					cp_disable_network_ports();
//				}
				
				return -1;
			}
			else if ( ret > 0)
			{
				ret = cp_network_install_finalize(install_type);
				if (ret < 0)
				{
					cp_indicate_boot_error();
					rc = -1;
					goto CleanUp;
				}
				/* Finished successfully */
				rc = 0;
				goto CleanUp;
			}
		}
		break;

	case CP_NET_INSTALL_MAN:
		/* Read IP */
		printf("\n\n\tPlease enter local IP address");
		ret = cp_read_ip_addr(ipaddr,	CP_IP_STR_LEN, getenv("ipaddr"));
		if (ret < 0)
		{
			rc = -1;
			goto CleanUp;
		}
		
		/* Read Server IP */
		printf("\n\n\tPlease enter server IP address");
		ret = cp_read_ip_addr(serverip,	CP_IP_STR_LEN, getenv("serverip"));
		if (ret < 0)
		{
			rc = -1;
			goto CleanUp;
		}
		
		/* Read filename */
		printf("\n\tPlease enter file name");
		ret = cp_read_boot_filename(filename ,CP_MAX_FILENAME_LEN);
		if (ret < 0)
		{
			rc = -1;
			goto CleanUp;
		}

		printf("Going to run tftp\n");
		printf( "\tFile       : %s\n"				
				"\tIP Address : %s\n"
				"\tServer IP  : %s\n", 
				filename, ipaddr, serverip);
		
		if (cp_confirm_menu())
		{
			setenv("ipaddr", ipaddr);
			setenv("serverip", serverip);

			//ret = run_command(local_buff, 0);
			strcpy(BootFile, filename);
			ret = cp_reset_from_net(&install_type, TFTP);
			
			if (ret < 0)
			{
				printf("\n\tERROR: %s tftp failed.\n\n", install_from);
				cp_indicate_boot_error();
				rc = -1;
				goto CleanUp;
			}

			ret = cp_network_install_finalize(install_type);
			if (ret < 0)
			{
				cp_indicate_boot_error();
				rc = -1;
				goto CleanUp;
			}
			/* Finished successfully */
			rc = 0;
			goto CleanUp;
		}
		else
		{
			printf("%s Network aborted\n", install_from);
			rc = -1;
			goto CleanUp;
		}
		break;
		
	default:
		{
			printf("%s Network aborted\n", install_from);
		}
	}
CleanUp:
//	if (cp_is_network_enabled()) {
//		cp_disable_network_ports();
//	}

	return rc;
}

void restart_pci(void) {
#ifdef SEATTLE
	run_command("pci restart", 0);
#endif
}


void main_loop (void)
{
#ifndef CFG_HUSH_PARSER
	static char lastcommand[CFG_CBSIZE] = { 0, };
	int len;
	int rc = 1;
	int flag;
#endif

#ifdef CP_LONDON_UBOOT_MENU
       char cp_menu_key=0;
       setenv ("cp_quiet", CP_BOOT_MODE_QUIET_STR);
	   setenv ("cp_boot_mode", stringify(CP_BOOT_MODE_QUIET));
	   char *install_from = NULL;
	   int install_type = 0;
#endif 


#if defined(CONFIG_BOOTDELAY) && (CONFIG_BOOTDELAY >= 0)
	char *s;
	int bootdelay;
#endif
#ifdef CONFIG_PREBOOT
	char *p;
#endif
#ifdef CONFIG_BOOTCOUNT_LIMIT
	unsigned long bootcount = 0;
	unsigned long bootlimit = 0;
	char *bcs;
	char bcs_set[16];
#endif /* CONFIG_BOOTCOUNT_LIMIT */

#if defined(CONFIG_MARVELL)
    if (g_stack_cachable == 1)
    {/* set the stack to be cachable */
                __asm__ __volatile__ ( " orr  sp, sp, #0x80000000 " :  );
    }
#endif /* CONFIG_MARVELL */
#if defined(CONFIG_VFD) && defined(VFD_TEST_LOGO)
	ulong bmp = 0;		/* default bitmap */
	extern int trab_vfd (ulong bitmap);



#ifdef CONFIG_MODEM_SUPPORT
	if (do_mdm_init)
		bmp = 1;	/* alternate bitmap */
#endif
	trab_vfd (bmp);
#endif	/* CONFIG_VFD && VFD_TEST_LOGO */

#ifdef CONFIG_BOOTCOUNT_LIMIT
	bootcount = bootcount_load();
	bootcount++;
	bootcount_store (bootcount);
	sprintf (bcs_set, "%lu", bootcount);
	setenv ("bootcount", bcs_set);
	bcs = getenv ("bootlimit");
	bootlimit = bcs ? simple_strtoul (bcs, NULL, 10) : 0;
#endif /* CONFIG_BOOTCOUNT_LIMIT */

#ifdef CONFIG_MODEM_SUPPORT
	debug ("DEBUG: main_loop:   do_mdm_init=%d\n", do_mdm_init);
	if (do_mdm_init) {
		char *str = strdup(getenv("mdm_cmd"));
		setenv ("preboot", str);  /* set or delete definition */
		if (str != NULL)
			free (str);
		mdm_init(); /* wait for modem connection */
	}
#endif  /* CONFIG_MODEM_SUPPORT */

#ifdef CONFIG_VERSION_VARIABLE
	{
		extern char version_string[];

		setenv ("ver", version_string);  /* set version variable */
	}
#endif /* CONFIG_VERSION_VARIABLE */

#ifdef CFG_HUSH_PARSER
	u_boot_hush_start ();
#endif

#ifdef CONFIG_AUTO_COMPLETE
	install_auto_complete();
#endif

#ifdef CONFIG_PREBOOT
	if ((p = getenv ("preboot")) != NULL) {
# ifdef CONFIG_AUTOBOOT_KEYED
		int prev = disable_ctrlc(1);	/* disable Control C checking */
# endif

# ifndef CFG_HUSH_PARSER
		run_command (p, 0);
# else
		parse_string_outer(p, FLAG_PARSE_SEMICOLON |
				    FLAG_EXIT_FROM_LOOP);
# endif

# ifdef CONFIG_AUTOBOOT_KEYED
		disable_ctrlc(prev);	/* restore Control C checking */
# endif
	}
#endif /* CONFIG_PREBOOT */

#if defined(CONFIG_BOOTDELAY) && (CONFIG_BOOTDELAY >= 0)
	s = getenv ("bootdelay");
	bootdelay = s ? (int)simple_strtol(s, NULL, 10) : CONFIG_BOOTDELAY;

	debug ("### main_loop entered: bootdelay=%d\n\n", bootdelay);

# ifdef CONFIG_BOOT_RETRY_TIME
	init_cmd_timeout ();
# endif	/* CONFIG_BOOT_RETRY_TIME */

#ifdef CONFIG_BOOTCOUNT_LIMIT
	if (bootlimit && (bootcount > bootlimit)) {
		printf ("Warning: Bootlimit (%u) exceeded. Using altbootcmd.\n",
		        (unsigned)bootlimit);
		s = getenv ("altbootcmd");
	}
	else
#endif /* CONFIG_BOOTCOUNT_LIMIT */
#if defined(CONFIG_MARVELL)
#ifdef MV78XX0
	    if (whoAmI() == 0)
#endif
	    {
		s = getenv ("bootcmd");
	    }
#ifdef MV78200
	    else
	    {
		s = getenv ("bootcmd2");
	    }
#endif
#else
		s = getenv ("bootcmd");
#endif

	debug ("### main_loop: bootcmd=\"%s\"\n", s ? s : "<UNDEFINED>");

#if defined (CONFIG_MARVELL) && (defined(MV_88F6183) || defined(MV_88F6183L))
	/* 6183 UART work around - need incase uart pin's left unconnected */
	if (tstc())
		(void) getc();  /* consume input	*/
#endif
#if defined (RD_88F6281A_SHEEVA_PLUG)
        (*((volatile unsigned int*)(0xf1010140))) &= (~0x20000);
#endif
	if (bootdelay >= 0 && s && !abortboot (bootdelay)) {
# ifdef CONFIG_AUTOBOOT_KEYED
		int prev = disable_ctrlc(1);	/* disable Control C checking */
# endif

# ifndef CFG_HUSH_PARSER
		//printf ("%s:%d before run_command \n",__FILE__,__LINE__); 
		restart_pci();
		run_command (s, 0);
# else
		parse_string_outer(s, FLAG_PARSE_SEMICOLON |
				    FLAG_EXIT_FROM_LOOP);
# endif

# ifdef CONFIG_AUTOBOOT_KEYED
		disable_ctrlc(prev);	/* restore Control C checking */
# endif
	}

# ifdef CONFIG_MENUKEY
	if (menukey == CONFIG_MENUKEY) {
	    s = getenv("menucmd");
	    if (s) {
# ifndef CFG_HUSH_PARSER
		run_command (s, 0);
# else
		parse_string_outer(s, FLAG_PARSE_SEMICOLON |
				    FLAG_EXIT_FROM_LOOP);
# endif
	    }
	}
#endif /* CONFIG_MENUKEY */
#endif	/* CONFIG_BOOTDELAY */

#ifdef CP_LONDON_UBOOT_MENU
	{
	int ret = 0;
	
	         
        int cp_stop_menu=0;
		int access_code_retries = 0;
		char ipaddr[CP_IP_STR_LEN];
		char serverip[CP_IP_STR_LEN];
		char filename[CP_MAX_FILENAME_LEN];

		cp_switch_LEDs_reset();  /* For the case where reset button is pressed, 
									* and then user hits 'Ctrl+C'
									*/	
		/* Stop boot blinking (started at serial_init() */
		/* Power - GREEN Static. Alert Off */
		cp_GPIO_LED_set(POWER_LED_GREEN,	LED_CMD_ON);
		cp_GPIO_LED_set(POWER_LED_GREEN,	LED_CMD_STATIC);

		cp_GPIO_LED_set(POWER_LED_RED,		LED_CMD_OFF);
		cp_GPIO_LED_set(POWER_LED_RED,		LED_CMD_STATIC);
		
		cp_GPIO_LED_set(ALERT_LED_GREEN,	LED_CMD_OFF);
		cp_GPIO_LED_set(ALERT_LED_GREEN,	LED_CMD_STATIC);
				
		cp_GPIO_LED_set(ALERT_LED_RED,		LED_CMD_OFF);
		cp_GPIO_LED_set(ALERT_LED_RED,		LED_CMD_STATIC);
									
		while (!cp_stop_menu)
		{
			printf("\n\nWelcome to Gaia Embedded Boot Menu:\n\n");
			printf("\t1. Start in normal Mode\n");
			printf("\t2. Start in debug Mode\n");
			printf("\t3. Start in maintenance Mode\n");
			printf("\t4. Restore to Factory Defaults (local)\n");
			printf("\t5. " CP_IMAGE_UBOOT_INSTALL_FROM	" Network\n");
			printf("\t6. " CP_IMAGE_INSTALL_FROM 		" USB\n");
			printf("\t7. " CP_UBOOT_INSTALL_FROM		" USB\n");
			printf("\t8. Restart Boot-Loader\n");
#ifdef SEATTLE
			printf("\t9. Install DSL Firmware / Upload preset configuration file\n");			
#endif
			
			printf(CP_ENTER_SELECTION_STR);
			cp_menu_key=getc();
			
			switch (cp_menu_key)    	
			{
				case '0':
#ifdef CP_REQUIRE_ACCESS_CODE
					access_code_retries++;
					if (cp_verify_access_code(access_code_retries))
					{
						access_code_retries = 0;
						cp_stop_menu=1;
						break;
					}
					continue;
#else
					cp_stop_menu=1;
					break;
#endif /* CP_REQUIRE_ACCESS_CODE */
					
				case '1': printf ("\n\nGoing to boot from local image ...\n\n");
					restart_pci();
					run_command("boot", 0);
					break;
					
				case '2': printf ("\n\nGoing to boot from local image (Debug Mode)...\n\n");
					setenv ("cp_quiet", CP_BOOT_MODE_DEBUG_STR);
					setenv ("cp_boot_mode", stringify(CP_BOOT_MODE_DEBUG));
					restart_pci();
					run_command("boot", 0);
					break;

				case '3': printf ("\n\nGoing to boot from local image (Maintenance Mode)...\n\n");
					setenv ("cp_quiet", CP_BOOT_MODE_MAINTENANCE_STR);
					setenv ("cp_boot_mode", stringify(CP_BOOT_MODE_MAINTENANCE));
					restart_pci();
					run_command("boot", 0);
					break;
					
				case '4': printf ("\n\nGoing to run Restore to Factory Defaults ... \n\n"); 
					if (cp_confirm_menu())
					{
						cp_recoveryHandle();
						/* User specifically requested to restore to factory defaults image.
						 * If cp_recoveryHandle succeeds system will boot and we never get here 
						 * If cp_recoveryHandle returned - an error message is needed
						 * This indicates a serious system error
						 */
						printf("\n\tERROR: Restore to Factory Defaults failed.\n\n");
						continue;
					}
					else
					{
						printf("Restore to Factory defaults aborted\n");
					}
					break;
					
				case '5':
					cp_network_install_handler();

				    break;
					
				case '6': 
				case '7':
					install_from = (cp_menu_key == '6') ?
											CP_IMAGE_INSTALL_FROM :
											CP_UBOOT_INSTALL_FROM ;
					install_type = (cp_menu_key == '6') ?
								CP_GET_TYPE_FIRMWARE :
								CP_GET_TYPE_UBOOT;
							
					printf ("\n\nGoing to %s USB ... \n\n", install_from);
					if (cp_confirm_menu())
					{
						if ( ! cp_usb_attempted() )
						{
							ret = cp_reset_from_usb(&install_type);
							
							/* Set indication that USB was attemted, and was unsuccessful (Otherwise 
							 * system would have booted, and this line is not reached)
							 *
							 * Due to USB limitation, this will later mean that in order to load USB 
							 * again from uboot, a reset is needed
							 */
							cp_usb_set_attempted();
							if ( ret <= 0 ) /* <=, not just < */
						    {
						        printf("\n\tERROR: %s USB failed.\n\n", install_from);
								cp_indicate_boot_error();
						    }
							else if ( ret > 0)
							{
								printf("\n%s USB succeeded.\n", install_from);
								printf("\nPlease remove USB device.\n");
								cp_indicate_install_success(1);
								if (install_type == CP_GET_TYPE_FIRMWARE)
								{
									restart_pci();
									run_command ("run bootImage",0);
								}
								else
								{
									run_command ("reset",0);
								}
							}
						}
						else
						{
							printf("\n\tERROR: USB operation failed before.");
							printf("You must restart Boot-Loader to attempt using USB again\n\n");
						}
						continue;
					}
					else
					{
						printf("%s USB aborted\n", install_from);
					}
					break;
		    case '8':
			    printf ("\n\nGoing to restart Boot-Loader ... \n\n");
			    if (cp_confirm_menu())
			    {
				    run_command("reset", 0);
				    /* This should NEVER happen... */
				    printf("\n\tERROR: Boot-Loader restart failed. Use reboot button or remove power cable to try again\n\n");
				    continue;
			    }
			    else
			    {
				    printf("Boot-Loader restart aborted\n");
			    }
		        break;
#ifdef SEATTLE
	                case '9':
	                        /* Read IP */	
		                printf("\n\n\tPlease enter local IP address");
		                ret = cp_read_ip_addr(ipaddr,	CP_IP_STR_LEN, getenv("ipaddr"));
		                if (ret < 0)
		                {
			                rc = -1;
			                return rc;
		                }
                
		                /* Read Server IP */
		                printf("\n\n\tPlease enter server IP address");
		                ret = cp_read_ip_addr(serverip,	CP_IP_STR_LEN, getenv("serverip"));
		                if (ret < 0)
		                {
			                rc = -1;
			                return rc;
		                }
                
		                /* Read filename */
		                printf("\n\tPlease enter file name");
		                ret = cp_read_filename(filename ,CP_MAX_FILENAME_LEN);
		                if (ret < 0)
		                {
			                rc = -1;
			                return rc;
		                }
		                printf("Going to run tftp\n");
		                printf( "\tFile       : %s\n"
				                "\tIP Address : %s\n"
				                "\tServer IP  : %s\n",
				                filename, ipaddr, serverip);
		                if (cp_confirm_menu())
		                {
			                setenv("ipaddr", ipaddr);
			                setenv("serverip", serverip);
			                strcpy(BootFile, filename);			
		                }
		                else 
			                return 0;
		                if (strstr(filename,"img"))
			                burn_dsl_firmware(filename);
						else if (strstr(filename,"cfg"))
							burn_preset_cfg(filename);
						else if (strstr(filename,"bls"))
						{
							burn_blob_cfg(filename);
							printf("Press any key to continue...\n");
							getc(); /* Wait for any key */
							cp_GPIO_LED_set(ALERT_LED_GREEN,LED_CMD_STATIC);
							cp_GPIO_LED_set(ALERT_LED_GREEN,LED_CMD_OFF);
							cp_GPIO_LED_set(ALERT_LED_RED,LED_CMD_STATIC);
							cp_GPIO_LED_set(ALERT_LED_RED,LED_CMD_OFF);
							set_read_only_error(0);
						}					    			 
						break;
#endif				
				default :
					printf ("\n\nUnknown option - %c - \n\n", 
								isprint(cp_menu_key) ? cp_menu_key : '?'); 
				continue;
			}
		}
		printf ("\n\n");
	}
#endif  /*  CP_LONDON_UBOOT_MENU */


#ifdef CONFIG_AMIGAONEG3SE
	{
	    extern void video_banner(void);
	    video_banner();
	}
#endif

    /*Password Support*/
    run_command ("passwd", 0);

	/*
	 * Main Loop for Monitor Command Processing
	 */
#ifdef CFG_HUSH_PARSER
	parse_file_outer();
	/* This point is never reached */
	for (;;);
#else
	for (;;) {
#ifdef CONFIG_BOOT_RETRY_TIME
		if (rc >= 0) {
			/* Saw enough of a valid command to
			 * restart the timeout.
			 */
			reset_cmd_timeout();
		}
#endif
		//printf ("%s:%d before the readline \n",__FILE__,__LINE__); 
		len = readline (CFG_PROMPT);

		flag = 0;	/* assume no special flags for now */
		if (len > 0)
			strcpy (lastcommand, console_buffer);
		else if (len == 0)
			flag |= CMD_FLAG_REPEAT;
#ifdef CONFIG_BOOT_RETRY_TIME
		else if (len == -2) {
			/* -2 means timed out, retry autoboot
			 */
			puts ("\nTimed out waiting for command\n");
# ifdef CONFIG_RESET_TO_RETRY
			/* Reinit board to run initialization code again */
			do_reset (NULL, 0, 0, NULL);
# else
			return;		/* retry autoboot */
# endif
		}
#endif

		if (len == -1)
			puts ("<INTERRUPT>\n");
		else
			//printf ("%s:%d before run_command - going to run %s \n",__FILE__,__LINE__,lastcommand); 
			rc = run_command (lastcommand, flag);

		if (rc <= 0) {
			/* invalid command or not repeatable, forget it */
			lastcommand[0] = 0;
		}
	}
#endif /*CFG_HUSH_PARSER*/
}

#ifdef CONFIG_BOOT_RETRY_TIME
/***************************************************************************
 * initialise command line timeout
 */
void init_cmd_timeout(void)
{
	char *s = getenv ("bootretry");

	if (s != NULL)
		retry_time = (int)simple_strtol(s, NULL, 10);
	else
		retry_time =  CONFIG_BOOT_RETRY_TIME;

	if (retry_time >= 0 && retry_time < CONFIG_BOOT_RETRY_MIN)
		retry_time = CONFIG_BOOT_RETRY_MIN;
}

/***************************************************************************
 * reset command line timeout to retry_time seconds
 */
void reset_cmd_timeout(void)
{
	endtime = endtick(retry_time);
}
#endif

/****************************************************************************/

/*
 * Prompt for input and read a line.
 * If  CONFIG_BOOT_RETRY_TIME is defined and retry_time >= 0,
 * time out when time goes past endtime (timebase time in ticks).
 * Return:	number of read characters
 *		-1 if break
 *		-2 if timed out
 */
int readline (const char *const prompt)
{
	char   *p = console_buffer;
	int	n = 0;				/* buffer index		*/
	int	plen = 0;			/* prompt length	*/
	int	col;				/* output column cnt	*/
	char	c;

	/* print prompt */
	if (prompt) {
		plen = strlen (prompt);
		puts (prompt);
	}
	col = plen;

	for (;;) {
#ifdef CONFIG_BOOT_RETRY_TIME
		while (!tstc()) {	/* while no incoming data */
			if (retry_time >= 0 && get_ticks() > endtime)
				return (-2);	/* timed out */
		}
#endif
		WATCHDOG_RESET();		/* Trigger watchdog, if needed */

#ifdef CONFIG_SHOW_ACTIVITY
		while (!tstc()) {
			extern void show_activity(int arg);
			show_activity(0);
		}
#endif
		c = getc();

		/*
		 * Special character handling
		 */
		switch (c) {
		case '\r':				/* Enter		*/
		case '\n':
			*p = '\0';
			puts ("\r\n");
			return (p - console_buffer);

		case '\0':				/* nul			*/
			continue;

		case 0x03:				/* ^C - break		*/
			console_buffer[0] = '\0';	/* discard input */
			return (-1);

		case 0x15:				/* ^U - erase line	*/
			while (col > plen) {
				puts (erase_seq);
				--col;
			}
			p = console_buffer;
			n = 0;
			continue;

		case 0x17:				/* ^W - erase word 	*/
			p=delete_char(console_buffer, p, &col, &n, plen);
			while ((n > 0) && (*p != ' ')) {
				p=delete_char(console_buffer, p, &col, &n, plen);
			}
			continue;

		case 0x08:				/* ^H  - backspace	*/
		case 0x7F:				/* DEL - backspace	*/
			p=delete_char(console_buffer, p, &col, &n, plen);
			continue;

		default:
			/*
			 * Must be a normal character then
			 */
			if (n < CFG_CBSIZE-2) {
				if (c == '\t') {	/* expand TABs		*/
#ifdef CONFIG_AUTO_COMPLETE
					/* if auto completion triggered just continue */
					*p = '\0';
					if (cmd_auto_complete(prompt, console_buffer, &n, &col)) {
						p = console_buffer + n;	/* reset */
						continue;
					}
#endif
					puts (tab_seq+(col&07));
					col += 8 - (col&07);
				} else {
					++col;		/* echo input		*/
					putc (c);
				}
				*p++ = c;
				++n;
			} else {			/* Buffer full		*/
				putc ('\a');
			}
		}
	}
}

/****************************************************************************/

static char * delete_char (char *buffer, char *p, int *colp, int *np, int plen)
{
	char *s;

	if (*np == 0) {
		return (p);
	}

	if (*(--p) == '\t') {			/* will retype the whole line	*/
		while (*colp > plen) {
			puts (erase_seq);
			(*colp)--;
		}
		for (s=buffer; s<p; ++s) {
			if (*s == '\t') {
				puts (tab_seq+((*colp) & 07));
				*colp += 8 - ((*colp) & 07);
			} else {
				++(*colp);
				putc (*s);
			}
		}
	} else {
		puts (erase_seq);
		(*colp)--;
	}
	(*np)--;
	return (p);
}

/****************************************************************************/

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

/****************************************************************************/

static void process_macros (const char *input, char *output)
{
	char c, prev;
	const char *varname_start = NULL;
	int inputcnt  = strlen (input);
	int outputcnt = CFG_CBSIZE;
	int state = 0;	/* 0 = waiting for '$'	*/
			/* 1 = waiting for '(' or '{' */
			/* 2 = waiting for ')' or '}' */
			/* 3 = waiting for '''  */
#ifdef DEBUG_PARSER
	char *output_start = output;

	printf ("[PROCESS_MACROS] INPUT len %d: \"%s\"\n", strlen(input), input);
#endif

	prev = '\0';			/* previous character	*/

	while (inputcnt && outputcnt) {
	    c = *input++;
	    inputcnt--;

	    if (state!=3) {
	    /* remove one level of escape characters */
	    if ((c == '\\') && (prev != '\\')) {
		if (inputcnt-- == 0)
			break;
		prev = c;
		c = *input++;
	    }
	    }

	    switch (state) {
	    case 0:			/* Waiting for (unescaped) $	*/
		if ((c == '\'') && (prev != '\\')) {
			state = 3;
			break;
		}
		if ((c == '$') && (prev != '\\')) {
			state++;
		} else {
			*(output++) = c;
			outputcnt--;
		}
		break;
	    case 1:			/* Waiting for (	*/
		if (c == '(' || c == '{') {
			state++;
			varname_start = input;
		} else {
			state = 0;
			*(output++) = '$';
			outputcnt--;

			if (outputcnt) {
				*(output++) = c;
				outputcnt--;
			}
		}
		break;
	    case 2:			/* Waiting for )	*/
		if (c == ')' || c == '}') {
			int i;
			char envname[CFG_CBSIZE], *envval;
			int envcnt = input-varname_start-1; /* Varname # of chars */

			/* Get the varname */
			for (i = 0; i < envcnt; i++) {
				envname[i] = varname_start[i];
			}
			envname[i] = 0;

			/* Get its value */
			envval = getenv (envname);

			/* Copy into the line if it exists */
			if (envval != NULL)
				while ((*envval) && outputcnt) {
					*(output++) = *(envval++);
					outputcnt--;
				}
			/* Look for another '$' */
			state = 0;
		}
		break;
	    case 3:			/* Waiting for '	*/
		if ((c == '\'') && (prev != '\\')) {
			state = 0;
		} else {
			*(output++) = c;
			outputcnt--;
		}
		break;
	    }
	    prev = c;
	}

	if (outputcnt)
		*output = 0;

#ifdef DEBUG_PARSER
	printf ("[PROCESS_MACROS] OUTPUT len %d: \"%s\"\n",
		strlen(output_start), output_start);
#endif
}

/****************************************************************************
 * returns:
 *	1  - command executed, repeatable
 *	0  - command executed but not repeatable, interrupted commands are
 *	     always considered not repeatable
 *	-1 - not executed (unrecognized, bootd recursion or too many args)
 *           (If cmd is NULL or "" or longer than CFG_CBSIZE-1 it is
 *           considered unrecognized)
 *
 * WARNING:
 *
 * We must create a temporary copy of the command since the command we get
 * may be the result from getenv(), which returns a pointer directly to
 * the environment data, which may change magicly when the command we run
 * creates or modifies environment variables (like "bootp" does).
 */

int run_command (const char *cmd, int flag)
{
	cmd_tbl_t *cmdtp;
	char cmdbuf[CFG_CBSIZE];	/* working copy of cmd		*/
	char *token;			/* start of token in cmdbuf	*/
	char *sep;			/* end of token (separator) in cmdbuf */
	char finaltoken[CFG_CBSIZE];
	char *str = cmdbuf;
	char *argv[CFG_MAXARGS + 1];	/* NULL terminated	*/
	int argc, inquotes;
	int repeatable = 1;
	int rc = 0;

#ifdef DEBUG_PARSER
	printf ("[RUN_COMMAND] cmd[%p]=\"", cmd);
	puts (cmd ? cmd : "NULL");	/* use puts - string may be loooong */
	puts ("\"\n");
#endif

	clear_ctrlc();		/* forget any previous Control C */

	if (!cmd || !*cmd) {
		return -1;	/* empty command */
	}

	if (strlen(cmd) >= CFG_CBSIZE) {
		puts ("## Command too long!\n");
		return -1;
	}

	strcpy (cmdbuf, cmd);

	/* Process separators and check for invalid
	 * repeatable commands
	 */

#ifdef DEBUG_PARSER
	printf ("[PROCESS_SEPARATORS] %s\n", cmd);
#endif
	while (*str) {

		/*
		 * Find separator, or string end
		 * Allow simple escape of ';' by writing "\;"
		 */
		for (inquotes = 0, sep = str; *sep; sep++) {
			if ((*sep=='\'') &&
			    (*(sep-1) != '\\'))
				inquotes=!inquotes;

			if (!inquotes &&
			    (*sep == ';') &&	/* separator		*/
			    ( sep != str) &&	/* past string start	*/
			    (*(sep-1) != '\\'))	/* and NOT escaped	*/
				break;
		}

		/*
		 * Limit the token to data between separators
		 */
		token = str;
		if (*sep) {
			str = sep + 1;	/* start of command for next pass */
			*sep = '\0';
		}
		else
			str = sep;	/* no more commands for next pass */
#ifdef DEBUG_PARSER
		printf ("token: \"%s\"\n", token);
#endif

		/* find macros in this token and replace them */
		process_macros (token, finaltoken);

		/* Extract arguments */
		argc = parse_line (finaltoken, argv);

                #if defined(CONFIG_MARVELL)
                if(enaMonExt()){
                        if ((cmdtp = find_cmd(argv[0])) == NULL) {
                                int i;
                                argv[argc+1]= NULL;
                                for(i = argc; i > 0; i--){
                                        argv[i] = argv[i-1];}
                                argv[0] = "FSrun";
                                argc++;
                        }
                }
                #endif

		/* Look up command in command table */
		if ((cmdtp = find_cmd(argv[0])) == NULL) {
			printf ("Unknown command '%s' - try 'help'\n", argv[0]);
			rc = -1;	/* give up after bad command */
			continue;
		}

		/* found - check max args */
		if (argc > cmdtp->maxargs) {
			printf ("Usage:\n%s\n", cmdtp->usage);
			rc = -1;
			continue;
		}

#if (CONFIG_COMMANDS & CFG_CMD_BOOTD)
		/* avoid "bootd" recursion */
		if (cmdtp->cmd == do_bootd) {
#ifdef DEBUG_PARSER
			printf ("[%s]\n", finaltoken);
#endif
			if (flag & CMD_FLAG_BOOTD) {
				puts ("'bootd' recursion detected\n");
				rc = -1;
				continue;
			}
			else
				flag |= CMD_FLAG_BOOTD;
		}
#endif	/* CFG_CMD_BOOTD */

		/* OK - call function to do the command */
		if ((cmdtp->cmd) (cmdtp, flag, argc, argv) != 0) {
			rc = -1;
		}

		repeatable &= cmdtp->repeatable;

		/* Did the user stop this? */
		if (had_ctrlc ())
			return 0;	/* if stopped then not repeatable */
	}

	return rc ? rc : repeatable;
}

/****************************************************************************/

#if (CONFIG_COMMANDS & CFG_CMD_RUN)
int do_run (cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int i;

	if (argc < 2) {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}

	for (i=1; i<argc; ++i) {
		char *arg;

		if ((arg = getenv (argv[i])) == NULL) {
			printf ("## Error: \"%s\" not defined\n", argv[i]);
			return 1;
		}
#ifndef CFG_HUSH_PARSER
		if (run_command (arg, flag) == -1)
			return 1;
#else
		if (parse_string_outer(arg,
		    FLAG_PARSE_SEMICOLON | FLAG_EXIT_FROM_LOOP) != 0)
			return 1;
#endif
	}
	return 0;
}
#endif	/* CFG_CMD_RUN */
