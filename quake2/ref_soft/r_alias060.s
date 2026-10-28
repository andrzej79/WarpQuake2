;
; r_alias060.s -- the alias model vertex transform for the 68060
;
; R_AliasTransformFinalVerts with R_AliasProjectAndClipTestFinalVert inline
; (and R_AliasDrawTriangles, R_AliasPreparePoints' triangle loop, below),
; replacing the C in r_alias.c (built with Q2_ASM=1): every vertex of a
; model's frame, lerped between the two frames, moved by the shell
; (powersuit) offset if the entity has one, transformed to view space, lit,
; and projected and clip-tested, or flagged as behind the near plane.
;
; The float work is vbcc's code for the C, operation for operation:
; - lerped_vert is a float array in the C, so each lerped coordinate (and
;   each shell-moved one) is rounded to single; here FSMOVE does that in a
;   register instead of a store and a reload (the same rounding: the values
;   are small, far inside the single range);
; - xyz[] are stored as floats and the projection reads them back: the
;   stores round them, the projection starts from the stored values;
; - lightcos and 1/z stay extended, as vbcc keeps them in registers.
; The light of a vertex depends only on its normal, so as in the C it is
; computed once per normal per model (v_lcache, valid while v_lstamp
; matches this call's stamp).  tools/vamos_crc.sh checks it all.
;
; What the C paid for besides the arithmetic: a call per vertex for the
; projection, with its FPU register saves, lerped_vert through the stack,
; and the divide waited out.  Here the divide is issued as soon as z is
; known and the lighting and clip flags that do not depend on it (integer
; work) run under it.
;
; Calling convention: vbcc's, arguments on the stack, d0-d1/a0-a1/fp0-fp1
; scratch, everything else preserved.
;
; void R_AliasTransformFinalVerts (int numpoints, finalvert_t *fv,
;                                  dtrivertx_t *oldv, dtrivertx_t *newv)
;

	machine	68060

	xref	_r_lerp_move		; vec3_t
	xref	_r_lerp_backv		; vec3_t
	xref	_r_lerp_frontv		; vec3_t
	xref	_aliastransform		; float [3][4]
	xref	_r_avertexnormals	; float [NUMVERTEXNORMALS][3]
	xref	_r_plightvec		; vec3_t
	xref	_r_ambientlight		; int
	xref	_r_shadelight		; float
	xref	_s_ziscale			; float
	xref	_aliasxscale		; float
	xref	_aliasyscale
	xref	_aliasxcenter
	xref	_aliasycenter
	xref	_r_refdef			; oldrefdef_t: the alias clip rectangle
	xref	_currententity		; entity_t *: flags

	xref	_aliastriangleparms	; finalvert_t *a, *b, *c
	xref	_R_DrawTriangle		; r_raster060.s
	xref	_R_AliasClipTriangle	; r_aclip.c
	xref	_q2c_count			; unsigned long[]: q2prof.h work counters

	xdef	_R_AliasTransformFinalVerts
	xdef	_R_AliasDrawTriangles

NUMVERTEXNORMALS	equ	162
; finalvert_t (r_local.h), 40 bytes
FV_U		equ	0
FV_V		equ	4
FV_S		equ	8
FV_T		equ	12
FV_L		equ	16
FV_ZI		equ	20
FV_FLAGS	equ	24
FV_X		equ	28
FV_Y		equ	32
FV_Z		equ	36
FV_SIZE		equ	40
; dtrivertx_t (qfiles.h): v[3], lightnormalindex
TV_SIZE		equ	4
TV_LNI		equ	3
; oldrefdef_t (r_local.h): r_alias.c checks these at compile time
RD_ALIASVRECT_X		equ	20
RD_ALIASVRECT_Y		equ	24
RD_ALIASVRECTRIGHT	equ	48
RD_ALIASVRECTBOTTOM	equ	52
ENTITY_FLAGS	equ	68			; offsetof (entity_t, flags)
SHELL_FLAGS	equ	$31c00			; RF_SHELL_RED | GREEN | BLUE | DOUBLE | HALF_DAM
ALIAS_LEFT_CLIP		equ	1
ALIAS_TOP_CLIP		equ	2
ALIAS_RIGHT_CLIP	equ	4
ALIAS_BOTTOM_CLIP	equ	8
ALIAS_Z_CLIP		equ	16
; dtriangle_t (qfiles.h): short index_xyz[3], index_st[3]; dstvert_t: short s, t
TRI_ST		equ	6
TRI_SIZE	equ	12
; q2c_count[] byte offsets: the order of q2p_counter_t in q2prof.h
Q2C_ALIAS_TRIS		equ	10*4
Q2C_ALIAS_DRAWN		equ	11*4
Q2C_ALIAS_CLIPPED	equ	12*4

	section	CODE,code

;------------------------------------------------------------------------------
;   a2 = fv   a3 = oldv   a4 = newv   d7 = vertices left
;   a5 = v_lcache   a6 = v_lstamp   d6 = this call's stamp   d5 = shell flag
;   fp7 = 4.0 (ALIAS_Z_CLIP_PLANE, and POWERSUIT_SCALE)
;------------------------------------------------------------------------------

	cnop	0,4
_R_AliasTransformFinalVerts
	movem.l	d2-d7/a2-a6,-(sp)
	fmovem.x	fp2-fp7,-(sp)
	move.l	4+11*4+6*12(sp),d7			; numpoints
	movem.l	8+11*4+6*12(sp),a2-a4			; fv, oldv, newv
	lea	v_lcache,a5
	lea	v_lstamp,a6
	move.l	v_lframe,d6
	addq.l	#1,d6
	move.l	d6,v_lframe
	move.l	_currententity,a0
	move.l	#SHELL_FLAGS,d5
	and.l	ENTITY_FLAGS(a0),d5
	fmove.s	#$40800000,fp7				; 4.0
	tst.l	d7
	ble	.done

.vertex
	; lerped_vert[k] = single ((oldv[k] * backv[k] + move[k]) + newv[k] * frontv[k])
	moveq	#0,d0
	move.b	(a3),d0
	fmove.l	d0,fp0
	fmul.s	_r_lerp_backv,fp0
	fadd.s	_r_lerp_move,fp0
	moveq	#0,d0
	move.b	(a4),d0
	fmove.l	d0,fp2
	fmul.s	_r_lerp_frontv,fp2
	fadd.x	fp0,fp2
	fsmove.x	fp2,fp2				; lv0
	moveq	#0,d0
	move.b	1(a3),d0
	fmove.l	d0,fp0
	fmul.s	4+_r_lerp_backv,fp0
	fadd.s	4+_r_lerp_move,fp0
	moveq	#0,d0
	move.b	1(a4),d0
	fmove.l	d0,fp3
	fmul.s	4+_r_lerp_frontv,fp3
	fadd.x	fp0,fp3
	fsmove.x	fp3,fp3				; lv1
	moveq	#0,d0
	move.b	2(a3),d0
	fmove.l	d0,fp0
	fmul.s	8+_r_lerp_backv,fp0
	fadd.s	8+_r_lerp_move,fp0
	moveq	#0,d0
	move.b	2(a4),d0
	fmove.l	d0,fp4
	fmul.s	8+_r_lerp_frontv,fp4
	fadd.x	fp0,fp4
	fsmove.x	fp4,fp4				; lv2

	; plightnormal = r_avertexnormals[newv->lightnormalindex]
	moveq	#0,d4
	move.b	TV_LNI(a4),d4				; ln
	move.l	d4,d0
	muls.l	#12,d0
	lea	_r_avertexnormals,a1
	add.l	d0,a1

	tst.l	d5
	beq.s	.noshell
	; lerped_vert[k] = single (lerped_vert[k] + n[k] * 4.0)
	fmove.s	(a1),fp0
	fmul.x	fp7,fp0
	fadd.x	fp0,fp2
	fsmove.x	fp2,fp2
	fmove.s	4(a1),fp0
	fmul.x	fp7,fp0
	fadd.x	fp0,fp3
	fsmove.x	fp3,fp3
	fmove.s	8(a1),fp0
	fmul.x	fp7,fp0
	fadd.x	fp0,fp4
	fsmove.x	fp4,fp4
.noshell

	; xyz[r] = single (((at[r][0]*lv0 + at[r][1]*lv1) + at[r][2]*lv2) + at[r][3])
	fmove.s	_aliastransform,fp5
	fmul.x	fp2,fp5
	fmove.s	4+_aliastransform,fp1
	fmul.x	fp3,fp1
	fadd.x	fp1,fp5
	fmove.s	8+_aliastransform,fp1
	fmul.x	fp4,fp1
	fadd.x	fp1,fp5
	fadd.s	12+_aliastransform,fp5
	fsmove.x	fp5,fp5				; x
	fmove.s	16+_aliastransform,fp6
	fmul.x	fp2,fp6
	fmove.s	20+_aliastransform,fp1
	fmul.x	fp3,fp1
	fadd.x	fp1,fp6
	fmove.s	24+_aliastransform,fp1
	fmul.x	fp4,fp1
	fadd.x	fp1,fp6
	fadd.s	28+_aliastransform,fp6
	fsmove.x	fp6,fp6				; y
	fmove.s	32+_aliastransform,fp0
	fmul.x	fp2,fp0
	fmove.s	36+_aliastransform,fp1
	fmul.x	fp3,fp1
	fadd.x	fp1,fp0
	fmove.s	40+_aliastransform,fp1
	fmul.x	fp4,fp1
	fadd.x	fp1,fp0
	fadd.s	44+_aliastransform,fp0
	fsmove.x	fp0,fp4				; z (lv2 is done with)
	fmove.s	fp5,FV_X(a2)
	fmove.s	fp6,FV_Y(a2)
	fmove.s	fp4,FV_Z(a2)

	; the light, once per normal per model
	cmp.l	(a6,d4.l*4),d6
	beq.s	.lit
	move.l	d6,(a6,d4.l*4)
	fmove.s	_r_plightvec,fp0
	fmul.s	(a1),fp0
	fmove.s	4+_r_plightvec,fp1
	fmul.s	4(a1),fp1
	fadd.x	fp1,fp0
	fmove.s	8+_r_plightvec,fp1
	fmul.s	8(a1),fp1
	fadd.x	fp0,fp1				; lightcos
	move.l	_r_ambientlight,d1
	ftst.x	fp1
	fbge	.storelight			; 0 <= lightcos: the ambient alone
	fmul.s	_r_shadelight,fp1
	fintrz.x	fp1
	fmove.l	fp1,d0
	add.l	d0,d1
	bge.s	.storelight
	moveq	#0,d1
.storelight
	move.l	d1,(a5,d4.l*4)
.lit
	move.l	(a5,d4.l*4),FV_L(a2)

	; behind the near plane: flagged, not projected
	fcmp.x	fp7,fp4
	fbge	.project			; 4.0 <= z
	move.l	#ALIAS_Z_CLIP,FV_FLAGS(a2)
	bra	.next

.project
	; zi = 1.0 / z, extended (the divide runs under the integer work below)
	fmove.s	#$3f800000,fp0
	fdiv.x	fp4,fp0
	moveq	#0,d2				; flags
	move.l	_r_refdef+RD_ALIASVRECT_X,a0
	move.l	_r_refdef+RD_ALIASVRECT_Y,a1
	move.l	_r_refdef+RD_ALIASVRECTRIGHT,d3
	move.l	_r_refdef+RD_ALIASVRECTBOTTOM,d4
	; fv->zi = (int)(zi * s_ziscale)
	fmove.s	_s_ziscale,fp1
	fmul.x	fp0,fp1
	fintrz.x	fp1
	fmove.l	fp1,FV_ZI(a2)
	; fv->u = (int)((x * aliasxscale) * zi + aliasxcenter)
	fmul.s	_aliasxscale,fp5
	fmul.x	fp0,fp5
	fadd.s	_aliasxcenter,fp5
	fintrz.x	fp5
	fmove.l	fp5,d0
	; fv->v = (int)(zi * (y * aliasyscale) + aliasycenter)
	fmul.s	_aliasyscale,fp6
	fmul.x	fp6,fp0
	fadd.s	_aliasycenter,fp0
	fintrz.x	fp0
	fmove.l	fp0,d1
	move.l	d0,FV_U(a2)
	move.l	d1,FV_V(a2)
	cmp.l	a0,d0
	bge.s	.c1
	moveq	#ALIAS_LEFT_CLIP,d2
.c1	cmp.l	a1,d1
	bge.s	.c2
	addq.l	#ALIAS_TOP_CLIP,d2
.c2	cmp.l	d3,d0
	ble.s	.c3
	addq.l	#ALIAS_RIGHT_CLIP,d2
.c3	cmp.l	d4,d1
	ble.s	.c4
	addq.l	#ALIAS_BOTTOM_CLIP,d2
.c4	move.l	d2,FV_FLAGS(a2)

.next
	lea	FV_SIZE(a2),a2
	addq.l	#TV_SIZE,a3
	addq.l	#TV_SIZE,a4
	subq.l	#1,d7
	bne	.vertex

.done
	fmovem.x	(sp)+,fp2-fp7
	movem.l	(sp)+,d2-d7/a2-a6
	rts

;------------------------------------------------------------------------------
; void R_AliasDrawTriangles (finalvert_t *fv, dtriangle_t *ptri,
;                            dstvert_t *pstverts, int numtris, int mirrored)
;
; R_AliasPreparePoints' triangle loop (r_alias.c): for each triangle not
; completely clipped, its vertices' s and t from its st indexes, then
; R_DrawTriangle if no vertex is clipped, R_AliasClipTriangle if some are.
; The vertex order is a, b, c = 0, 1, 2, or 2, 1, 0 for the mirrored
; (left-handed) weapon.  As in the C, an unclipped triangle R_DrawTriangle
; would not draw (d_xdenom >= 0 for its order) is skipped before its s/t are
; written; reversing the order negates d_xdenom exactly (screen coordinates,
; nowhere near overflow), so the mirrored test is D <= 0 on the 0, 1, 2
; order's D.
;
;   a5 = fv   a3 = the triangle   a4 = pstverts   d7 = triangles left
;   d6 = mirrored   a0, a1, a2 = vertices 0, 1, 2   d5 = any clip flags
;------------------------------------------------------------------------------

; s, t of vertex \1 (an address register) from st index \2 of the triangle
SetST	macro
	moveq	#0,d0
	move.w	TRI_ST+\2*2(a3),d0
	lea	(a4,d0.l*4),a6
	move.w	(a6),d0
	ext.l	d0
	swap	d0
	clr.w	d0				; s << 16
	move.l	d0,FV_S(\1)
	move.w	2(a6),d0
	ext.l	d0
	swap	d0
	clr.w	d0				; t << 16
	move.l	d0,FV_T(\1)
	endm

	cnop	0,4
_R_AliasDrawTriangles
	movem.l	d2-d7/a2-a6,-(sp)
	move.l	4+11*4(sp),a5			; fv
	move.l	8+11*4(sp),a3			; ptri
	move.l	12+11*4(sp),a4			; pstverts
	move.l	16+11*4(sp),d7
	move.l	20+11*4(sp),d6
	tst.l	d7
	ble	.done

.tri
	moveq	#0,d0
	move.w	(a3),d0
	mulu.w	#FV_SIZE,d0
	lea	(a5,d0.l),a0
	moveq	#0,d0
	move.w	2(a3),d0
	mulu.w	#FV_SIZE,d0
	lea	(a5,d0.l),a1
	moveq	#0,d0
	move.w	4(a3),d0
	mulu.w	#FV_SIZE,d0
	lea	(a5,d0.l),a2

	move.l	FV_FLAGS(a0),d5
	move.l	d5,d1
	move.l	FV_FLAGS(a1),d2
	and.l	d2,d1
	or.l	d2,d5
	move.l	FV_FLAGS(a2),d2
	and.l	d2,d1
	bne	.next				; completely clipped
	or.l	d2,d5
	addq.l	#1,_q2c_count+Q2C_ALIAS_TRIS
	tst.l	d5
	bne.s	.st				; partly clipped: no back-face test

	; D = (u0 - u2) * (v0 - v1) - (u0 - u1) * (v0 - v2)
	move.l	FV_U(a0),d0
	move.l	d0,d1
	sub.l	FV_U(a2),d0			; u0 - u2
	sub.l	FV_U(a1),d1			; u0 - u1
	move.l	FV_V(a0),d2
	move.l	d2,d3
	sub.l	FV_V(a1),d2			; v0 - v1
	sub.l	FV_V(a2),d3			; v0 - v2
	muls.l	d2,d0
	muls.l	d3,d1
	sub.l	d1,d0
	tst.l	d6
	bne.s	.mirror
	tst.l	d0
	bge	.next				; hidden in the 0, 1, 2 order
	bra.s	.st
.mirror
	tst.l	d0
	ble	.next				; hidden in the 2, 1, 0 order

.st
	SetST	a0,0
	SetST	a1,1
	SetST	a2,2

	tst.l	d6
	beq.s	.ordered
	exg	a0,a2				; a, b, c = 2, 1, 0
.ordered
	tst.l	d5
	bne.s	.clip
	addq.l	#1,_q2c_count+Q2C_ALIAS_DRAWN
	movem.l	a0-a2,_aliastriangleparms
	jsr	_R_DrawTriangle
	bra.s	.next
.clip
	addq.l	#1,_q2c_count+Q2C_ALIAS_CLIPPED
	move.l	a2,-(sp)
	move.l	a1,-(sp)
	move.l	a0,-(sp)
	jsr	_R_AliasClipTriangle
	lea	12(sp),sp

.next
	lea	TRI_SIZE(a3),a3
	subq.l	#1,d7
	bne	.tri

.done
	movem.l	(sp)+,d2-d7/a2-a6
	rts

;------------------------------------------------------------------------------

	section	BSS,bss

	cnop	0,4
v_lcache	ds.l	NUMVERTEXNORMALS	; a normal's light ...
v_lstamp	ds.l	NUMVERTEXNORMALS	; ... valid while this equals v_lframe
v_lframe	ds.l	1				; bumped per call (per model)

	end
