org 24000

             jp start_load

; aPPack decompressor
; original source by dwedit
; very slightly adapted by utopian
; optimized by Metalbrain

;hl = source
;de = dest

depack:         ld      ixl,128
apbranch1:      ldi
aploop0:        ld      ixh,1           ;LWM = 0
aploop:         call    ap_getbit
               jr      nc,apbranch1
               call    ap_getbit
               jr      nc,apbranch2
               call    ap_getbit
               jr      nc,apbranch3

               ld      bc,16           ;get an offset
apget4bits:     call    ap_getbit
               rl      c
               jr      nc,apget4bits
               jr      nz,apbranch4
               ld      a,b
apwritebyte:    ld      (de),a          ;write a 0
               inc     de
               jr      aploop0
apbranch4:      and     a
               ex      de,hl           ;write a previous byte (1-15 away from dest)
               sbc     hl,bc
               ld      a,(hl)
               add     hl,bc
               ex      de,hl
               jr      apwritebyte
apbranch3:      ld      c,(hl)          ;use 7 bit offset, length = 2 or 3
               inc     hl
               rr      c
               ret     z               ;if a zero is encountered here, it's EOF
               ld      a,2
               ld      b,0
               adc     a,b
               push    hl
               ld      iyh,b
               ld      iyl,c
               ld      h,d
               ld      l,e
               sbc     hl,bc
               ld      c,a
               jr      ap_finishup2
apbranch2:      call    ap_getgamma     ;use a gamma code * 256 for offset, another gamma code for length
               dec     c
               ld      a,c
               sub     ixh
               jr      z,ap_r0_gamma           ;if gamma code is 2, use old r0 offset,
               dec     a
               ;do I even need this code?
               ;bc=bc*256+(hl), lazy 16bit way
               ld      b,a
               ld      c,(hl)
               inc     hl
               ld      iyh,b
               ld      iyl,c

               push    bc

               call    ap_getgamma

               ex      (sp),hl         ;bc = len, hl=offs
               push    de
               ex      de,hl

               ld      a,4
               cp      d
               jr      nc,apskip2
               inc     bc
               or      a
apskip2:        ld      hl,127
               sbc     hl,de
               jr      c,apskip3
               inc     bc
               inc     bc
apskip3:        pop     hl              ;bc = len, de = offs, hl=junk
               push    hl
               or      a
ap_finishup:    sbc     hl,de
               pop     de              ;hl=dest-offs, bc=len, de = dest
ap_finishup2:   ldir
               pop     hl
               ld      ixh,b
               jr      aploop

ap_r0_gamma:    call    ap_getgamma             ;and a new gamma code for length
               push    hl
               push    de
               ex      de,hl

               ld      d,iyh
               ld      e,iyl
               jr      ap_finishup


ap_getbit:      ld      a,ixl
               add     a,a
               jr	nz,ap_getbitend
               ld      a,(hl)
               inc     hl
               rla
ap_getbitend:  ld      ixl,a
               ret

ap_getgamma:    ld      bc,1
ap_getgammaloop:call    ap_getbit
               rl      c
               rl      b
               call    ap_getbit
               jr      c,ap_getgammaloop
               ret

; This is not a truly correct way of doing it, as we are ignoring the
; current value of $5B5C, but it seems to be the only way to make it
; work in USR0 mode

set_ram_bank:  or $10			; select always ROM1 (128K) or ROM3 (+2A/+3)
               ld BC, $7FFD
               ld ($5B5C), A           ; store the new bank in the ROM variable
               out (C), A              ; and change the RAM bank
               ret


start_load:     ; copy turbo loader to high RAM
		ld hl, TURBO_LOADER
		ld bc, 197
		ld de, 65345
		ldir			
		
		ld hl, loader_table
	        ld b, (hl)	; b==iteratons
		inc hl
load_loop:
		push bc
		ld e, (hl)		
		inc hl
		ld d, (hl)
		inc hl			; de==load address, hl== address of size
		push de
		pop ix			; ix==load address
		ld e, (hl)		
		inc hl
		ld d, (hl)
		inc hl			; ix==load address, hl== start of store address, de == length
		push ix
		push hl
		push de			; save for later
                ld a, 255               ; flag (data block)
                call 65345		; call the modified ROM routine
   		; check for errors!!!!!
		pop de
		pop hl
		pop ix
		ld c, (hl)
		inc hl
		ld b, (hl)
		inc hl	
		ld a, (hl)		; a== RAM BANK & compressed, bc == store address, ix == load address
					; de= length, 
		di			; disable interrupts
		push bc
    		and $07			; get the BANK bits
                call set_ram_bank       ; set the RAM BANK
		pop bc			; restore BC (used by set_ram_bank)
		ld a, (hl)
		push hl			; save HL for the time being!
		and $80		
		jr z, not_compressed	
compressed:	push ix
		ld d,b
		ld e,c		; de = destination address
		pop hl		; hl = source address
		push iy		; save IY, it is needed while we use the standard ROM interrupt handler
                call depack             ; call depacker		
		pop iy
		jr continue_loop

not_compressed:	; block is not compressed
		push bc
		push ix
		ld b,d
		ld c,e		; bc = length
		pop hl		; hl = source address
		pop de		; de = dest address
		ldir		; just copy!

continue_loop:  xor a
                call set_ram_bank       ; set the RAM BANK back to 0
		pop hl			; get the HL pointer back!
		inc hl			; and point to the next block
		pop bc			; get the counter back!
		ei			; re-enable interrupts
		djnz load_loop

launch_program: jp (hl)		; go and run!


TURBO_LOADER 	INCBIN "rom_modified.bin"	; include "turbo" loader


loader_table: 
; the loader table is composed of the following data
; number of entries (byte)
; repeat number of entries
;	 where to load (word)
;	 length (word)
;	 where to store after loading (word) 
;	 rambank/compressed (byte) -> X000YYY
;		X=1: compressed, X=0: not compressed
;		YYY: RAM Bank (0-7)
; execution address(word)
;	db 5		; 5 entries
;
;	dw 32768	; load at 32768
;	dw 6912		; load 6912 bytes
;	dw 16384	; after loading, copy to address 16384 (screen)
;	db $00		; RAM Bank 0, not compressed
;
;	dw 32768	; load at 32768
;	dw 5689		; load 6912 bytes
;	dw 49152	; after loading, copy to address 49152
;	db $01		; RAM Bank 1, not compressed
;
;	dw 32768	; load at 32768
;	dw 11607		; load x bytes
;	dw 49152	; after loading, copy to address 49152
;	db $03		; RAM Bank 1, not compressed
;	
;	dw 24600	; load at 24600
;	dw 11630	; load 11630 bytes
;	dw 24600	; after loading, copy to address 24600 (no copy)
;	db $00		; RAM Bank 0, not compressed
;	
;	dw 36230 	; load at 36230
;	dw 24616	; load 24616 bytes
;	dw 36230	; after loading, copy to address 36230 (no copy)
;	db $00		; RAM Bank 0, not compressed
;
;	dw 36230	; randomize usr 36230
;
