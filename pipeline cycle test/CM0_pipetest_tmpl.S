/*!
 * \file CM0_pipetest_tmpl.S
 * \brief cortex-m0 dedicated template fore pipeline testing
 *
 * function is self timming, returns cycle count
 *
 * \author Jan Oleksiewicz <jnk0le@hotmail.com>
 * \copyright SPDX-License-Identifier: CC0-1.0
 */

.syntax unified
.thumb
.text

//uint32_t CM0_pipetest_tmpl(void);
.global CM0_pipetest_tmpl
.type   CM0_pipetest_tmpl,%function
CM0_pipetest_tmpl:
	mov r0, r8
	mov r1, r9
	mov r2, r10
	mov r3, r11

	push {r0-r7,lr}

	// prepare registers here
	// r0 to r6, r10-r12 and lr are available for use
	// r7 is a loop counter

	ldr r7, =1000

	ldr r0, =0xE000E010 // SysTick Base
	cpsid i // disable interrupts

	.balign 4
	dsb
	isb

	mov r8, r0 // save systick base address
	ldr r2, [r0, #8] // get systick current value

	mov r9, r2 // save starttime
	nop // loop alignment

	
1:  //replace code below
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	subs r7, #1
	bne 1b // 3 cycles taken

	//end
	mov r0, r8 // get systick base address
	ldr r1, [r0, #8] // get systick current value

	cpsie i // reenable interrupts
	mov r2, r9 // get starttime
	cmp r2, r1
	bhs 2f // handle underflow case
	movs r4, #0xff // make 2s complement number to subtract
	lsls r4, #24
	orrs r1, r4
2:
	subs r3, r2, r1 // SysTick is downcounter so subtract current from startime
	subs r3, #3+2-2 // subtract extra overhead outside the loop
	                // 3 cycles from extra instructions, 2 from tick ldr's, -2 from last branch
	mov r12, r3 // save result

	pop {r0-r7} // can't pop into lr

	mov r8, r0
	mov r9, r1
	mov r10, r2
	mov r11, r3

	mov r0, r12

	pop {pc}
