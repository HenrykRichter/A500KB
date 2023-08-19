;APS00000723000000000000000000000000000000000000000000000000000000000000000000000000
; ciacomm.s
;
; (C) 2022 Henryk Richter
;
; communicate with A500KB over CIA serial port
;
; The Amiga CIA serial is a little different from RS232 interfaces.
; It is a direct 8 Bit serial I/O. 
;
; It's use is a little tricky (or hacky) as the keyboard input 
; is tied to the keyboard.device and is not meant to be overridden.
;
; This program allocates CIA-A timer A and will fail when that timer
; (needed for serial output) is unavailable. 
;
; Please note that these functions are "one off" and not re-entrant.
;
;
	incdir	include:
	include	"exec/interrupts.i"
	include	"hardware/cia.i"
	include "lvo/exec_lib.i"
	include "lvo/cia_lib.i"
	include "dos/dos.i"

_ciaa	EQU	$BFE001

; keyboard returns ACK/NACK when an incoming sequence was detected
; or does nothing when the start of sequence was missed, also a 
; classic keyboard won't answer at all
CMD_IDLE	EQU	0	;we haven't sent anything
CMD_ACK		EQU	$80|$73	;ACK = OK
CMD_NACK	EQU	$80|$77	;failed reception
CMD_TIMEOUT	EQU	$5A	;no answer
CMD_ACK1        EQU     $80|$7B ;ACK1 = ack command, please wait

TIMEOUT_WAIT2	EQU	50	;in 92 ms units -> 50 equals 4.6s
	;
	XDEF	_CIAKB_Init	;startup
	XDEF	 _CIAKB_Send	;send a sequence
	XDEF	 _CIAKB_Wait	;wait for end of sequence (and stop)
	XDEF	 _CIAKB_IsBusy  ;
	XDEF	 _CIAKB_GetData ;get data stream (after CIAKB_Wait)
	XDEF	 _CIAKB_Stop	;stop sending instance (implicit in "Wait")
	XDEF	_CIAKB_Exit	;shutdown


; next position in ring buffer (argument: Dn)
KBRING_SIZE	EQU	64	;must be 2^n
KBRING_NEXT	MACRO
		addq.w	#1,\1
		and.w	#KBRING_SIZE-1,\1
		ENDM


	section	text,code
DBG:
	bsr	_CIAKB_Init

.loop:
	tst.b	kbsend_sending
	bne.s	.nosend

	lea	sendbuf(pc),a1
	moveq	#4,d0
	bsr	_CIAKB_Send

	bsr	_CIAKB_Wait	;does implicit "STOP"	

		;extra debug: check return
		lea	recvbuf(pc),a1
		moveq	#32,d0	
		bsr	_CIAKB_GetData
		bra.s	.exit
.nosend

	;pause loop
	move	#30000,d0
.loop2
	tst.b	$bfe001
	dbf	d0,.loop2

	btst	#6,$bfe001
	bne	.loop

	bsr	_CIAKB_Wait	;does implicit "STOP"	
.exit:
	bsr	_CIAKB_Exit
	rts

sendbuf:
	dc.b	$00,$03
;	dc.b	$80,$00
	dc.b	$62,$00
	;,$BA,00,$ff,$55,$AA,$55,$AA,$ff,$00,$11
	even
recvbuf:	ds.b	32
	dc.w	$BAAF

; void
; return: 0=OK, else FAIL
_CIAKB_Init:
	movem.l	a6,-(sp)

	moveq	#-1,d0
	move.l	d0,kbsend_sigbit

	;CIA Timer A interrupt
	lea	cia_int(pc),a0
	clr.l	LN_SUCC(a0)
	clr.l	LN_PRED(a0)
	move.b	#NT_INTERRUPT,LN_TYPE(a0)
	clr.b	LN_PRI(a0)
	lea	cia_name(pc),a1
	move.l	a1,LN_NAME(a0)
	
	suba.l	a1,a1
	move.l	a1,IS_DATA(a0)
	lea	CIAKB_TimerInt(pc),a1
	move.l	a1,IS_CODE(a0)


	lea	ciares_name(pc),a1
	move.l	4.w,a6
	jsr	_LVOOpenResource(a6)
	move.l	d0,cia_res
	beq.s	.rts_fail

	move.l	d0,a6
	lea	cia_int(pc),a1
	moveq	#CIAICRB_TA,d0
	jsr	_LVOAddICRVector(A6)
	tst.l	d0
	bne.s	.rts_fail

	;OK, we've obtained the CIA-A Timer
	moveq	#CIAICRF_TA,d0	;disable TimerA interrupt (not needed for us)
	jsr	_LVOAbleICR(A6)

	;CIA Keyboard interrupt
	lea	cia_kbint(pc),a0
	clr.l	LN_SUCC(a0)
	clr.l	LN_PRED(a0)
	move.b	#NT_INTERRUPT,LN_TYPE(a0)
	clr.b	LN_PRI(a0)
	lea	cia_name(pc),a1
	move.l	a1,LN_NAME(a0)
	
	suba.l	a1,a1
	move.l	a1,IS_DATA(a0)
	lea	CIAKB_KeyboardInt(pc),a1
	move.l	a1,IS_CODE(a0)

	;
	;"steal" keyboard interrupt and insert our own one
	;
	move.l	cia_res(pc),a6
	lea	cia_kbint(pc),a1
	moveq	#CIAICRB_SP,d0
	jsr	_LVOAddICRVector(A6)
	move.l	d0,cia_oriKBInt		;remember original keyboard interrupt handler

	move.l	d0,a1
	moveq	#CIAICRB_SP,d0
	jsr	_LVORemICRVector(A6)	;steal keyboard interrupt (disable)

	lea	cia_kbint(pc),a1
	moveq	#CIAICRB_SP,d0
	jsr	_LVOAddICRVector(A6)

	moveq	#1,d0
	bra.s	.ret
.rts_fail:	
	moveq	#0,d0
.ret:
	move.w	d0,cia_allocated ; remember successful allocation
	bchg	#0,d0		 ; we come here with 0 for fail, 1 for OK and would like to:
	;return 1 for error, 0 for success

	movem.l	(sp)+,a6
	rts

; Shutdown (void)
_CIAKB_Exit:
	movem.l	a6,-(sp)

	move.w	cia_allocated(pc),d0
	beq.s	.rts

	bsr	_CIAKB_Stop			;

	lea	_ciaa,a0
	move.b	#%10000000,ciacra(a0)		;E01 CIACRAF_TODIN

	move.l	cia_res(pc),a6
	lea	cia_int(pc),a1
	moveq	#CIAICRB_TA,d0
	jsr	_LVORemICRVector(A6)

	lea	cia_kbint(pc),a1		;remove custom interrupt handler
	moveq	#CIAICRB_SP,d0
	jsr	_LVORemICRVector(A6)

	move.l	cia_oriKBInt(pc),a1		;get original keyboard interrupt handler
	moveq	#CIAICRB_SP,d0
	jsr	_LVOAddICRVector(A6)		;restore original keyboard interrupt handler

	moveq	#0,d0
	move.w	d0,cia_allocated
.rts:
	movem.l	(sp)+,a6
	rts


_CIAKB_IsBusy:
	moveq	#0,d0
	move.w  cia_allocated(pc),d0
	beq.s	.exit

	move.b	kbsend_sending(pc),d0	; will be on as long as neither a timeout or ACK/NACK occured
	beq.s	.exit

	moveq	#1,d0
.exit:
	rts


; Wait for end of sending sequence (void)
; Returns: received code
_CIAKB_Wait:
	movem.l	a5/a6,-(sp)

	move.w  cia_allocated(pc),d0
	beq.s	.error_nosig

	move.l	kbsend_sigbit(pc),d0
	blt.s	.error_nosig

	move.l	4.w,a6
	jsr	_LVODisable(A6)

;	move.b	kbsend_active(pc),d0	; KB Interrupt will go "inactive" when sequence is sent
;	beq.s	.ret			; 

	move.b	kbsend_sending(pc),d0	; will be on as long as neither a timeout or ACK/NACK occured
	beq.s	.ret

	move.l	kbsend_sigmask(pc),d0
	jsr	_LVOWait(A6)

.ret:
	jsr	_LVOEnable(A6)

.error_nosig:
	bsr	_CIAKB_Stop		; implicit CIAKB_ClearSigTask

	moveq	#0,d0
	move.b	kbsend_result,d0	; remember outcome

	movem.l	(sp)+,a5/a6
	rts

;
; Get Data that arrived while waiting into supplied buffer
;  A1 = buffer
;  D0 = buffersize
; Semantics:
;  The keyboard sends CMD_ACK1 for the beginning of the data stream.
;  CMD_ACK terminates the byte stream.
;  While waiting for CMD_ACK, inputs are swallowed and not handed
;  to keyboard.device.
;
; Returns: number of received bytes (D0)
_CIAKB_GetData:
	movem.l	d2-d3/a2,-(sp)

	move.b	kbsend_result(pc),d1	; last command successful ?
	cmp.b	#CMD_ACK,d1		;
	bne.s	.getdata_retzero	; nope: we did not finish successfully.

	lea	kbroll(pc),a0
	move.w	kback1off(pc),d1	; at ACK1
	blt.s	.getdata_retzero	; no ACK1 received

	move.b	(a0,d1.w),d2		; overwritten in ring buffer ?
	cmp.b	#CMD_ACK1,d2		; 
	bne.s	.getdata_retzero	; sorry, can't get data -> the damn user typed too much

	KBRING_NEXT	d1		; +1 & 63
	moveq	#0,d2
	move.b	(a0,d1.w),d2		; number of bytes received (beore ACK)
	beq.s	.getdata_retzero	; 0 = we're out

	cmp.l	d0,d2			; out buffer too small ? 
	ble.s	.getdata_fit		;
	move.l	d0,d2			; just store what fits
.getdata_fit
	move.l	d2,d0			; number of copied bytes

	;we come in with the length tag position in d1, advance first
.getdata_copy:
	KBRING_NEXT	d1		; next byte to read
	move.b	(a0,d1.w),(a1)+		; copy from keyboard roll
	subq.b	#1,d2
	bne.s	.getdata_copy

.getdata_ret:
	movem.l	(sp)+,d2-d3/a2
	rts

.getdata_retzero:
	moveq	#0,d0
	bra.s	.getdata_ret


CIAKB_ClearSigTask:
	movem.l	d1/a1/a6,-(sp)
	move.l	4.w,a6

	move.l	kbsend_sigbit(pc),d0
	blt.s	.nosig
	 jsr	_LVOFreeSignal(a6)
	 moveq	#-1,d0
	 move.l	d0,kbsend_sigbit
.nosig:
	moveq	#0,d0
	move.l	d0,kbsend_sigtask
	move.l	d0,kbsend_sigmask

	movem.l	(sp)+,d1/a1/a6
	rts


CIAKB_SetSigTask:
	movem.l	d1/a1/a6,-(sp)

	moveq	#0,d0
	move.l	kbsend_sigbit,d1		;consecutive send ? (keep signal -> alternative: re-allocate)
	bge.s	.rts
	move.l	d0,kbsend_sigtask		;no task, for now

	move.l	4.w,a6
	moveq	#-1,d0
	jsr	_LVOAllocSignal(A6)
	move.l	d0,kbsend_sigbit
	blt.s	.error				;-1 = error
	moveq	#0,d1
	bset	d0,d1
	move.l	d1,kbsend_sigmask		;we have a mask

	suba.l	a1,a1
	jsr	_LVOFindTask(a6)
	move.l	d0,kbsend_sigtask		;add task

	moveq	#0,d0				;OK
.rts:
	movem.l	(sp)+,d1/a1/a6
	rts
.error:
	moveq	#-1,d0
	bra.s	.rts



;
;
_CIAKB_Stop:
	bsr	CIAKB_ClearSigTask		;deallocate Signal

	lea	_ciaa,a0
	moveq	#0,d0
;	move.b	d0,kbsend_active
	move.b	d0,kbsend_sending
	move.b	d0,kbsend_timercount

;	move.b	#%10000000,ciacra(a0)		;E01 CIACRAF_TODIN
;	move.b	#0,ciasdr(a0)			;avoid dormant keycode
	rts



; A1: sendbuffer
; D0: length
;out: D0 ==0 OK
;     D0 !=0 FAIL
_CIAKB_Send:
	movem.l	d1-d7/a2-a6,-(sp)
	;TODO: check if sending is already in progress
	move.l	d0,d6				;remember length

	clr.b	kbsend_result			;unspecified result of sending operation

	moveq	#-1,d0
	tst.w	cia_allocated			;are we live ?
	beq.s	.fail				;CIA not allocated

	clr.b	kbsend_timercount

	bsr	CIAKB_SetSigTask		;get current task, allocate signal bit
	tst.l	d0
	bne.s	.fail

	move.l	cia_res(pc),a6
	moveq	#CIAICRF_TA,d0			;disable TimerA interrupt (we're running 20kHz clock in sending phase)
	jsr	_LVOAbleICR(A6)

	;CIA timer A interrupt was disabled in allocation
	lea	_ciaa,a0
	move.b	#42,ciatalo(a0)			;401 PAL: 709379/36 = 19.704 kHz (=10k Serial Rate) ;42=8kHz,52=6.8kHz
	move.b	#0,ciatahi(a0)			;501
	;start timer, activate "OUT" serial port mode
	move.b	#%11010001,ciacra(a0)		;E01 CIACRAF_TODIN|CIACRAF_SPMODE|CIACRAF_LOAD|CIACRAF_START

	;TODO: buffer/task/signal in data pointer of interrupt struct
	addq.l	#1,a1
	move.l	a1,cia_sendbuf
	subq.l	#1,d6
	move.l	d6,cia_sendlen

	move.b	#-1,kback1streamlen		;no length after ACK1 yet
	move.w	#-1,kback1off			;no ACK1 received yet
	move.b	#1,kbsend_sending		;stays on until ACK/NACK or timeout
	move.b	#1,kbsend_active		;actively send data
	;write first byte -> starts serial output
	move.b	-1(a1),ciasdr(a0)		;D01  (move.b	#$03,ciasdr(a0)	)

	moveq	#0,d0				;OK
.fail:
.rts:
	movem.l	(sp)+,d1-d7/a2-a6
	rts



	ifne	0
* control register A bit masks
;CIACRAF_START	  EQU	(1<<0)
;CIACRAF_PBON	  EQU	(1<<1)
;CIACRAF_OUTMODE   EQU	(1<<2)
;CIACRAF_RUNMODE   EQU	(1<<3)
;CIACRAF_LOAD	  EQU	(1<<4)
;CIACRAF_INMODE	  EQU	(1<<5)
;CIACRAF_SPMODE	  EQU	(1<<6)
;CIACRAF_TODIN	  EQU	(1<<7)

ciapra		  EQU	$0000
ciaprb		  EQU	$0100
ciaddra	  EQU	$0200
ciaddrb	  EQU	$0300
ciatalo	  EQU	$0400
ciatahi	  EQU	$0500
ciatblo	  EQU	$0600
ciatbhi	  EQU	$0700
ciatodlow	  EQU	$0800
ciatodmid	  EQU	$0900
ciatodhi	  EQU	$0A00
ciasdr		  EQU	$0C00
ciaicr		  EQU	$0D00
ciacra		  EQU	$0E00
ciacrb		  EQU	$0F00
	endc


	rts

;
;-----  TimerA interrupt (waiting timeout) ---------
;
CIAKB_TimerInt:
	move.b	kbsend_timercount(pc),d0
	beq.s	.exit
	subq.b	#1,d0
	move.b	d0,kbsend_timercount	;did we reach 0 ?
	bne.s	.exit			;no, wait for next interrupt

	move.b	kbsend_sending(pc),d0	; will be on as long as neither a timeout or ACK/NACK occured
	beq.s	.exit

	clr.b	kbsend_sending			; timeout: we're done sending
	move.b	#CMD_TIMEOUT,kbsend_result	; remember outcome

	move.b	#%10000000,ciacra+_ciaa		;E01 CIACRAF_TODIN (disable timer)

	;signal waiting task
	bsr	CIAKB_SendSignal

.exit:
	moveq	#0,d0
	rts


;------- Keyboard interrupt --------------------
;normally (inactive), just forwards the request
;to the regular functions in lowlevel or
;keyboard.device
;
;If active, swallow the interrupt and do our
;thing sending the commend sequence to the other
;side. If done, then parse incoming keyboard 
;strokes for ACK/NACK codes. This process
;is accompanied by a timer that will stop
;the process after at most 200ms.
;
;
;
CIAKB_KeyboardInt:
;IS_DATA is in A1
	move.b	kbsend_active(pc),d0
	beq	.inactive_orikeyboard

	move.l	cia_sendlen(pc),d0
	ble.s	.nosend				;nothing more to send
	subq.l	#1,d0
	move.l	d0,cia_sendlen			;remaining length

	move.l	cia_sendbuf(pc),a0
	move.b	(a0)+,_ciaa+ciasdr
	move.l	a0,cia_sendbuf

	move.l	kbsend_num(pc),d0
	addq.l	#1,d0
	move.l	d0,kbsend_num			;total sent bytes

	bra	.rts
.nosend:
	move.b	#0,kbsend_active

	;we're done sending, now wait for ACK/NACK or Timeout
	bsr	Delay75us		;wait for actual finish of the last bit sent out (i.e. required on 100 MHz 060)

	;revert CIA serial to input
	lea	_ciaa,a0
	move.b	#%10000000,ciacra(a0)		;E01 CIACRAF_TODIN (disable timer, for now)
	move.b	#0,ciasdr(a0)			;avoid dormant keycode

	;enable timeout via TimerA interrupt(s)
	move.b	#$ff,ciatalo(a0)		;1/(709379/65535) = 1/10.8 or 92ms 
	move.b	#$ff,ciatahi(a0)
	move.b	#2,kbsend_timercount

	movem.l	a0/a6,-(sp)
	move.l	cia_res(pc),a6
	move.l	#$80|CIAICRF_TA,d0		;enable TimerA interrupt
	jsr	_LVOAbleICR(A6)
	movem.l	(sp)+,a0/a6

	;run timer (CIACRAF_SPMODE stays 0)
	move.b	#%10010001,ciacra(a0)		;E01 CIACRAF_TODIN|CIACRAF_LOAD|CIACRAF_START

	;AbleICR(0x81)	;needed to enable redirection in INT2
	;
	;We want ACK/NACK
	;- by clearing kbsend_active, we record incoming keyboard strokes
	;- if the keyboard doesn't answer, we need a timeout
	;-> set up timer interrupt, also sample keystrokes 
	;-> either result (timeout or keystroke) will signal the calling task
	;
	;don't forget AbleICR(0x1) and timer stop when done
	;make sure
	;bsr	CIAKB_SendSignal
	bra	.rts

.inactive_orikeyboard:
	move.b	ciasdr+_ciaa,d1			;get current keycode
	ror.b	#1,d1				;Bit7 = UP(1)/DOWN(0)
	not.b	d1				;invert to get actual keycode

	move.b	kbsend_sending(pc),d0		;stays on until ACK/NACK or timeout
	beq.s	.notsending

;approach:
; - check if incoming data stream is expected and don't parse commands in that case
; - if we have seen ACK1, then use next byte as stream length, unless it's again ACK1
; - 

	move.b	kback1streamlen(pc),d0		; did we get a stream length byte yet ? (after CMD_ACK1)
	blt.s	.checkcmd			; no stream length (-1)
	beq.s	.noack1				; stream done, wait for ACK
	; we have a "literal" stream, store in kbroll -> after we get to 0, resume regular processing
	subq.b	#1,d0
	move.b	d0,kback1streamlen		; remember remaining bytes
	bra.s	.notsending

.checkcmd:

	move.w	kback1off(pc),d0
	blt.s	.noack1_before

	cmp.b	#CMD_ACK1,d1		;keyboard may send ACK1 twice for the reason that sometimes the first
	beq.s	.another_ack1		;transmitted character is damaged after OUT-IN turnaround of CIA serial

	;ok, we have seen ACK1 and this is the next byte after: store stream length
	move.b	d1,kback1streamlen
	bra.s	.notsending

.noack1_before:
	cmp.b	#CMD_ACK1,d1
	bne.s	.noack1
.another_ack1:
	move.w	kbrolloff(pc),kback1off
	move.b	#TIMEOUT_WAIT2,kbsend_timercount	;allow for longer delay (65536*100-> ~10s)
	bra.s	.notsending

.noack1:
	cmp.b	#CMD_ACK,d1
	beq.s	.haveack
	cmp.b	#CMD_NACK,d1
	beq.s	.haveack
	bra.s	.notsending			;ACK/NACK code not found
.haveack:
	move.b	#%10000000,_ciaa+ciacra		;E01 CIACRAF_TODIN (disable timer, we don't need the timeout anymore)
	move.b	d1,kbsend_result		;remember outcome

	bsr	CIAKB_SendSignal		;preserves registers

	clr.b	kbsend_sending			;we're done here
	clr.b	kbsend_timercount		;disable timer interrupt handler
.notsending:

	; record keystrokes (and incoming data)
		lea	kbroll(pc),a0
		move.w	kbrolloff(pc),d0
		move.b	d1,(a0,d0)
		KBRING_NEXT	d0		; +1 & 63
		move.w	d0,kbrolloff
	

	move.b	kbsend_sending(pc),d0		;stays on until ACK/NACK or timeout
;	move.b	kbsend_active(pc),d0
	beq.s	.dontswallow
	
	or.b    #CIACRAF_SPMODE,_ciaa+ciacra		;
	bsr	Delay75us
	and.b   #~(CIACRAF_SPMODE)&$ff,_ciaa+ciacra	;

	bra.s	.swallowed
.dontswallow:
	move.l	cia_oriKBInt(pc),d0		;get original keyboard interrupt handler
	beq.s	.rts
	move.l	d0,a1
	move.l	IS_CODE(a1),a0
	move.l	IS_DATA(a1),a1
	jsr	(a0)
.swallowed:

.rts
	moveq	#0,d0
	rts
	
; preserves registers
CIAKB_SendSignal:
	movem.l	d0/d1/a0/a1/a6,-(sp)
	move.l	4.w,a6
	move.l	kbsend_sigtask(pc),d0
	beq.s	.nosig
	move.l	d0,a1
	move.l	kbsend_sigmask(pc),d0		;move.l	#SIGBREAKF_CTRL_E,d0
	jsr	_LVOSignal(A6)
.nosig:
	movem.l	(sp)+,d0/d1/a0/a1/a6
	rts

; 75us busy loop
Delay75us:
	move.l	d0,-(sp)

	moveq	#53,d0
.loop:
	tst.b	$bfe001				;1.4 us per CIA access
	dbf	d0,.loop
	
	move.l	(sp)+,d0
	rts

;
; -> Override CIA-A interrupt (INT2 handler Pri 127) OPTION A (problem: can't forward ints to other routines like regular KB while active)
; -> Steal Keyboard Interrupt like Lowlevel.library (AddICRVector,RemICRVector,AddICRVector) OPTION B
;
; Set CIA to output
; Enable Timer
; Send Bytes
; Stop Timer
; Restore Keyboard interrupt
;

	cnop	0,4
cia_res:	dc.l	0
cia_int: 	ds.b	IS_SIZE
cia_kbint: 	ds.b	IS_SIZE
cia_sendbuf:	dc.l	0
cia_sendlen:	dc.l	0
cia_oriKBInt:	dc.l	0	;original keyboard interrupt handler
cia_allocated:	dc.w	0

kbsend_sigtask:	dc.l	0
kbsend_sigbit:	dc.l	-1
kbsend_sigmask: dc.l	0

kbsend_active:	dc.b	0	;1 if kbsend is active
kbsend_sending:	dc.b	0	;still sending?
kbsend_timercount: dc.b 0	;return when 0, send signal when reaching 0
kbsend_result:	dc.b	0	;result (CMD_ACK,CMD_NACK,CMD_TIMEOUT)

; 
kbsend_num:	 dc.l	0	;total sent bytes (debug)
kback1off:	 dc.w	0	;offset of CMD_ACK1 in current command cycle (-1)
kback1streamlen: dc.b	0	;stream length after CMD_ACK1 (-1, counted down to zero if present)
		 dc.b	0	;align
kbrolloff:	dc.w	0	;
kbroll:		ds.b	KBRING_SIZE	;64

	cnop	0,4
ciares_name:	dc.b	'ciaa.resource',0
cia_name:	dc.b	'A500KB Support',0
	cnop	0,4

;   STRUCTURE    LN,0    ; List Node
;        APTR    LN_SUCC ; Pointer to next (successor)
;        APTR    LN_PRED ; Pointer to previous (predecessor)
;        UBYTE   LN_TYPE
;        BYTE    LN_PRI  ; Priority, for sorting
;        APTR    LN_NAME ; ID string, null terminated
;        LABEL   LN_SIZE ; Note: word aligned
;STRUCTURE  IS,LN_SIZE
;    APTR    IS_DATA
;    APTR    IS_CODE

