; r_scan060.s -- the world span and z-span drawers for the 68060
;
; D_DrawSpans8, D_DrawSpans16 and D_DrawZSpans, replacing the C versions in r_scan.c (built with Q2_ASM=1,
; the default).  It is WarpQuake's D_DrawSpans8 (its d_spans060.s, the generic
; SpanDrawer body), because Quake II's D_DrawSpans16 is Quake's D_DrawSpans8
; operation for operation: 8-pixel blocks despite the name, so it is D_DrawSpans8
; here; D_DrawSpans16 below is WarpQuake's 16-pixel one (sw_subdiv16 1).  It must draw
; exactly what the C draws (tools/vamos_crc.sh checks it frame by frame), so
; the floating-point part repeats vbcc's code for the C operation for
; operation, and only the scheduling and the integer side are new.
;
; From WarpQuake's notes, where the time went in the C: ~53 cycles a pixel, of
; which the FDIV every 8 pixels stalled the CPU for ~16% and the pixel loop,
; with a MULS from memory and two steps reloaded from the stack per pixel,
; took ~47%.  Here:
;
; - The divide for a block's far end is issued one block early, before the
;   previous block's pixel loop.  The 68060 runs the FDIV alongside integer
;   instructions and only waits at the next FPU instruction, which now comes
;   after 8 pixels of integer work.
; - The texel address steps without a multiply.  The integer part of
;   s + t*cachewidth is one register, the fractions of s and t ride in the
;   upper words of two others, and their carries are folded in with ADDX and
;   a SUBX mask, all in registers.  Two offset registers alternate, so each
;   pixel's fetch indexes a register finished several instructions earlier
;   and the independent halves can pair in the two pipelines (see Pixel).
; - The pixel loop is unrolled 8 times and entered part-way for shorter
;   blocks, so there is no per-pixel loop branch.
;
; Calling convention: vbcc's, arguments on the stack, d0-d1/a0-a1/fp0-fp1
; scratch, everything else preserved.

	machine	68060

; r_scan060rgb.s assembles this again with PIX16 = 1: the 16 bpp drawers
; D_DrawSpans8RGB and D_DrawSpans16RGB (16-bit texels from the cache, 16-bit
; frame); D_DrawZSpans is only in the 8-bit object, for both.
	ifnd	PIX16
PIX16	equ	0
	endif

	xref	_cacheblock			; pixel_t *: the surface's cache block
	xref	_cachewidth			; int: its width in pixels
	xref	_d_viewbuffer		; pixel_t *: the frame
	xref	_r_screenwidth		; int: its row length
	xref	_d_sdivzstepu		; float: s/z, t/z, 1/z gradients and origins
	xref	_d_tdivzstepu
	xref	_d_zistepu
	xref	_d_sdivzstepv
	xref	_d_tdivzstepv
	xref	_d_zistepv
	xref	_d_sdivzorigin
	xref	_d_tdivzorigin
	xref	_d_ziorigin
	xref	_sadjust			; fixed16_t: texture offsets
	xref	_tadjust
	xref	_bbextents			; fixed16_t: largest s and t in the block
	xref	_bbextentt
	xref	_q2c_count			; unsigned long[]: q2prof.h work counters

	if PIX16
	xdef	_D_DrawSpans8RGB
	xdef	_D_DrawSpans16RGB
	else
	xdef	_D_DrawSpans8
	xdef	_D_DrawSpans16
	xdef	_D_DrawZSpans
	endif

	xref	_d_pzbuffer		; short *: the z-buffer
	xref	_d_zwidth		; unsigned: its row length, in shorts

; espan_t (r_local.h)
SPAN_U		equ	0
SPAN_V		equ	4
SPAN_COUNT	equ	8
SPAN_NEXT	equ	12

; q2c_count[] byte offsets: the order of q2p_counter_t in q2prof.h
Q2C_SPAN_PIXELS	equ	0*4
Q2C_SPANS	equ	1*4
Q2C_ZSPAN_PIXELS	equ	3*4

; Saved on entry.
SAVED_INT	equ	11*4			; d2-d7/a2-a6
SAVED_FP	equ	6*12			; fp2-fp7

	section	CODE,code

;------------------------------------------------------------------------------
; void D_DrawSpans16 (espan_t *pspan)
;
; SpanDrawer takes the block length, its shift and the float step constant
; (WarpQuake instantiates it for 8 and 16); Quake II's C uses 8.
;
; Each span is drawn in blocks of up to N pixels.  At each block's far end s
; and t are computed exactly (one divide); in between they step linearly.
;
; Across blocks:
;   fp6 = s/z  fp5 = t/z  fp2 = 1/z     at the far end of the newest block
;   fp3 = 65536/z for that end (the divide in flight)
;   fp4 = 65536.0
;   a1 = pdest  a5 = cacheblock
;   a3 = pixels left in the span, the current block included
;   a4 = s  a6 = t                      at the start of the current block
;   a2 = snext, v_tnext                 at its end (a2 is the span pointer
;                                       only while a span starts: v_pspan)
;
; A block's length and kind follow from a3 alone, exactly as the C's
; "spancount = min(count, 8); count -= spancount; if (count) ...": with more
; than N pixels left it is a middle block of N, otherwise the span's last.
;------------------------------------------------------------------------------

; StartDiv: step s/z, t/z, 1/z to the far end of the block that a3 begins,
; and start its divide.  Exactly the C's two cases:
;   middle block (a3 > N): += the N-pixel steps (floats, as in the C)
;   last block:            += the 1-pixel steps * (a3-1), to its last pixel
; \1 = N.  Uses d0, d1, fp0, fp1.
StartDiv	macro
	move.l	a3,d1
	moveq	#\1,d0
	cmp.l	d0,d1
	ble.s	.last\@
	fadd.s	v_sdivzn,fp6
	fadd.s	v_tdivzn,fp5
	fadd.s	v_zin,fp2
	fmove.x	fp4,fp3
	fdiv.x	fp2,fp3				; issued; runs while the integer unit works
	bra.s	.done\@
.last\@
	subq.l	#1,d1
	fmove.l	d1,fp0				; spancount - 1
	fmove.s	_d_sdivzstepu,fp1
	fmul.x	fp0,fp1
	fadd.x	fp1,fp6
	fmove.s	_d_tdivzstepu,fp1
	fmul.x	fp0,fp1
	fadd.x	fp1,fp5
	fmul.s	_d_zistepu,fp0
	fadd.x	fp0,fp2
	fmove.x	fp4,fp3
	fdiv.x	fp2,fp3
.done\@
	endm

; SpanFP span: s/z, t/z and 1/z at the first pixel of the span at address
; register \1, into fp6/fp5/fp2, and its divide (65536/zi into fp3) started.
; Issued for the next span during the current one's last block, so the
; divide overlaps those pixels.  Uses d0, fp0, fp1.
SpanFP	macro
	fmove.l	SPAN_U(\1),fp1			; du
	fmove.l	SPAN_V(\1),fp0			; dv
	fmove.s	_d_sdivzstepv,fp2
	fmul.x	fp0,fp2
	fadd.s	_d_sdivzorigin,fp2
	fmove.s	_d_sdivzstepu,fp6
	fmul.x	fp1,fp6
	fadd.x	fp2,fp6				; sdivz
	fmove.s	_d_tdivzstepv,fp2
	fmul.x	fp0,fp2
	fadd.s	_d_tdivzorigin,fp2
	fmove.s	_d_tdivzstepu,fp5
	fmul.x	fp1,fp5
	fadd.x	fp2,fp5				; tdivz
	fmul.s	_d_zistepv,fp0
	fadd.s	_d_ziorigin,fp0
	fmul.s	_d_zistepu,fp1
	fadd.x	fp0,fp1
	fmove.s	fp1,d0				; zi is a float in the C: round it
	fmove.s	d0,fp2
	fmove.x	fp4,fp3
	fdiv.x	fp2,fp3				; z = 65536 / zi
	endm

; FixedAtZ: (int)(fp3 * \1) + \2, the C's "(int)(sdivz * z) + sadjust".
; Result in \3.  Uses fp0.  (Span starts only; block ends do s and t
; interleaved, see .block.)
FixedAtZ	macro
	fmove.x	fp3,fp0
	fmul.x	\1,fp0
	fintrz.x	fp0
	fmove.l	fp0,\3
	add.l	\2,\3
	endm

; ClampEnd: a block end, clamped to 8 .. \2 as in the C ("prevent round-off
; error on <0 steps from overstepping").  Uses d0.
ClampEnd	macro
	cmp.l	\2,\1
	ble.s	.lo\@
	move.l	\2,\1
	bra.s	.ok\@
.lo\@
	moveq	#8,d0
	cmp.l	d0,\1
	bge.s	.ok\@
	move.l	d0,\1
.ok\@
	endm

PIXEL_SIZE	equ	20		; bytes per Pixel expansion, both variants

; Pixel cur, next: draw the pixel at offset cur and compute the next pixel's
; offset into next - the s fraction's carry through ADDX, the t fraction's
; as a 0 / cachewidth mask (SUBX gives 0 or -1).  The texel fetch sits after
; the ADDX, so cur was last written four instructions earlier and the 68060
; does not stall on it as an index; the independent halves (s carry, t
; carry, fetch) give the two pipelines pairs to issue together.  The texel
; goes through d7 (free in the pixel loops) rather than memory to memory: a
; MOVE with two memory operands issues alone, and the store, two
; instructions after the load, pairs with the ALU work around it.
Pixel	macro
	move.l	\1,\2
	add.l	a4,d2
	addx.l	d4,\2
	if PIX16
	move.w	(a5,\1.l*2),d7
	else
	move.b	(a5,\1.l),d7
	endif
	add.l	a6,d3
	subx.l	d6,d6
	if PIX16
	move.w	d7,(a1)+
	else
	move.b	d7,(a1)+
	endif
	and.l	d5,d6
	add.l	d6,\2
	endm

; SpanDrawer N, shift, N as a float: the whole function body for N-pixel
; blocks.  Its local labels (.span, .block, ...) are scoped to the global
; label each instantiation follows.
SpanDrawer	macro
	movem.l	d2-d7/a2-a6,-(sp)
	fmovem.x	fp2-fp7,-(sp)
	move.l	4+SAVED_INT+SAVED_FP(sp),a2
	move.l	a2,v_pspan
	move.l	_cacheblock,a5
	fmove.s	#$47800000,fp4			; 65536.0: z is prescaled to 16.16

	; the N-pixel steps, rounded to float as the C's locals are
	fmove.s	_d_sdivzstepu,fp0
	fmul.s	#\3,fp0			; * N.0
	fmove.s	fp0,v_sdivzn
	fmove.s	_d_tdivzstepu,fp0
	fmul.s	#\3,fp0
	fmove.s	fp0,v_tdivzn
	fmove.s	_d_zistepu,fp0
	fmul.s	#\3,fp0
	fmove.s	fp0,v_zin

	SpanFP	a2				; the first span's; later ones come prefetched

.span
	; fp6/fp5/fp2 hold this span's s/z, t/z, 1/z, and fp3 its divide
	move.l	SPAN_COUNT(a2),d0
	move.l	d0,a3

	; s and t at the first pixel, clamped to 0 .. bbextents/t
	FixedAtZ	fp6,_sadjust,d0
	cmp.l	_bbextents,d0
	ble.s	.s_lo
	move.l	_bbextents,d0
	bra.s	.s_ok
.s_lo
	tst.l	d0
	bge.s	.s_ok
	moveq	#0,d0
.s_ok
	move.l	d0,a4
	FixedAtZ	fp5,_tadjust,d0
	cmp.l	_bbextentt,d0
	ble.s	.t_lo
	move.l	_bbextentt,d0
	bra.s	.t_ok
.t_lo
	tst.l	d0
	bge.s	.t_ok
	moveq	#0,d0
.t_ok
	move.l	d0,a6

	StartDiv	\1				; the first block's divide

	; integer setup while that divide runs:
	; pdest = d_viewbuffer + screenwidth * v + u
	move.l	_r_screenwidth,d0
	muls.l	SPAN_V(a2),d0
	move.l	_d_viewbuffer,a1
	if PIX16
	add.l	SPAN_U(a2),d0
	add.l	d0,d0				; 2 bytes a pixel
	add.l	d0,a1
	else
	add.l	d0,a1
	add.l	SPAN_U(a2),a1
	endif
	move.l	a3,d0
	add.l	d0,_q2c_count+Q2C_SPAN_PIXELS
	addq.l	#1,_q2c_count+Q2C_SPANS

.block
	; snext, tnext at this block's far end (fp3 is ready by now), s and t
	; interleaved so the FPU latencies overlap
	fmove.x	fp3,fp0
	fmove.x	fp3,fp1
	fmul.x	fp6,fp0
	fmul.x	fp5,fp1
	fintrz.x	fp0
	fintrz.x	fp1
	fmove.l	fp0,d2
	fmove.l	fp1,d3
	add.l	_sadjust,d2
	add.l	_tadjust,d3
	ClampEnd	d2,_bbextents
	ClampEnd	d3,_bbextentt
	move.l	d2,a2				; snext
	move.l	d3,v_tnext

	; This block's length n (d6), and sstep, tstep: a middle block divides
	; by N with a shift, the last by n-1 (biased low so it cannot step off
	; the polygon)
	sub.l	a4,d2				; snext - s
	sub.l	a6,d3				; tnext - t
	move.l	a3,d6
	moveq	#\1,d0
	cmp.l	d0,d6
	ble.s	.laststep
	moveq	#\1,d6
	asr.l	#\2,d2
	asr.l	#\2,d3
	bra.s	.stepped
.laststep
	; (snext - s) / (n-1), truncated toward zero like the C's "/" - but as
	; a multiply by 1/(n-1) on the FPU: DIVS.L costs ~38 cycles and nearly
	; every span ends in a last block.  Exact: see recip_up below.
	move.l	d6,d0
	subq.l	#1,d0
	beq.s	.stepped			; one pixel: the steps are never used
	mulu.w	#12,d0				; an extended is 12 bytes
	lea	recip_up-12,a0
	fmove.x	(a0,d0.l),fp1
	fmove.l	d2,fp0
	fmul.x	fp1,fp0
	fintrz.x	fp0
	fmove.l	fp0,d2
	fmove.l	d3,fp0
	fmul.x	fp1,fp0
	fintrz.x	fp0
	fmove.l	fp0,d3
.stepped

	; the next block's divide, so it overlaps this block's pixels - or, in
	; the span's last block, the next span's start
	sub.l	d6,a3				; pixels left after this block
	cmp.w	#0,a3
	beq.s	.nextspan
	StartDiv	\1
	bra	.nonext
.nextspan
	move.l	v_pspan,a0
	move.l	SPAN_NEXT(a0),d0
	beq	.nonext
	move.l	d0,a0
	SpanFP	a0
.nonext

	; Enter the unrolled loop at the copy that leaves n pixels: each copy is
	; PIXEL_SIZE bytes, so skip (N - n) of them.
	moveq	#\1,d7
	sub.l	d6,d7
	mulu.w	#PIXEL_SIZE,d7
	lea	.pixels(pc,d7.l),a0

	; Registers for the pixel loop:
	;   d0, d1 = texel offsets (s >> 16) + (t >> 16) * cachewidth of the
	;            current pixel and the next, alternating roles
	;   d2 = s fraction << 16          a4 = sstep fraction << 16
	;   d3 = t fraction << 16          a6 = tstep fraction << 16
	;   d4 = (sstep >> 16) + (tstep >> 16) * cachewidth
	;   d5 = cachewidth                d6 = scratch: the t carry mask
	; (a4, a6 hold s, t across blocks; they are reloaded after the loop.)
	move.l	_cachewidth,d5
	move.l	d2,d4
	swap	d4
	ext.l	d4				; sstep >> 16, signed
	move.l	d3,d7
	swap	d7
	ext.l	d7				; tstep >> 16, signed
	muls.l	d5,d7
	add.l	d7,d4
	swap	d2
	clr.w	d2				; sstep << 16
	swap	d3
	clr.w	d3				; tstep << 16
	move.l	a6,d0
	clr.w	d0
	swap	d0
	mulu.w	d5,d0				; (t >> 16) * cachewidth; s, t >= 0
	move.l	a4,d7
	clr.w	d7
	swap	d7
	add.l	d7,d0				; + (s >> 16)
	move.l	d0,d1				; whichever role the entry pixel has
	move.l	a4,d7
	swap	d7
	clr.w	d7				; s << 16
	move.l	a6,d6
	swap	d6
	clr.w	d6				; t << 16
	move.l	d2,a4
	move.l	d3,a6
	move.l	d7,d2
	move.l	d6,d3
	jmp	(a0)


	cnop	0,4
.pixels
	rept	\1/2
	Pixel	d0,d1
	Pixel	d1,d0
	endr
.pixels_end
	if	(.pixels_end-.pixels)<>(\1*PIXEL_SIZE)
	fail	"SpanDrawer: a pixel must be PIXEL_SIZE bytes for the entry computation"
	endif

	move.l	a2,a4				; s, t = snext, tnext
	move.l	v_tnext,a6
	cmp.w	#0,a3
	bne	.block

	move.l	v_pspan,a2
	move.l	SPAN_NEXT(a2),d0
	move.l	d0,v_pspan
	move.l	d0,a2
	bne	.span

	fmovem.x	(sp)+,fp2-fp7
	movem.l	(sp)+,d2-d7/a2-a6
	rts
	endm

	cnop	0,4
	if PIX16
_D_DrawSpans8RGB
	else
_D_DrawSpans8
	endif
	SpanDrawer	8,3,$41000000

;------------------------------------------------------------------------------
; void D_DrawSpans16 (espan_t *pspan)
;
; SpanDrawer's algorithm for N = 16, with the float work software
; pipelined.  The 68060's FPU is not pipelined and SpanDrawer issued a
; block's float work back to back - s and t at its far end (8 FPU
; operations), then the next divide's setup (5, 11, or 22 for the next
; span) - while the integer pipes idled; the pixels after it are pure
; integer.  That was ~5 ms of the ~11.5 a frame spent here (hardware
; profile, 2026-10-04).
;
; Here a full 16-pixel block (any block but a span's last) carries the NEXT
; block's float work in its pixel loop, one FPU operation between pixels:
; first that block's far-end s and t (A, from the divide issued during
; this block's predecessor), then the divide after it (C: a middle block's,
; a last block's, or the next span's start).  The operations and their
; order are SpanDrawer's - only their placement moved - so the result is
; the same, bit for bit; -crc checks it.  A span's first block and its last
; block (entered part-way into the unrolled loop, so it cannot carry
; anything) still do their float work in line.
;
; Registers as in SpanDrawer.  A span's first block and the later ones take
; separate paths (the later ones' next float work is already done, in the
; previous block's loop); d4, d5, d7 carry PixStart's results across the
; block-end code.
;------------------------------------------------------------------------------

; Pixel-loop setup (SpanDrawer's, see the register list there), in two
; halves.  PixStart needs only s (a4) and t (a6), so it runs while a span's
; first divide does - the A after it would otherwise wait the FDIV out:
;   d7 = the first texel offset, d4 = s << 16, d5 = t << 16
PixStart	macro
	move.l	a6,d7
	clr.w	d7
	swap	d7
	mulu.w	_cachewidth+2,d7		; (t >> 16) * cachewidth; s, t >= 0
	move.l	a4,d0
	clr.w	d0
	swap	d0
	add.l	d0,d7				; + (s >> 16)
	move.l	a4,d4
	swap	d4
	clr.w	d4				; s << 16
	move.l	a6,d5
	swap	d5
	clr.w	d5				; t << 16
	endm

; PixEnd: the rest, once the steps are known (d2, d3).  Uses d0, d1.
PixEnd	macro
	move.l	d2,d0
	swap	d0
	ext.l	d0				; sstep >> 16, signed
	move.l	d3,d1
	swap	d1
	ext.l	d1				; tstep >> 16, signed
	muls.l	_cachewidth,d1
	add.l	d1,d0
	swap	d2
	clr.w	d2				; sstep << 16
	swap	d3
	clr.w	d3				; tstep << 16
	move.l	d2,a4
	move.l	d3,a6
	move.l	d4,d2				; s << 16
	move.l	d5,d3				; t << 16
	move.l	d0,d4				; (sstep >> 16) + (tstep >> 16) * cachewidth
	move.l	_cachewidth,d5
	move.l	d7,d0				; whichever role the entry pixel has
	move.l	d7,d1
	endm

; BlockEnd last: clamp this block's far end (d2, d3 before the adjustment),
; keep it (a2, v_tnext) and step to it - by shifting for a full block, which
; falls through with d6 = 16 and a3 = the pixels left after it; the span's
; last block goes to \1 with d6 = its length.
BlockEnd	macro
	add.l	_sadjust,d2
	add.l	_tadjust,d3
	ClampEnd	d2,_bbextents
	ClampEnd	d3,_bbextentt
	move.l	d2,a2				; snext
	move.l	d3,v_tnext
	sub.l	a4,d2				; snext - s
	sub.l	a6,d3				; tnext - t
	move.l	a3,d6
	moveq	#16,d0
	cmp.l	d0,d6
	ble	\1
	moveq	#16,d6
	asr.l	#4,d2
	asr.l	#4,d3
	sub.l	d6,a3				; r = pixels left after this block
	endm

; LastSteps: the span's last block (n = d6 <= 16): (snext - s) / (n-1) on
; the FPU as SpanDrawer does (see recip_up).  Uses d0, a0, fp0, fp1.
LastSteps	macro
	move.l	d6,d0
	subq.l	#1,d0
	beq	.ls\@				; one pixel: the steps are never used
	mulu.w	#12,d0				; an extended is 12 bytes
	lea	recip_up-12,a0
	fmove.x	(a0,d0.l),fp1
	fmove.l	d2,fp0
	fmul.x	fp1,fp0
	fintrz.x	fp0
	fmove.l	fp0,d2
	fmove.l	d3,fp0
	fmul.x	fp1,fp0
	fintrz.x	fp0
	fmove.l	fp0,d3
.ls\@
	endm

	cnop	0,4
	if PIX16
_D_DrawSpans16RGB
	else
_D_DrawSpans16
	endif
	movem.l	d2-d7/a2-a6,-(sp)
	fmovem.x	fp2-fp7,-(sp)
	move.l	4+SAVED_INT+SAVED_FP(sp),a2
	move.l	a2,v_pspan
	move.l	_cacheblock,a5
	fmove.s	#$47800000,fp4			; 65536.0: z is prescaled to 16.16

	; the 16-pixel steps, rounded to float as the C's locals are
	fmove.s	_d_sdivzstepu,fp0
	fmul.s	#$41800000,fp0
	fmove.s	fp0,v_sdivzn
	fmove.s	_d_tdivzstepu,fp0
	fmul.s	#$41800000,fp0
	fmove.s	fp0,v_tdivzn
	fmove.s	_d_zistepu,fp0
	fmul.s	#$41800000,fp0
	fmove.s	fp0,v_zin

	SpanFP	a2

.span
	; A span's start: s and t at its first pixel, its first block's divide
	; and far end, and the block-end setup - SpanDrawer's float and integer
	; work, hand-scheduled so the integer work (which does not depend on
	; the float results it sits between) runs while the FPU computes.
	move.l	SPAN_COUNT(a2),d0
	move.l	d0,a3
	; s at the first pixel (FixedAtZ), with pdest and the counters
	fmove.x	fp3,fp0
	move.l	_r_screenwidth,d1
	fmul.x	fp6,fp0
	muls.l	SPAN_V(a2),d1
	fintrz.x	fp0
	move.l	_d_viewbuffer,a1
	if PIX16
	add.l	SPAN_U(a2),d1
	add.l	d1,d1				; 2 bytes a pixel
	add.l	d1,a1
	else
	add.l	d1,a1
	add.l	SPAN_U(a2),a1
	endif
	fmove.l	fp0,d0
	move.l	a3,d1
	add.l	d1,_q2c_count+Q2C_SPAN_PIXELS
	; t (FixedAtZ, in fp1), with s clamped to 0 .. bbextents
	fmove.x	fp3,fp1
	add.l	_sadjust,d0
	addq.l	#1,_q2c_count+Q2C_SPANS
	fmul.x	fp5,fp1
	cmp.l	_bbextents,d0
	ble	.s_lo
	move.l	_bbextents,d0
	bra	.s_ok
.s_lo
	tst.l	d0
	bge	.s_ok
	moveq	#0,d0
.s_ok
	move.l	d0,a4
	fintrz.x	fp1
	move.l	SPAN_NEXT(a2),v_nextspan
	fmove.l	fp1,d0
	add.l	_tadjust,d0
	cmp.l	_bbextentt,d0
	ble	.t_lo
	move.l	_bbextentt,d0
	bra	.t_ok
.t_lo
	tst.l	d0
	bge	.t_ok
	moveq	#0,d0
.t_ok
	move.l	d0,a6

	; the first block's divide (StartDiv): a full block, or the span's last
	move.l	a3,d1
	moveq	#16,d0
	cmp.l	d0,d1
	ble	.c0last
	fadd.s	v_sdivzn,fp6
	fadd.s	v_tdivzn,fp5
	fadd.s	v_zin,fp2
	fmove.x	fp4,fp3
	fdiv.x	fp2,fp3
	; While it runs: the next span's record into the cache (its u and v are
	; read later; loads block on the 68060, but this one overlaps the
	; divide), and which loop the first block will run (from the count; as
	; .select does from what is left after a block).
	move.l	v_nextspan,d0
	beq	.nopf
	move.l	d0,a0
	tst.b	(a0)
.nopf
	move.l	a3,d0
	sub.l	#16,d0				; pixels left after the first block
	cmp.l	#16,d0
	ble	.fnextlast
	sub.l	#16,d0
	cmp.l	#16,d0
	bgt	.fselmid
	subq.l	#1,d0
	move.l	d0,v_lastn
	lea	.vlast(pc),a0
	bra	.fsel
.fselmid
	lea	.vmid(pc),a0
	bra	.fsel
.fnextlast
	lea	.vend(pc),a0
	tst.l	v_nextspan
	beq	.fsel
	lea	.vspan(pc),a0
.fsel
	; the first block's far end (A), with PixStart between
	fmove.x	fp3,fp0
	move.l	a6,d7
	clr.w	d7
	fmove.x	fp3,fp1
	swap	d7
	mulu.w	_cachewidth+2,d7
	fmul.x	fp6,fp0
	move.l	a4,d0
	clr.w	d0
	fmul.x	fp5,fp1
	swap	d0
	fintrz.x	fp0
	add.l	d0,d7
	move.l	a4,d4
	fintrz.x	fp1
	swap	d4
	clr.w	d4
	fmove.l	fp0,d2
	move.l	a6,d5
	swap	d5
	fmove.l	fp1,d3
	clr.w	d5
	BlockEnd	.lastfirst		; (a full block: falls through)
	; the second block's divide (StartDiv, kind by what is left: d6 free
	; until the loop), with PixEnd between
	move.l	a3,d6
	cmp.l	#16,d6
	ble	.c1last
	fadd.s	v_sdivzn,fp6
	move.l	d2,d0
	swap	d0
	ext.l	d0
	move.l	d3,d1
	fadd.s	v_tdivzn,fp5
	swap	d1
	ext.l	d1
	muls.l	_cachewidth,d1
	add.l	d1,d0
	fadd.s	v_zin,fp2
	swap	d2
	clr.w	d2
	swap	d3
	clr.w	d3
	fmove.x	fp4,fp3
	move.l	d2,a4
	move.l	d3,a6
	move.l	d4,d2
	move.l	d5,d3
	fdiv.x	fp2,fp3
	move.l	d0,d4
	move.l	_cachewidth,d5
	move.l	d7,d0
	move.l	d7,d1
	jmp	(a0)
.c1last
	subq.l	#1,d6
	fmove.l	d6,fp0
	move.l	d2,d0
	swap	d0
	fmove.s	_d_sdivzstepu,fp1
	ext.l	d0
	move.l	d3,d1
	fmul.x	fp0,fp1
	swap	d1
	ext.l	d1
	fadd.x	fp1,fp6
	muls.l	_cachewidth,d1
	add.l	d1,d0
	fmove.s	_d_tdivzstepu,fp1
	swap	d2
	clr.w	d2
	fmul.x	fp0,fp1
	swap	d3
	fadd.x	fp1,fp5
	clr.w	d3
	move.l	d2,a4
	fmul.s	_d_zistepu,fp0
	move.l	d3,a6
	move.l	d4,d2
	fadd.x	fp0,fp2
	move.l	d5,d3
	move.l	d0,d4
	fmove.x	fp4,fp3
	move.l	_cachewidth,d5
	move.l	d7,d0
	fdiv.x	fp2,fp3
	move.l	d7,d1
	jmp	(a0)

.c0last
	subq.l	#1,d1
	fmove.l	d1,fp0
	fmove.s	_d_sdivzstepu,fp1
	fmul.x	fp0,fp1
	fadd.x	fp1,fp6
	fmove.s	_d_tdivzstepu,fp1
	fmul.x	fp0,fp1
	fadd.x	fp1,fp5
	fmul.s	_d_zistepu,fp0
	fadd.x	fp0,fp2
	fmove.x	fp4,fp3
	fdiv.x	fp2,fp3
	move.l	v_nextspan,d0
	beq	.nopf2
	move.l	d0,a0
	tst.b	(a0)
.nopf2
	; the (only) block's far end (A), with PixStart between
	fmove.x	fp3,fp0
	move.l	a6,d7
	clr.w	d7
	fmove.x	fp3,fp1
	swap	d7
	mulu.w	_cachewidth+2,d7
	fmul.x	fp6,fp0
	move.l	a4,d0
	clr.w	d0
	fmul.x	fp5,fp1
	swap	d0
	fintrz.x	fp0
	add.l	d0,d7
	move.l	a4,d4
	fintrz.x	fp1
	swap	d4
	clr.w	d4
	fmove.l	fp0,d2
	move.l	a6,d5
	swap	d5
	fmove.l	fp1,d3
	clr.w	d5
	BlockEnd	.lastfirst		; (always taken: the span's last block)

.after
	; s, t = this block's far end; the next block's A is in v_sraw/v_traw
	move.l	a2,a4
	move.l	v_tnext,a6
	PixStart
	move.l	v_sraw,d2
	move.l	v_traw,d3
	BlockEnd	.lastlater

.select
	; which loop: the next block is full (r > 16) or the last; the one
	; after it is full, the last (its count - 1 into v_lastn), or the next
	; span's start
	move.l	a3,d0
	cmp.l	#16,d0
	ble	.nextlast
	sub.l	#16,d0				; pixels left after the next block
	cmp.l	#16,d0
	bgt	.selmid
	subq.l	#1,d0
	move.l	d0,v_lastn
	lea	.vlast(pc),a0
	bra	.selected
.selmid
	lea	.vmid(pc),a0
	bra	.selected
.nextlast
	lea	.vend(pc),a0
	tst.l	v_nextspan
	beq	.selected
	lea	.vspan(pc),a0
.selected
	PixEnd
	jmp	(a0)

;  next block full, the one after it full: A, then its divide
.vmid
	Pixel	d0,d1
	fmove.x	fp3,fp0
	Pixel	d1,d0
	fmove.x	fp3,fp1
	Pixel	d0,d1
	fmul.x	fp6,fp0
	Pixel	d1,d0
	fmul.x	fp5,fp1
	Pixel	d0,d1
	fintrz.x	fp0
	Pixel	d1,d0
	fintrz.x	fp1
	Pixel	d0,d1
	fmove.l	fp0,v_sraw
	Pixel	d1,d0
	fmove.l	fp1,v_traw
	Pixel	d0,d1
	fadd.s	v_sdivzn,fp6
	Pixel	d1,d0
	fadd.s	v_tdivzn,fp5
	Pixel	d0,d1
	fadd.s	v_zin,fp2
	Pixel	d1,d0
	Pixel	d0,d1
	fmove.x	fp4,fp3
	Pixel	d1,d0
	fdiv.x	fp2,fp3
	Pixel	d0,d1
	Pixel	d1,d0
	bra	.after

;  next block full, the one after it the last: A, then its divide
.vlast
	Pixel	d0,d1
	fmove.x	fp3,fp0
	Pixel	d1,d0
	fmove.x	fp3,fp1
	Pixel	d0,d1
	fmul.x	fp6,fp0
	Pixel	d1,d0
	fmul.x	fp5,fp1
	Pixel	d0,d1
	fintrz.x	fp0
	Pixel	d1,d0
	fintrz.x	fp1
	Pixel	d0,d1
	fmove.l	fp0,v_sraw
	Pixel	d1,d0
	fmove.l	fp1,v_traw
	Pixel	d0,d1
	fmove.l	v_lastn,fp0
	fmove.s	_d_sdivzstepu,fp1
	Pixel	d1,d0
	fmul.x	fp0,fp1
	fadd.x	fp1,fp6
	Pixel	d0,d1
	fmove.s	_d_tdivzstepu,fp1
	Pixel	d1,d0
	fmul.x	fp0,fp1
	fadd.x	fp1,fp5
	Pixel	d0,d1
	fmul.s	_d_zistepu,fp0
	Pixel	d1,d0
	fadd.x	fp0,fp2
	fmove.x	fp4,fp3
	Pixel	d0,d1
	fdiv.x	fp2,fp3
	Pixel	d1,d0
	bra	.after

;  next block the last: A, then the next span's start and divide
.vspan
	Pixel	d0,d1
	fmove.x	fp3,fp0
	Pixel	d1,d0
	fmove.x	fp3,fp1
	Pixel	d0,d1
	fmul.x	fp6,fp0
	Pixel	d1,d0
	fmul.x	fp5,fp1
	Pixel	d0,d1
	fintrz.x	fp0
	Pixel	d1,d0
	fintrz.x	fp1
	Pixel	d0,d1
	fmove.l	fp0,v_sraw
	Pixel	d1,d0
	fmove.l	fp1,v_traw
	Pixel	d0,d1
	move.l	v_nextspan,a0
	fmove.l	SPAN_U(a0),fp1
	fmove.l	SPAN_V(a0),fp0
	fmove.s	_d_sdivzstepv,fp2
	Pixel	d1,d0
	fmul.x	fp0,fp2
	fadd.s	_d_sdivzorigin,fp2
	fmove.s	_d_sdivzstepu,fp6
	Pixel	d0,d1
	fmul.x	fp1,fp6
	fadd.x	fp2,fp6
	fmove.s	_d_tdivzstepv,fp2
	Pixel	d1,d0
	fmul.x	fp0,fp2
	fadd.s	_d_tdivzorigin,fp2
	fmove.s	_d_tdivzstepu,fp5
	fmul.x	fp1,fp5
	Pixel	d0,d1
	fadd.x	fp2,fp5
	fmul.s	_d_zistepv,fp0
	fadd.s	_d_ziorigin,fp0
	Pixel	d1,d0
	fmul.s	_d_zistepu,fp1
	fadd.x	fp0,fp1
	fmove.s	fp1,v_zitmp
	Pixel	d0,d1
	fmove.s	v_zitmp,fp2
	fmove.x	fp4,fp3
	fdiv.x	fp2,fp3
	Pixel	d1,d0
	bra	.after

;  next block the last of the last span: A only
.vend
	Pixel	d0,d1
	fmove.x	fp3,fp0
	Pixel	d1,d0
	fmove.x	fp3,fp1
	Pixel	d0,d1
	fmul.x	fp6,fp0
	Pixel	d1,d0
	fmul.x	fp5,fp1
	Pixel	d0,d1
	fintrz.x	fp0
	Pixel	d1,d0
	fintrz.x	fp1
	Pixel	d0,d1
	fmove.l	fp0,v_sraw
	Pixel	d1,d0
	fmove.l	fp1,v_traw
	Pixel	d0,d1
	Pixel	d1,d0
	Pixel	d0,d1
	Pixel	d1,d0
	Pixel	d0,d1
	Pixel	d1,d0
	Pixel	d0,d1
	Pixel	d1,d0
	bra	.after

.lastfirst
	; a span of one block: the next span's start in line too (SpanFP, a2
	; = the next span), with PixEnd between
	LastSteps
	moveq	#16,d0
	sub.l	d6,d0
	mulu.w	#PIXEL_SIZE,d0
	lea	.pixels(pc,d0.l),a0
	move.l	v_nextspan,d0
	beq	.lastnofp
	move.l	d0,a2
	fmove.l	SPAN_U(a2),fp1
	move.l	d2,d0
	fmove.l	SPAN_V(a2),fp0
	swap	d0
	fmove.s	_d_sdivzstepv,fp2
	ext.l	d0
	fmul.x	fp0,fp2
	move.l	d3,d1
	fadd.s	_d_sdivzorigin,fp2
	swap	d1
	fmove.s	_d_sdivzstepu,fp6
	ext.l	d1
	fmul.x	fp1,fp6
	muls.l	_cachewidth,d1
	fadd.x	fp2,fp6
	add.l	d1,d0
	fmove.s	_d_tdivzstepv,fp2
	swap	d2
	fmul.x	fp0,fp2
	clr.w	d2
	fadd.s	_d_tdivzorigin,fp2
	fmove.s	_d_tdivzstepu,fp5
	swap	d3
	fmul.x	fp1,fp5
	clr.w	d3
	fadd.x	fp2,fp5
	move.l	d2,a4
	fmul.s	_d_zistepv,fp0
	move.l	d3,a6
	fadd.s	_d_ziorigin,fp0
	move.l	d4,d2
	fmul.s	_d_zistepu,fp1
	move.l	d5,d3
	fadd.x	fp0,fp1
	move.l	d0,d4
	fmove.s	fp1,v_zitmp
	move.l	_cachewidth,d5
	fmove.s	v_zitmp,fp2
	move.l	d7,d0
	fmove.x	fp4,fp3
	move.l	d7,d1
	fdiv.x	fp2,fp3
	jmp	(a0)
.lastnofp
	PixEnd
	jmp	(a0)
.lastlater
	; the next span's start is in flight already (.vspan)
	LastSteps
.lastgo
	; enter the unrolled loop at the copy that leaves n pixels
	moveq	#16,d0
	sub.l	d6,d0
	mulu.w	#PIXEL_SIZE,d0
	lea	.pixels(pc,d0.l),a0
	PixEnd
	jmp	(a0)

	cnop	0,4
.pixels
	rept	8
	Pixel	d0,d1
	Pixel	d1,d0
	endr

	; the next span
	move.l	v_nextspan,d0
	move.l	d0,v_pspan
	move.l	d0,a2
	bne	.span

	fmovem.x	(sp)+,fp2-fp7
	movem.l	(sp)+,d2-d7/a2-a6
	rts


	if PIX16==0
;------------------------------------------------------------------------------
; void D_DrawZSpans (espan_t *pspan)
;
; The z of each span pixel: izi = (int)(1/z * 0x8000 * 0x10000) at the span's
; start, stepped by izistep a pixel, and (short)(izi >> 16) stored.  The float
; part is vbcc's code for the C, operation for operation: 1/z stays extended
; (the C's float zi lives in a register and is never rounded to single), times
; 32768 then 65536, truncated (FINTRZ).
;
; The C stored one word a pixel, shifting each.  Here pairs: two
; accumulators, for the even and the odd pixels, each stepped by 2 * izistep
; (the same values mod 2^32), and the odd one's high word SWAPped into the
; even one's low word, so a pair is one longword store (the C's pair, packed
; big-endian).  Like the C, a span on an odd word address starts with one
; word, and an odd count ends with one.
;
;   a0 = pdest   a1 = the span   d1 = izi (even pixels)   d6 = izi (odd)
;   d4 = izistep   d5 = 2 * izistep   d2 = count   d3 = pairs left
;------------------------------------------------------------------------------

	cnop	0,4
_D_DrawZSpans
	movem.l	d2-d7,-(sp)
	move.l	4+6*4(sp),a1
	fmove.s	_d_zistepu,fp0
	fmul.s	#$47000000,fp0			; * 32768.0
	fmul.s	#$47800000,fp0			; * 65536.0
	fintrz.x	fp0,fp0
	fmove.l	fp0,d4				; izistep
	move.l	d4,d5
	add.l	d5,d5				; 2 * izistep

.span
	move.l	SPAN_V(a1),d0
	move.l	_d_zwidth,d1
	mulu.l	d0,d1
	add.l	SPAN_U(a1),d1
	move.l	_d_pzbuffer,a0
	lea	(a0,d1.l*2),a0			; pdest
	move.l	SPAN_COUNT(a1),d2
	add.l	d2,_q2c_count+Q2C_ZSPAN_PIXELS
	; zi = d_ziorigin + dv*d_zistepv + du*d_zistepu, as vbcc evaluates it
	fmove.l	SPAN_U(a1),fp1
	fmove.l	d0,fp0
	fmul.s	_d_zistepv,fp0
	fadd.s	_d_ziorigin,fp0
	fmul.s	_d_zistepu,fp1
	fadd.x	fp1,fp0
	fmul.s	#$47000000,fp0
	fmul.s	#$47800000,fp0
	fintrz.x	fp0,fp0
	fmove.l	fp0,d1				; izi

	move.l	a0,d0
	btst	#1,d0
	beq.s	.aligned
	move.l	d1,d0
	swap	d0
	move.w	d0,(a0)+
	add.l	d4,d1
	subq.l	#1,d2
.aligned
	move.l	d2,d3
	asr.l	#1,d3				; pairs
	ble.s	.tail
	move.l	d1,d6
	add.l	d4,d6				; the odd pixels' izi
.pair
	move.l	d1,d0				; this pixel's z in the high word ...
	move.l	d6,d7
	swap	d7
	move.w	d7,d0				; ... the next one's in the low word
	add.l	d5,d1
	add.l	d5,d6
	move.l	d0,(a0)+
	subq.l	#1,d3
	bgt.s	.pair
.tail
	btst	#0,d2
	beq.s	.next
	move.l	d1,d0
	swap	d0
	move.w	d0,(a0)
.next
	move.l	SPAN_NEXT(a1),d0
	move.l	d0,a1
	bne	.span

	movem.l	(sp)+,d2-d7
	rts

	endif

;------------------------------------------------------------------------------
; recip_up: 1/d for d = 1..15 as 80-bit extendeds, rounded UP (generated with
; exact rational arithmetic).  For |x| < 2^25 and a quotient truncated toward
; zero, x * recip_up[d] gives exactly x / d: the product can only err upward
; in magnitude, by far less than the 1/15 that separates a non-integer
; quotient from the next integer, and an integer quotient is representable,
; so rounding the product cannot fall below it.
;------------------------------------------------------------------------------

	section	DATA,data

	cnop	0,4
recip_up
	dc.l	$3FFF0000,$80000000,$00000000		; 1/1
	dc.l	$3FFE0000,$80000000,$00000000		; 1/2
	dc.l	$3FFD0000,$AAAAAAAA,$AAAAAAAB		; 1/3
	dc.l	$3FFD0000,$80000000,$00000000		; 1/4
	dc.l	$3FFC0000,$CCCCCCCC,$CCCCCCCD		; 1/5
	dc.l	$3FFC0000,$AAAAAAAA,$AAAAAAAB		; 1/6
	dc.l	$3FFC0000,$92492492,$4924924A		; 1/7
	dc.l	$3FFC0000,$80000000,$00000000		; 1/8
	dc.l	$3FFB0000,$E38E38E3,$8E38E38F		; 1/9
	dc.l	$3FFB0000,$CCCCCCCC,$CCCCCCCD		; 1/10
	dc.l	$3FFB0000,$BA2E8BA2,$E8BA2E8C		; 1/11
	dc.l	$3FFB0000,$AAAAAAAA,$AAAAAAAB		; 1/12
	dc.l	$3FFB0000,$9D89D89D,$89D89D8A		; 1/13
	dc.l	$3FFB0000,$92492492,$4924924A		; 1/14
	dc.l	$3FFB0000,$88888888,$88888889		; 1/15

;------------------------------------------------------------------------------
; D_DrawSpans16's state across blocks.  Statics, not stack: one caller, no
; recursion, and the 060 data cache keeps them as close as a frame would.
;------------------------------------------------------------------------------

	section	BSS,bss

	cnop	0,4
v_sdivzn	ds.l	1			; float: d_sdivzstepu * N
v_tdivzn	ds.l	1			; float: d_tdivzstepu * N
v_zin	ds.l	1				; float: d_zistepu * N
v_tnext	ds.l	1				; t at the current block's far end
v_pspan	ds.l	1				; the span being drawn
v_nextspan	ds.l	1			; D_DrawSpans16: the span after it
v_sraw	ds.l	1				; D_DrawSpans16: the next block's far-end s ...
v_traw	ds.l	1				; ... and t, before the adjustment (A)
v_lastn	ds.l	1				; D_DrawSpans16: a last block's count - 1
v_zitmp	ds.l	1				; D_DrawSpans16: zi rounded to float

	end
