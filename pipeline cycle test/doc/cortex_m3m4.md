# cortex m3 and m4

other sources:

- https://developer.arm.com/documentation/100166/0001/ric1417175924567
- https://developer.arm.com/documentation/100166/0001/Floating-Point-Unit/FPU-functional-description/FPU-instruction-set-table?lang=en
- https://www.cse.scu.edu/~dlewis/book3/docs/Cortex-M4%20Instruction%20Timing.pdf
- https://eprint.iacr.org/2025/523.pdf ("multi-cycle instruction followed by three unaligned 32-bit instructions", not just consecutive unaligned loads)

uses `DWT.CYCCNT`, it must be initialized by application, otherwise randomly doesn't work
```
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
```

cortex m3 as tested on stm32f103 rev X

cortex m4 as tested on stm32f407 rev Y

## overall


## loads

[both] base address and offset must be available 1 cycle earlier than normally

[both] load pipelining happens only when targeting the same memory block by all loads (e.g. SRAM1 vs SRAM2 (and CCM) on f407)

[both] 32bit opcode loads (`ldr.w`, `ldrb.w`) may fail to pipeline correctly if they are not positioned
at word aligned boundaries

[both] load instructions can eat following `nop` (`cmp`, `tst` don't work) effectively executing in 1 cycle

[cm3] post and pre indexed loads cannot be chained back to back at all

[cm4] post and pre indexed loads cannot be chained back to back on same register base address (alternating 2 different
 registers works)

[cm3] only first load in a chain can be pre or post indexed mode

## stores

[both] base address and offset must be available 1 cycle earlier than normally

[both?] stores can pipeline with preceding load (incl data forwarding) only when targeting the same 
memory block (e.g. SRAM1 vs SRAM2 (and CCM) on f407)

[both] immediate offset store (+ pre and post indexed on cm4) effectively execute in 1 cycle

[cm3] pre and post indexed stores effectively execute in 1 cycle when preceeding instruction is not an load

[both] immediate offset (+ pre and post indexed on cm4) store placed after load, removes the stall, 
making effective execution of 1 IPC, no need to forward data from preceding load

[both] post and pre indexed stores cannot be chained back to back on same register base address (alternating 2 different
 registers works)

[both] reg offset store execute in 2 cycles. it can eat following nop (reg + imm or `mov` instructions 
don't work) effectively executing in 1 cycle (even though cm4 has 4 read ports for MAC)

[both] stores cannot be pipelined with following load instructions (adds extra cycle to store)

[both] stores can't be pipelined with following load multiple (but can with `stm`)
