/*!
 * \file ch32v_pipetest_tmpl.S
 * \brief ch32v (riscv) dedicated template fore pipeline testing
 *
 * function is self timming, returns cycle count
 *
 * \author Jan Oleksiewicz <jnk0le@hotmail.com>
 * \license SPDX-License-Identifier: CC0-1.0
 */

// ilp32e abi, can run on ilp32i
// uses wch proprietary systick

.text

//uint32_t ch32v_pipetest_tmpl(void);
.global ch32v_pipetest_tmpl
.type   ch32v_pipetest_tmpl,%function
ch32v_pipetest_tmpl:
	addi sp, sp, -12
	sw s0, 0(sp)
	sw s1, 4(sp)
	sw ra, 8(sp)

	// prepare registers here
	// ra, s0, s1, a0-a4, t0 are available for use
	// a5 is a loop counter

	li a5, 1000

	li t2, 0xE000F000 // Systick base // +8

	.balign 8
	fence.i
	lw t1, 8(t2) // read systick->CNT (aka CNTR)
.option push

1: // replace code below
.option rvc
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

	addi a5, a5, -1
	bnez a5, 1b // 3 cycle taken at 0ws, 5 at 1ws

.option pop
	lw a0, 8(t2) // read systick->CNT (aka CNTR)
	sub a0, a0, t1 // get cycle diff
	addi a0, a0, (2-2) // adjust the baseline
	// -2 cycles from systick load 2 from taken branch at 0ws
	// for 1ws you need to comment out 2 nops and add 2 extra cycles from taken branch (2+2-2)

	lw s0, 0(sp)
	lw s1, 4(sp)
	lw ra, 8(sp)
	addi sp, sp, 12
	ret
