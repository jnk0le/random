# cortex-m33

uses `DWT.CYCCNT`, it must be initialized by application, otherwise will not work
```
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
```

tested on RP2350 (cm33 r1p0)

## overall

result latency of operand2 shifted reg, instructions is 2 cycles (can't be chained like multiply accumulates)

## "limited dual issue" and branching

only two 16 bit (`.n`) instructions can dual issue (random comment on ycombinator is incorrect)

dual issue pair doesn't need to be aligned at word boundary (but the second op needs to be pre fetched)

`nop` can dual issue with arithmetic or load/store instruction

can't dual issue arithmetic+arithmetic or load/store + aritmetic (as described in M55 optimization guide)
Those also cannot dual issue with `nop` from "slot 1"

not taken branch, can dual issue (from "slot 1") even when preceded with flag generating instruction

```
	subs r7, #1 // 1 cycle (must be .n)
	bne 1b // 2 cycles taken, 0 not taken
	
	adds r3, r4 // 1 cycle (must be .n)
	beq 2f // 2 cycles taken, 0 not taken
```

taken branch, can dual issue (from "slot 1") when preceded by 16 bit instruction that doesn't generate
condition flags. It's effectively executing in 1 cycle.

```
	subs r7, #1 // 1 cycle
	add r0, r1 // 1 cycle (must be .n)
	bne 1b // 1 cycles taken, 0 not taken
```
 

## load/store

load to use latency is 2 cycles

load/store double execute in 2 cycles



