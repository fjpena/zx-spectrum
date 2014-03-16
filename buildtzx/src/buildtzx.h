#ifndef BUILDTZX_H
#define BUILDTZX_H

// Struct definitions


typedef struct TZXHeader
{
 unsigned char signature[7];	// ZXTape!
 unsigned char eof;		// 0x1A
 unsigned char major;		// 1
 unsigned char minor;		// 20 
}TZXHeader;

typedef struct TZXTurboBlock
{
 unsigned char id;		// 0x11
 unsigned char pilot_low;
 unsigned char pilot_high;	// Length of PILOT pulse (2168)
 unsigned char sync1st_low;
 unsigned char sync1st_high;	// Length of first SYNC pulse (667)
 unsigned char sync2nd_low;
 unsigned char sync2nd_high;	// Length of second SYNC pulse (667)
 unsigned char zero_low;
 unsigned char zero_high;	// Length of zero BIT pulse (ROM default: 855; my turbo loader: 518)
 unsigned char one_low;
 unsigned char one_high;	// Length of one BIT pulse (ROM default: 1710; my turbo loader: 1036)
 unsigned char pilottone_low;
 unsigned char pilottone_high;  // Length of the pilot tone (8063 for header -not used-, 3223 for data)
 unsigned char usedbits;	// Used bits in the last bite (8)
 unsigned char pause_low;
 unsigned char pause_high;	// pause after this block (ms.)
 unsigned char length_low;
 unsigned char length_mid;
 unsigned char length_high;	// Length in bytes, not counting marker and checksum. High byte is usually 0
 unsigned char *data;		// Data 
}TZXTurboBlock;

typedef struct TZXROMBlock
{
 unsigned char id;		// 0x10
 unsigned char pause_low;
 unsigned char pause_high;	// pause after this block (ms.)
 unsigned char length_low;
 unsigned char length_high;	// Length in bytes, not counting marker and checksum
 unsigned char *data;		// Data
}TZXROMBlock;

typedef struct HeaderBlock
{
 unsigned char type;		// 0 for program, 3 for code
 char name[10];			// Pad character is 0x20
 unsigned char length_low;
 unsigned char length_high;	// Length of data block, not including the flag and checksum bytes
 unsigned char param1_low;
 unsigned char param1_high;	// For program, the start line. For code, the load address
 unsigned char param2_low;
 unsigned char param2_high;	// For program, the start of the variable area. For code, 32768
}HeaderBlock;

typedef struct Block
{
 char filename[128]; // Max character length
 int load_address, copy_address, bank, compressed; 	
}Block;

typedef struct Tape
{
 int nblocks; // Number of blocks
 Block blocks[256];
 int randomize_addr;		
}Tape;


#endif

