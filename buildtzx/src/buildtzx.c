// Tool to create a TZX file with a custom loader, from a bunch of binary files
//
// By utopian / CEZ GS
//
// Usage:  buildtzx -l[1|2|3] -i <template_file.txt> -o <file.tzx> -n <BASIC loader name>
//
// 	-l: Loader type
//		1: standard ROM loader, headerless (261 bytes for the loader, + the table)
//		2: modified ROM loader, just with a different color, headerless (453 bytes for the loader, + the table)
//		3: Turbo loader, based on the ROM code, headerless (453 bytes for the loader, + the table)
//
//	-i: template file. This file will specify the binary files to include in the TZX, in a special format:
//	
//	# Sample template file. Comment liness start with #, no comments allowed in real lines
//	# Separator character is BLANK
//	# The format is
//	#
//	# <number of entries>   <- Then, for each entry
//	# <filename> <where to load> <where to copy after loading> <RAM bank to store at> <1|0>  
//	# (1 means compress block before storing, 0 do not compress)
//	# <execution address> 	<- RANDOMIZE USR value!
//	#
//	# Example: load a compressed screen$ at 32768, then uncompress it to 16384, then RANDOMIZE USR 12345
//	# 1
//	# myscr.scr 32768 16384 0 1
//	# 12345
//	# 
//
//	-o output file (TZX)
//
//	-n BASIC loader file name, max 10 characters
//#define DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include all loaders...
#include "basic_loader.h"
#include "utoload_1500.h"
#include "utoload_2250.h"
#include "utoload_rom.h"
// Struct definitions
#include "buildtzx.h"

const char *packer_default="appack.exe";
const char *packer;


// Global variables
unsigned char loader_type=0;
char *template_filename=NULL;
char *tzx_filename=NULL;
char *BASIC_name=NULL;
Tape mytape;
FILE *outfile=NULL;
unsigned char ASMloader[1024]; // Buffer for the ASM loader
unsigned char binary_block[65536]; // Maximum block size will be 64K
char commandline[256];	// For command line execution


void usage(void)
{
printf("Tool to create a TZX file with a custom loader, from a set of binary files\n");
printf("\n");
printf("By utopian / RetroWorks\n");
printf("\n");
printf("Usage:  buildtzx -l [1|2|3] -i <template_file.txt> -o <file.tzx> -n <BASIC name>\n");
printf("\n");
printf("-l: Loader type\n");
printf("	1: standard ROM loader, headerless (261 bytes + load table)\n");
printf("	2: modified ROM loader, just with a different color, headerless\n");
printf("	   (453 bytes + load table)\n");
printf("	3: Turbo loader, based on the ROM code, headerless\n");
printf("	   (453 bytes + load table)\n");
printf("\n");
printf("-i: template file. Specify the binary files to include in the TZX:\n");
printf("\n");
printf("\n");
printf("-o output file (TZX)\n");
printf("\n");
printf("-n BASIC loader file name, max 10 characters\n");
printf("\n");
}

// Returns 0 if all parameters are correct, 1 if something is not correct, 2 if a parameter is missing (help)

int check_parameters(char **argv)
{
	int i;
	for(i=1;i<9;i+=2)
	{
		if(!strcmp(argv[i],"-l") || !strcmp(argv[i],"-L")) // Get the loader type
		{
			loader_type = atoi(argv[i+1]);
			if((loader_type < 1) || (loader_type > 3))
			{
				printf("Wrong loader type. Run buildtzx with no arguments for help.\n");	
				return 1;
			}
		}
		
		if(!strcmp(argv[i],"-i") || !strcmp(argv[i],"-I")) // Get the template file name
		{
			template_filename = argv[i+1];
		}		
		if(!strcmp(argv[i],"-o") || !strcmp(argv[i],"-O")) // Get the output TZX file name
		{
			tzx_filename = argv[i+1];
		}	
		if(!strcmp(argv[i],"-n") || !strcmp(argv[i],"-N")) // Get the BASIC program name
		{
			BASIC_name = argv[i+1];
		}							
	}
	if(loader_type && template_filename && tzx_filename && BASIC_name) // No parameter is NULL, so it must be more or less ok
		return 0;
	else
		return 2; // Some parameter is wrong		
}

// 0: file successfully parsed
// 1: error in parse

int parse_template_file(void)
{
	FILE *in;
	char buffer[256];
	int currblock;
		
	mytape.nblocks=currblock=0;
	
	in=fopen(template_filename,"r");
	if(!in)
	{
		printf("Error opening template file %s\n",template_filename);
		return 1;	
	}
	while(fgets(buffer,255,in))
	{
		if(buffer[0] == '#') continue; // ignore comment lines
		if(!mytape.nblocks) // First non-comment is number of blocks
		{
			mytape.nblocks=atoi(buffer);
			#ifdef DEBUG
			printf("Number of blocks: %d\n",mytape.nblocks);
			#endif
		}
		else if (currblock == mytape.nblocks) // last line is randomize addr
		{
			mytape.randomize_addr=atoi(buffer);
			#ifdef DEBUG
			printf("Randomize address: %d\n",mytape.randomize_addr);
			#endif
		}		
		else
		{
		 sscanf(buffer,"%s %d %d %d %d", mytape.blocks[currblock].filename,
		 				&(mytape.blocks[currblock].load_address), 
		 				&(mytape.blocks[currblock].copy_address),
		 				&(mytape.blocks[currblock].bank),
		 				&(mytape.blocks[currblock].compressed));
		 #ifdef DEBUG
		 printf("\nFile name: %s\n",mytape.blocks[currblock].filename);
		 printf("Load address: %d\n",mytape.blocks[currblock].load_address);
		 printf("Copy address: %d\n",mytape.blocks[currblock].copy_address);
		 printf("RAM bank: %d\n",mytape.blocks[currblock].bank);
		 printf("Compressed: %d\n",mytape.blocks[currblock].compressed);
		 #endif
		 currblock++;	
		}				
	}		
	fclose(in);	
	return 0;
}

int write_tzx_header(void)
{
	TZXHeader header;

	outfile=fopen(tzx_filename,"wb");
	if(!outfile)	
	{
		printf("Error opening output file %s\n",tzx_filename);
		return 1; // Error opening file
	}	

	header.signature[0]='Z';
	header.signature[1]='X';
	header.signature[2]='T';
	header.signature[3]='a';
	header.signature[4]='p';
	header.signature[5]='e';
	header.signature[6]='!';
	header.eof=0x1a;
	header.major=1;
	header.minor=20;

	if(fwrite(&header,sizeof(header),1,outfile) != 1)
	{
		printf("Error writing TZX header\n");
		fclose(outfile);
		return 1;
	}	
	return 0;
}

int write_TZXROMblock(TZXROMBlock *block, int isheader)
{
	unsigned char checksum;  // Calculate checksum
	int i,length;
	unsigned char byte;

	if(fwrite(block,5,1,outfile)!=1)  // We know the TZX ROM block is 5 bytes long
	{
		printf("Error writing TZX block\n");
		fclose(outfile);
		return 1;
	}	

	if(isheader)
		byte=0;			// This is a header block
	else	
		byte=0xff;		// This is a data block

	if(fwrite(&byte,1,1,outfile)!=1)
	{
		printf("Error writing TZX block\n");
		fclose(outfile);
		return 1;
	}	

	length = block->length_high;
	length <<= 8;
	length |= block->length_low;	// Reconstruct the length
	length -= 2;			// And adjust (no flag, no checksum!)

	if(fwrite(block->data,length,1,outfile) != 1) // Write the data itself
	{
		printf("Error writing TZX block\n");
		fclose(outfile);
		return 1;
	}

	// Finally, calculate checksum
	checksum=byte;			// first value is the flag byte
	for(i=0;i<length;i++)
		checksum ^= block->data[i];
	if(fwrite(&checksum,1,1,outfile) != 1)	// Write checksum to the tzx block
	{
		printf("Error writing TZX block\n");
		fclose(outfile);
		return 1;
	}

	return 0;
}

int write_TZXturboblock(TZXTurboBlock *block, int isheader)
{
	unsigned char checksum;  // Calculate checksum
	int i,length;
	unsigned char byte;

	if(fwrite(block,19,1,outfile) != 1)  // We know the TZX turbo block is 19 bytes long
	{
		printf("Error writing TZX block\n");
		fclose(outfile);
		return 1;
	}

	if(isheader)
		byte=0;			// This is a header block
	else	
		byte=0xff;		// This is a data block

	if( fwrite(&byte,1,1,outfile) != 1 )
	{
		printf("Error writing TZX block\n");
		fclose(outfile);
		return 1;
	}


	length = block->length_mid;
	length <<= 8;
	length |= block->length_low;	// Reconstruct the length
	length -= 2;			// And adjust (no flag, no checksum!)

	if(fwrite(block->data,length,1,outfile) != 1) // Write the data itself
	{
		printf("Error writing TZX block\n");
		fclose(outfile);
		return 1;
	}
	// Finally, calculate checksum
	checksum=byte;			// first value is the flag byte
	for(i=0;i<length;i++)
		checksum ^= block->data[i];
	if(fwrite(&checksum,1,1,outfile) != 1)	// Write checksum to the tzx block
	{
		printf("Error writing TZX block\n");
		fclose(outfile);
		return 1;
	}
	return 0;
}


int write_basic_loader(void)
{
	HeaderBlock header;
	TZXROMBlock tzxheader;
	int i;

	tzxheader.id=0x10;
	tzxheader.pause_low = 1000 & 0xff;	     // 1 second pause after the block
	tzxheader.pause_high = (1000 & 0xff00) >> 8;
	tzxheader.length_low = 19;		     // 17 bytes
	tzxheader.length_high = 0;
	tzxheader.data = (unsigned char *)(&header);  		     // Data is the actual speccy header


	header.type=0; // program
	for(i=0;i<strlen(BASIC_name),i<10;i++)
	{
	 header.name[i] = BASIC_name[i];
	}
	for(i=strlen(BASIC_name);i<10;i++) 
		header.name[i]=0x20; // Pad character

	header.length_low = BASIC_LOADER_LEN & 0xff;
	header.length_high = (BASIC_LOADER_LEN & 0xff00) >> 8;	
	header.param1_low = 0;	// Start line
	header.param1_high = 0;
	header.param2_low= header.length_low; // Start of the variable area
	header.param2_high= header.length_high;  // according to some tests, this works (?)

	if(write_TZXROMblock(&tzxheader,1)) return 1;	// Write header
	
	tzxheader.id=0x10;
	tzxheader.pause_low = 1000 & 0xff;	     // 1 second pause after the block
	tzxheader.pause_high = (1000 & 0xff00) >> 8;
	tzxheader.length_low = (BASIC_LOADER_LEN+2) & 0xff;		     // BASIC listing length
	tzxheader.length_high = ((BASIC_LOADER_LEN+2) & 0xff00) >> 8;
	tzxheader.data = basic_loader;  		     // Data 

	if(write_TZXROMblock(&tzxheader,0)) return 1;	// Write data


	printf("PROGRAM: %18s \n",BASIC_name);

	return 0; // Ok!
}

int filelength(char *file)
{
	FILE *in;
	int length;
	
	in=fopen(file,"rb");
	if (!in) return 0;
	fseek(in,0,SEEK_END);
	length=ftell(in);
	fclose(in);
	
	return length; 
}

int read_file(char *filename)
{
	FILE *in;
	int length;
	
	in=fopen(filename,"rb");
	if (!in) return 0;
	fseek(in,0,SEEK_END);
	length=ftell(in);
	fseek(in,0,SEEK_SET);
	if(fread(binary_block,length,1,in) != 1)
	{
		fclose(in);
		return 0;
	}
	fclose(in);	
	return length; 
}

void build_load_table(unsigned char *addr)
{
 unsigned char dummy;
 int length;
 int i;


 *(addr++) = (unsigned char)(mytape.nblocks); // First, the # of blocks
 for(i=0;i<mytape.nblocks;i++)
 {

 	*(addr++) = (unsigned char)(mytape.blocks[i].load_address & 0xff);  // Load address
	*(addr++) = (unsigned char)((mytape.blocks[i].load_address & 0xff00)>>8);
	// Load length. It will depend on the block being compressed or not, and so...

	if(mytape.blocks[i].compressed == 0) // Not a compressed block!
	{
		length = filelength(mytape.blocks[i].filename);
	#ifdef DEBUG
		printf("%s is not a compressed block. Length: %d bytes\n",mytape.blocks[i].filename,length);
	#endif
	 	*(addr++) = (unsigned char)(length & 0xff);  // Length
		*(addr++) = (unsigned char)((length & 0xff00)>>8);		
	}
	else	// Compressed block
	{
		// To know the compressed size, we need to compress :)
	#ifdef LINUX
		sprintf(commandline,"%s %s buildtzx.tmp > /dev/null",packer,mytape.blocks[i].filename);
	#else
		sprintf(commandline,"%s %s buildtzx.tmp",packer,mytape.blocks[i].filename);
	#endif
	#ifdef DEBUG
		printf("Going to execute: %s\n", commandline);
	#endif
		system(commandline);

		length = filelength("buildtzx.tmp");
	#ifdef DEBUG
		printf("%s is a compressed block. Length: %d bytes\n",mytape.blocks[i].filename,length);
	#endif
	 	*(addr++) = (unsigned char)(length & 0xff);  // Length
		*(addr++) = (unsigned char)((length & 0xff00)>>8);	

	#ifdef LINUX
		sprintf(commandline,"rm -f buildtzx.tmp");
	#else
		sprintf(commandline,"del buildtzx.tmp");
	#endif
	#ifdef DEBUG
		printf("Going to execute: %s\n", commandline);	
	#endif
		system(commandline);
	}	
 	*(addr++) = (unsigned char)(mytape.blocks[i].copy_address & 0xff);  // Copy address
	*(addr++) = (unsigned char)((mytape.blocks[i].copy_address & 0xff00)>>8);
	
	dummy = mytape.blocks[i].bank;
	if (mytape.blocks[i].compressed) dummy |= 0x80; // If compressed, set the flag
	*(addr++) = dummy;	
 }

 *(addr++) = (unsigned char)0xc3;				    // Jump opcode
 *(addr++) = (unsigned char)(mytape.randomize_addr & 0xff);
 *(addr) = (unsigned char)((mytape.randomize_addr & 0xff00) >> 8);  // Finally, the randomize address
}

int write_asm_loader(void)
{
	HeaderBlock header;
	TZXROMBlock tzxheader;
	int i;
	int table_size; // size of the load table

	tzxheader.id=0x10;
	tzxheader.pause_low = 1000 & 0xff;	     // 1 second pause after the block
	tzxheader.pause_high = (1000 & 0xff00) >> 8;
	tzxheader.length_low = 19;		     // 17 bytes + flag + checksum
	tzxheader.length_high = 0;
	tzxheader.data = (unsigned char *)(&header);  		     // Data is the actual speccy header

	header.type=3; // code
	for(i=0;i<strlen(BASIC_name),i<10;i++)
	{
	 header.name[i] = BASIC_name[i];
	}
	for(i=strlen(BASIC_name);i<10;i++) 
		header.name[i]=0x20; // Pad character

	header.param1_low = 24000 & 0xff;	// Load address, same for all loaders
	header.param1_high = (24000 & 0xff00 ) >> 8;
	header.param2_low= 32768 & 0xff; // for code blocks, this is always 32768 (?)
	header.param2_high= (32768 & 0xff00 ) >> 8;

	// The formula for the load table size is:
	// 1 + (7*number of entries) + 3

	table_size = 4 + 7*mytape.nblocks;

	switch(loader_type)
	{
		case 1: // Standard ROM, headerless
			header.length_low = (UTOLOAD_ROM_LEN+table_size) & 0xff;
			header.length_high = ((UTOLOAD_ROM_LEN+table_size) & 0xff00) >> 8;	
			break;
		case 2: // Modified ROM (just different colors), headerless
			header.length_low =  (UTOLOAD_1500_LEN+table_size) & 0xff;
			header.length_high = ((UTOLOAD_1500_LEN+table_size) & 0xff00) >> 8;	
			break;
		case 3: // Modified ROM (turbo-2250, different colors), headerless
			header.length_low =  (UTOLOAD_2250_LEN+table_size) & 0xff;
			header.length_high = ((UTOLOAD_2250_LEN+table_size) & 0xff00) >> 8;	
			break;
		default: printf("Unknown loader type %d\n",loader_type);
			 return 1;
			 break;
	}
	if(write_TZXROMblock(&tzxheader,1))// Write header
	{
		printf("Error writing header block for loader\n");
		return 1;	
	}
	tzxheader.id=0x10;
	tzxheader.pause_low = 1000 & 0xff;	     // 1 second pause after the block
	tzxheader.pause_high = (1000 & 0xff00) >> 8;

	switch(loader_type)
	{
		case 1: // Standard ROM, headerless
			tzxheader.length_low = (UTOLOAD_ROM_LEN+2+table_size) & 0xff;		     // ASM binary length
			tzxheader.length_high = ((UTOLOAD_ROM_LEN+2+table_size) & 0xff00) >> 8;
			memcpy(ASMloader,utoload_rom,UTOLOAD_ROM_LEN);
			build_load_table(ASMloader+UTOLOAD_ROM_LEN);
			tzxheader.data = ASMloader;  		     // Data 
			break;
		case 2: // Modified ROM (just different colors), headerless
			tzxheader.length_low = (UTOLOAD_1500_LEN+2+table_size) & 0xff;		     // ASM binary length
			tzxheader.length_high = ((UTOLOAD_1500_LEN+2+table_size) & 0xff00) >> 8;
			memcpy(ASMloader,utoload_1500,UTOLOAD_1500_LEN);
			build_load_table(ASMloader+UTOLOAD_1500_LEN);
			tzxheader.data = ASMloader;  		     // Data 
			break;
		case 3: // Modified ROM (turbo-2250, different colors), headerless
			tzxheader.length_low = (UTOLOAD_2250_LEN+2+table_size) & 0xff;		     // ASM binary length
			tzxheader.length_high = ((UTOLOAD_2250_LEN+2+table_size) & 0xff00) >> 8;
			memcpy(ASMloader,utoload_2250,UTOLOAD_2250_LEN);
			build_load_table(ASMloader+UTOLOAD_2250_LEN);
			tzxheader.data = ASMloader;  		     // Data 
			break;
	}
	if(write_TZXROMblock(&tzxheader,0)) 
	{
		printf("Error writing binary block for loader\n");
		return 1;	// Write data
	}

	printf("CODE:    %18s 24000\n",BASIC_name);

	return 0; // Ok!
}




int write_turbo_blocks(void)
{
 int i,length;
 TZXROMBlock   romheader;
 TZXTurboBlock turboheader;

 switch(loader_type)
 {
	case 1: // The ROM loader and the 1500 loader use the same block type
	case 2:
	 	for(i=0;i<mytape.nblocks;i++)
	 	{ 
			romheader.id=0x10;
			romheader.pause_low = 1000 & 0xff;	     // 1 second pause after the block
			romheader.pause_high = (1000 & 0xff00) >> 8;
			if(mytape.blocks[i].compressed == 0) // Not a compressed block!
			{
				length = read_file(mytape.blocks[i].filename);  // Read the file and get the length
				if(!length)
				{
					printf("Cannot read file %s\n",mytape.blocks[i].filename);
					return 1;
				}
			}	
			else	// Compressed block
			{
				// To know the compressed size, we need to compress :)
			#ifdef LINUX
				sprintf(commandline,"%s %s buildtzx.tmp > /dev/null",packer,mytape.blocks[i].filename);
			#else
				sprintf(commandline,"%s %s buildtzx.tmp",packer,mytape.blocks[i].filename);
			#endif
				#ifdef DEBUG
					printf("Going to execute: %s\n", commandline);
				#endif
				system(commandline);
				length = read_file("buildtzx.tmp");
				#ifdef DEBUG
					printf("%s is a compressed block. Length: %d bytes\n",mytape.blocks[i].filename,length);
				#endif
			#ifdef LINUX
				sprintf(commandline,"rm -f buildtzx.tmp");
			#else
				sprintf(commandline,"del buildtzx.tmp");
			#endif
				#ifdef DEBUG
					printf("Going to execute: %s\n", commandline);	
				#endif
				system(commandline);
			}
			romheader.length_low = (length+2) & 0xff;		     // Binary length
			romheader.length_high = ((length+2) & 0xff00) >> 8;
			romheader.data = binary_block; 
		
			if(write_TZXROMblock(&romheader,0)) // Write data 		
			{	
				printf("Error writing block  from %s\n",mytape.blocks[i].filename);
				return 1;	
			}

			printf("BLOCK: %20s load at %5d, length %5d, copy to %5d, RAM bank: %1d compressed:%1d\n",
											   mytape.blocks[i].filename,
											   mytape.blocks[i].load_address,
											   length,
											   mytape.blocks[i].copy_address,
											   mytape.blocks[i].bank,
											   mytape.blocks[i].compressed);
	 	}
		break;
	case 3: // 2250 baud loader
	 	for(i=0;i<mytape.nblocks;i++)
	 	{ 
			turboheader.id=0x11;
			turboheader.pilot_low   = 2168 & 0xff;
			turboheader.pilot_high  = (2168 & 0xff00)>>8;	// Length of PILOT pulse (2168)
			turboheader.sync1st_low = 667 & 0xff;
			turboheader.sync1st_high= (667 & 0xff00)>>8;	// Length of first SYNC pulse (667)
			turboheader.sync2nd_low = 735 & 0xff;
			turboheader.sync2nd_high= (735 & 0xff00)>>8;	// Length of second SYNC pulse (735)
			turboheader.zero_low    = 518 & 0xff;
			turboheader.zero_high   = (518 & 0xff00) >> 8;	// Length of zero BIT pulse (ROM default: 855; 
									// my turbo loader: 518)
			turboheader.one_low     = 1036 & 0xff;
			turboheader.one_high    = (1036 & 0xff00) >>8;	// Length of one BIT pulse (ROM default: 1710; 
									// my turbo loader: 1036)
			turboheader.pilottone_low= 3223 & 0xff;
			turboheader.pilottone_high= (3223 & 0xff00) >>8;// Length of the pilot tone (8063 for header 
									//-not used-, 3223 for data)
			turboheader.usedbits = 8;	// Used bits in the last bite (8)
			turboheader.pause_low = 1000 & 0xff;
			turboheader.pause_high = (1000 & 0xff00)>>8;	// pause after this block (ms.)
			if(mytape.blocks[i].compressed == 0) // Not a compressed block!
			{
				length = read_file(mytape.blocks[i].filename);  // Read the file and get the length
				if(!length)
				{
					printf("Cannot read file %s\n",mytape.blocks[i].filename);
					return 1;
				}
			}	
			else	// Compressed block
			{
				// To know the compressed size, we need to compress :)
			#ifdef LINUX
				sprintf(commandline,"%s %s buildtzx.tmp > /dev/null",packer,mytape.blocks[i].filename);
			#else
				sprintf(commandline,"%s %s buildtzx.tmp",packer,mytape.blocks[i].filename);
			#endif
				#ifdef DEBUG
					printf("Going to execute: %s\n", commandline);
				#endif
				system(commandline);
				length = read_file("buildtzx.tmp");
				#ifdef DEBUG
					printf("%s is a compressed block. Length: %d bytes\n",mytape.blocks[i].filename,length);
				#endif
			#ifdef LINUX
				sprintf(commandline,"rm -f buildtzx.tmp");
			#else
				sprintf(commandline,"del buildtzx.tmp");
			#endif
				#ifdef DEBUG
					printf("Going to execute: %s\n", commandline);	
				#endif
				system(commandline);
			}
			turboheader.length_low = (length+2) & 0xff;		     // Binary length
			turboheader.length_mid = ((length+2) & 0xff00) >> 8;
			turboheader.length_high = 0;				// For now we will not support >64K blocks
			turboheader.data = binary_block; 
			if(write_TZXturboblock(&turboheader,0)) // Write data 		
			{	
				printf("Error writing block  from %s\n",mytape.blocks[i].filename);
				return 1;	
			}
			printf("%30s load at %d, copy to %d, RAM bank: %d compressed:%d\n",mytape.blocks[i].filename,
											   mytape.blocks[i].load_address,
											   mytape.blocks[i].copy_address,
											   mytape.blocks[i].bank,
											   mytape.blocks[i].compressed);
	 	}
		break;	
 }
 return 0;
}


// Main program: read arguments and go

int main(int argc, char **argv)
{
	unsigned char errorcode;
	
	if (argc != 9)
	{
		usage();
		return 1;
	}
	packer=getenv("PACKER");
	if (packer == NULL) 
		packer=packer_default; 
	else
		printf("Using %s as a packer\n\n",packer);

	

	errorcode=check_parameters(argv);
	switch(errorcode)
	{
		case 0: // All went fine
		#ifdef DEBUG
			printf("Loader type: %d\n",loader_type);
			printf("Template file name: %s\n",template_filename);
			printf("TZX output file name: %s\n",tzx_filename);
			printf("BASIC program name: %s\n",BASIC_name);
		#endif
			if(parse_template_file()) return 1;
			if(write_tzx_header()) return 1;
			if(write_basic_loader()) return 1;
			if(write_asm_loader()) return 1;
			if(write_turbo_blocks()) return 1;
			if(outfile) fclose(outfile);
			break;
		case 1: // Some parameter is wrong;
			return 1;
			break;
		case 2: // Wrong usage;
			usage();
			return 1;
			break;			
	}	
	return 0;
}

