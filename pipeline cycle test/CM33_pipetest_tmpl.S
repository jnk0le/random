/*!
 * \file CM33_pipetest_tmpl.S
 * \brief cortex-m33 dedicated template fore pipeline testing
 *
 * \author jnk0le <jnk0le@hotmail.com>
 * \copyright SPDX-License-Identifier: CC0-1.0
 */

.syntax unified
.thumb
.text

//uint32_t CM33_pipetest_tmpl(void);
.global CM33_pipetest_tmpl
.type   CM33_pipetest_tmpl,%function
CM33_pipetest_tmpl:
	push {r4-r12,lr} // push r12 to keep stack aligned for testing use
	vpush {s16-s31}

	ldr r12, =0x20000000

	// prepare registers here
	// r0 to r8, r12 and lr are available for use
	// r9 is a loop counter

	ldr r11, =0xE0001000 // DWT_BASE
	cpsid i // disable interrupts

	.balign 8
	dsb
	isb

	movs.w r9, #1000 // also start with consistent condition flags
	nop.w

	ldr.w r10, [r11, #4] // read DWT.CYCCNT
	nop.w

1:  //replace code below, don't use nops as those dual issue
	mov.n r10, r10
	mov.n r10, r10
	mov.n r10, r10
	mov.n r10, r10
	mov.n r10, r10
	mov.n r10, r10
	mov.n r10, r10
	mov.n r10, r10
	mov.n r10, r10
	mov.n r10, r10
	mov.n r10, r10
	mov.n r10, r10
	mov.n r10, r10
	mov.n r10, r10
	mov.n r10, r10
	mov.n r10, r10
	mov.n r10, r10

	subs.w r9, #1 // generate condition 2 cycles ahead
	mov.n r10, r10 // must be .n
	bne 1b // 1 cycle taken, 0 not taken

	//end
	nop.w
	ldr.w r9, [r11, #4] // read DWT.CYCCNT

	cpsie i // reenable interrupts

	subs r0, r9, r10 // get cycle diff
	subs r0, #2 // subtract extra overhead outside the loop
				// 1 from tick reads, 2 from extra nops, -1 from last branch

	vpop {s16-s31}
	pop {r4-r12,pc}
