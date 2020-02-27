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

/**************************************************************************
 *
 * Support for persistent environment data
 *
 * The "environment" is stored as a list of '\0' terminated
 * "name=value" strings. The end of the list is marked by a double
 * '\0'. New entries are always added at the end. Deleting an entry
 * shifts the remaining entries to the front. Replacing an entry is a
 * combination of deleting the old value and adding the new one.
 *
 * The environment is preceeded by a 32 bit CRC over the data part.
 *
 **************************************************************************
 */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <watchdog.h>
#include <serial.h>
#include <linux/stddef.h>
#include <asm/byteorder.h>
#if (CONFIG_COMMANDS & CFG_CMD_NET)
#include <net.h>
#endif

#include <cp_blocked_vars.h>

#if !defined(CFG_ENV_IS_IN_NVRAM)	&& \
    !defined(CFG_ENV_IS_IN_EEPROM)	&& \
    !defined(CFG_ENV_IS_IN_FLASH)	&& \
    !defined(CFG_ENV_IS_IN_DATAFLASH)	&& \
    !defined(CFG_ENV_IS_IN_NAND)	&& \
    !defined(CFG_ENV_IS_NOWHERE)
# error Define one of CFG_ENV_IS_IN_{NVRAM|EEPROM|FLASH|DATAFLASH|NOWHERE}
#endif

#define XMK_STR(x)	#x
#define MK_STR(x)	XMK_STR(x)

/************************************************************************
************************************************************************/

/* Function that returns a character from the environment */
extern uchar (*env_get_char)(int);

/* Function that returns a pointer to a value from the environment */
/* (Only memory version supported / needed). */
extern uchar *env_get_addr(int);

/* Function that updates CRC of the enironment */
extern void env_crc_update (void);

/************************************************************************
************************************************************************/

static int envmatch (uchar *, int);

/*
 * Table with supported baudrates (defined in config_xyz.h)
 */
static const unsigned long baudrate_table[] = CFG_BAUDRATE_TABLE;
#define	N_BAUDRATES (sizeof(baudrate_table) / sizeof(baudrate_table[0]))

int g_cp_read_only_disabled = 0;
int read_only_error = 0;

void set_read_only_error(int i)
{
	read_only_error = i;
}

int get_read_only_error()
{
	return read_only_error ;
}

void
cp_disable_read_only(void)
{
	g_cp_read_only_disabled = 1;
}

void
cp_enable_read_only(void)
{
	g_cp_read_only_disabled = 0;
}


int cp_read_only_var(const char *name)
{
	int _i, num_vars;

	/* Allowing setting of vars from flash when booting. 
	 * See cp_disable_read_only() above
	 */
	if ( g_cp_read_only_disabled )
	{
		return 0;
	}
	
	num_vars = sizeof(cp_blocked_vars) / sizeof(blocked_var_single);
	for ( _i = 0 ; _i < num_vars ; _i++)
	{
		if ( !strcmp( name , cp_blocked_vars[_i].name) )
		{
			printf ("%s is read only!\n", cp_blocked_vars[_i].name);
			read_only_error = 1;
			return 1;
		}
	}
	return 0;
}

/************************************************************************
 * Command interface: print one or all environment variables
 */

int do_printenv (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int i, j, k, nxt;
	int rcode = 0;

	if (argc == 1) {		/* Print all env variables	*/
		for (i=0; env_get_char(i) != '\0'; i=nxt+1) {
			for (nxt=i; env_get_char(nxt) != '\0'; ++nxt)
				;
			for (k=i; k<nxt; ++k)
				putc(env_get_char(k));
			putc  ('\n');

			if (ctrlc()) {
				puts ("\n ** Abort\n");
				return 1;
			}
		}

		printf("\nEnvironment size: %d/%d bytes\n", i, ENV_SIZE);

		return 0;
	}

	for (i=1; i<argc; ++i) {	/* print single env variables	*/
		char *name = argv[i];

		k = -1;

		for (j=0; env_get_char(j) != '\0'; j=nxt+1) {

			for (nxt=j; env_get_char(nxt) != '\0'; ++nxt)
				;
			k = envmatch((uchar *)name, j);
			if (k < 0) {
				continue;
			}
			puts (name);
			putc ('=');
			while (k < nxt)
				putc(env_get_char(k++));
			putc ('\n');
			break;
		}
		if (k < 0) {
			printf ("## Error: \"%s\" not defined\n", name);
			rcode ++;
		}
	}
	return rcode;
}

/************************************************************************
 * Set a new environment variable,
 * or replace or delete an existing one.
 *
 * This function will ONLY work with a in-RAM copy of the environment
 */

int _do_setenv (int flag, int argc, char *argv[])
{
	DECLARE_GLOBAL_DATA_PTR;

	int   i, len, oldval;
	int   console = -1;
	uchar *env, *nxt = NULL;
	char *name;
	bd_t *bd = gd->bd;

	uchar *env_data = env_get_addr(0);

	if (!env_data)	/* need copy in RAM */
		return 1;

	name = argv[1];

	if (cp_read_only_var(name))
	{
		printf("READ ONLY !!!!!!!!!\n");
		return 1;
	}
	/*
	 * search if variable with this name already exists
	 */
	oldval = -1;
	for (env=env_data; *env; env=nxt+1) {
		for (nxt=env; *nxt; ++nxt)
			;
		if ((oldval = envmatch((uchar *)name, env-env_data)) >= 0)
			break;
	}

	/*
	 * Delete any existing definition
	 */
	if (oldval >= 0) {
#ifndef CONFIG_ENV_OVERWRITE

		/*
		 * Ethernet Address and serial# can be set only once,
		 * ver is readonly.
		 */
		if ( (strcmp (name, "serial#") == 0) ||
		    ((strcmp (name, "ethaddr") == 0)
#if defined(CONFIG_OVERWRITE_ETHADDR_ONCE) && defined(CONFIG_ETHADDR)
		     && (strcmp ((char *)env_get_addr(oldval),MK_STR(CONFIG_ETHADDR)) != 0)
#endif	/* CONFIG_OVERWRITE_ETHADDR_ONCE && CONFIG_ETHADDR */
		    ) ) {
			printf ("Can't overwrite \"%s\"\n", name);
			return 1;
		}
#endif

		/* Check for console redirection */
		if (strcmp(name,"stdin") == 0) {
			console = stdin;
		} else if (strcmp(name,"stdout") == 0) {
			console = stdout;
		} else if (strcmp(name,"stderr") == 0) {
			console = stderr;
		}

		if (console != -1) {
			if (argc < 3) {		/* Cannot delete it! */
				printf("Can't delete \"%s\"\n", name);
				return 1;
			}

			/* Try assigning specified device */
			if (console_assign (console, argv[2]) < 0)
				return 1;

#ifdef CONFIG_SERIAL_MULTI
			if (serial_assign (argv[2]) < 0)
				return 1;
#endif
		}

		/*
		 * Switch to new baudrate if new baudrate is supported
		 */
		if (strcmp(argv[1],"baudrate") == 0) {
			int baudrate = simple_strtoul(argv[2], NULL, 10);
			int i;
			for (i=0; i<N_BAUDRATES; ++i) {
				if (baudrate == baudrate_table[i])
					break;
			}
			if (i == N_BAUDRATES) {
				printf ("## Baudrate %d bps not supported\n",
					baudrate);
				return 1;
			}
		//	printf ("## Switch baudrate to %d bps and press ENTER ...\n",
		//		baudrate);
			udelay(50000);
			gd->baudrate = baudrate;
#ifdef CONFIG_PPC
			gd->bd->bi_baudrate = baudrate;
#endif

		//	serial_setbrg ();
		//	udelay(50000);
			/*for (;;) {
				if (getc() == '\r')
				      break;
			}*/
		}

		if (*++nxt == '\0') {
			if (env > env_data) {
				env--;
			} else {
				*env = '\0';
			}
		} else {
			for (;;) {
				*env = *nxt++;
				if ((*env == '\0') && (*nxt == '\0'))
					break;
				++env;
			}
		}
		*++env = '\0';
	}

#ifdef CONFIG_NET_MULTI
	if (strncmp(name, "eth", 3) == 0) {
		char *end;
		int   num = simple_strtoul(name+3, &end, 10);

		if (strcmp(end, "addr") == 0) {
			eth_set_enetaddr(num, argv[2]);
		}
	}
#endif


	/* Delete only ? */
	if ((argc < 3) || argv[2] == NULL) {
		env_crc_update ();
		return 0;
	}
	/*
	 * Append new definition at the end
	 */
	for (env=env_data; *env || *(env+1); ++env)
		;
	if (env > env_data)
		++env;
	/*
	 * Overflow when:
	 * "name" + "=" + "val" +"\0\0"  > ENV_SIZE - (env-env_data)
	 */
	len = strlen(name) + 2;
	/* add '=' for first arg, ' ' for all others */
	for (i=2; i<argc; ++i) {
		len += strlen(argv[i]) + 1;
	}
	if (len > (&env_data[ENV_SIZE]-env)) {
		printf ("## Error: environment overflow, \"%s\" deleted\n", name);
		return 1;
	}
	while ((*env = *name++) != '\0')
		env++;
	for (i=2; i<argc; ++i) {
		char *val = argv[i];

		*env = (i==2) ? '=' : ' ';
		while ((*++env = *val++) != '\0')
			;
	}

	/* end is marked with double '\0' */
	*++env = '\0';

	/* Update CRC */
	env_crc_update ();

	/*
	 * Some variables should be updated when the corresponding
	 * entry in the enviornment is changed
	 */

	if (strcmp(argv[1],"ethaddr") == 0) {
		char *s = argv[2];	/* always use only one arg */
		char *e;
		for (i=0; i<6; ++i) {
			bd->bi_enetaddr[i] = s ? simple_strtoul(s, &e, 16) : 0;
			if (s) s = (*e) ? e+1 : e;
		}
#ifdef CONFIG_NET_MULTI
		eth_set_enetaddr(0, argv[2]);
#endif
		return 0;
	}

	if (strcmp(argv[1],"ipaddr") == 0) {
		char *s = argv[2];	/* always use only one arg */
		char *e;
		unsigned long addr;
		bd->bi_ip_addr = 0;
		for (addr=0, i=0; i<4; ++i) {
			ulong val = s ? simple_strtoul(s, &e, 10) : 0;
			addr <<= 8;
			addr  |= (val & 0xFF);
			if (s) s = (*e) ? e+1 : e;
		}
		bd->bi_ip_addr = htonl(addr);
		return 0;
	}
	if (strcmp(argv[1],"loadaddr") == 0) {
		load_addr = simple_strtoul(argv[2], NULL, 16);
		return 0;
	}
#if (CONFIG_COMMANDS & CFG_CMD_NET)
	if (strcmp(argv[1],"bootfile") == 0) {
		copy_filename (BootFile, argv[2], sizeof(BootFile));
		return 0;
	}
#endif	/* CFG_CMD_NET */

#ifdef CONFIG_AMIGAONEG3SE
	if (strcmp(argv[1], "vga_fg_color") == 0 ||
	    strcmp(argv[1], "vga_bg_color") == 0 ) {
		extern void video_set_color(unsigned char attr);
		extern unsigned char video_get_attr(void);

		video_set_color(video_get_attr());
		return 0;
	}
#endif	/* CONFIG_AMIGAONEG3SE */

	return 0;
}

void setenv (char *varname, char *varvalue)
{
	char *argv[4] = { "setenv", varname, varvalue, NULL };
	_do_setenv (0, 3, argv);
}

int do_setenv ( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	if (argc < 2) {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}

	return _do_setenv (flag, argc, argv);
}

/************************************************************************
 * Prompt for environment variable
 */

#if (CONFIG_COMMANDS & CFG_CMD_ASKENV)
int do_askenv ( cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	extern char console_buffer[CFG_CBSIZE];
	char message[CFG_CBSIZE];
	int size = CFG_CBSIZE - 1;
	int len;
	char *local_args[4];

	local_args[0] = argv[0];
	local_args[1] = argv[1];
	local_args[2] = NULL;
	local_args[3] = NULL;

	if (argc < 2) {
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}
	/* Check the syntax */
	switch (argc) {
	case 1:
		printf ("Usage:\n%s\n", cmdtp->usage);
		return 1;

	case 2:		/* askenv envname */
		sprintf (message, "Please enter '%s':", argv[1]);
		break;

	case 3:		/* askenv envname size */
		sprintf (message, "Please enter '%s':", argv[1]);
		size = simple_strtoul (argv[2], NULL, 10);
		break;

	default:	/* askenv envname message1 ... messagen size */
	    {
		int i;
		int pos = 0;

		for (i = 2; i < argc - 1; i++) {
			if (pos) {
				message[pos++] = ' ';
			}
			strcpy (message+pos, argv[i]);
			pos += strlen(argv[i]);
		}
		message[pos] = '\0';
		size = simple_strtoul (argv[argc - 1], NULL, 10);
	    }
		break;
	}

	if (size >= CFG_CBSIZE)
		size = CFG_CBSIZE - 1;

	if (size <= 0)
		return 1;

	/* prompt for input */
	len = readline (message);

	if (size < len)
		console_buffer[size] = '\0';

	len = 2;
	if (console_buffer[0] != '\0') {
		local_args[2] = console_buffer;
		len = 3;
	}

	/* Continue calling setenv code */
	return _do_setenv (flag, len, local_args);
}
#endif	/* CFG_CMD_ASKENV */

/************************************************************************
 * Look up variable from environment,
 * return address of storage for that variable,
 * or NULL if not found
 */

char *getenv (char *name)
{
	int i, nxt;

	WATCHDOG_RESET();

	for (i=0; env_get_char(i) != '\0'; i=nxt+1) {
		int val;

		for (nxt=i; env_get_char(nxt) != '\0'; ++nxt) {
			if (nxt >= CFG_ENV_SIZE) {
				return (NULL);
			}
		}
		if ((val=envmatch((uchar *)name, i)) < 0)
			continue;
		return ((char *)env_get_addr(val));
	}

	return (NULL);
}

int getenv_r (char *name, char *buf, unsigned len)
{
	int i, nxt;

	for (i=0; env_get_char(i) != '\0'; i=nxt+1) {
		int val, n;

		for (nxt=i; env_get_char(nxt) != '\0'; ++nxt) {
			if (nxt >= CFG_ENV_SIZE) {
				return (-1);
			}
		}
		if ((val=envmatch((uchar *)name, i)) < 0)
			continue;
		/* found; copy out */
		n = 0;
		while ((len > n++) && (*buf++ = env_get_char(val++)) != '\0')
			;
		if (len == n)
			*buf = '\0';
		return (n);
	}
	return (-1);
}

#if defined(CFG_ENV_IS_IN_NVRAM) || defined(CFG_ENV_IS_IN_EEPROM) || defined(CFG_ENV_IS_IN_NAND) || \
    ((CONFIG_COMMANDS & (CFG_CMD_ENV|CFG_CMD_FLASH)) == \
      (CFG_CMD_ENV|CFG_CMD_FLASH))
int do_saveenv (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	extern char * env_name_spec;

	printf ("Saving Environment to %s...\n", env_name_spec);

	return (saveenv() ? 1 : 0);
}


#endif


/************************************************************************
 * Match a name / name=value pair
 *
 * s1 is either a simple 'name', or a 'name=value' pair.
 * i2 is the environment index for a 'name2=value2' pair.
 * If the names match, return the index for the value2, else NULL.
 */

static int
envmatch (uchar *s1, int i2)
{

	while (*s1 == env_get_char(i2++))
		if (*s1++ == '=')
			return(i2);
	if (*s1 == '\0' && env_get_char(i2-1) == '=')
		return(i2);
	return(-1);
}
#ifdef SEATTLE
int isnum(char num)
{
    if ((num >= '0') && (num <= '9'))
        return 1;
    return 0;
}

#define TYPE_TWSI_I2C   0
#define TYPE_GPIO_I2C   1
int get_argv(char *argv[],unsigned char * set_time,unsigned int output_type)
{
    unsigned char year[5],month[3],day[3],hour[3],min[3],sec[3],dow/*day of week*/;

    if ((argv[2][4] == '-') && (argv[2][7] == '-'))
    {
        memcpy(year,argv[2],4);
        year[4] = '\0';
        memcpy(month,&argv[2][5],2);
        month[2] = '\0';
        memcpy(day,&argv[2][8],2);
        day[2] = '\0';
    }
    else
    {
        return 1;
    }

    if ((argv[3][2] == ':') && (argv[3][5] == ':'))
    {
        memcpy(hour,argv[3],2);
        hour[2] = '\0';
        memcpy(min,&argv[3][3],2);
        min[2] = '\0';
        memcpy(sec,&argv[3][6],2);
        sec[2] = '\0';
    }
    else
    {
        return 1;
    }

    if (isnum(argv[4][0]))
        dow = argv[4][0] - '0';
    else
    {
        return 1;
    }
    
    if (output_type == TYPE_GPIO_I2C)
    {
        set_time[6] = (unsigned char)simple_strtoul(year,NULL,10) - 2000;
        set_time[5] = (unsigned char)simple_strtoul(month,NULL,10);
        set_time[4] = (unsigned char)simple_strtoul(day,NULL,10);
        set_time[3] = dow;
        set_time[2] = (unsigned char)simple_strtoul(hour,NULL,10);
        set_time[1] = (unsigned char)simple_strtoul(min,NULL,10);
        set_time[0] = (unsigned char)simple_strtoul(sec,NULL,10);
    }
    else if (output_type == TYPE_TWSI_I2C)
    {
        set_time[0] = (unsigned char)simple_strtoul(year,NULL,10) - 2000;
        set_time[1] = (unsigned char)simple_strtoul(month,NULL,10);
        set_time[2] = (unsigned char)simple_strtoul(day,NULL,10);
        set_time[3] = dow;
        set_time[4] = (unsigned char)simple_strtoul(hour,NULL,10);
        set_time[5] = (unsigned char)simple_strtoul(min,NULL,10);
        set_time[6] = (unsigned char)simple_strtoul(sec,NULL,10);
    }
#define RTC_DEBUG
#ifdef RTC_DEBUG
    if (output_type == TYPE_GPIO_I2C)
    {
        printf("year      : %d\n",2000+set_time[6]);
        printf("month     : %d\n",set_time[5]);
        printf("day       : %d\n",set_time[4]);
        printf("dayofweek : %d\n",set_time[3]);
        printf("hour      : %d\n",set_time[2]);
        printf("minute    : %d\n",set_time[1]);
        printf("second    : %d\n",set_time[0]);
    }
    else if (output_type == TYPE_TWSI_I2C)
    {
        printf("year      : %d\n",2000+set_time[0]);
        printf("month     : %d\n",set_time[1]);
        printf("day       : %d\n",set_time[2]);
        printf("dayofweek : %d\n",set_time[3]);
        printf("hour      : %d\n",set_time[4]);
        printf("minute    : %d\n",set_time[5]);
        printf("second    : %d\n",set_time[6]);
    }
#endif
    return 0;
}

extern int RTC_S_35390A_Test(void);
extern void print_GetTime(void);
extern void s3530_RdID(void);
int rtc_test (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    if (argc<2)
    {
        printf("rtc test/get/set yyyy-mm-dd hh:mm:ss dayofweek\n");
        return 0;
    }

    s3530_RdID();

    if (strnicmp(argv[1], "get",sizeof("get")) == 0)
    {
        print_GetTime();
    }
    else if (strnicmp(argv[1], "set",sizeof("set")) == 0)
    {
        unsigned char set_time[7];

        if (argc != 5)
        {
            printf("Error : invalid parameter number\n");
            return 0;
        }

        if (!get_argv(argv,set_time,TYPE_GPIO_I2C))
        {
            cmd_SetTime(set_time);
        }
        else
            printf("rtc test/get/set yyyy-mm-dd hh:mm:ss dayofweek\n");
    }
    else if (strnicmp(argv[1], "test",sizeof("test")) == 0)
    {
        RTC_S_35390A_Test();
    }
    else
    {
        printf("rtc test/get/set yyyy:mm:dd\n");
    }
    return 0;
}

extern void cmd_GetTime(unsigned char * get_time);
int do_date_env (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    unsigned char tm[7];
    unsigned char env_string[200];

    if (argc<2)
    {
        printf("date_env env\n");
        return 0;
    }

    s3530_RdID();

    memset(tm,0,7*sizeof(unsigned char));
    memset(env_string,0,200*sizeof(unsigned char));

    cmd_GetTime(tm);
    sprintf(env_string,"setenv %s '%02d%02d%02d%02d%04d.%02d'",argv[1],tm[5],tm[4],tm[2],tm[1],2000+tm[6],tm[0]);

    if (run_command(env_string,0) == -1)
        return 1;

    return 0;
}

/**************************************************/
U_BOOT_CMD(
	date_env, CFG_MAXARGS, 1,	do_date_env,
	"date_env- get current time and set it to env\n",
	"       -date_env env\n"
);

U_BOOT_CMD(
	rtc, CFG_MAXARGS, 1,	rtc_test,
	"rtc- driver for RTC\n",
	"    -rtc get/set yyyy:mm:dd/test\n"
);
#endif
U_BOOT_CMD(
	printenv, CFG_MAXARGS, 1,	do_printenv,
	"printenv- print environment variables\n",
	"\n    - print values of all environment variables\n"
	"printenv name ...\n"
	"    - print value of environment variable 'name'\n"
);

U_BOOT_CMD(
	setenv, CFG_MAXARGS, 0,	do_setenv,
	"setenv  - set environment variables\n",
	"name value ...\n"
	"    - set environment variable 'name' to 'value ...'\n"
	"setenv name\n"
	"    - delete environment variable 'name'\n"
);

#if defined(CFG_ENV_IS_IN_NVRAM) || defined(CFG_ENV_IS_IN_EEPROM) || defined(CFG_ENV_IS_IN_NAND) || \
    ((CONFIG_COMMANDS & (CFG_CMD_ENV|CFG_CMD_FLASH)) == \
      (CFG_CMD_ENV|CFG_CMD_FLASH))
U_BOOT_CMD(
	saveenv, 1, 0,	do_saveenv,
	"saveenv - save environment variables to persistent storage\n",
	NULL
);

#endif	/* CFG_CMD_ENV */

#if (CONFIG_COMMANDS & CFG_CMD_ASKENV)

U_BOOT_CMD(
	askenv,	CFG_MAXARGS,	1,	do_askenv,
	"askenv  - get environment variables from stdin\n",
	"name [message] [size]\n"
	"    - get environment variable 'name' from stdin (max 'size' chars)\n"
	"askenv name\n"
	"    - get environment variable 'name' from stdin\n"
	"askenv name size\n"
	"    - get environment variable 'name' from stdin (max 'size' chars)\n"
	"askenv name [message] size\n"
	"    - display 'message' string and get environment variable 'name'"
	"from stdin (max 'size' chars)\n"
);
#endif	/* CFG_CMD_ASKENV */

#if (CONFIG_COMMANDS & CFG_CMD_RUN)
int do_run (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]);
U_BOOT_CMD(
	run,	CFG_MAXARGS,	1,	do_run,
	"run     - run commands in an environment variable\n",
	"var [...]\n"
	"    - run the commands in the environment variable(s) 'var'\n"
);
#endif  /* CFG_CMD_RUN */
