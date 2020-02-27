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

/*
 * serial.c - serial support.
 */

#include <common.h>
#include <command.h>
#include "uart/mvUart.h"

#if defined(MV78XX0)
extern unsigned int whoAmI(void);
#else
    #define whoAmI()    0
#endif

extern void print_mvBanner(void);
extern void print_cpBanner(void);
extern void print_dev_id(void);
extern void cp_GPIO_LED_set(int num, int state);
int g_cp_boot_no_output = 0;

int serial_init (void)
{
	DECLARE_GLOBAL_DATA_PTR;
	
    int clock_divisor = (CFG_TCLK / 16)/gd->baudrate;

	if (whoAmI() == MASTER_CPU)
    {	
#ifdef CFG_INIT_CHAN1
	mvUartInit(0, clock_divisor, mvUartBase(0));
#endif
    }
    
#if defined(MV78XX0)
    if ((whoAmI() == MASTER_CPU) || (whoAmI() == SLAVE_CPU))
#endif
    {
#ifdef CFG_INIT_CHAN2
	mvUartInit(1, clock_divisor, mvUartBase(1));
#endif
    }
        /* print banner */
        //print_cpBanner();
        //print_dev_id();

		/* Start boot blinking */
		/* Power - GREEN Static. Alert GREEN Blinking*/
		cp_GPIO_LED_set(POWER_LED_GREEN,	LED_CMD_ON);
		cp_GPIO_LED_set(POWER_LED_GREEN,	LED_CMD_STATIC);

		cp_GPIO_LED_set(POWER_LED_RED,		LED_CMD_OFF);
		cp_GPIO_LED_set(POWER_LED_RED,		LED_CMD_STATIC);
		
		cp_GPIO_LED_set(ALERT_LED_GREEN,	LED_CMD_ON);
		cp_GPIO_LED_set(ALERT_LED_GREEN,	LED_CMD_BLINK);
				
		cp_GPIO_LED_set(ALERT_LED_RED,		LED_CMD_OFF);
		cp_GPIO_LED_set(ALERT_LED_RED,		LED_CMD_STATIC);

	return (0);
}

void
serial_putc(const char c)
{
	if (g_cp_boot_no_output == 1) {
		return;
	}
#if defined(CONFIG_MV_SMP) || (defined(MV78XX0) && defined(MV78200))
        if (c == '\n')
                mvUartPutc((whoAmI())%2, '\r');

        mvUartPutc((whoAmI())%2, c);
#else
	if (c == '\n')
		mvUartPutc(CFG_DUART_CHAN, '\r');

	mvUartPutc(CFG_DUART_CHAN, c);
#endif
}

int
serial_getc(void)
{	
	if (g_cp_boot_no_output == 1) {
		return 0;
	}
	
#if defined(CONFIG_MV_SMP) || (defined(MV78XX0) && defined(MV78200))
        return mvUartGetc((whoAmI())%2);
#else
	return mvUartGetc(CFG_DUART_CHAN);
#endif
}

int
serial_tstc(void)
{
	if (g_cp_boot_no_output == 1) {
		return 0;
	}
	
#if defined(CONFIG_MV_SMP) || (defined(MV78XX0) && defined(MV78200))
        return mvUartTstc((whoAmI())%2);
#else
	return mvUartTstc(CFG_DUART_CHAN);
#endif
}

void
serial_setbrg (void)
{
	DECLARE_GLOBAL_DATA_PTR;

	int clock_divisor = (CFG_TCLK / 16)/gd->baudrate;

	if (g_cp_boot_no_output == 1) {
		return;
	}
	

#ifdef CFG_INIT_CHAN1
	mvUartInit(0, clock_divisor, mvUartBase(0));
#endif
#ifdef CFG_INIT_CHAN2
	mvUartInit(1, clock_divisor, mvUartBase(1));
#endif
}


void
serial_puts (const char *s)
{
	if (g_cp_boot_no_output == 1) {
		return;
	}
	
	while (*s) {
		serial_putc (*s++);
	}
}

#if (CONFIG_COMMANDS & CFG_CMD_KGDB)
void
kgdb_serial_init(void)
{
}

void
putDebugChar (int c)
{
	serial_putc (c);
}

void
putDebugStr (const char *str)
{
	serial_puts (str);
}

int
getDebugChar (void)
{
	return serial_getc();
}

void
kgdb_interruptible (int yes)
{
	return;
}
#endif	/* CFG_CMD_KGDB	*/
