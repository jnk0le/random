/*!
 * \file CM3_pipetest_tmpl.S
 * \brief cortex-m3 dedicated template fore pipeline testing
 *
 * function is self timming, returns cycle count
 *
 * \author Jan Oleksiewicz <jnk0le@hotmail.com>
 * \copyright SPDX-License-Identifier: CC0-1.0
 */

.syntax unified
.thumb
.text

//uint32_t CM3_pipetest_tmpl(void);
.global CM3_pipetest_tmpl
.type   CM3_pipetest_tmpl,%function
CM3_pipetest_tmpl:
	push {r4-r12,lr} // push r12 to keep stack aligned for testing use

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

1:  //replace code below
	nop.w
	nop.w
	nop.w
	nop.w
	nop.w
	nop.w
	nop.w
	nop.w
	nop.w
	nop.w

	nop.w
	nop.w
	nop.w
	nop.w
	nop.w
	nop.w
	nop.w

	subs.w r9, #1
	bne.w 1b // 2 cycles taken // not taken will also take 2 cycles if there is less than 2 compressed instructions

	//end
	nop.w
	ldr.w r9, [r11, #4] // read DWT.CYCCNT

	cpsie i // reenable interrupts

	subs r0, r9, r10 // get cycle diff
	subs r0, #3 // subtract extra overhead outside the loop
	            // 2 cycles from nops, 1 cycle from CYCCNT ldr's

	pop {r4-r12,pc}
