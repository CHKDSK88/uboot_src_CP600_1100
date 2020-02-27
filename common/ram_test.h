/* ram_test.h 
 * Athour	: Tony Tang 
 * Date 	: 2008-04-15
 * Usage	: Common Ram Test Code header file, which includes data structures.
 */
#ifndef RAM_TEST_H
#define RAM_TEST_H

#define diag_printf printf

extern struct vars * const v;
extern struct mem_info_t mem_info;
#define BAILOUT		if (bail) goto skip_test;
#define BAILR		if (bail) return;
#define MOD_SZ		20

#if 1 /* TONY CODE */
//typedef unsigned long unsigned long;

struct err_info 
{
	unsigned long ebits;
	long	      tbits;
	short         min_bits;
	short         max_bits;
	unsigned long maxl;
	unsigned long eadr;
    unsigned long exor;
    unsigned long cor_err;
};

struct mem_info_t {
	unsigned long 	start;	
	unsigned long   end;
};

struct tseq {
	short cache;
	short pat;
	short iter;
	short ticks;
	short errors;
	char *msg;
};
struct vars 
{
	int test;			/* test number of test sequence */
	int pass;			/* what this for ??? - if set 1, equals to the test is passed and memory test is OK */
	int msg_line;		/* ??? */
	int ecount;			/* Error Counter ? */
	int ecc_ecount;		/* ECC Error Counter ? */
	int testsel;		/* test number in progress */
	struct err_info erri;
	struct mem_info_t map;	/* The memory map for test, including start & end address */
	//struct mem_info_t *test_region;
	//unsigned long clks_msec;		/* CPU clock in khz */
};

#define SPINSZ 0x100000 /* 1MB */

void disableECC(void);
void sleep(int n);
void set_cache(int val) ;
void get_test_region(unsigned long start, unsigned long end);
void get_mem_map(void);
void get_cpu_freq(void);
void poll_errors(void);


void addr_tst2(void);
void addr_tst1(void);
void bit_fade(void);
void block_move(int iter);
void modtst(int offset, int iter, unsigned long p1, unsigned long p2);
void movinv1(int iter, unsigned long p1, unsigned long p2);
void movinv32(int iter, unsigned long p1, unsigned long lb, unsigned long hb, int sval, int off);
void movinvr(void);
unsigned int rand(void); 
void rand_seed( unsigned int seed1, unsigned int seed2 );

void ad_err1(unsigned long *adr1, unsigned long *mask, unsigned long bad, unsigned long good);
void ad_err2(unsigned long *adr, unsigned long bad);
void error(unsigned long *adr, unsigned long good, unsigned long bad);

#endif

#endif //RAM_TEST_H
