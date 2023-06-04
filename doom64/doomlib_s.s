#include <asm.h>
#include <PR/R4300.h>
#include <PR/ultratypes.h>

    .text
	.set noreorder

.globl D_memcpy
.ent D_memcpy

D_memcpy:
	slti	$8, $6, 8		# Less than 8?
	bne	$8, $0, L_last8
	move	$2, $4			# Setup exit value before too late

	xor	$8, $5, $4		# Find $4/$5 displacement
	andi	$8, 0x3
	bne	$8, $0, L_shift	# Go handle the unaligned case
	subu	$9, $0, $5
	andi	$9, 0x3			# $4/$5 are aligned, but are we
	beq	$9, $0, L_chk8w	#  starting in the middle of a word?
	subu	$6, $9
	lwl	$8, 0($5)		# Yes we are... take care of that
	addu	$5, $9
	swl	$8, 0($4)
	addu	$4, $9

L_chk8w:	
	andi	$8, $6, 0x1f		# 32 or more bytes left?
	beq	$8, $6, L_chk1w
	subu	$7, $6, $8		# Yes
	addu	$7, $5			# $7 = end address of loop
	move	$6, $8			# $6 = what will be left after loop
L_lop8w:	
	lw	$8,  0($5)		# Loop taking 8 words at a time
	lw	$9,  4($5)
	lw	$10,  8($5)
	lw	$11, 12($5)
	lw	$12, 16($5)
	lw	$13, 20($5)
	lw	$14, 24($5)
	lw	$15, 28($5)
	addiu	$4, 32
	addiu	$5, 32
	sw	$8, -32($4)
	sw	$9, -28($4)
	sw	$10, -24($4)
	sw	$11, -20($4)
	sw	$12, -16($4)
	sw	$13, -12($4)
	sw	$14,  -8($4)
	bne	$5, $7, L_lop8w
	sw	$15,  -4($4)

L_chk1w:	
	andi	$8, $6, 0x3		# 4 or more bytes left?
	beq	$8, $6, L_last8
	subu	$7, $6, $8		# Yes, handle them one word at a time
	addu	$7, $5			# $7 again end address
	move	$6, $8
L_lop1w:	
	lw	$8, 0($5)
	addiu	$4, 4
	addiu	$5, 4
	bne	$5, $7, L_lop1w
	sw	$8, -4($4)

L_last8:	
	blez	$6, L_lst8e		# Handle last 8 bytes, one at a time
	addu	$7, $6, $5
L_lst8l:	
	lb	$8, 0($5)
	addiu	$4, 1
	addiu	$5, 1
	bne	$5, $7, L_lst8l
	sb	$8, -1($4)
L_lst8e:	
	jr	$31			# Bye, bye
	nop

L_shift:	
	subu	$7, $0, $4		# Src and Dest unaligned 
	andi	$7, 0x3			#  (unoptimized case...)
	beq	$7, $0, L_shf$9
	subu	$6, $7			# $6 = bytes left
	lwl	$8, 0($5)		# Take care of first odd part
	lwr	$8, 3($5)
	addu	$5, $7
	swl	$8, 0($4)
	addu	$4, $7
L_shf$9:	
	andi	$8, $6, 0x3
	subu	$7, $6, $8
	addu	$7, $5
L_shfth:	
	lwl	$9, 0($5)		# Limp through, word by word
	lwr	$9, 3($5)
	addiu	$4, 4
	addiu	$5, 4
	bne	$5, $7, L_shfth
	sw	$9, -4($4)
	b	L_last8		# Handle anything which may be left
	move	$6, $8
	nop

.end D_memcpy

.globl D_memset
.ent D_memset

D_memset:

	slti	$9, $6, 8		# Less than 8?
	bne	$9, $0, L_last8s
	move	$2, $4			# Setup exit value before too late

	beq	$5, $0, L_ueven	# If $0 pattern, no need to extend
	andi	$5, 0xff		# Avoid problems with bogus arguments
	sll	$8, $5, 8
	or	$5, $8
	sll	$8, $5, 16
	or	$5, $8			# $5 is now pattern in full word

L_ueven:	
	subu	$8, $0, $4		# Unaligned address?
	andi	$8, 0x3
	beq	$8, $0, L_chkw
	subu	$6, $8
	swl	$5, 0($4)		# Yes, handle first unaligned part
	addu	$4, $8			# Now both $4 and $6 are updated

L_chkw:	
	andi	$8, $6, 0x7		# Enough left for one loop iteration?
	beq	$8, $6, L_chkl
	subu	$7, $6, $8
	addu	$7, $4			# $7 is last loop address +1
	move	$6, $8			# $6 is now # of bytes left after loop
L_loopw:	
	addiu	$4, 8			# Handle 2 words pr. iteration
	sw	$5, -8($4)
	bne	$4, $7, L_loopw
	sw	$5, -4($4)

L_chkl:	
	andi	$8, $6, 0x4		# Check if there is at least a full
	beq	$8, $0, L_last8s	#  word remaining after the loop
	subu	$6, $8
	sw	$5, 0($4)		# Yes...
	addiu	$4, 4

L_last8s:	
	blez	$6, L_exit		# Handle last 8 bytes (if cnt>0)
	addu	$7, $6, $4		# $7 is last address +1
L_lst8ls:	
	addiu	$4, 1
	bne	$4, $7, L_lst8ls
	sb	$5, -1($4)
L_exit:	
	j	$31			# Bye, bye
	nop
.end D_memset