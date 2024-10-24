A small set of simple templates to do cycle accurate measurements of in-order pipelines.

For testing various instruction parallelim or dependency hazards.


Template files by default measure 20 cycles per loop x1000



## cortex m0(+)

cortex m0+ not yet available

can't use `DWT.CYCCNT`, instead uses `SysTick`

requires maximum 24bit reload value for correct handling of arithmetic underflow:
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


### loads

[both] base address and offset must be available 1 cycle earlier than normally

[both] load pipelining happens only when targeting the same memory block by all loads (e.g. SRAM1 vs SRAM2 (and CCM) on f407)

[both] 32bit opcode loads (`ldr.w`, `ldrb.w`) may fail to pipeline correctly if they are not positioned
at word aligned boundaries

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

[both] stores can't be pipelined with following load multiple (but can with `stm`)

## cortex m7

there are another independent cm7 pipeline analysis: 
- https://www.quinapalus.com/cm7cycles.html
- https://www.researchgate.net/publication/355865610_Exploring_Cortex-M_Microarchitectural_Side_Channel_Information_Leakage
- https://web.archive.org/web/20220710195033id_/https://eprint.iacr.org/2022/405.pdf (DP is not constant time)
- https://community.arm.com/support-forums/f/architectures-and-processors-forum/9930/cortex-m7-vfma-usage (VFMA canot dual issue with VLDR)
- https://community.arm.com/support-forums/f/architectures-and-processors-forum/5567/what-is-the-advantage-of-floating-point-of-cm7-versus-cm4 (VFMA canot dual issue with VLDR)
- https://web.archive.org/web/20240224123717/https://semiaccurate.com/2015/04/30/arm-goes-great-detail-m7-core/ (some hints about fpu weirdness)

uses `DWT.CYCCNT`, it must be initialized by application, otherwise may not work
```
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->LAR = 0xC5ACCE55; // required on cm7
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
```

tested on stm32h743 rev Y

it is recommended to start with `*.w` opcodes as the short ones (even in fours) can cause half cycle slide (remove one nop before branch to confirm)

little glossary:
```
// for a given dual issued instruction bundle:
080004a0:   add.w r10, r11, r12 // this is "older op"
080004a4:   nop.w               // this is "younger op"
```

non obvious findings:

### overall

there is no `nop` elimination, they are just not creating execution stalls as things listed below.\
It is recommended to align loop entries by enlarging instructions with `.w` suffix rather than using straight `nop` alignment (e.g. `.balign 8`)

`movw` + `movt` pair can dual issue with mutual dependency

"slippery condition" in my observations seems to be sliding execution by a half cycle without taking stall at branch point.
i.e 20 instructions of which one is a branch, execute in exactly 10 cycles with 2 cycles of loop invariant stalls
(as measured in CM7_pipetest_tmpl.S) Most likely it's dual issue across branch. See [branching/cmp](#branching/cmp)
It is observable as a swap of the young and old opcode slot.
It could also be a source of false-positives in micro benchmarking of assembly implementations.

The "post loop entry" alignment seems to be necessary to avoid stalls. 
e.g. sometimes four `.n` instructions need to just be in close proximity to each other
and sometimes there is a hard wall (relative to absolute number of instructions from loop start, not the cache lines etc.)
that no `.n` instruction can't cross even though all 4 instructions are paired next to each other (across the wall).
Putting new compressed instruction depends on if the previous instructions were compressed, i.e. 4 `.n` instructions at 
beggining of the loop will make further compression easier (effect seems to carry forward and far, but not into the next iteration)

there is an early and late ALU (similarly to SweRV or Sifive E7) one cycle apart, if e.g. instruction X result cannot
be consumed by instruction Y in next cycle, it most likely means that if instruction X result is processed by regular ALU 
(e.g. `mov`) in following cycles, there still needs to be a 1 cycle gap in processing chain to dodge stall from Y

result of early alu can be forwarded to late alu in 0 cycles, or early alu next cycle:
- `add`, `sub`, reg-reg or simple and constructed constants (not shifted ones)
- `mov`, reg-reg or simple and constructed constants (not shifted ones) + simple shifts and rotations (aliased to mov) except rrx
- `rev`, `rev16`, `revsh`, `rbit` (result can't be used by AGU, another `rev*`,`rbit` instruction or as inline shifted operand next cycle)
- AGU with pre/post increments (result can't be used by `add`, `sub` and non shifting `mov` in early ALU next cycle)

AGU, `add`, `sub` and non shifting `mov` in earlu ALU is not clobbered by inline shifted operand of the younger op

e.g. following snippet doesn't stall:
```
	add.w r0, r2
	eor.w r6, r0, r6, ror #22

	ldr.w r4, [r14, r0]
	ldr.w r5, [r14, r1] // r1 is r0 + 4
```

`add`, `sub` and non shifting `mov` can enter early alu from older op only.

shifts and rotations (aliased to mov) can enter early alu from older or younger op provided that the 
other instruction is not inline shifted operand one or bitmanip (e.g. `rev`, `bfi`, `uxtab`) op using early alu shifter

AGUs can execute from either op, with no extra restrictions

flags generated in early ALU cannot be forwarded to late ALU in 0 cycles (slippery)

### bitfield and DSP instructions except multiplication (e.g. `uxtb`,`uxtab`,`ubfi`,`pkhbt`,`uadd8`,`qadd`,`clz`,`rev`)

cannot dual issue wrt each other

the bitfield/dsp instructions must be placed in older op (slippery otherwise)

bitfield/dsp instruction (e.g. `uxtb`,`uxtab`,`rev`) result cannot be used as index or address by load/store instructions in 
next cycle (1 extra cycle latency)

`sbfx`, `ubfx`, `rbit`,`rev`, `rev16`, `revsh`, execute in early alu and can't source from late ALU of previous 
cycle (source must be available a cycle earlier than normally)

in `bfi`, `pkh{bt,tb}` and `{s,u}xta{b,h,b16}` instructions the "extracted" or shifted (even when no shift in case of `pkhbt`) 
register consume its operand in early ALU (source must be available a cycle earlier than normally)

`pkhtb` also the first operand (Rn) needs to be available for early ALU (source must be available a cycle earlier than normally)

`bfi`, `bfc`, `sbfx`, `ubfx`, `rbit`, `rev`, `rev16`, `revsh`, `{s,u}xta{b,h,b16}`, `pkh{bt,tb}` instructions can't be dual 
issued with operand2 inline_shifted_reg/shifted_constant instruction 

### operand2

inline shifted/rotated (register) operand is consumed in early ALU (source must be available a cycle earlier than normally)

operand2 dual issuing matrix

| older\younger op | simple constant | constant pattern | shifted constant | inline shifted reg | shift by constant | shift by register |
| --- | --- | --- | --- | --- | --- | --- |
| simple constant    |  +  |  +  |  +  |  +  |  +  |  +  |
| constant pattern   |  +  |  +  |  +  |  +  |  +  |  +  |
| shifted constant   |  +  |  +  |  -  |  -  |  +  |  +  |
| inline shifted reg |  +  |  +  |  -  |  -  |  +  |  +  |
| shift by constant  |  +  |  +  | -(?)| -(?)|  +  |  +  |
| shift by register  |  +  |  +  | -(?)| -(?)|  +  |  +  |

legend:

`+` - can dual issue

`-` - cannot dual issue

`?` - slippery (2 cycles of loop invariant stalls, no matter of repetition)

- simple constant - 8bit and 12bit (in add/sub instruction) constants e.g. `add.w r0, r1, #1` and `add.w r0, r1, #0xff9`
(if 12 bit constant can be created by shifted 8 bits, compiler might use it instead. use `addw`/`subw` as a workaround to force encoding T4)
- constant pattern - pattern constructed from 8bit imm as `0x00XY00XY`,`0xXY00XY00` or `0xXYXYXYXY` e.g. `eor.w r6, r7, #0x1b1b1b1b`
- shifted constant - immediate constructed from shifted 8bit imm e.g. `eor r0, r1, #0x1fc0`
- inline shifted reg - shift/rotate second reg operand e.g. `add.w r3, r4, r5, ror #24`
- shift by constant - simple shift/rotate reg by constant e.g. `lsr.w r2, r3, #12`
- shift by register - simple shift/rotate reg by register content e.g. `ror.w r2, r3, r4`

### multiplication, MAC

can't dual issue wrt each other (slippery)

can't dual issue with stores (slippery)
Theoretically long MAC (4R due to separately adressed RdLo and RdHi) combined with reg offset store (3R) can exceed 
total number of RF read ports (6), but regular `mul` with imm offset store behaves exactly the same.

`smlaxy`,`smlalxy`,`smulxy` (where xy={bt,tb,tt,bb,wb,wt,d,dx}) instructions can be dual issued with dsp/bitmanip and operand2 
inline_shifted_reg/shifted_constant instructions as well as execute in any opcode slot, even though few sources 
(e.g. gcc cortex-m7.md file) suggest otherwise

all multiply accumulates can be chained on accumulator dependency each cycle

all multiplications and multiply accumulates have 1 additional cycle of result latency

all multiplications and multiply accumulates can't consume `ldr` result from previous cycle
(everything else works normally as late alu consumption)

accumulate dependency is consumed cycle later in another stage after the late alu
(so it can consume `ldrb` result from previous cycle, or late ALU from the same cycle)

### loads

word loads can be consumed by late ALU in next cycle, except instructions that consume one of the operands in early ALU,
(e.g. inline shifted operand2 shift or `uxtab`), even though this result is consumed in late ALU
(byte and half loads are not affected, probably related to how load bus-fault trapping is implemented)

byte/half loads can be consumed in secend cycle by late ALU (1 extra cycle of result latency), or third in early ALU

two loads (targeting dtcm or cache or both) can be dual issued if each one is targeting different bank (even and odd words)

can't do simultaneous access to dtcm and dcache at the same (even or odd) bank (slippery if the load 
pairs are targeting different bank than previous ones (reg offset or immediate doesn't matter))

pre indexed and post indexed loads can be dual issued if there is no bank conflict and resulting address is not reused 
in same cycle (can be issued back to back every cycle)

`pld` instruction can stall due to address dependency (same as normal loads)

unaligned loads are severely underperforming. +3 cycles as older op, +4 as younger op or when both older and younger ops 
are unaligned loads (banks or unaligment amount doesn't matter), -1 if the other load is aligned and targets the same bank 
that is accessed first by the unaligned one (part at lower address)

### stores

cannot dual issue two stores

can be dual issued with load

can store any result in same cycle, including `ldr`,`ldrh`,`ldrb`

unaligned stores execute without any stalls until store buffer clogs up (5 consecutive stores). When doing one
unaligned store every cycle, the throughput is 4 cycles per one store (unalignment amount or address advancement doesn't matter)

store buffer doesn't merge series of consecutive stores, so `strb` and `strh` will clog the store buffer with RMW operations. 
When doing one byte/half store every cycle, the throughput is 4 cycles per store if targeting one bank, 2 cycles per store 
if targeting even/odd bank and 2.4 cycles for unit stride advancing `strb`. (`strh` is still 2 cycles)

### load/store pair/multiple (incl pushpop)

cannot dual issue with preceeding or folowing instructions

takes `ceil(regnum/2)` cycles to execute

store multiple requires 8 byte memory alignment otherwise storing even number of registers will add 1 extra cycle to execution

require instruction stream compression by 4bytes (2x .n instr) before (any amount) or up to 7 cycles after, otherwise incurs additional stall

### branching/cmp

flag setting seems to happen in late alu even when instruction executes early

cmp instruction executes similarly to regular (operand2) ALU instructions (not possible to check if it goes through early alu)

there is flag forwarding that can reduce branch misprediction penalty from 8 to 6 cycles (in case of subs + bne), 
instruction generating flags have to be placed at least 3 cycles before branch. Effect is not gradual 
meaning that setting flags within 4-5 instruction slots prior to branch will cause a worst case penalty. 

not-taken branch can dual issue with following instruction

(the slippery condition seems to not be related to branch mispredictions (sums with misprediction cycles))

Predicted (any direction and length) taken branch is capable of dual issuing with one instruction at destination address.
It is highly sensitive to alignment and compression of surrounding instructions, requires at 
least 2 `.n` instructions executed prior to branch. (this is most likely the cause of slippery condition)

`it` instruction (over one instruction) behaves similar to predicted
not-taken branch (including scenarios of 0.67 cycles of averaged penalty)

### fpu (single precision)

not affected by denormals

arithmetic instructions cannot be dual issued (e.g `vadd.f` + `vadd.f` or `vmul.f` + `vadd.f`)

multiply accumulate instructions cannot be dual issued with vldr, vstr or any vmov, with the 
exception of first mac instructin

after executing multiply accumulate instructions `vadd.f`, `vldm`, `vstm`, `vldr.d`, `vstr.d`, `vmov sd, #fimm` instructions
cannot be executed for 3 next cycles. 

after executing multiply accumulate instructions `vldr.f`,`vstr.f`,`vmov`instructions
cannot be executed from younger opcode slot for 3 next cycles.

(independent) `vfma.f` cannot be pipelined within next cycle after `vmla.f` (due to use of additional stage
for mid result rounding in `vmla.f`)

multiply accumulates can be pipelined every cycle but at least 3 independent accumulations must be
interleaved

| result latency | `vfma.f` | `vmla.f` | `vadd.f` | `vmul.f` |
| --- | --- | --- | --- | --- |
| as `vadd.f` operand     | x   | x   | 3   | 3 |
| as `vmul.f` operand     | 5   | 6   | 3   | 3 |
| as fma/mla accumulate   | 3/2 | x/3 | 3/2 | 3/2 |
| as fma/mla multiplicand | 5/5 | x/6 | 3   | 3 |
| as `vstr` operand       | 3   | 4   | 1   | 1 |


cannot dual issue two `vmov sd, #fimm` instructions

moving two registers (fpu to integer and integer to fpu) cannot be dual issued with anything

`vdiv.f`/`vsqrt.f` execute in 16/14 cycles, dual issues with one integer instruction and
locks pipeline for the rest of the duration (doesn't retire "out of order")

`vldm` behaves as regular `ldm`. (`vstm` similarly)
The last loaded registers have up to 3 cycles of latency as `vfma.f` accumulate (!) operand and 1 cycle to anything else

`vldr.f` can be dual issed.

`vldr.f` has 1 cycle of result latency, except as the `vfma.f` accumulate (!) dependency where it is 3 cycles
(2 for `vmla.f`)

`vstr.d`, `vldr.d` can't dual issue with anything

`vmov` to GPR (`vmov r0, s0`) has 1 cycle of result latency into late ALU and 0 cycles to MAC accumulate dependency.
It has 3 cycles of input latency from fp ALU, 2 cycles from `vldr` and 5 cycles from `vfma.f`
(not summing with another `vmov` output latency)

`vmov` from GPR to FPR (`vmov s0, r0`) has 3 cycles of result latency (to another vmov, FP ALU and `vfma.f` accumulate (!) dependency)
It has 1 cycle of input latency from any ALU (early/late/MAC)

`vmov` from FPR to FPR (`vmov s0, s1`) has 3 cycles of result result latency (to another vmov, FP ALU and `vfma.f` accumulate (!) dependency)
It has 3 cycles of input latency from fp ALU, 1 cycle from `vldr` and 5 cycles from `vfma.f`

### fpu (double precision)

all arithmetic instructions can dual issue with integer instructions (except FMA which can't dual issue with int loads/stores)
from older opcode slot (sometimes it is capable from younger slot but that's very inconsistent)

Instructin CPI table (no dependency, all registers initialized to 1.0, "different exponent" to 0.25):

| instruction execution cycles per scenario | same exponent | different exponents | one denormal | one zero | notes |
| --- | --- | --- | --- | --- | --- |
| `vadd.d` | 1 | 2 | 3 | 3 | |
| `vmul.d` | 5 | 5 | 5 | 4 | |
| `vdiv.d` | 30 | 30 | 30 | 2 | |
| `vfma.d` (first/each additional chained) | 7/6 | 8/7 | 10/9 | 6/5 | offending number as multiplicand, accumulator restored every round |
| `vmla.d` (first/each additional chained) | 8/5 | 9/7 | 10/8 | 7/5 | offending number as multiplicand, accumulator restored every round |
| `vfma.d` (first/each additional chained) | 8/7 | 8/7 | 10/9 | 6/5 | offending number as multiplicand, different exponent accumulator (31.0) |
| `vfma.d` (first/each additional chained) | 10/9 | 10/9 | 7/5 | 6/5 | denormal as accumulator, restored every round |

first `vmul.d` chained after `vfma.d`/`vmla.d` executes in 4 cycles

after executing multiply accumulate instructions, `vadd.d` cannot be executed in first following cycle

`vfma.d` seems to require interleaving 4 independent FMA accumulations or have 3 cycle result latency (not extended by
multi cycle floating point instructions)

result latency of `vmul.d`, `vadd.d` is 3 cycles or interleaved with at least 2 fp instructions (not extended by
multi cycle floating point instructions)

## cortex-m85

There is an official optimization manual for cortex-m85: https://developer.arm.com/documentation/107950/0100

(the "latency from the xxx source operand" means latency when output of given isntruction is forwarded to xxx operand
of the same following instruction)

I'll note only things that are not montioned, wrong or unclear in the official manual.

uses `DWT.CYCCNT`, it must be initialized by application, otherwise will not work
```
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
```

tested on RA8D1 (cm85 r0p2)

little glossary:
```
// for a given dual issued instruction bundle:
080004a0:   add.w r10, r11, r12 // this is "older op/issue slot"
080004a4:   nop.w               // this is "younger op/issue slot"
```

### overall/scalar ALU

`nop` instructions can be tripple issued even as `.w` opcode with 2 other (e.g ALU) instructions provided that there is
sufficient fetch bandwidth (e.g. 2x `.n` ALU instructions and one `nop.w` used for padding)

The operand2 (reg-reg) instructions have 1 extra cycle of input latency to the second register operand,
even when the second operand is not shifted (e.g. `add r0, r1, r2`)

`bfi` instruction cannot be dual issued with other `bfi` or "slot 0" instructions (unlike suggested by optimization manual)

Most of the ALU instructions can be executed in 3 total (symmetric) pipeline stages, EX1, EX2, EX3.

- basic ALU instructions (e.g. `add`, `mov`, operand2 constant) can execute in all 3 stages
- bitwise instructions (e.g. `eors`, `and`) can execute in EX2 and EX3
- shift instructions (e.g. `lsrs`, `ubfx`) can execute in EX1 and EX2
- reg-reg operand2 (e.g. `add.w`, `and.w`) instructions execute throughout 2 stages (SHIFT + ALU) EX1+EX2 or EX2+EX3.\
Non shifted input operand (for ALU stage) is consumed in the stage, where it's used (no false dependecy by shifter stage) 
- "slot 0" instructions (e.g. `uxtb`, `uadd8`) instruction can execute only in EX2 (from any issue slot)
- `bfi` instruction executes throughout EX1 and EX2 (SHIFT + ALU), destination operand is consumed in
EX2 stage (no false dependecy by shifter stage) 

It is possible to forward dependent operands in 0 cycles into a later stages.

```
	adds r0, r1 // EX1
	adds r0, r1 // EX2

	adds r0, r1 // EX2
	adds r0, r1 // EX3

	adds r0, r1 // EX3
	mov.n r11, r11 // can't forward more
```


### "slippery condition"

Observable as half cycle execution slide, which results in 1 cycle of loop invariant stall
(if no further stall is hit) due to tripple issue across branch.\
Obsrvation of a skewed pipeline is a false positive caused by this.

- issuing instruction pairs that can't be dual issued (`uxtb`+`uxtb`, `ldrd`+`ldrd` etc.)
- certain



### scalar multiplication, MAC

mul/MAC instructions execute throughout EX2 and EX3 satges (MUL + ACC)

even the simple `mul` instructions occupy accumulate stage and have same result latency

can't forward `ldr` result into accumulate operand in 0 cycles (theoretically: AGU in EX1, DATA and MUL in EX2, ACC in EX3),
all other combinations (from EX2 in 0 cycles or EX3 in 1 cycle) work as expected

can't dual issue 4 operand MAC (with 64bit accumulator, e.g. `umlal`,`umaal`) with reg offset store or `strd`.
(scalar regfile has only 6 read ports)

can't dual issue 4 operand MAC (with 64bit accumulator, e.g. `umlal`,`umaal`) with post and pre indexed `ldrd`
(scalar regfile has only 4 write ports)

### scalar load/store

loads execute throughout EX1 and EX2 (AGU, and DATA)

Two post/pre indexed loads/stores can be chained back to back each cycle.

DCACHE has only 2 SRAM banks while DTCM 4 (all are 4 byte striped)

Effective load to use latency for `ldr` is 0 cycle into the EX3 stage.

Optimization manual suggests 2 cycle load to use, which is the case of "pointer chasing".

```
	mov.n r10, r10
	ldr r0, [r5]

	mov.n r10, r10
	mov.n r11, r11

	ldr r1, [r5, r0]
	mov.n r11, r11
```

`ldr{b,h,sb,sh}` executes throughout 3 pipeline stages (AGU, DATA, FORMAT)
Realistic load to use latency is 1 cycle into EX3.

unaligned loads add 0.5 - 1 cycle of stall each. 


`ldrd`/`strd` can be dual issued together
- infinitely if targetting DTCM and the transfers are distributed across all 4 banks (regardless of overlaps)
- with up to 6 (4 when not 8 byte aligned) `strd` when targetting DCACHE. After filling, the store buffer must be fully
drained (no DCACHE loads in subsequent cycles) to not hit (up to 5 cycle) stall.

`ldrd`/`strd` are not affected by 4 byte misalignment.

store instruction consume its operand in EX3 stage, so the effective input latency is 0 cycles from EX1 and EX2


### branching

predicted taken branch can tripple issue with 2 prior instructions or 1 prior and 1 at destination address, provided that there is enough
fetch bandwidth (at least 4 (when close, or doesn't tripple across branch) or 8 (when far and tripples across branch) `.n` 
instructions prior to branch (including branch opcode))\
It is observable as one pair taking 0.5 cycle to execute

branch mispredict penalty for case of all `.w` instructions, is 7 to 11 cycles.
The penalty is gradual depending on distance from branch and is sensitive to older/younger op placement.

when compressed instructions are involved, misprediction penalty ranges from 8 to 15 cycles
(1 of which can come from unaligned fetch group after the loop), it's no longer gradual\
5-6 cycle penalty is observed only when branch fails to tripple issue with target due to operand dependency

overall, flag settings need to happen at least 2-4 cycles ahead of branch.

In a nested loop scenario, the inner branch shows 4 cycles of mispredict penalty. There is also 
around 30 accumulated loop invariant cycles of penealty. (which includes outer loop mispredict penalty)

`it` instruction tripple issues provided that there is enough fetch bandwidth.\
`it` instruction perform actual predication (no branching, "wastes" execution slots instead)\
Predicated out instruction cause less stalls due to operand contention, than when allowed to retire. (stalls are still being measured)\
`it` intruction can onsume flags in 0 cycles, however it is recommended to keep 1-2 cycle clearance as it cannot
immediately consume flags from EX4 ALU or shifts in younger op previous cycle etc.

### HW loop (`WLS`/`LE`)

`le` instruction behaves like regular branch instruction with 4-5 cycles of "misprediction penalty".
Meaning that it is executed every round.\
Net gain is however positive due to one less instruction (e.g. `cmp`) in inner loops.

In a nested loop scenario, `le` instruction doesn't show any "misprediction penalty". There is also
around 30 accumulated loop invariant cycles of penealty. (which includes outer loop mispredict penalty)

### scalar floating point

cannot dual issue two fp arithmetic/move instructions (`vmov`, `vadd` etc.)

can dual issue fp arithmetic/move instructions with fp loads/stores only. (except `vldr.64`)

cannot dual issue double precision arithmetic even with integer instructions (`vadd.f64`, double moves can)

### MVE overall

("scalar" means integer instructions, e.g. `add`, `uxtb`)

### vector loads/stores

scatter/gather doesn't support unaligned access

basic vector load/store instructions can dual issue with scalar, only from younger slot,
gather/scatter can't dual issue with scalar at all

scalar instructions can't be issued a cycle after vector loads/stores (during its second beat)

```
	mov.n r10, r10 // can't dual issue with scatter/gather
	vldrw.u32 q0, [r12]

	veor q1, q2, q3 
	//mov.n r11, r11 // can't issue any scalar here
```


scatter/gather cannot overlap (not pipelined) into the following cycle (any insn), resulting in e.g. 1 extra
stall cycle in word/doubleword scatter/gather.

scatter/gather cannot overlap with prior vector loads/stores (e.g. `vldrw.32 q0, [r12, #0]` into `vstrw.32 q1, [r11, q3]`)

non widening byte/half gather/scatter stalls pipeline for additional transfers (2 per cycle) 

byte scatter (`vstrb.8`) has 47 cycles of stall overhead at 16 byte stride (11 at 4 byte stride and 7 at 1 byte stride)

pseudorandom byte permutation within at least 16byte (32 for half) area or 0 stride scatter, has the same timmings as unit stride
(unlike e.g. 16 stride)

vector load to scatter/gather indices latency is 4 cycles

vector ALU (bitwise) to scatter/gather indices latency is 3 cycles

vector store to vector load latency is 2 cycles (if not gathering from store "tick 1" into load "tick0")

```
	mov.n r10, r10
	vstrw.u32 q0, [r12]

	veor q1, q2, q3 // no scalar

	mov.n r10, r10
	vldrw.u32 q7, [r12]

	veor q1, q2, q3 // no scalar
```

### other optimization tips


some instructions have issuing limitations so you may want to replace them with other equivalents:


| <img width=200/> offending instruction | <img width=200/> more efficient equivalent | notes |
|----------------------------|----------------------------------------------|------------------------|
| `uxtb{.n} r0, r1`          | `ubfx r0, r1, #0, #8`<br />`and r0, r1, #0xff` | no `.n` equivalent, `ubfx` can execute in EX1, `and` can execute in EX3 |
| `uxtb r0, r1, ror #8`      | `ubfx r0, r1, #8, #8` | |
| `uxtb r0, r1, ror #16`     | `ubfx r0, r1, #16, #8` | |
| `uxtb r0, r1, ror #24`     | `lsrs{.n} r0, r1, #24`<br />`ubfx r0, r1, #24, #8` | |
| `ldm`(`pop`)<br />`stm`(`push`)| sequence of `ldrd`<br />sequence of `strd` | load/store double can dual issue with other instructions per 64bits of transferred data |
| `vldm`(`vpop`)<br />`vstm`(`vpush`) | sequence of `vldr.64` (scalar)<br />sequence of `vstr.64` (scalar) | can dual issue with other scalar instructions (fixed point)<br /> you can use vector loads/stores but those impose limitations on issuing scalar instructions (use only in vector dominated prologues/epilogues) |


## ch32v003

Uses proprietary WCH systick.

Requires HCLK/1 and full 32bit reload value for proper handling of arithmetic underflow:
```
	SysTick->CMP = 0xffffffff;
	SysTick->CTLR = 0b1101; // of course there are no bit definitions in headers
```

findings:

unaligned long instructions seem to sometimes have 1 cycle penalty (mostly loop invariant)

loads stores are 2 cycle

unaligned (word/half) loads/stores trigger an unaligned load/store exception.
(there is no `mtval` csr register)

taken branch/jump is 
- at 0ws
- - 3 cycles
- - +1 cycle when jumping to unaligned 32bit instruction
- at 1ws:
- - 4 cycles from an older op (in synthetic scenario)
- - 5 cycles from an younger op (in synthetic scenario)
- - +1 cycle when jumping to unaligned location 
- - prior execution of 2 cycle instructions might cause a swap of the older/younger op timmings, 
addition of long instruction might further affect it (ie. return to "normal")

to set `MIE` in `mstatus` it must be written together with `MPIE`. ie. write 0x88 to enable interrupts

bits [9:2] of `mtvec` are hardwired to zero (minimum 1KiB granularity).
Entry in unvectored mode also needs to be 1KiB aligned.

The core will reset when experiencing fault while in exception handler. 
(`ebreak` without debugger also causes this)

uncore findings:

FLASH is organized in 32 bit (4byte) lines

2 waitstate option in `FLASH->ACTLR` (aka `FLASH->ACR`), which is defined in headers but stated as 
invalid in RM, seems to enable 3 or 4 waitstates. The corresponding 3ws cofig (not in headers) is also 
doing same 3 or 4 waitstates.

1920 byte area at 0x1ffff000 can be reprogrammed with custom bootloaders.
It's expected to be entered by performing system reset with `MODE` bit set in `FLASH_STATR` register.
This will cause remap to 0x00000000.

### encodings of custom instructions (aka "xw" extension)

Of course there is no wch documentation of those instructions

Using wavedrom.

#### xw.c.lbu

```
{reg:[
 { bits: 2, name: 0x0, attr: ['xw.c.lbu'] },
 { bits: 3, name: 'rd\'' },
 { bits: 2, name: `uimm[2:1]` },
 { bits: 3, name: 'rs1\'' },
 { bits: 3, name: `uimm[0|4:3]` },
 { bits: 3, name: 0x1, attr: ['xw.c.lbu'] },
]}
```

#### xw.c.lhu

```
{reg:[
 { bits: 2, name: 0x2, attr: ['xw.c.lhu'] },
 { bits: 3, name: 'rd\'' },
 { bits: 2, name: `uimm[2:1]` },
 { bits: 3, name: 'rs1\'' },
 { bits: 3, name: `uimm[5:3]` },
 { bits: 3, name: 0x1, attr: ['xw.c.lhu'] },
]}
```

#### xw.c.sb

```
{reg:[
 { bits: 2, name: 0x0, attr: ['xw.c.sb'] },
 { bits: 3, name: 'rd\'' },
 { bits: 2, name: `uimm[2:1]` },
 { bits: 3, name: 'rs1\'' },
 { bits: 3, name: `uimm[0|4:3]` },
 { bits: 3, name: 0x5, attr: ['xw.c.sb'] },
]}
```

#### xw.c.sh

```
{reg:[
 { bits: 2, name: 0x2, attr: ['xw.c.sh'] },
 { bits: 3, name: 'rd\'' },
 { bits: 2, name: `uimm[2:1]` },
 { bits: 3, name: 'rs1\'' },
 { bits: 3, name: `uimm[5:3]` },
 { bits: 3, name: 0x5, attr: ['xw.c.sh'] },
]}
```