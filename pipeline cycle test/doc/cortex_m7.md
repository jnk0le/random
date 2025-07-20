# cortex m7

there are another independent cm7 pipeline analysis: 
- https://www.quinapalus.com/cm7cycles.html
- https://www.researchgate.net/publication/355865610_Exploring_Cortex-M_Microarchitectural_Side_Channel_Information_Leakage
- https://web.archive.org/web/20220710195033id_/https://eprint.iacr.org/2022/405.pdf (DP is not constant time)
- https://community.arm.com/support-forums/f/architectures-and-processors-forum/9930/cortex-m7-vfma-usage (VFMA canot dual issue with VLDR)
- https://community.arm.com/support-forums/f/architectures-and-processors-forum/5567/what-is-the-advantage-of-floating-point-of-cm7-versus-cm4 (VFMA canot dual issue with VLDR)
- https://web.archive.org/web/20240224123717/https://semiaccurate.com/2015/04/30/arm-goes-great-detail-m7-core/ (some hints about fpu weirdness)

(not analysis, but from ARM and matching observations)
- https://github.com/llvm/llvm-project/blob/main/llvm/lib/Target/ARM/ARMScheduleM7.td
- https://github.com/llvm/llvm-project/blob/main/llvm/lib/Target/ARM/ARMLatencyMutations.cpp#L637

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

## overall

there is no `nop` elimination, they are just not creating execution stalls as things listed below.\
It is recommended to align loop entries by enlarging instructions with `.w` suffix rather than using straight `nop` alignment (e.g. `.balign 8`)

`movw` + `movt` pair can dual issue with mutual dependency

"slippery condition" in my observations seems to be sliding execution by a half cycle without taking stall at branch point.
i.e 20 instructions of which one is a branch, execute in exactly 10 cycles with 2 cycles of loop invariant stalls
(as measured in CM7_pipetest_tmpl.S) Most likely it's dual issue across branch. See [branching/cmp](#branching/cmp)
It is observable as a swap of the young and old opcode slot.
It could also be a source of false-positives in micro benchmarking of assembly implementations.

The "linear execution" alignment seems to be necessary to avoid stalls.
Easiest way is to force all instructions to `.w` opcodes, otherwise it gets quite BS.
e.g. sometimes four `.n` instructions need to just be in close proximity to each other
and sometimes there is a hard wall (relative to absolute number of instructions from loop start, not the cache lines etc.)
that no `.n` instruction can't cross even though all 4 instructions are paired next to each other (across the wall).
Putting new compressed instruction depends on if the previous instructions were compressed, i.e. 4 `.n` instructions at 
beggining of the loop will make further compression easier (effect seems to carry forward and far, but not into the next loop iteration)

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

## bitfield and DSP instructions except multiplication (e.g. `uxtb`,`uxtab`,`ubfi`,`pkhbt`,`uadd8`,`qadd`,`clz`,`rev`)

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

## operand2

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

## multiplication, MAC

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

## loads

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

## stores

cannot dual issue two stores

can be dual issued with load

can store any result in same cycle, including `ldr`,`ldrh`,`ldrb`

unaligned stores execute without any stalls until store buffer clogs up (5 consecutive stores). When doing one
unaligned store every cycle, the throughput is 4 cycles per one store (unalignment amount or address advancement doesn't matter)

store buffer doesn't merge series of consecutive stores, so `strb` and `strh` will clog the store buffer with RMW operations. 
When doing one byte/half store every cycle, the throughput is 4 cycles per store if targeting one bank, 2 cycles per store 
if targeting even/odd bank and 2.4 cycles for unit stride advancing `strb`. (`strh` is still 2 cycles)

## load/store pair/multiple (incl pushpop)

cannot dual issue with preceeding or folowing instructions

takes `ceil(regnum/2)` cycles to execute

store multiple requires 8 byte memory alignment otherwise storing even number of registers will add 1 extra cycle to execution

require instruction stream compression by 4bytes (2x .n instr) before (any amount) or up to 7 cycles after, otherwise incurs additional stall

## branching/cmp

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

## predication

`it` instruction cannot be tripple issued.

predicated out instruction cause pipeline contention as if they were fully executed. 

predicated out multi-cycle instructions take the same number of cycles as if their results
were commited (i.e. predicated out `ldmia` still wastes all cycles as if it executed normally)

## fpu (single precision)

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

`vmov` from FPR to FPR (`vmov s0, s1`) has 3 cycles of result latency (to another vmov, FP ALU and `vfma.f` accumulate (!) dependency)
It has 3 cycles of input latency from fp ALU, 1 cycle from `vldr` and 5 cycles from `vfma.f`

## fpu (double precision)

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
