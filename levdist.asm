
.686
.model flat
.code

; int __stdcall levdist_asm (const char *s1, int l1, const char *s2, int l2);
; int __stdcall levdist_mt_asm (const char *s1, int l1, const char *s2, int l2, collthrd_ctx* pctx);
; int __stdcall levdamdist_asm (const char *s1, int l1, const char *s2, int l2);
; int __stdcall levdamdist_mt_asm (const char *s1, int l1, const char *s2, int l2, collthrd_ctx* pctx);
; int __stdcall levdamdist_asm2 (const char *s1, int l1, const char *s2, int l2);
; int __stdcall levdamdist_mt_asm2 (const char *s1, int l1, const char *s2, int l2, collthrd_ctx* pctx);

_levdist_asm@16 proc
	pushad
	mov	ebp, esp
	cmp	dword ptr [ebp][40], 0
	jnz	short nzl1
	mov	eax, dword ptr [ebp][48]
	mov	[ebp][28], eax
	jmp	toret
nzl1:	cmp	dword ptr [ebp][48], 0
	jnz	short nzl2
	mov	eax, [ebp][40]
	mov	[ebp][28], eax
	jmp	toret
nzl2:	mov	ebx, offset _levdp1a
	cld
	mov	edi, ebx
	xor	eax, eax
	mov	ecx, [ebp][48]
	inc	ecx
@@:	stosd
	inc	eax
	loop	@b

	mov	ebp, offset _levdp2a
i1:	push	ecx
	mov	eax, [ebx]	; eax = p1 [0]
	inc	eax		; p1 [0] + 1
	mov	[ebp], eax	; p2 [0] = p1 [0] + 1
	xor	ecx, ecx	; i2 = 0

; ecx = i2
; ebx = p1
; ebp = p2
@@:	xor	eax, eax
	mov	esi, [esp][40]		; esi = s1
	mov	edx, [esp]		; edx = i1
	mov	edi, [esp][48]		; edi = s2
	mov	al, [esi][edx]		; s1 [i1]
	cmp	al, [edi][ecx]		; s1 [i1] == s2 [i2]
	setne	al
	mov	edx, [ebp][ecx*4]	; p2 [i2]
	add	eax, [ebx][ecx*4]	; c0 = p1 [i2] + (s1 [i1] == s2 [i2])
	inc	ecx			; i2 + 1
	mov	esi, [ebx][ecx*4]	; p1 [i2 + 1]
	inc	edx			; c0 = p2 [i2] + 1
	inc	esi			; c1 = p1 [i2 + 1] + 1
	cmp	edx, eax		; c2 < c0
	cmovb	eax, edx		; c0 = min (c0, c2)
	cmp	esi, eax		; c1 < c0
	cmovb	eax, esi		; c0 = min (c0, c1)
	mov	[ebp][ecx*4], eax	; p2 [i2 + 1] = c0 (min)
	cmp	ecx, [esp][52]		; i2 < l2
	jb	short @b

	xchg	ebx, ebp
	pop	ecx
	inc	ecx
	cmp	ecx, [esp][40]
	jb	short i1

	mov	[esp+28], eax
toret:	popad
	ret	16
_levdist_asm@16 endp

_levdist_mt_asm@20 proc		; TODO
	pushad
	mov	ebp, esp
	cmp	dword ptr [ebp][40], 0
	jnz	short nzl1
	mov	eax, dword ptr [ebp][48]
	mov	[ebp][28], eax
	jmp	toret
nzl1:	cmp	dword ptr [ebp][48], 0
	jnz	short nzl2
	mov	eax, [ebp][40]
	mov	[ebp][28], eax
	jmp	toret
nzl2:	mov	esi, [ebp][52]
	mov	ebx, [esi]
	cld
	mov	edi, ebx
	xor	eax, eax
	mov	ecx, [ebp][48]
	inc	ecx
@@:	stosd
	inc	eax
	loop	@b
	mov	ebp, [esi][4]
i1:	push	ecx
	mov	eax, [ebx]
	inc	eax
	mov	[ebp], eax
	xor	ecx, ecx
@@:	xor	eax, eax
	mov	esi, [esp][40]
	mov	edx, [esp]
	mov	edi, [esp][48]
	mov	al, [esi][edx]
	cmp	al, [edi][ecx]
	setne	al
	mov	edx, [ebp][ecx*4]
	add	eax, [ebx][ecx*4]
	inc	ecx
	mov	esi, [ebx][ecx*4]
	inc	edx
	inc	esi
	cmp	edx, eax
	cmovb	eax, edx
	cmp	esi, eax
	cmovb	eax, esi
	mov	[ebp][ecx*4], eax
	cmp	ecx, [esp][52]
	jb	short @b
	xchg	ebx, ebp
	pop	ecx
	inc	ecx
	cmp	ecx, [esp][40]
	jb	short i1

	mov	[esp][28], eax
toret:	popad
	ret	20
_levdist_mt_asm@20 endp

_levdamdist_asm@16 proc
	pushad
	mov	ebp, esp
	cmp	dword ptr [ebp][40], 0
	jnz	short nzl1
	mov	eax, dword ptr [ebp][48]
	mov	[ebp][28], eax
	jmp	toret
nzl1:	cmp	dword ptr [ebp][48], 0
	jnz	short nzl2
	mov	eax, [ebp][40]
	mov	[ebp][28], eax
	jmp	toret
nzl2:	mov	ebx, offset _levdp1a
	cld
	mov	edi, ebx
	xor	eax, eax
	mov	ecx, [ebp][48]
	inc	ecx
@@:	stosd
	inc	eax
	loop	@b

	mov	ebp, offset _levdp2a
	push	offset _levdp3a
	push	ecx
	mov	eax, [ebx]	; p1 [0]
	inc	eax		; p1 [0] + 1
	mov	[ebp], eax	; p2 [0] = p1 [0] + 1
	xor	ecx, ecx	; i2 = 0

; [esp] = i1
; ecx = i2
; ebx = p1 (row1)
; ebp = p2 (row2)
; [esp][4] = p0 (row0)
@@:	xor	eax, eax
	mov	edx, [esp]		; edx = i1
	mov	esi, [esp][44]		; esi = s1
	mov	edi, [esp][52]		; edi = s2
	mov	al, [esi][edx]		; s1 [i1]
	cmp	al, [edi][ecx]		; s1 [i1] == s2 [i2]
	setne	al
	mov	edx, [ebp][ecx*4]	; p2 [i2]
	add	eax, [ebx][ecx*4]	; c0 = p1 [i2] + (s1 [i1] == s2 [i2])
	inc	ecx			; i2 + 1
	mov	esi, [ebx][ecx*4]	; p1 [i2 + 1]
	inc	edx			; c0 = p2 [i2] + 1
	inc	esi			; c1 = p1 [i2 + 1] + 1
	cmp	edx, eax		; c2 < c0
	cmovb	eax, edx		; c0 = min (c0, c2)
	cmp	esi, eax		; c1 < c0
	cmovb	eax, esi		; c0 = min (c0, c1)
	mov	[ebp][ecx*4], eax	; p2 [i2 + 1] = c0 (min)
	cmp	ecx, [esp][56]		; i2 < l2
	jb	short @b

	pop	ecx
	xchg	ebx, [esp]
	inc	ecx
	xchg	ebp, ebx
	cmp	ecx, [esp][48]
	jae	exit

i1:	push	ecx
	mov	eax, [ebx]	; p1 [0]
	inc	eax		; p1 [0] + 1
	mov	[ebp], eax	; p2 [0] = p1 [0] + 1
	xor	ecx, ecx	; i2 = 0

@@:	xor	eax, eax
	mov	edx, [esp]		; edx = i1
	mov	esi, [esp][44]		; esi = s1
	mov	edi, [esp][52]		; edi = s2
	mov	al, [esi][edx]		; s1 [i1]
	cmp	al, [edi][ecx]		; s1 [i1] == s2 [i2]
	setne	al
	mov	edx, [ebp][ecx*4]	; p2 [i2]
	add	eax, [ebx][ecx*4]	; c0 = p1 [i2] + (s1 [i1] == s2 [i2])
	inc	ecx			; i2 + 1
	mov	esi, [ebx][ecx*4]	; p1 [i2 + 1]
	inc	edx			; c0 = p2 [i2] + 1
	inc	esi			; c1 = p1 [i2 + 1] + 1
	cmp	edx, eax		; c2 < c0
	cmovb	eax, edx		; c0 = min (c0, c2)
	cmp	esi, eax		; c1 < c0
	cmovb	eax, esi		; c0 = min (c0, c1)
	mov	[ebp][ecx*4], eax	; p2 [i2 + 1] = c0 (min)
	cmp	ecx, [esp][56]		; i2 < l2
	jae	short l21

@@:	xor	eax, eax
	mov	edx, [esp]		; i1
	mov	esi, [esp][44]		; esi = s1
	mov	edi, [esp][52]		; edi = s2

	mov	al, [esi][edx][-1]
	cmp	al, [edi][ecx]
	jne	short nosw
	mov	al, [esi][edx]
	cmp	al, [edi][ecx][-1]
	jne	short nosw

	mov	al, [esi][edx]		; s1 [i1]
	mov	esi, [esp][4]		; p0
	cmp	al, [edi][ecx]		; s1 [i1] == s2 [i2]
	mov	esi, [esi][ecx*4][-4]
	setne	al
	inc	esi
	add	eax, [ebx][ecx*4]	; c0 = p1 [i2] + (s1 [i1] == s2 [i2])
	cmp	esi, eax
	mov	edx, [ebp][ecx*4]
	cmovb	eax, esi
	jmp	short sksw

nosw:	mov	al, [esi][edx]		; s1 [i1]
	cmp	al, [edi][ecx]		; s1 [i1] == s2 [i2]
	setne	al
	mov	edx, [ebp][ecx*4]	; p2 [i2]
	add	eax, [ebx][ecx*4]	; c0 = p1 [i2] + (s1 [i1] == s2 [i2])
sksw:	inc	ecx			; i2 + 1
	mov	esi, [ebx][ecx*4]	; p1 [i2 + 1]
	inc	edx			; c2 = p2 [i2] + 1
	inc	esi			; c1 = p1 [i2 + 1] + 1
	cmp	edx, eax		; c2 < c0
	cmovb	eax, edx		; c0 = min (c0, c2)
	cmp	esi, eax		; c1 < c0
	cmovb	eax, esi		; c0 = min (c0, c1)
	mov	[ebp][ecx*4], eax	; p2 [i2 + 1] = c0 (min)
	cmp	ecx, [esp][56]		; i2 < l2
	jb	short @b

l21:	pop	ecx
	xchg	ebx, [esp]
	inc	ecx
	xchg	ebp, ebx
	cmp	ecx, [esp][44]
	jb	i1

exit:	add	esp, 4
	mov	[esp][28], eax
toret:	popad
	ret	16
_levdamdist_asm@16 endp

_levdamdist_mt_asm@20 proc
	pushad
	mov	ebp, esp
	cmp	dword ptr [ebp][40], 0
	jnz	short nzl1
	mov	eax, dword ptr [ebp][48]
	mov	[ebp][28], eax
	jmp	toret
nzl1:	cmp	dword ptr [ebp][48], 0
	jnz	short nzl2
	mov	eax, [ebp][40]
	mov	[ebp][28], eax
	jmp	toret
nzl2:	mov	esi, [ebp][52]
	mov	ebx, [esi]		; p1
	cld
	mov	edi, ebx
	xor	eax, eax
	mov	ecx, [ebp][48]
	inc	ecx
@@:	stosd
	inc	eax
	loop	@b

	mov	ebp, [esi][4]		; p2
	push	dword ptr [esi][8]	; p3
	push	ecx
	mov	eax, [ebx]	; p1 [0]
	inc	eax		; p1 [0] + 1
	mov	[ebp], eax	; p2 [0] = p1 [0] + 1
	xor	ecx, ecx	; i2 = 0

; [esp] = i1
; ecx = i2
; ebx = p1 (row1)
; ebp = p2 (row2)
; [esp][4] = p0 (row0)
@@:	xor	eax, eax
	mov	edx, [esp]		; edx = i1
	mov	esi, [esp][44]		; esi = s1
	mov	edi, [esp][52]		; edi = s2
	mov	al, [esi][edx]		; s1 [i1]
	cmp	al, [edi][ecx]		; s1 [i1] == s2 [i2]
	setne	al
	mov	edx, [ebp][ecx*4]	; p2 [i2]
	add	eax, [ebx][ecx*4]	; c0 = p1 [i2] + (s1 [i1] == s2 [i2])
	inc	ecx			; i2 + 1
	mov	esi, [ebx][ecx*4]	; p1 [i2 + 1]
	inc	edx			; c0 = p2 [i2] + 1
	inc	esi			; c1 = p1 [i2 + 1] + 1
	cmp	edx, eax		; c2 < c0
	cmovb	eax, edx		; c0 = min (c0, c2)
	cmp	esi, eax		; c1 < c0
	cmovb	eax, esi		; c0 = min (c0, c1)
	mov	[ebp][ecx*4], eax	; p2 [i2 + 1] = c0 (min)
	cmp	ecx, [esp][56]		; i2 < l2
	jb	short @b

	pop	ecx
	xchg	ebx, [esp]
	inc	ecx
	xchg	ebp, ebx
	cmp	ecx, [esp][48]
	jae	exit

i1:	push	ecx
	mov	eax, [ebx]	; p1 [0]
	inc	eax		; p1 [0] + 1
	mov	[ebp], eax	; p2 [0] = p1 [0] + 1
	xor	ecx, ecx	; i2 = 0

@@:	xor	eax, eax
	mov	edx, [esp]		; edx = i1
	mov	esi, [esp][44]		; esi = s1
	mov	edi, [esp][52]		; edi = s2
	mov	al, [esi][edx]		; s1 [i1]
	cmp	al, [edi][ecx]		; s1 [i1] == s2 [i2]
	setne	al
	mov	edx, [ebp][ecx*4]	; p2 [i2]
	add	eax, [ebx][ecx*4]	; c0 = p1 [i2] + (s1 [i1] == s2 [i2])
	inc	ecx			; i2 + 1
	mov	esi, [ebx][ecx*4]	; p1 [i2 + 1]
	inc	edx			; c0 = p2 [i2] + 1
	inc	esi			; c1 = p1 [i2 + 1] + 1
	cmp	edx, eax		; c2 < c0
	cmovb	eax, edx		; c0 = min (c0, c2)
	cmp	esi, eax		; c1 < c0
	cmovb	eax, esi		; c0 = min (c0, c1)
	mov	[ebp][ecx*4], eax	; p2 [i2 + 1] = c0 (min)
	cmp	ecx, [esp][56]		; i2 < l2
	jae	short l21

@@:	xor	eax, eax
	mov	edx, [esp]		; i1
	mov	esi, [esp][44]		; esi = s1
	mov	edi, [esp][52]		; edi = s2

	mov	al, [esi][edx][-1]
	cmp	al, [edi][ecx]
	jne	short nosw
	mov	al, [esi][edx]
	cmp	al, [edi][ecx][-1]
	jne	short nosw

	mov	al, [esi][edx]		; s1 [i1]
	mov	esi, [esp][4]		; p0
	cmp	al, [edi][ecx]		; s1 [i1] == s2 [i2]
	mov	esi, [esi][ecx*4][-4]
	setne	al
	inc	esi
	add	eax, [ebx][ecx*4]	; c0 = p1 [i2] + (s1 [i1] == s2 [i2])
	cmp	esi, eax
	mov	edx, [ebp][ecx*4]
	cmovb	eax, esi
	jmp	short sksw

nosw:	mov	al, [esi][edx]		; s1 [i1]
	cmp	al, [edi][ecx]		; s1 [i1] == s2 [i2]
	setne	al
	mov	edx, [ebp][ecx*4]	; p2 [i2]
	add	eax, [ebx][ecx*4]	; c0 = p1 [i2] + (s1 [i1] == s2 [i2])
sksw:	inc	ecx			; i2 + 1
	mov	esi, [ebx][ecx*4]	; p1 [i2 + 1]
	inc	edx			; c2 = p2 [i2] + 1
	inc	esi			; c1 = p1 [i2 + 1] + 1
	cmp	edx, eax		; c2 < c0
	cmovb	eax, edx		; c0 = min (c0, c2)
	cmp	esi, eax		; c1 < c0
	cmovb	eax, esi		; c0 = min (c0, c1)
	mov	[ebp][ecx*4], eax	; p2 [i2 + 1] = c0 (min)
	cmp	ecx, [esp][56]		; i2 < l2
	jb	short @b

l21:	pop	ecx
	xchg	ebx, [esp]
	inc	ecx
	xchg	ebp, ebx
	cmp	ecx, [esp][44]
	jb	i1

exit:	add	esp, 4
	mov	[esp][28], eax
toret:	popad
	ret	20
_levdamdist_mt_asm@20 endp

_levdamdist_asm2@16 proc
	pushad
	mov	ebp, esp
	cmp	dword ptr [ebp][40], 0
	jnz	short nzl1
	mov	eax, dword ptr [ebp][48]
	mov	[ebp][28], eax
	jmp	toret
nzl1:	cmp	dword ptr [ebp][48], 0
	jnz	short nzl2
	mov	eax, [ebp][40]
	mov	[ebp][28], eax
	jmp	toret
nzl2:	mov	ebx, offset _levdp1a
	cld
	mov	edi, ebx
	xor	eax, eax
	mov	ecx, [ebp][48]
	inc	ecx
@@:	stosd
	inc	eax
	loop	@b

	mov	ebp, offset _levdp2a
	push	offset _levdp3a

i1:	push	ecx
	mov	eax, [ebx]	; p1 [0]
	inc	eax		; p1 [0] + 1
	mov	[ebp], eax	; p2 [0] = p1 [0] + 1
	xor	ecx, ecx	; i2 = 0

@@:	xor	eax, eax
	mov	edx, [esp]		; i1
	mov	esi, [esp][44]		; esi = s1
	mov	edi, [esp][52]		; edi = s2

	cmp	edx, eax
	jecxz	short nosw
	jz	short nosw
	mov	al, [esi][edx][-1]
	cmp	al, [edi][ecx]
	jne	short nosw
	mov	al, [esi][edx]
	cmp	al, [edi][ecx][-1]
	jne	short nosw

	mov	al, [esi][edx]		; s1 [i1]
	mov	esi, [esp][4]		; p0
	cmp	al, [edi][ecx]		; s1 [i1] == s2 [i2]
	mov	esi, [esi][ecx*4][-4]
	setne	al
	inc	esi
	add	eax, [ebx][ecx*4]	; c0 = p1 [i2] + (s1 [i1] == s2 [i2])
	cmp	esi, eax
	mov	edx, [ebp][ecx*4]
	cmovb	eax, esi
	jmp	short sksw

nosw:	mov	al, [esi][edx]		; s1 [i1]
	cmp	al, [edi][ecx]		; s1 [i1] == s2 [i2]
	setne	al
	mov	edx, [ebp][ecx*4]	; p2 [i2]
	add	eax, [ebx][ecx*4]	; c0 = p1 [i2] + (s1 [i1] == s2 [i2])
sksw:	inc	ecx			; i2 + 1
	mov	esi, [ebx][ecx*4]	; p1 [i2 + 1]
	inc	edx			; c2 = p2 [i2] + 1
	inc	esi			; c1 = p1 [i2 + 1] + 1
	cmp	edx, eax		; c2 < c0
	cmovb	eax, edx		; c0 = min (c0, c2)
	cmp	esi, eax		; c1 < c0
	cmovb	eax, esi		; c0 = min (c0, c1)
	mov	[ebp][ecx*4], eax	; p2 [i2 + 1] = c0 (min)
	cmp	ecx, [esp][56]		; i2 < l2
	jb	short @b

	pop	ecx
	xchg	ebx, [esp]
	inc	ecx
	xchg	ebp, ebx
	cmp	ecx, [esp][44]
	jb	i1

	add	esp, 4
	mov	[esp][28], eax
toret:	popad
	ret	16
_levdamdist_asm2@16 endp

_levdamdist_mt_asm2@20 proc
	pushad
	mov	ebp, esp
	cmp	dword ptr [ebp][40], 0
	jnz	short nzl1
	mov	eax, dword ptr [ebp][48]
	mov	[ebp][28], eax
	jmp	toret
nzl1:	cmp	dword ptr [ebp][48], 0
	jnz	short nzl2
	mov	eax, [ebp][40]
	mov	[ebp][28], eax
	jmp	toret
nzl2:	mov	esi, [ebp][52]
	mov	ebx, [esi]		; p1
	cld
	mov	edi, ebx
	xor	eax, eax
	mov	ecx, [ebp][48]
	inc	ecx
@@:	stosd
	inc	eax
	loop	@b

	mov	ebp, [esi][4]		; p2
	push	dword ptr [esi][8]	; p3

i1:	push	ecx
	mov	eax, [ebx]	; p1 [0]
	inc	eax		; p1 [0] + 1
	mov	[ebp], eax	; p2 [0] = p1 [0] + 1
	xor	ecx, ecx	; i2 = 0

@@:	xor	eax, eax
	mov	edx, [esp]		; i1
	mov	esi, [esp][44]		; esi = s1
	mov	edi, [esp][52]		; edi = s2

	cmp	edx, eax
	jecxz	short nosw
	jz	short nosw
	mov	al, [esi][edx][-1]
	cmp	al, [edi][ecx]
	jne	short nosw
	mov	al, [esi][edx]
	cmp	al, [edi][ecx][-1]
	jne	short nosw

	mov	al, [esi][edx]		; s1 [i1]
	mov	esi, [esp][4]		; p0
	cmp	al, [edi][ecx]		; s1 [i1] == s2 [i2]
	mov	esi, [esi][ecx*4][-4]
	setne	al
	inc	esi
	add	eax, [ebx][ecx*4]	; c0 = p1 [i2] + (s1 [i1] == s2 [i2])
	cmp	esi, eax
	mov	edx, [ebp][ecx*4]
	cmovb	eax, esi
	jmp	short sksw

nosw:	mov	al, [esi][edx]		; s1 [i1]
	cmp	al, [edi][ecx]		; s1 [i1] == s2 [i2]
	setne	al
	mov	edx, [ebp][ecx*4]	; p2 [i2]
	add	eax, [ebx][ecx*4]	; c0 = p1 [i2] + (s1 [i1] == s2 [i2])
sksw:	inc	ecx			; i2 + 1
	mov	esi, [ebx][ecx*4]	; p1 [i2 + 1]
	inc	edx			; c2 = p2 [i2] + 1
	inc	esi			; c1 = p1 [i2 + 1] + 1
	cmp	edx, eax		; c2 < c0
	cmovb	eax, edx		; c0 = min (c0, c2)
	cmp	esi, eax		; c1 < c0
	cmovb	eax, esi		; c0 = min (c0, c1)
	mov	[ebp][ecx*4], eax	; p2 [i2 + 1] = c0 (min)
	cmp	ecx, [esp][56]		; i2 < l2
	jb	short @b

	pop	ecx
	xchg	ebx, [esp]
	inc	ecx
	xchg	ebp, ebx
	cmp	ecx, [esp][44]
	jb	i1

	add	esp, 4
	mov	[esp][28], eax
toret:	popad
	ret	20
_levdamdist_mt_asm2@20 endp

public _levdist_asm@16
public _levdist_mt_asm@20
public _levdamdist_asm@16
public _levdamdist_mt_asm@20
public _levdamdist_asm2@16
public _levdamdist_mt_asm2@20

extrn _levdp1a : dword
extrn _levdp2a : dword
extrn _levdp3a : dword

end
