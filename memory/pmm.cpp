#include "pmm.h"


#include <stdint.h>
#include <stdbool.h>

extern "C" int _kernel_start; // suppose we have this.. 
extern "C" int _kernel_end;

int bm_size = 16384;
uint64_t bitmap[16384];
int last_free_page;


void init_pmm()
{
	uintptr_t kernel_end_addr = (uintptr_t)&_kernel_end;
	last_free_page = (kernel_end_addr + 4095) / 4096;
	int start_array_idx = last_free_page/64;
	
    for(int i =0;i<bm_size;i++)
    {
    	if(i < start_array_idx)bitmap[i] = 0xFFFFFFFFFFFFFFFFULL;
        else bitmap[i] = 0;
    }
}

void setbit(int ind,int off)
{
	bitmap[ind] |= (1ULL<<off);     
}
int getbit(int ind,int off)
{
	return bitmap[ind]>>off & 1ULL;
}
void freebit(int ind,int off)
{
	bitmap[ind] &= ~(1ULL << off); 
}


uintptr_t alloc_page(int n)
{
	int vi = last_free_page/(64);
	int vo = last_free_page%(64);
	int i = 0;
	while(1)
	{
		if(i>=n)break;
		int c_page = (vi*64+vo)+i;
		int c_idx = c_page / 64;
		int c_off = c_page % 64;
		if(getbit(c_idx,c_off) == 0)
		{
			i++;
		}
		else
		{
			int p = c_page +1;
			vi = p/64;
			vo = p%64;
			i = 0;
		}
	}
	for(i=0;i<n;i++)
	{
		int p = (vi*64+vo)+i;
		setbit(p/64,p%64);
	}
	last_free_page = (vi*64+vo)+n;
    return (vi*64+vo)*4096;
}

extern "C" void free_page(uintptr_t addr)
{
	int p = addr/4096;
	freebit(p/64,p%64);
	if(p<last_free_page)
	{
		last_free_page = p;
	}
}
