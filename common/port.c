/* This file inclues all functions which are changed on different platforms. */
/* This means : these APIs should be supported by externel of this test tool */

#include <common.h>
#include <command.h>
#include <watchdog.h>
#include <malloc.h>
#include <net.h>
#include <flash.h>
#include <exports.h>
//#include <upgrade.h>
//#include <asm/addrspace.h>

/*===============================*/

#include "ram_test.h"
/*
 * Delay 1s function 
 */
extern void delay_1s(void);
/* 
 * Sleep fucntion used by bit fade test 
 */
void sleep(int n)
{
	int i = 0, j;
    printf("inter debug sleep........\n");
	for(;i<n;i++)
	{
		udelay(10000);
		if(i==60)
			diag_printf("Elapsed time : about 1 minute \n");
	}
}

/* 
 * Cache Operation 
 */
/* Here we use uncached memory */
void set_cache(int val) /* 1 - on , 0 - off */
{
	/*
	if(val)
		cache_init();
	else
		cache_disable();
	*/
} 
/*
 * Find out how much memory there is.
 */
/*
void get_test_region(ulong start, ulong end)
{
	v->map.start = start;
	v->map.end 	= end;
}
*/
//extern BYTE * memoryStart;
//extern BYTE * memorySize;
void get_mem_map(void)
{
	int i = 0;
	mem_info.start 	= 0x100000;
//	mem_info.end 	= 0x81f00000;
	mem_info.end 	= 0x120000;
	
	diag_printf("Test Range : %08x - %08x.\n", mem_info.start, mem_info.end);
}
/*
extern ulong cpu_freq;
void get_cpu_freq(void)
{
	v->clks_msec = cpu_freq;
}
*/
void disable_DDR_ECC(void)
{
}
void enable_DDR_ECC(void)
{
}
void poll_errors(void)
{
}

