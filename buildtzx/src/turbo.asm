org 65345

LD_BYTES       INC  D                  ;This resets the zero flag (D cannot
                                       ;hold +FF)
               EX   AF,AF'             ;The A register holds +00 for a header
                                       ;and +FF for a block of data
               DEC  D                  ;Restore D to its original value
               DI                      ;The maskable interrupt is now disabled
               LD   A,$0F              ;The border is made WHITE
               OUT  ($FE),A
               LD   HL,SA_LD_RET       ;Pre-load the machine stack with the
               PUSH HL                 ;address - SA/LD-RET
               IN   A,($FE)            ;Make an initial read of port 254
               RRA                     ;Rotate the byte obtained
               AND  $20                ;but keep only the EAR bit
               OR   $02                ;Signal RED border
               LD   C,A                ;Store the value in the C register
                                       ;(+22 for 'off' and +02 for 'on' - the
                                       ;present EAR state)
               CP   A                  ;Set the zero flag

                                       ;The first stage of reading a tape involves showing that a pulsing signal
                                       ;actually exists. (i.e. 'On/off' or 'off/on' edges.)

LD_BREAK       RET  NZ                 ;Return if the BREAK key is being pressed
LD_START       CALL LD_EDGE_1          ;Return with the carry flag reset if
               JR   NC,LD_BREAK        ;there is no 'edge' within approx.
                                       ;14,000 T states. But if an 'edge' is
                                       ;found the border will go CYAN

                                       ;The next stage involves waiting a while and then showing that the signal is
                                       ;still pulsing.

               LD   HL,$0415           ;The length of this waiting period will
LD_WAIT        DJNZ LD_WAIT            ;be almost one second in duration.
               DEC  HL
               LD   A,H
               OR   L
               JR   NZ,LD_WAIT
               CALL LD_EDGE_2          ;Continue only if two edges are found
               JR   NC,LD_BREAK        ;within the allowed time period.

                                       ;Now accept only a 'leader signal'.

LD_LEADER      LD   B,$9C              ;The timing constant
               CALL LD_EDGE_2          ;Continue only if two edges are found
               JR   NC,LD_BREAK        ;within the allowed time period
               LD   A,$C6              ;However the edges must have been found
               CP   B                  ;within about 3,000 T states of each
               JR   NC,LD_START        ;other
               INC  H                  ;Count the pair of edges in the H
               JR   NZ,LD_LEADER       ;register until 256 pairs have been found

                                       ;After the leader come the 'off' and 'on' parts of the sync pulse.

LD_SYNC        LD   B,$C9              ;The timing constant
               CALL LD_EDGE_1          ;Every edge is considered until two edges
               JR   NC,LD_BREAK        ;are found close together - these will be
               LD   A,B                ;the start and finishing edges of the
               CP   $D4                ;'off' sync pulse
               JR   NC,LD_SYNC
               CALL LD_EDGE_1          ;The finishing edge of the 'on' pulse
               RET  NC                 ;must exist
                                       ;(Return carry flag reset)

                                       ;The bytes of the header or the program/data block can now be LOADed or VERIFied.
                                       ;But the first byte is the flag byte.

               LD   A,C                ;The border colours from now on will be
               XOR  $03                ;BLUE & YELLOW
               LD   C,A
               LD   H,$00              ;Initialize the 'parity matching' byte
                                       ;to zero
               LD   B,$80              ;Set the timing constant for the flag
                                       ;byte.
               JR   LD_MARKER          ;Jump forward into the byte LOADing loop

                                       ;The byte LOADing loop is used to fetch the bytes one at a time. The flag byte is
                                       ;first. This is followed by the data bytes and the last byte is the 'parity'
                                       ;byte.

LD_LOOP        EX   AF,AF'             ;Fetch the flags
               JR   Z,LD_NEXT          ;Jump forward to LOAD the next byte
LD_FLAG        XOR  L                  ;Return now if the flag byte does not
               RET  NZ                 ;match the first byte on the tape
               JR   LD_DEC

LD_NEXT        LD   (IX+00),L          ;Make the actual LOAD when required
	       INC  IX                 ;Increase the 'destination'
      	       DEC  DE                 ;Decrease the 'counter'
LD_DEC         EX   AF,AF'             ;Save the flags
               LD   B,$82              ;Set the timing constant
LD_MARKER      LD   L,$01              ;Clear the 'object' register apart from
                                       ;a 'marker' bit

                                       ;The 'LD-8-BITS' loop is used to build up a byte in the L register.

LD_8_BITS      CALL LD_EDGE_2          ;Find the length of the 'off' and 'on'
                                       ;pulses of the next bit
               RET  NC                 ;Return if the time period is exceeded
                                       ;(Carry flag reset)
               LD   A,$92              ;Compare the length against approx.
               CP   B                  ;2,400 T states; resetting the carry flag
                                       ;for a '0' and setting it for a '1'
               RL   L                  ;Include the new bit in the L register
               LD   B,$80              ;Set the timing constant for the next bit
               JR   NC,LD_8_BITS       ;Jump back whilst there are still bits to
                                       ;be fetched

                                       ;The 'parity matching' byte has to be updated with each new byte.

               LD   A,H                ;Fetch the 'parity matching' byte and
               XOR  L                  ;include the new byte
               LD   H,A                ;Save it once again

                                       ;Passes round the loop are made until the 'counter' reaches zero. At that point
                                       ;the 'parity matching' byte should be holding zero.

               LD   A,D                ;Make a furter pass if the DE register
               OR   E                  ;pair does not hold zero
               JR   NZ,LD_LOOP
               LD   A,H                ;Fetch the 'parity matching' byte
               CP   $01                ;Return with the carry flag set if the
               RET                     ;value is zero (Carry flag reset if in
                                       ;error)


                                       ;THE 'LD-EDGE-2' and 'LD-EDGE-1' SUBROUTINES
                                       ;These two subroutines form the most important part of the LOAD/VERIFY operation.
                                       ;The subroutines are entered with a timing constant in the B register, and the
                                       ;previous border colour and 'edge-type' in the C register.
                                       ;The subroutines return with the carry flag set if the required number of 'edges'
                                       ;have been found in the time allowed; and the change to the value in the B
                                       ;register shows just how long it took to find the 'edge(s)'.
                                       ;The carry flag will be reset if there is an error. The zero flag then signals
                                       ;'BREAK pressed' by being reset, or 'time-up' by being set.
                                       ;The entry point LD-EDGE-2 is used when the length of a complete pulse is
                                       ;required and LD-EDGE-1 is used to find the time before the next 'edge'.

LD_EDGE_2      CALL LD_EDGE_1          ;In effect call LD-EDGE-1 twice;
               RET  NC                 ;returning in between in there is an
                                       ;error.
LD_EDGE_1      LD   A,$09              ;Wait 358 T states before entering the
LD_DELAY       DEC  A                  ;sampling loop
               JR   NZ,LD_DELAY
               AND  A

                                       ;The sampling loop is now entered. The value in the B register is incremented for
                                       ;each pass; 'time-up' is given when B reaches zero.

LD_SAMPLE      INC  B                  ;Count each pass
               RET  Z                  ;Return carry reset & zero set if
                                       ;'time-up'.
               LD   A,$7F              ;Read from port +7FFE
               IN   A,($FE)            ;i.e. BREAK and EAR
               RRA                     ;Shift the byte
               RET  NC                 ;Return carry reset & zeroreset if BREAK
                                       ;was pressed
               XOR  C                  ;Now test the byte against the 'last
               AND  $20                ;edge-type'
               JR   Z,LD_SAMPLE        ;Jump back unless it has changed

                                       ;A new 'edge' has been found within the time period allowed for the search.
                                       ;So change the border colour and set the carry flag.

               LD   A,C                ;Change the 'last edge-type' and border
               CPL                     ;colour
               LD   C,A
               AND  $02                ;Keep only the border colour
               OR   $08                ;Signal 'MIC off'
               OUT  ($FE),A            ;Change the border colour (RED/CYAN or
                                       ;BLUE/YELLOW)
               SCF                     ;Signal the successful search before
               RET                     ;returning


SA_LD_RET      PUSH AF                 ;Save the carry flag (It is reset after
                                       ;a LOADing error)
               LD   A,(23624)          ;Fetch the original border colour from
               AND  $38                ;its system variable
               RRCA                    ;Move the border colour to bits 2, 1 & 0
               RRCA
               RRCA
               OUT  ($FE),A            ;Set the border to its original colour
               LD   A,$7F              ;Read the BREAK key for a last time
               IN   A,($FE)
               RRA
               EI                      ;Enable the maskable interrupt
               JR   C,SA_LD_END        ;Jump unless a break is to be made

                                       ;Report D - BREAK-CONT repeats

REPORT_D       RST  0008               ;Call the error handling routine
               DEFB $0C

                                       ;Continue here.

SA_LD_END      POP  AF                 ;Retrieve the carry flag
               RET                     ;Return to the calling routine