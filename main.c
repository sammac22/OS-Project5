/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int nfilled = 0;
int dr_count = 0; 	//disk read count
int dw_count =0;	//disk write count
int pg_faults = 0;	//page fault count
int sam_count = 0;	//count for fifo algorithm

int * frame_table;

const char *algo;
struct disk *disk;


int random_algo(int nframes){
	return rand() % nframes;
}

int fifo_algo(int nframes){
	if (sam_count<nframes){
		return sam_count++;
	}

	sam_count=0;
	return sam_count++;
}

int custom_algo(int nframes, struct page_table *pt){
	int checked = 0;
	int f; //frame
	int bit_check;
	while(1){
		if(checked >= nframes){
			return random_algo(nframes);
		}
		if(sam_count > nframes){
			sam_count = 0;
		}
		page_table_get_entry(pt,sam_count,&f,&bit_check);
		if((PROT_WRITE & bit_check)==0){
			checked++;
			sam_count++;
			continue;
		}
		return sam_count++;
	}

}

void page_fault_handler( struct page_table *pt, int page )
{
	pg_faults = pg_faults+1;
	//printf("page fault on page #%d\n",page);

	unsigned char *physmem = page_table_get_physmem(pt);

	int npages = page_table_get_npages(pt);
	int nframes = page_table_get_nframes(pt);

	int f; //frame
	int bit_check;

	page_table_get_entry(pt,page,&f,&bit_check);
	if(bit_check == 0){
		for(nfilled=0; nfilled < nframes; nfilled++){
			if(frame_table[nfilled]==-1){
				frame_table[nfilled]= page;
				page_table_set_entry(pt,page,nfilled,PROT_READ);
				//second chance
				disk_read(disk,page,&physmem[nfilled*PAGE_SIZE]);
				dr_count=dr_count+1;
				return;
			}
		}
		//remove represents the frame of the page I am going to evict
		int remove = -1;
		//printf("replacing a page\n");

		if(!strcmp(algo,"rand")){
			remove = random_algo(nframes);
		}else if(!strcmp(algo,"fifo")){
			remove = fifo_algo(nframes);
		}else  if(!strcmp(algo,"custom")){
			remove = custom_algo(nframes,pt);
		}

		int throw_out_page = frame_table[remove];
		int bits;

		page_table_get_entry(pt,throw_out_page,&remove,&bits);
		if((PROT_WRITE & bits) != 0){
			disk_write(disk,throw_out_page,&physmem[remove*PAGE_SIZE]);
			dw_count=dw_count+1;
		}
		// read new page in its place
		disk_read(disk,page,&physmem[remove*PAGE_SIZE]);
		dr_count=dr_count+1;

		//update page table (2 entries)
		page_table_set_entry(pt,page,remove,PROT_READ);
		page_table_set_entry(pt,throw_out_page, 0 ,0); //dthain apparently does this
		frame_table[remove] =page;
		//done
		return;
	}
	else if((PROT_WRITE & bit_check)==0){
		page_table_set_entry(pt,page,f,bit_check | PROT_WRITE);
		// buffer2[f]=1;
		// second chance
	}
	else if((PROT_EXEC & bit_check) == 0){
		page_table_set_entry(pt,page,f,bit_check | PROT_EXEC);
	}
}

int main( int argc, char *argv[] )
{
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <alpha|beta|gamma|delta>\n");
		return 1;
	}

	int npages = atoi(argv[1]);
	int nframes = atoi(argv[2]);
	frame_table = malloc(sizeof(int)*nframes);
	int i;
	for(i=0; i<nframes;i++){
		frame_table[i]=-1;
	}
	algo = argv[3];
	const char *program = argv[4];

	//struct disk *disk = disk_open("myvirtualdisk",npages);
	disk = disk_open("myvirtualdisk",npages);
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}


	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	unsigned char *virtmem = page_table_get_virtmem(pt);

	unsigned char *physmem = page_table_get_physmem(pt);

	if(!strcmp(program,"alpha")) {
		alpha_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"beta")) {
		beta_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"gamma")) {
		gamma_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"delta")) {
		delta_program(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[4]);
		return 1;
	}

	page_table_delete(pt);
	disk_close(disk);
	printf("pg faults: %d\n", pg_faults);

	return 0;
}
}
