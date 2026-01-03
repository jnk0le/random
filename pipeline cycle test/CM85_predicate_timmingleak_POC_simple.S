/*!
 * \file CM85_predicate_timmingleak_POC_simple.S
 * \brief minimum POC of timming leak from IT predicated execution in cortex-m85
 *
 * function is self timming, returns cycle count
 *
 * \author Jan Oleksiewicz <jnk0le@hotmail.com>
 * \copyright SPDX-License-Identifier: CC0-1.0
 */

/*
	This has been submitted to ARM psirt and, after investigation, concluded as a non-vulnerability.
	The reason is that DIT is applied only if instruction passes "Condition code check"
	(i.e. their destination result is being commited). Instructions that have been
	predicated out, are allowed to cause different timming.

	See: Chapter B3.34 of the Armv8-M Architecture Reference Manual, section RDNPM.


	prerequisites, or steps to reproduce:

	- condition flags must be generated ahead of predication (2 cycles in EX2 or 3 cycles
	in EX3, to be able to forward from EX1) otherwise consumption of the offending result
	needs to wait until condition is resolved (resulting in same behaviour regardless of condition).

	- predicated instructions compute (or are expected to) offending result in EX2 or EX3 stage
	(POC_simple outputs in EX3)

	- following instructions are scheduled in a way that they expect availability of offending result
	in an earlier stage than available (POC_simple expects in EX1), so that execution ends in a stall{s}
	due to forwarding hazards (note that in a random compiled code, the 0.5 cycle stall can get hidden
	by e.g. some multi cycle insns or a "slippery condition").

	- the offending consumers in following cycle{s} must cause different amount of stalls, than if
	the offending predicated producers were unrelated (i.e. not producing the offending result)
	(POC_simple fills EX1-EX3 stages in first cycle there)

	- offending condition flag{s} can now be extracted through a typical timming channels.
	(like in classic cache timming attacks)


	The principle of the timming leak, is that the predicated out instructions are not "allocating"
	EX pipeline stages (i.e. execute as a `nop`), which can lead to a variable stall, in subsequent
	code, that depends on results from the predicated instructions.
	Offending dependency can be chained in EX2 or EX3 very far. It can reach another predication block
	which doesn't even need to be secret dependent, to cause O(2^n) analysis complexity.
	Dependency chain is broken once the offending result is available in EX1. (in both paths)


	I'm suspecting that the probability of the occurence in randomnly compiled code is rather low.
	First we need a predication happy compilers to begin with. Then the generated sequence must
	fulfill prerequisites and the leak cannot be hidden by e.g. multi cycle insns or a certain
	stall scenarios.

	Most importantly. If your crypto code can be hit by predication, it's more likely to receive a secret
	dependent branches, "because screw you" (see: https://web.archive.org/web/20251130134132/https://eprint.iacr.org/2025/435.pdf)

	In order to get architecturally guaranteed constant time predication, you need to
	use the new conditional/predicated instructions added in armv8.1-m arch (`csel`, `csinc` etc.)

	Cortex-m7 is unaffected even with multicycle instructions (i.e. predicated out `ldmia` still wastes
	all cycles) and "else" path depending on destination of predicated out instructions.
	
	Cortex-m33 is similarly affected as the "accumulator" stage used by some instructions, is
	available for exploitation or just blatant multi cycle instructions
	(like `muls r0, r1` or `adds.w r0, r1, #256` ...)
	
	ARM didn't tell me whether cortex-A/R (and which ones) are affected, but architecturally
	it's the same story. Their pipelines are also more complex than M85, which suggests
	the obvious. "Partially deprecated in T32" forms of IT predication should be the most offending.
	Not sure about A32 predication. A64 ones should be architecturally guaranteed for DIT.
	
	Update: 27.12.2025
	Leak doesn't manifest if you try to obtain it in the most obvious way. e.g.
	
	  itt ne
	  addne r0, r1, r0, ror #23 // EX1 EX2        // supposed to dual issue with insn below
	  addne r0, r1, r0, ror #23 // stall EX2 EX3
	
	Above snipped will always stall regardless of condition
	
	Predicated out `ldm`/`stm` will consume all cycles regardless of condition
	(not sure if memory transfers actually come out onto the bus, performance degradation
	when "accessing" illegal areas or using unaligned pointers)
	
	Hovewer, you can still achieve a leak within long `it` sequence:
	
	  itttt ne
	  addne r0, r1, r0, ror #23 // EX1 EX2
	  addne r0, r2              // EX3
	
	  // stalls only if condition is met
	  addne r0, r1, r0, ror #23 // stall EX2 EX3
	  addne r0, r2              // stall EX3
*/

.syntax unified
.thumb
.text

//uint32_t CM85_predicate_timmingleak_POC_simple(uint32_t secret);
.global CM85_predicate_timmingleak_POC_simple
.type   CM85_predicate_timmingleak_POC_simple,%function
CM85_predicate_timmingleak_POC_simple:
	push {r4-r12,lr} // push r12 to keep stack aligned for testing use
	vpush {s16-s31}

	// prepare registers here
	// r0 to r8, r12 and lr are available for use
	// r9 is a loop counter

	// set DIT (no effect on IT predication)
	ldr r2, =0xE000ED0C // AIRCR
	ldr r1, [r2]
	orr r1, r1, #(1<<4)
	str r1, [r2]

	ldr r11, =0xE0001000 // DWT_BASE
	cpsid i // disable interrupts

	.balign 8
	dsb
	isb

	movs.w r9, #1000 // also start with consistent condition flags
	nop.w

	ldr.w r10, [r11, #4] // read DWT.CYCCNT
	dls lr, r9 //nop.w

1:  //replace code below // don't use nops as those tripple issue
	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	movs r1, #0

	mov.n r10, r10
	mov.n r11, r11

	// r0 is our secret value, was passed as function argument

	// now generate condition flags based on the secret value
	// can compute in EX1 but flags will be available from EX2 anyway

	orrs.w r2, r1, r0, ror #1 // EX1 EX2 // flags in EX2
	mov.n r11, r11

	mov.n r10, r10 // nothing here
	mov.n r11, r11

	// now do the predication
	// if secret is zero then the r3 result is commited (left (eq) scenario)

	itt eq            // r2 is //       eq         |       ne
	addeq.w r3, r4, r5, lsl #1 // EX1 EX2          | ???? - NOP
	addeq.n r3, r6             // EX3              | ???? - NOP

	// r3 should be available in EX3 now
	// this pair expects r3 to be available in EX1

	add.w r4, r7, r3, lsl #1   // stall -> EX2 EX3 | EX1 EX2 (r3 consumed in EX1)
	add.n r4, r5               // stall -> EX3     | EX3

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
	uxtb r0, r0 // remove slippery condition

	uxtb r1, r1 // remove slippery condition
	mov.n r11, r11

	mov.n r10, r10
	mov.n r11, r11
	le lr, 1b //bne.n 1b

	//end
	mov.n r10, r10
	ldr.w r9, [r11, #4] // read DWT.CYCCNT

	cpsie i // reenable interrupts

	subs r0, r9, r10 // get cycle diff
	subs r0, #1 // subtract extra overhead outside the loop // unaccounted cycles come from mispredicted branch

	vpop {s16-s31}
	pop {r4-r12, pc}

/*
#include <stdio.h>
#include <stdint.h>

// replace with your device header, can't just use core_* headers
#include <R7FA8D1BH.h>

uint32_t CM85_predicate_timmingleak_POC_simple(uint32_t secret);

int main()
{
	// workaround for RA8 (r0p2)
	MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_FORCEWT_Msk; // errata 3175626
	ICB->ACTLR |= (1 << 16); // errata 3190818 // works without
	__DSB();
	__ISB();
	SCB->CCR |= SCB_CCR_IC_Msk | SCB_CCR_DC_Msk | SCB_CCR_LOB_Msk | SCB_CCR_BP_Msk;
	__DSB();
	__ISB();

	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

	printf("testing template loops 1000 times over 20 instructions pairs\n");
	printf("+1000 execution cycles means that a repeatable stall occured within loop\n");
	printf("\n\n");

	while(1)
	{
		printf("running POC_simple\n\n");

		uint32_t time = CM85_predicate_timmingleak_POC_simple(0);
		printf("secret is zero - cycles: %u\n", time);

		time = CM85_predicate_timmingleak_POC_simple(2137);
		printf("secret contains something - cycles: %u\n", time);

		printf("\n");

		// semihosting is slow enough
		// add your delay if needed
	}
}

// secret is zero - cycles: 22005
// secret contains something - cycles: 20001

*/
