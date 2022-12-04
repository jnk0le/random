A small set of simple templates to do cycle accurate measurements of in-order pipelines.

For testing various instruction parallelim or dependency hazards.


Template files by default measure 20 cycles per loop x1000



## cortex m0(+)

cortex m0+ not yet available

can't use `DWT.CYCCNT`, instead uses `SysTick`

requires maximum 24bit reload value for correct handling of underflow:
```
SysTick->LOAD = 0x00ffffff; // max value
SysTick->CTRL = SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_CLKSOURCE_Msk; // enable SysTick with cpu clock
```

findings:

there is no load to use latency as stated in some sources

tips:

using rev16 allows to extract middle bytes in one less cycle at the cost of higher reg pressure if source register needs to be preserved

```
	// 4 instr
	lsrs r1, r4, #8
	uxtb r1, r1
	lsrs r2, r4, #16
	uxtb r2, r2

	// 3 instr
	rev16 r2, r4
	uxtb r1, r2
	lsrs r2, r2, #24
```

`rev16` + `rev` sequence is equivalent to rotate by 16 bits (more flexible and lower reg pressure than `movs`+`rors`)

`sbcs r0,r0` can turn carry flag into predication mask


## cortex m3 and m4

uses `DWT.CYCCNT`, it must be initialized by application, otherwise randomly doesn't work
```
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
```

cortex m3 as tested on stm32f103 rev X

cortex m4 as tested on stm32f407 rev Y

### overall

TBD

### loads

[both] base address and offset must be available 1 cycle earlier than normally

[both] load pipelining happens only when targeting the same memory block by all loads (e.g. SRAM1 vs SRAM2 (and CCM) on f407)

[both] 32bit opcode loads (`ldr.w`, `ldrb.w`) may fail to pipeline correctly if they are not positioned
at word aligned boundaries

[both] 16 bit opcode load after aligned 32bit one could also cause a stall (couldn't reproduce as minimal scenario) 

[both] load instructions can eat following `nop` (`cmp`, `tst` don't work) effectively executing in 1 cycle

[cm3] post and pre indexed loads cannot be chained back to back at all

[cm4] post and pre indexed loads cannot be chained back to back on same register base address (alternating 2 different
 registers works)

[cm3] only first load in a chain can be pre or post indexed mode

### stores

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

## cortex m7

there are another independent cm7 pipeline analysis: 
- https://www.quinapalus.com/cm7cycles.html
- https://www.researchgate.net/publication/355865610_Exploring_Cortex-M_Microarchitectural_Side_Channel_Information_Leakage

uses `DWT.CYCCNT`, it must be initialized by application, otherwise may not work
```
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->LAR = 0xC5ACCE55; // required on cm7
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
```

tested on stm32h743 rev Y

it is recommended to start with `*.w` opcodes as the short ones (even in fours) can cause half cycle slide (remove one nop before branch to confirm)

non obvious findings:

### overall

there is no `nop` elimination, they are just not creating execution stalls as things listed below

`movw` + `movt` pair can dual issue with mutual dependency

there is an early and late ALU (similarly to SweRV or Sifive E7) one cycle apart, if e.g. instruction X result cannot
be consumed by instruction Y in next cycle, it most likely means that if instruction X result is processed by regular ALU 
(e.g. `mov`) in following cycles, there still needs to be a 1 cycle gap in processing chain to dodge stall from Y

result of early alu can be forwarded to late alu in 0 cycles, or early alu next cycle:
- `add`, `sub` with simple and constructed constants (not shifted ones)
- `mov` with simple and constructed constants (not shifted ones) + simple shifts and rotations (aliased to mov) except rrx
- `rev`, `rev16`, `revsh`, `rbit` (result can't be used by AGU, another `rev*`,`rbit` instruction or as inline shifted operand next cycle)

`add`, `sub` and non shifting `mov` in earlu ALU is not clobbered by inline shifted operand of the older op

e.g. following snippet doesn't stall:
```
	add.w r0, r2
	eor.w r6, r0, r6, ror #22

	ldr.w r4, [r14, r0]
	ldr.w r5, [r14, r1] // r1 is r0 + 4
```

`add`, `sub` and non shifting `mov` can enter early alu from younger op only.

shifts and rotations (aliased to mov) can enter early alu from younger or older op provided that the 
other instruction is not inline shifted operand one or bitmanip (e.g. `rev`, `bfi`, `uxtab`) op using early alu shifter

the only case when both instructions can be forwarded from early alu (to early alu next cycle) is `add`, `sub` or non shifting 
`mov` in younger slot and simple shift or rotation in older slot

### bitfield and DSP instructions except multiplication (e.g. `uxtb`,`uxtab`,`ubfi`,`pkhbt`,`uadd8`,`qadd`,`clz`,`rev`)

cannot dual issue wrt each other

if first instruction was placed in "younger" or "older" slot, all folowing bitfield/dsp instructions also need to be placed
in the same slot, distance inbetween occurences doesn't seem to matter

if the bitfield/dsp instructions are placed in older slot there will be a slippery condition with 2 cycles of loop 
invariant stalls no matter of the amount or dispersion of bitfield/dsp instructions. Due to this there may be extra 
stalls when combined with other stall sources (listed below in this chapter), if result is consumed by alu in (expectable) younger slot next 
cycle then slippery condition turns into regular stall (further rules will assume younger op placement)

bitfield/dsp instruction (e.g. `uxtb`,`uxtab`,`rev`) result cannot be used as index or address by load/store instructions in 
next cycle (1 extra cycle latency)

`sbfx`, `ubfx`, `rbit`,`rev`, `rev16`, `revsh`, instructions can't source result of another 
bitfield/dsp or operand2 inline_shifted_reg/shifted_constant instruction from a previous cycle 
(`uxtb` or `uadd8` and regular ALU can)

in `bfi`, `pkh{bt,tb}` and `{s,u}xta{b,h,b16}` instructions the "extracted" or shifted (even when no shift in case of `pkhbt`) register 
can't be sourced from previous cycle of another bitfield/dsp or operand2 inline_shifted_reg/shifted_constant instruction, 

`pkhtb` also can't source result of another bitfield/dsp or operand2 inline_shifted_reg/shifted_constant instruction from a previous 
cycle by the first operand (Rn)

`bfi`, `bfc`, `sbfx`, `ubfx`, `rbit`, `rev`, `rev16`, `revsh`, `{s,u}xta{b,h,b16}`, `pkh{bt,tb}` instructions can't be dual 
issued with operand2 inline_shifted_reg/shifted_constant instruction 

some cases (incl load to use) might be younger/older op sensitive for bitfield/dsp instructions (TBD)

### operand2

inline shifted/rotated (register) operand needs to be available one cycle earlier than for regular ALU 
(can be sourced from younger op of simple alu, when sourcing from older op of simple alu stalls as a younger, sliperry 
as an older)

operand2 dual issuing matrix (except cmp - not tested yet)

| younger\older op | simple constant | constant pattern | shifted constant | inline shifted reg | shift by constant | shift by register |
| --- | --- | --- | --- | --- | --- | --- |
| simple constant    |  +  |  +  |  +  |  +  |  +  |  +  |
| constant pattern   |  +  |  +  |  +  |  +  |  +  |  +  |
| shifted constant   |  +  |  +  |  -  |  -  |  +  |  +  |
| inline shifted reg |  +  |  +  |  -  |  -  |  +  |  +  |
| shift by constant  |  +  |  +  |  ?  |  ?  |  +  |  +  |
| shift by register  |  +  |  +  |  ?  |  ?  |  +  |  +  |

legend:

`+` - can dual issue

`-` - cannot dual issue

`?` - slippery (2 cycles of loop invariant stalls, no matter of repetition)

- simple constant - 8bit and 12bit (in add/sub instruction) constants e.g. `add.w r0, r1, #1` and `add.w r0, r1, #0xff9`
(if 12 bit constant can be created by shifted 8 bits, compiler might use it instead. use `addw`/`subw` as a workarund to force encoding T4)
- constant pattern - pattern constructed from 8bit imm as `0x00XY00XY`,`0xXY00XY00` or `0xXYXYXYXY` e.g. `eor.w r6, r7, #0x1b1b1b1b`
- shifted constant - immediate constructed from shifted 8bit imm e.g. `eor r0, r1, #0x1fc`
- inline shifted reg - shift/rotate second reg operand e.g. `add.w r3, r4, r5, ror #24`
- shift by constant - simple shift/rotate reg by constant e.g. `lsr.w r2, r3, #12`
- shift by register - simple shift/rotate reg by register content e.g. `ror.w r2, r3, r4`

### multiplication, MAC

can't dual issue wrt each other, when preceding instruction pair cantains any multiply/MAC instruction (slippery otherwise)

can't dual issue with stores when preceding instruction pair contains any multiply/MAC instruction. (slippery otherwise)
Theoretically long MAC (4R due to separately adresses RdLo and RdHi) combined with reg offset store (3R) can exceed 
total number of RF read ports (6), but regular `mul` with imm offset store behaves exactly the same.

`smlaxy`,`smlalxy`,`smulxy` (where xy={bt,tb,tt,bb,wb,wt,d,dx}) instructions can be dual issued with dsp/bitmanip and operand2 
inline_shifted_reg/shifted_constant instructions as well as execute in any opcode slot, even though few sources 
(e.g. gcc cortex-m7.md file) suggest otherwise

### loads

word loads can be consumed by late ALU in next cycle, except instructions that consume one of the operands in early ALU,
even when ldr result is consumed as a late ALU operand (in e.g. inline shifted op2 or uxtab)

byte/half loads can be consumed in secend cycle in late ALU (1 extra cycle of result latency), or third in early ALU

two loads (targeting dtcm or cache or both) can be dual issued if each pair is targeting different bank (even and odd words)

simultaneous access to dtcm and dcache at the same (even or odd) bank is slippery (2 cycles of loop invariant stalls) 
if the load pairs are targeting different bank than previous ones (reg offset or immediate doesn't matter)

pre indexed and post indexed loads can be dual issued if there is no bank conflict and resulting address is not reused 
in same cycle (can be issued back to back every cycle)

`pld` instruction can stall due to address dependency (same as normal loads)

unaligned loads are severely underperforming. +3 cycles as younger op, +4 as older op or when both younger and older ops 
are unaligned loads (banks or unaligment amount doesn't matter), -1 if the other load is aligned and targets the same bank 
that is accessed first by the unaligned one (part at lower address)

address dependency might be younger/older op sensitive (TBD)

### stores

cannot dual issue two stores

can be dual issued with load

can store any result in same cycle, including `ldr`,`ldrh`,`ldrb`

unaligned stores execute without any stalls until store buffer clogs up (5 consecutive stores). When doing one
unaligned store every cycle, the throughput is 4 cycles per one store (unalignment amount or address advancement doesn't matter)

store buffer doesn't merge series of consecutive stores, so `strb` and `strh` will clog the store buffer with RMW operations. 
When doing one byte/half store every cycle, the throughput is 4 cycles per store if targeting one bank, 2 cycles per store 
if targeting even/odd bank and 2.4 cycles for unit stride advancing `strb`. (`strh` is still 2 cycles)

address dependency might be younger/older op sensitive (TBD)

### load/store pair/multiple (incl pushpop)

cannot dual issue with preceeding or folowing instructions

takes `ceil(regnum/2)` cycles to execute


### branching/cmp

[] 

there is flag forwarding that can reduce branch misprediction penalty from 8 to 6 cycles (in case of subs + bne), 
instruction generating flags have to be placed at least 3 cycles before branch (there may be a window like in cm3 TBD)

the "slippery condition" seems to not be related to branching (sums with extra cycles of branch misprediction
when flags are not forwarded)

TBD

### fpu

TBD


## RI5CY

TBD
