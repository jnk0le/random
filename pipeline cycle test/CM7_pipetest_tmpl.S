.syntax unified
.thumb
.text

//uint32_t CM7_pipetest_tmpl(void);
.global CM7_pipetest_tmpl
.type   CM7_pipetest_tmpl,%function
CM7_tst_tmpl:
	push {r4-r12,lr} // push r12 to keep stack even for testing use

	// prepare registers here
	// r0 to r8, r12 and lr are available for use
	// r9 is a loop counter

	ldr r11, =0xE0001000 // DWT_BASE

	.align 3
	dsb
	isb

	movs.w r9, #1000 // also start with consistent condition flags
	nop.w

	ldr.w r10, [r11, #4] // read DWT.CYCCNT
	nop.w

	//replace code below
1:
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
	nop.w

	nop.w
	nop.w

	nop.w
	nop.w

	nop.w
	nop.w

	nop.w
	bne.w 1b // branch can only be the old op

	//end
	nop.w
	ldr.w r9, [r11, #4] // read DWT.CYCCNT

	subs r0, r9, r10 // get cycle diff
	subs r0, #1 // subtract extra overhead outside the loop // extra 6 cycles comes from mispredicted branch

	pop {r4-r12,pc}
