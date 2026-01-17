/*!
 * \file CM7_pipetest_tmpl.S
 * \brief cortex-m7 dedicated template fore pipeline testing
 *
 * function is self timming, returns cycle count
 *
 * \author Jan Oleksiewicz <jnk0le@hotmail.com>
 * \copyright SPDX-License-Identifier: CC0-1.0
 */

.syntax unified
.thumb
.text

//uint32_t CM7_pipetest_tmpl(void);
.global CM7_pipetest_tmpl
.type   CM7_pipetest_tmpl,%function
CM7_pipetest_tmpl:
	push {r4-r12,lr} // push r12 to keep stack aligned for testing use
	vpush {s16-s31}

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
	subs.w r9, #1 // generate branch condition few cycles ahead of branch // 2 extra cycles to branch mispredict if placed next to branch

	nop.w
	nop.w

	uxtb.w r0, r0 // remove slippery condition
	nop.w

	nop.w
	bne.w 1b

	//end
	nop.w
	ldr.w r9, [r11, #4] // read DWT.CYCCNT
	
	cpsie i // reenable interrupts

	subs r0, r9, r10 // get cycle diff
	subs r0, #1 // subtract extra overhead outside the loop // unaccounted cycles come from mispredicted branch

	vpop {s16-s31}
	pop {r4-r12,pc}
