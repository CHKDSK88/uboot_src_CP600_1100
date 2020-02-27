/* test_algo.c - change from MemTest-86 
 * tony tang
 */

#include "ram_test.h"

extern int bail;
extern volatile unsigned long *p;
extern unsigned long p1, p2;
extern struct tseq tseq[];
extern void poll_errors(void);

int ecount = 0;

static inline unsigned long roundup(unsigned long value, unsigned long mask)
{
	return (value + mask) & ~mask;
}
/*
 * Memory address test, walking ones
 */
void addr_tst1()
{
	int i, j, k;
	volatile unsigned long *pt;
	volatile unsigned long *end;
	unsigned long bad, mask, bank;

	/* Test the global address bits */
	for (p1=0, j=0; j<2; j++) 
	{        
		/* Set pattern in our lowest multiple of 0x20000 */
		p = (unsigned long *)roundup((unsigned long)(v->map.start), 0x1ffff);
		*p = p1;
	
		/* Now write pattern compliment */
		p1 = ~p1;
		end = (unsigned long *)(v->map.end);
		for (i=0; i<1000; i++) 
		{
			mask = 4;
			do 
			{
				pt = (unsigned long *)((unsigned long)p | mask);
				if (pt == p) 
				{
					mask = mask << 1;
					continue;
				}
				if (pt >= end) 
				{
					break;
				}
				*pt = p1;
				if ((bad = *p) != ~p1) 
				{
					diag_printf("excepted data : %08x.\n",  ~p1);
					ad_err1((unsigned long *)p, (unsigned long *)mask, bad, ~p1);
					i = 1000;
				}
				mask = mask << 1;
			} while(mask);
		}
		//do_tick();
		BAILR
	}

	/* Now check the address bits in each bank */
	/* If we have more than 8mb of memory then the bank size must be */
	/* bigger than 256k.  If so use 1mb for the bank size. */
#if 0
	if (v->pmap[v->msegs - 1].end > (0x800000 >> 12)) 
	{
		bank = 0x100000;
	} 
	else 
	{
		bank = 0x40000;
	}
#else /* set default bank size now */
	bank = 0x40000;
#endif
	for (p1 = 0, k = 0; k<2; k++) 
	{
		{
			p = (v->map.start);
			/* Force start address to be a multiple of 256k */
			p = (unsigned long *)roundup((unsigned long)p, bank - 1);
			end = (v->map.end);
			while (p < end) 
			{
				*p = p1;

				p1 = ~p1;
				for (i=0; i<200; i++) 
				{
					mask = 4;
					do{
						pt = (unsigned long *)((unsigned long)p | mask);
						if (pt == p) 
						{
							mask = mask << 1;
							continue;
						}
						if (pt >= end) 
						{
							break;
						}
						*pt = p1;
						if ((bad = *p) != ~p1) 
						{
							ad_err1((unsigned long *)p,(unsigned long *)mask,bad,~p1);
							i = 200;
						}
						mask = mask << 1;
					} while(mask);
				}
				if (p + bank > p) 
				{
					p += bank;
				} 
				else 
				{
					p = end;
				}
				p1 = ~p1;
			}
		}
		//do_tick();
		BAILR
		p1 = ~p1;
	}
}

/*
 * Memory address test, own address
 */
void addr_tst2()
{
	int j, done;
	volatile unsigned long *pe;
	volatile unsigned long *end, *start;
	unsigned long bad;

	/* Write each address with it's own address */
	{
		start = v->map.start;
		end = v->map.end;
		pe = (unsigned long *)start;
		p = start;
		done = 0;
		do 
		{
			/* Check for overflow */
			if (pe + SPINSZ > pe) 
			{
				pe += SPINSZ;
			} 
			else 
			{
				pe = end;
			}
			if (pe >= end) 
			{
				pe = end;
				done++;
			} 	
			if (p == pe ) 
			{
				break;
			}
			for (; p < pe; p++) 
 			{
 				*p = (unsigned long)p;
 			}
			//do_tick();
			BAILR
		} while (!done);
	}

	/* Each address should have its own address */
	{
		start = v->map.start;
		end = v->map.end;
		pe = (unsigned long *)start;
		p = start;
		done = 0;
		do {
			/* Check for overflow */
			if (pe + SPINSZ > pe) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
 			for (; p < pe; p++) 
 			{
 				if((bad = *p) != (unsigned long)p) 
 				{
					diag_printf("excepted data : %08x.\n",  p);
 					ad_err2((unsigned long)p, bad);
 				}
 			}
			//do_tick();
			BAILR
		} while (!done);
	}
}

/*
 * Test all of memory using a "half moving inversions" algorithm using random
 * numbers and their complment as the data pattern. Since we are not able to
 * produce random numbers in reverse order testing is only done in the forward
 * direction.
 */
static int random_run = 0;
void movinvr()
{
	int i, done, seed1, seed2;
	volatile unsigned long *pe;
	volatile unsigned long *start,*end;
	unsigned long num;
	unsigned long bad;

	random_run ++;
	/* Initialize memory with initial sequence of random numbers.  */
	{
#if 0 
		seed1 = 521288629 + v->pass;
		seed2 = 362436069 - v->pass;
#else
		seed1 = 521288629 + random_run;
		seed2 = 362436069 - random_run;
#endif
	}

	/* Display the current seed */
    diag_printf("Current Seed :%08x .\n", seed1);
    
	rand_seed(seed1, seed2);
	
	{
		start = v->map.start;
		end = v->map.end;
		pe = start;
		p = start;
		done = 0;
		do 
		{
			/* Check for overflow */
			if (pe + SPINSZ > pe) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
			for (; p < pe; p++) 
			{
				*p = rand();
			}
			//do_tick();
			BAILR
		} while (!done);
	}

	/* Do moving inversions test. Check for initial pattern and then
	 * write the complement for each memory location. Test from bottom
	 * up and then from the top down.  */
	for (i=0; i<2; i++) 
	{
		rand_seed(seed1, seed2);
		{
			start = v->map.start;
			end = v->map.end;
			pe = start;
			p = start;
			done = 0;
			do 
			{
				/* Check for overflow */
				if (pe + SPINSZ > pe) {
					pe += SPINSZ;
				} else {
					pe = end;
				}
				if (pe >= end) {
					pe = end;
					done++;
				}
				if (p == pe ) {
					break;
				}
				for (; p < pe; p++) 
				{
					num = rand();
					if (i) 
					{
						num = ~num;
					}
					if ((bad=*p) != num) 
					{
						diag_printf("excepted data : %08x.\n",  num);
						error((unsigned long*)p, num, bad);
					}
					*p = ~num;
				}
				//do_tick();
				BAILR
			} while (!done);
		}
	}
}

/*
 * Test all of memory using a "moving inversions" algorithm using the
 * pattern in p1 and it's complement in p2. parameter iter is times to be executed 
 */
void movinv1(int iter, unsigned long p1, unsigned long p2)
{
	int i, done;
	volatile unsigned long *pe;
	volatile unsigned long len;
	volatile unsigned long *start,*end;
	unsigned long bad;

	/* Display the current pattern */
    diag_printf("Current Pattern  %08x .\n", p1);

	/* Initialize memory with the initial pattern.  */
	{
		start = v->map.start;
		end = v->map.end;
		pe = start;
		p = start;
		done = 0;
		
		do 
		{
			/* Check for overflow */
			if (pe + SPINSZ > pe) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			//len = pe - p;
			if (p == pe ) {
				break;
			}
 			for (; p < pe; p++) 
 			{
 				*p = p1;
 			}
 			//do_tick();
			BAILR
		} while (!done);
	}

	/* Do moving inversions test. Check for initial pattern and then
	 * write the complement for each memory location. Test from bottom
	 * up and then from the top down.  */
	for (i=0; i<iter; i++) 
	{
		{
			start = v->map.start;
			end = v->map.end;
			pe = start;
			p = start;
			done = 0;
			do 
			{
				/* Check for overflow */
				if (pe + SPINSZ > pe) {
					pe += SPINSZ;
				} else {
					pe = end;
				}
				if (pe >= end) {
					pe = end;
					done++;
				}
				if (p == pe ) {
					break;
				}
 				for (; p < pe; p++) 
 				{
 					if ((bad=*p) != p1) 
 					{
						diag_printf("excepted data : %08x.\n",  p1);
 						error((unsigned long*)p, p1, bad);
 					}
 					*p = p2;
 				}
				//do_tick();
				BAILR
			} while (!done);
		}
		{
			start = v->map.start;
			end = v->map.end;
			pe = end -1;
			p = end - 1;
			done = 0;
			do 
			{
			/* Check for underflow */
				if (pe - SPINSZ < pe) {
					pe -= SPINSZ;
				} else {
					pe = start;
				}
				if (pe <= start) {
					pe = start;
					done++;
				}
				if (p == pe ) {
					break;
				}
 				do 
 				{
 					if ((bad=*p) != p2) 
 					{
						diag_printf("excepted data : %08x.\n",  p2);
 						error((unsigned long*)p, p2, bad);
 					}
 					*p = p1;
 				} while (p-- > pe);
				//do_tick();
				BAILR
			} while (!done);
		}
	}
}

void movinv32(int iter, unsigned long p1, unsigned long lb, unsigned long hb, int sval, int off)
{
	int i, j, k=0, done;
	volatile unsigned long *pe;
	volatile unsigned long *start, *end;
	unsigned long pat = 0;
	unsigned long bad;

	/* Display the current pattern */
    diag_printf("Current Pattern  %08x .\n", p1);
    
	/* Initialize memory with the initial pattern.  */
	{
		start = v->map.start;
		end = v->map.end;
		pe = start;
		p = start;
		done = 0;
		k = off;
		pat = p1;
		do 
		{
			/* Check for overflow */
			if (pe + SPINSZ > pe) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
 			while (p < pe) 
 			{
 				*p = pat;
 				if (++k >= 32) 
 				{
 					pat = lb;
 					k = 0;
 				} 
 				else 
 				{
 					pat = pat << 1;
 					pat |= sval;
 				}
 				p++;
 			}
			/* Do a SPINSZ section of memory */
			//do_tick();
			BAILR
		} while (!done);
	}

	/* Do moving inversions test. Check for initial pattern and then
	 * write the complement for each memory location. Test from bottom
	 * up and then from the top down.  */
	for (i=0; i<iter; i++) 
	{
		{
			start = v->map.start;
			end = v->map.end;
			pe = start;
			p = start;
			done = 0;
			k = off;
			pat = p1;
			do 
			{
				/* Check for overflow */
				if (pe + SPINSZ > pe) {
					pe += SPINSZ;
				} else {
					pe = end;
				}
				if (pe >= end) {
					pe = end;
					done++;
				}
				if (p == pe ) {
					break;
				}
				while (p < pe) 
				{
 					if ((bad=*p) != pat) 
 					{
						diag_printf("excepted data : %08x.\n",  pat);
						error((unsigned long*)p, pat, bad);
					}
 					*p = ~pat;
 					if (++k >= 32) 
 					{
 						pat = lb;
 						k = 0;
 					} 
 					else 
 					{
 						pat = pat << 1;
 						pat |= sval;
 					}
 					p++;
 				}
				//do_tick();
				BAILR
			} while (!done);
		}

		/* Since we already adjusted k and the pattern this
		 * code backs both up one step
		 */
		pat = lb;
 		if ( 0 != (k = (k-1) & 31) ) 
 		{
 			pat = (pat << k);
 			if ( sval )
 				pat |= ((sval << k) - 1);
 		}
 		k++;
#if 0 /* for tony debug use */
 		diag_printf("pat now %08x, k now %08x\n", pat, k);
/* CDH start */
		{
			int iiiii = 0;
			volatile unsigned long *ptr = (v->map.end - 20);
			for(;iiiii<20; iiiii++)
			{
				diag_printf("%08x\n", ptr[iiiii]);
			}
		}
#endif
/* CDH end */
		{
			start = v->map.start;
			end = v->map.end;
			p = end -1;
			pe = end -1;
			done = 0;
			do 
			{
				/* Check for underflow */
				if (pe - SPINSZ < pe) {
					pe -= SPINSZ;
				} else {
					pe = start;
				}
				if (pe <= start) {
					pe = start;
					done++;
				}
				if (p == pe ) {
					break;
				}
 				do 
 				{
 					if ((bad=*p) != ~pat) 
 					{
						diag_printf("excepted data : %08x.\n",  ~pat);
 						error((unsigned long*)p, ~pat, bad);
 					}
 					*p = pat;
 					if (--k <= 0) 
 					{
 						pat = hb;
 						k = 32;
 					} 
 					else 
 					{
 						pat = pat >> 1;
 						pat |= (sval<<31);   // org : p3 -- ??????????? -- this is decided by TONY TANG
 					}
 				} while (p-- > pe);
				//do_tick();
				BAILR
			} while (!done);
		}
	}
}

/*
 * Test all of memory using modulo X access pattern.
 */
void modtst(int offset, int iter, unsigned long p1, unsigned long p2)
{
	int k, l, done;
	volatile unsigned long *pe;
	volatile unsigned long *start, *end;
	unsigned long bad;

	/* Display the current pattern */
	diag_printf("Modelo X test using pattern %08x with offset %08x.\n", p1, offset);

	/* Write every nth location with pattern */
	{
		start = v->map.start;
		end = v->map.end;
		pe = (unsigned long *)start;
		p = start + offset;
		done = 0;
		do 
		{
			/* Check for overflow */
			if (pe + SPINSZ > pe) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
 			for (; p < pe; p += MOD_SZ) 
 			{
 				*p = p1;
 			}
			//do_tick();
			BAILR
		} while (!done);
	}

	/* Write the rest of memory "iter" times with the pattern complement */
	for (l=0; l<iter; l++) 
	{
		{
			start = v->map.start;
			end = v->map.end;
			pe = (unsigned long *)start;
			p = start;
			done = 0;
			k = 0;
			do 
			{
				/* Check for overflow */
				if (pe + SPINSZ > pe) {
					pe += SPINSZ;
				} else {
					pe = end;
				}
				if (pe >= end) {
					pe = end;
					done++;
				}
				if (p == pe ) {
					break;
				}
 				for (; p < pe; p++) 
 				{
 					if (k != offset) 
 					{
 						*p = p2;
 					}
 					if (++k > MOD_SZ-1) 
 					{
 						k = 0;
 					}
 				}
				//do_tick();
				BAILR
			} while (!done);
		}
	}

	/* Now check every nth location */
	{
		start = v->map.start;
		end = v->map.end;
		pe = (unsigned long *)start;
		p = start+offset;
		done = 0;
		do 
		{
			/* Check for overflow */
			if (pe + SPINSZ > pe) {
				pe += SPINSZ;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
 			for (; p < pe; p += MOD_SZ) 
 			{
 				if ((bad=*p) != p1) 
 				{
					diag_printf("excepted data : %08x.\n",  p1);
 					error((unsigned long*)p, p1, bad);
 				}
 			}
			//do_tick();
			BAILR
		} while (!done);
	}
	diag_printf("\n");
}

/*
 * Test memory using block moves 
 * Adapted from Robert Redelmeier's burnBX test
 */
void block_move(int iter)
{
#if 1
	int i, j, done;
	unsigned long len;
	volatile unsigned long p, pe, pp;
	volatile unsigned long start, end;

	unsigned long p1 = 1;

	/* Initialize memory with the initial pattern.  */
	{
		start = (unsigned long)v->map.start;

		end = (unsigned long)v->map.end;
		pe = start;
		p = start;
		done = 0;
		do 
		{
			/* Check for overflow */
			if (pe + SPINSZ*4 > pe) 
			{
				pe += SPINSZ*4;
			} 
			else 
			{
				pe = end;
			}
			if (pe >= end) 
			{
				pe = end;
				done++;
			}
			if (p == pe ) 
			{
				break;
			}
			len  = ((unsigned long)pe - (unsigned long)p) / 64;
			
			do
			{
				int off = 0;
				unsigned long p2 = ~p1;
				volatile unsigned long *ptr = (unsigned long *)p;
								
				for(;off<16;off++)
				{
					if((off==4) || (off==5) ||(off==10)||(off==11))
						*(ptr+off) = p2;
					else
						*(ptr+off) = p1;
				}
				p += 64;
				/****/// 带进位的循环左移！！！
				p1 = p1 >> 1;  	
				if(!p1)
				{
					p1 |= 0x80000000;
				}
				/****/			
			}while(--len);
#if 0 /* TONY debug for init pattern */
		{
			int iiiii = 0;
			volatile unsigned long *ptr = (v->map.start);
			for(;iiiii<128; iiiii++)
			{
				if(!((iiiii+1)%7))
					diag_printf("\n");
				diag_printf("%08x\t", ptr[iiiii]);
			}
		}
#endif
			//do_tick();
			BAILR
		} while (!done);
	}
	//diag_printf("pattern filled in\n");
	/* Now move the data around 
	 * First move the data up half of the segment size we are testing
	 * Then move the data to the original location + 32 bytes
	 */
	{
		start = (unsigned long)v->map.start;
		end = (unsigned long)v->map.end;
		pe = start;
		p = start;
		done = 0;
		do {
			/* Check for overflow */
			if (pe + SPINSZ*4 > pe) {
				pe += SPINSZ*4;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
			pp = p + ((pe - p) / 2);
			len  = ((unsigned long)pe - (unsigned long)p) / 8;
			for(i=0; i<iter; i++) 
			{
				unsigned long *src = (unsigned long *)p;
				unsigned long *dst = (unsigned long *)pp;
				unsigned long tmp = p;
				unsigned long len_tmp = len;
				do
				{					
					*dst = *src;
					src += 1;
					dst += 1;										
				}while(--len_tmp);
				
				tmp = tmp + 32;
				dst = (unsigned long *)tmp;
				src = (unsigned long *)pp;
				len_tmp = len - 8;
				
				do
				{
					*dst = *src;
					src += 1;
					dst += 1;
				}while(--len_tmp);
				
				dst = (unsigned long *)p;
				len_tmp = 8;
				do
				{
					*dst = *src;
					src += 1;
					dst += 1;
				}while(--len_tmp);
				//do_tick();
				BAILR
			}
			p = pe;
		} while (!done);
	}
	//diag_printf("moves finished\n");
	/* Now check the data 
	 * The error checking is rather crude.  We just check that the
	 * adjacent words are the same.
	 */
	{
		start = (unsigned long)v->map.start;

		end = (unsigned long)v->map.end;
		pe = start;
		p = start;
		done = 0;
		do {
			/* Check for overflow */
			if (pe + SPINSZ*4 > pe) {
				pe += SPINSZ*4;
			} else {
				pe = end;
			}
			if (pe >= end) {
				pe = end;
				done++;
			}
			if (p == pe ) {
				break;
			}
			
			do
			{
				unsigned long tmp;
				unsigned long *tmp1, *tmp2;
				tmp = p;
				tmp1 = (unsigned long *)tmp;
				tmp = tmp + 4;
				tmp2 = (unsigned long *)tmp;			
				
				if(*tmp1 != *tmp2)
				{
					diag_printf("Block Move has error!!!\n");
					error((unsigned long *)tmp, *tmp1, *tmp2);
				}
				
				p = p + 8;				
			}while(p < pe);
			
			BAILR
		} while (!done);
	}
#else
	diag_printf("block_move - dummy now.\n");
#endif
}

/*
 * Test memory for bit fade.
 */
#define STIME 5400
void bit_fade()
{
#if 0
	volatile unsigned long *pe;
	volatile unsigned long bad;
	volatile unsigned long *start,*end;


	/* Do -1 and 0 patterns */
	p1 = 0;
	while (1) 
	{
		/* Display the current pattern */

		/* Initialize memory with the initial pattern.  */
		{
			start = v->map.start;
			end = v->map.end;
			pe = start;
			p = start;
			for (p=start; p<end; p++) 
			{
				*p = p1;
			}
			//do_tick();
			BAILR
		}
		/* Snooze for 90 minutes */
		sleep (STIME); ///?????

		/* Make sure that nothing changed while sleeping */
		{
			start = v->map.start;
			end = v->map.end;
			pe = start;
			p = start;
			for (p=start; p<end; p++) 
			{
 				if ((bad=*p) != p1) 
 				{
					diag_printf("excepted data : %08x.\n",  p1);
					error((unsigned long*)p, p1, bad);
				}
			}
			//do_tick();
			BAILR
		}
		if (p1 == 0) 
		{
			p1=-1;
		} 
		else 
		{
			break;
		}
	}
#endif
}

