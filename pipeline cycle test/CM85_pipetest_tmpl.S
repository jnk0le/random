/*!
 * \file CM85_pipetest_tmpl.S
 * \brief cortex-m85 dedicated template fore pipeline testing
 *
 * \author Jan Oleksiewicz <jnk0le@hotmail.com>
 * \copyright SPDX-License-Identifier: CC0-1.0
 */

.syntax unified
.thumb
.text

//uint32_t CM85_pipetest_tmpl(void);
.global CM85_pipetest_tmpl
.type   CM85_pipetest_tmpl,%function
CM85_pipetest_tmpl:
	push {r4-r12,lr} // push r12 to keep stack aligned for testing use
	vpush {s16-s31}

	// prepare registers here
	// r0 to r8, r12 and lr are available for use
	// r7 is a loop counter

	ldr r11, =0xE0001000 // DWT_BASE
	cpsid i // disable interrupts

	.balign 8
	dsb
	isb

	movs.w r9, #1000 // also start with consistent condition flags
	nop.w

	ldr.w r10, [r11, #4] // read DWT.CYCCNT
	nop.w //dls lr, r7, 2f

1:  //replace code below // don't use nops as those tripple issue
	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	mov.n r11, r11

	subs.w r9, #1 // generate branch condition few cycles ahead of branch
	uxtb r0, r0 // remove slippery condition

	uxtb r1, r1 // remove slippery condition
	mov.n r11, r11

	mov.n r10, r10 
	mov.n r11, r11
	bne.n 1b // le lr, 1b // can tripple issue with target or previous 2 insn


	//end
	mov.n r10, r10 // odd .n in loop
	ldr.w r9, [r11, #4] // read DWT.CYCCNT

	cpsie i // reenable interrupts

	subs r0, r9, r10 // get cycle diff
	subs r0, #1 // subtract extra overhead outside the loop // unaccounted cycles come from mispredicted branch

	vpop {s16-s31}
	pop {r4-r12, pc}
