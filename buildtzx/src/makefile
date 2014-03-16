all: buildtzx.exe

clean:
	del uto*.bin
	del turbo.bin
	del rom_modified.bin
	del *.bak
	del utoload*.h

buildtzx.exe: buildtzx.c basic_loader.h utoload_1500.h utoload_rom.h utoload_2250.h buildtzx.h
	gcc buildtzx.c -o buildtzx.exe

basic_loader.h: basic_loader.bin
	bin2c basic_loader.bin basic_loader basic_loader

utoload_1500.h: utoload_1500.bin
	bin2c utoload_1500.bin utoload_1500 utoload_1500

utoload_rom.h: utoload_rom.bin
	bin2c utoload_rom.bin utoload_rom utoload_rom
	
utoload_2250.h: utoload_2250.bin
	bin2c utoload_2250.bin utoload_2250 utoload_2250	

utoload_1500.bin: utoload_1500.asm rom_modified.bin
	pasmo utoload_1500.asm utoload_1500.bin
	
utoload_2250.bin: utoload_2250.asm turbo.bin
	pasmo utoload_2250.asm utoload_2250.bin
	
utoload_rom.bin: utoload_rom.asm
	pasmo utoload_rom.asm utoload_rom.bin
	
rom_modified.bin: rom_modified.asm
	pasmo rom_modified.asm rom_modified.bin
	
turbo.bin: turbo.asm
	pasmo turbo.asm turbo.bin