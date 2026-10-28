;
; r_part060.s -- the particle drawer for the 68060
;
; R_DrawParticleList (particle_t *p, int n): R_DrawParticles' loop with
; R_DrawParticle inline, replacing both in r_part.c (built with Q2_ASM=1).
; For each particle: its blend level from its alpha, its view-space position,
; the projection, the screen test, and a pix x pix block, z-tested, of its
; colour - opaque, or through vid.alphamap at 66% or 33%.  The C called
; R_DrawParticle per particle through a global parameter block and kept
; every value in memory.
;
; The float work is vbcc's code for the C, operation for operation: the
; local position and the transformed one are float variables (rounded to
; single, here by FSMOVE), 1/z stays extended, and the screen position is
; truncated after adding 0.5.  tools/vamos_crc.sh checks it.
;
; Calling convention: vbcc's, arguments on the stack, d0-d1/a0-a1/fp0-fp1
; scratch, everything else preserved.
;

	machine	68060

	xref	_r_origin			; vec3_t
	xref	_r_pright			; vec3_t: vright * xscaleshrink (R_DrawParticles)
	xref	_r_pup				; vec3_t: vup * yscaleshrink
	xref	_r_ppn				; vec3_t: vpn
	xref	_xcenter			; float
	xref	_ycenter
	xref	_d_vrectx			; int: the particle clip rectangle
	xref	_d_vrecty
	xref	_d_vrectright_particle
	xref	_d_vrectbottom_particle
	xref	_d_pzbuffer			; short *
	xref	_d_zwidth			; unsigned, in shorts
	xref	_d_viewbuffer		; byte *
	xref	_d_scantable		; int []: frame row offsets
	xref	_r_screenwidth		; int
	xref	_d_pix_shift		; int
	xref	_d_pix_min
	xref	_d_pix_max
	xref	_vid				; viddef_t: alphamap at VID_ALPHAMAP

	xdef	_R_DrawParticleList

; particle_t (ref.h), 20 bytes
P_ORG		equ	0
P_COLOR		equ	12
P_ALPHA		equ	16
P_SIZE		equ	20
VID_ALPHAMAP	equ	8			; offsetof (viddef_t, alphamap); r_part.c checks it

	section	CODE,code

;------------------------------------------------------------------------------
;   a5 = particle   d7 = particles left
;   per particle: a2 = pz   a3 = pdest   d5 = izi   d6 = color   d4 = pix
;------------------------------------------------------------------------------

; The pixel stores, with d1 the column, a3 the row, d6 the colour, a1
; vid.alphamap: 33% is alphamap[color + (dest << 8)], 66% alphamap[(color <<
; 8) + dest], opaque the colour itself (its low byte, as the C stores it).
Store33	macro
	moveq	#0,d0
	move.b	(a3,d1.l),d0
	lsl.l	#8,d0
	add.l	d6,d0
	move.b	(a1,d0.l),(a3,d1.l)
	endm

Store66	macro
	moveq	#0,d2
	move.b	(a3,d1.l),d2
	move.l	d6,d0
	lsl.l	#8,d0
	add.l	d2,d0
	move.b	(a1,d0.l),(a3,d1.l)
	endm

StoreOpaque	macro
	move.b	d6,(a3,d1.l)
	endm

; One particle's block: pix rows of pix pixels, each drawn if pz <= izi.
; \1 = the pixel store macro.
Block	macro
	move.l	d4,d3				; rows
	move.l	_d_zwidth,a4
	add.l	a4,a4				; a row of the z-buffer, in bytes
	move.l	_r_screenwidth,a6
.row\@
	moveq	#0,d1
.pix\@
	move.w	(a2,d1.l*2),d0
	ext.l	d0
	cmp.l	d0,d5
	blt.s	.hid\@				; *pz > izi: behind
	move.w	d5,(a2,d1.l*2)
	\1
.hid\@
	addq.l	#1,d1
	cmp.l	d1,d4
	bgt.s	.pix\@
	add.l	a4,a2
	add.l	a6,a3
	subq.l	#1,d3
	bne.s	.row\@
	endm

	cnop	0,4
_R_DrawParticleList
	movem.l	d2-d7/a2-a6,-(sp)
	fmovem.x	fp2-fp7,-(sp)
	move.l	4+11*4+6*12(sp),a5
	move.l	8+11*4+6*12(sp),d7
	tst.l	d7
	ble	.done

.particle
	; local = single (origin - r_origin)
	fmove.s	P_ORG(a5),fp2
	fsub.s	_r_origin,fp2
	fsmove.x	fp2,fp2
	fmove.s	P_ORG+4(a5),fp3
	fsub.s	_r_origin+4,fp3
	fsmove.x	fp3,fp3
	fmove.s	P_ORG+8(a5),fp4
	fsub.s	_r_origin+8,fp4
	fsmove.x	fp4,fp4
	; transformed[2] = single ((ppn0*l0 + ppn1*l1) + ppn2*l2), tested first
	fmove.s	_r_ppn,fp0
	fmul.x	fp2,fp0
	fmove.s	_r_ppn+4,fp1
	fmul.x	fp3,fp1
	fadd.x	fp1,fp0
	fmove.s	_r_ppn+8,fp7
	fmul.x	fp4,fp7
	fadd.x	fp0,fp7
	fsmove.x	fp7,fp7
	fcmp.d	#$4020000000000000,fp7	; PARTICLE_Z_CLIP 8.0
	fblt	.next
	; zi = 1.0 / transformed[2]
	fmove.d	#$3ff0000000000000,fp6
	fdiv.x	fp7,fp6
	; transformed[0], [1]: the same, along r_pright and r_pup
	fmove.s	_r_pright,fp0
	fmul.x	fp2,fp0
	fmove.s	_r_pright+4,fp1
	fmul.x	fp3,fp1
	fadd.x	fp1,fp0
	fmove.s	_r_pright+8,fp5
	fmul.x	fp4,fp5
	fadd.x	fp0,fp5
	fsmove.x	fp5,fp5				; t0
	fmove.s	_r_pup,fp0
	fmul.x	fp2,fp0
	fmove.s	_r_pup+4,fp1
	fmul.x	fp3,fp1
	fadd.x	fp1,fp0
	fmove.s	_r_pup+8,fp7
	fmul.x	fp4,fp7
	fadd.x	fp0,fp7
	fsmove.x	fp7,fp7				; t1
	; u = (int)((zi * t0 + xcenter) + 0.5), v = (int)((-(zi * t1) + ycenter) + 0.5)
	fmove.x	fp6,fp0
	fmul.x	fp5,fp0
	fadd.s	_xcenter,fp0
	fadd.d	#$3fe0000000000000,fp0
	fintrz.x	fp0
	fmove.l	fp0,d2				; u
	fmove.x	fp6,fp0
	fmul.x	fp7,fp0
	fneg.x	fp0
	fadd.s	_ycenter,fp0
	fadd.d	#$3fe0000000000000,fp0
	fintrz.x	fp0
	fmove.l	fp0,d3				; v
	cmp.l	_d_vrectbottom_particle,d3
	bgt	.next
	cmp.l	_d_vrectright_particle,d2
	bgt	.next
	cmp.l	_d_vrecty,d3
	blt	.next
	cmp.l	_d_vrectx,d2
	blt	.next

	; pz, pdest
	move.l	_d_zwidth,d0
	mulu.l	d3,d0
	add.l	d2,d0
	move.l	_d_pzbuffer,a2
	lea	(a2,d0.l*2),a2
	lea	_d_scantable,a0
	move.l	(a0,d3.l*4),a3
	add.l	_d_viewbuffer,a3
	add.l	d2,a3
	; izi = (int)(zi * 0x8000); pix = izi >> d_pix_shift, clamped
	fmul.s	#$47000000,fp6
	fintrz.x	fp6
	fmove.l	fp6,d5
	move.l	d5,d4
	move.l	_d_pix_shift,d0
	asr.l	d0,d4
	cmp.l	_d_pix_min,d4
	bge.s	.notmin
	move.l	_d_pix_min,d4
	bra.s	.pixok
.notmin
	cmp.l	_d_pix_max,d4
	ble.s	.pixok
	move.l	_d_pix_max,d4
.pixok
	tst.l	d4
	ble	.next				; nothing to draw (the C's loops run no times)
	move.l	P_COLOR(a5),d6
	move.l	_vid+VID_ALPHAMAP,a1

	; the level, as R_DrawParticles sets it: > 0.66 opaque, > 0.33 66%, else 33%
	fmove.s	P_ALPHA(a5),fp0
	fcmp.d	#$3fe51eb851eb851f,fp0	; 0.66
	fbgt	.opaque
	fcmp.d	#$3fd51eb851eb851f,fp0	; 0.33
	fbgt	.p66

	Block	Store33
	bra	.next
.p66
	Block	Store66
	bra	.next
.opaque
	Block	StoreOpaque

.next
	lea	P_SIZE(a5),a5
	subq.l	#1,d7
	bne	.particle

.done
	fmovem.x	(sp)+,fp2-fp7
	movem.l	(sp)+,d2-d7/a2-a6
	rts

	end
