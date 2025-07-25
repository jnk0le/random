# cortex-m33

uses `DWT.CYCCNT`, it must be initialized by application, otherwise will not work
```
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
```

tested on RP2350 (cm33 r1p0)

little glossary:

__operand2__ 
- shifted reg - `add.w r3, r4, r5, ror #24`
- shifted constant - `eor r0, r1, #0x1fc0`
- simple constant - 8bit and 12bit (in add/sub instruction) constants e.g. `add.w r0, r1, #1` and `add.w r0, r1, #0xff9`
(if 12 bit constant can be created by shifted 8 bits, compiler might use it instead. use `addw`/`subw` as a workaround to force encoding T4)
- constant pattern - pattern constructed from 8bit imm as `0x00XY00XY`,`0xXY00XY00` or `0xXYXYXYXY` e.g. `eor.w r6, r7, #0x1b1b1b1b`

## overall

result latency of operand2 shifted reg/constant, instructions is 2 cycles (can't be chained like multiply accumulates)

`{u,x}xta{b,h,b16}` have 2 cycle result latenct, accumulate dependency can be chained like in multiply accumulates

multiply accumulates have 2 cycle result latency, accumualte dependency can be chained from previous MAC,
`*xta*` or operand2 (shifted reg/constant), insns

`bfi` is a single stage instruction (like the basic ones)

## "limited dual issue" and branching

only two 16 bit (`.n`) instructions can dual issue (random comment on ycombinator is incorrect)

dual issue pair doesn't need to be aligned at word boundary (but the second op needs to be pre fetched)

`nop` can dual issue with arithmetic or load/store instruction

can't dual issue arithmetic+arithmetic or load/store + aritmetic (as described in M55 optimization guide)
Those also cannot dual issue with `nop` from "slot 1"

not taken branch, can dual issue (from "slot 1") even when preceded with flag generating instruction

```
	subs r7, #1 // 1 cycle (must be .n)
	bne 1b // 2 cycles taken, 0 not taken (must be .n)
	
	adds r3, r4 // 1 cycle (must be .n)
	beq 2f // 2 cycles taken, 0 not taken (must be .n)
```

taken branch, can dual issue (from "slot 1") when preceded by 16 bit instruction that doesn't generate
condition flags. It's effectively executing in 1 cycle.

```
	subs r7, #1 // 1 cycle
	add r0, r1 // 1 cycle (must be .n)
	bne 1b // 1 cycle taken, 0 not taken (must be .n)
```
 

## load/store

load to use latency is 2 cycles

load result can be consumed as MAC accumulate dependency in 1 cycle (`*xta*` or operand2
shifted insns, cannot consume like that)

load to store latency is 1 cycle

load/store double execute in 2 cycles



