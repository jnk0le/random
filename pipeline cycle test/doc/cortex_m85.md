# cortex-m85

There is an official optimization manual for cortex-m85: https://developer.arm.com/documentation/107950/0100

(the "latency from the xxx source operand" means latency when output of given isntruction is forwarded to xxx operand
of the same following instruction)

I'll note only things that are not mentioned, wrong or unclear in the official manual.

additional resources:
- https://github.com/llvm/llvm-project/blob/main/llvm/lib/Target/ARM/ARMScheduleM85.td (slightly incorrect about shifts)

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

## overall/scalar ALU

`nop` instructions can be tripple issued even as `.w` opcode with 2 other (e.g ALU) instructions provided that there is
sufficient fetch bandwidth (e.g. 2x `.n` ALU instructions and one `nop.w` used for padding)

The operand2 (reg-reg) instructions have 1 extra cycle of input latency to the second register operand,
even when the second operand is not shifted (e.g. `eor.w r0, r1, r2`)

`bfi` instruction cannot be dual issued with other `bfi` or "slot 0" instructions (unlike suggested by optimization manual)

`rev` instructions execute and bypass as every other shift instructions, llvm is incorrect about it.

Most of the ALU instructions can be executed in 3 total (symmetric) pipeline stages, EX1, EX2, EX3.

- basic ALU instructions (e.g. `add`, `mov`, operand2 constant) can execute in all 3 stages
- bitwise instructions (e.g. `eors`, `and`) can execute in EX2 and EX3
- shift instructions (e.g. `lsrs`, `ubfx`, `rev`) can execute in EX1 and EX2 (there are two shifters per stage)
- reg-reg operand2 (e.g. `add.w`, `and.w`) instructions execute throughout 2 stages (SHIFT + ALU) EX1+EX2 or EX2+EX3.\
Non shifted input operand (for ALU stage) is consumed in the stage, where it's used (no false dependecy by shifter stage) 
- "slot 0" instructions (e.g. `uxtb`, `uadd8`) instruction can execute only in EX2 (from any issue slot)
- `bfi` instruction executes throughout EX1 and EX2 (SHIFT + ALU), destination operand is consumed in
EX2 stage (no false dependecy by shifter stage)
- {u,s}xta{b,h,b16} instruction executes throughout EX1 and EX2 (SHIFT + ALU), destination and addend operand is consumed in
EX2 stage, there is false dependency hazard when attempting to forward from older slot (in EX1) same cycle


It is possible to forward dependent operands in 0 cycles into a later stages.

```
	adds r0, r1 // EX1
	adds r0, r1 // EX2

	adds r0, r1 // EX2
	adds r0, r1 // EX3

	adds r0, r1 // EX3
	mov.n r11, r11 // can't forward more
```


## "slippery condition"

Observable as half cycle execution slide, which results in 1 cycle of loop invariant stall
(if no further stall is hit) due to tripple issue across branch.\
Observation of a skewed pipeline is a false positive caused by this.

can be caused by:
- issuing instruction pairs that can't be dual issued (`uxtb`+`uxtb`, `ldrd`+`ldrd` etc.)
- certain prohibited forwardings (e.g. EX2 to EX2 or EX3 to (nonexistent) EX4) and some rare exceptions

there needs to be no stall after slide for slippery condition to manifest

## scalar multiplication, MAC

mul/MAC instructions execute throughout EX2 and EX3 stages (MUL + ACC)

even the simple `mul` instructions occupy accumulate stage and have same result latency

can't forward `ldr` result into accumulate operand in 0 cycles (theoretically: AGU in EX1, DATA and MUL in EX2, ACC in EX3),
all other combinations (from EX2 in 0 cycles or EX3 in 1 cycle) work as expected

can't dual issue 4 operand MAC (with 64bit accumulator, e.g. `umlal`,`umaal`) with reg offset store or `strd`.
(scalar regfile has only 6 read ports)

can't dual issue 4 operand MAC (with 64bit accumulator, e.g. `umlal`,`umaal`) with post and pre indexed `ldrd`
(scalar regfile has only 4 write ports)

## scalar load/store

loads execute throughout EX1 and EX2 (AGU, and DATA)

Two post/pre indexed loads/stores can be chained back to back each cycle.

DCACHE has only 2 SRAM banks while DTCM 4 (all are 4 byte striped)

Effective load to use latency for `ldr` is 0 cycle into the EX3 stage.

Optimization manual suggests 2 cycle load to use, which is the case of "pointer chasing".

```
	mov.n r10, r10
	ldr r0, [r5] // EX1 EX2

	mov.n r10, r10 // r0 available in EX2
	mov.n r11, r11

	ldr r1, [r5, r0] // r0 available in EX1
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

store instruction consume its data operand in EX3 stage, so the effective input latency is 0 cycles from EX1 and EX2


## branching

branch predictor is even less deterministic than the one in cortex-m7, randomly adding extra cycles penalty
in the simplest loops that are executed repeatedly (happens usually when tripple issuing across branch)

`isb`/`dsb` instructions have no effect on observable "branch mispredicion" times

flag setting for branching seems to take effect in EX2 and EX3 stages

flag setting has to happen at least 1 cycle (from EX1 and EX2) or 2 cycle (from EX3) ahead of branch,
otherwise you get +5 cycle to misprediction penalty

predicted taken branch can tripple issue with 2 prior instructions or 1 prior and 1 at destination address, provided that there is enough
fetch bandwidth (at least 4 (when close, or doesn't tripple across branch) or 8 (when far and tripples across branch) `.n` 
instructions prior to branch (including branch opcode))\
It is observable as one pair taking 0.5 cycle to execute

simple loop scenario as in provided template shows total 9-15 cycles of "mispredict penalty" (7-12 cycles when not tripple issuing
across branch, upper end is reached by lacking code compression (1 cycle))

anomalies suggest that the total loop invariant cost ("misprediction penalty") of a loop (as in provided template) 
consists of one branch misprediction (at the end) and a setup cost. Part of the setup cost is inability to tripple
issue in first iteration which costs 2 or 3 cycles (scenario as in provided template).

In a nested loop scenario, the inner branch shows 3-4 cycles (5-6 when flags are generated last cycle EX2, 6-7 EX3) of "mispredict penalty.
(1 cycle difference comes from inability to tripple issue in first iteration)\
There is also around 30 accumulated loop invariant cycles of penealty. (which includes outer loop mispredict penalty)

## HW loop (`WLS`/`LE`)

`le` instruction behaves like regular branch instruction with 4-5 cycles of "misprediction penalty".
(cycle difference comes from inability to tripple issue in first iteration)\

In a nested loop scenario, `le` instruction doesn't show any "misprediction penalty". There is also
around 30 accumulated loop invariant cycles of penealty. (which includes outer loop penalty)

## predicated execution

`it` instruction tripple issues provided that there is enough fetch bandwidth.

`it` instruction perform actual predication (no branching, "wastes" execution slots instead)

result of predicated instruction cannot be forwarded to anything in 0 cycles

similarly to branching, flags are effective from EX2 and EX3 only, but cannot forward flags from EX3 in same cycle

availability of results from predicated instruction to unpredicated ones depends on the availability of condition flags.\
The result is available in stage EXn a cycle later than (implied) result of flag setting instructions would be available in EXn stage\
Generating flags 2 (in EX2) or 3 (in EX3) cycles ahead of predication allows forwarding from EX1.\
It doesn't matter if instruction was or wasn't predicated out.

```
	add.w r6, r0, r0 // EX1 EX2
	adds r6, #1 // EX3 // generate flags

	mov.n r10, r10
	mov.n r11, r11 // can generate here from EX2

	mov.n r10, r10
	mov.n r11, r11

	mov.n r10, r10
	it ne
	addne.n r0, r1 // EX1

	add.w r3, r1, r0 // EX1 EX2
	add.n r3, r4 // EX3
```

```
	add.w r6, r0, r0 // EX1 EX2
	adds r6, #1 // EX3

	mov.n r10, r10 // can generate here from EX2
	it ne
	addne.w r0, r1 // EX1 // it can produce even in ex3 in this case

	mov.n r10, r10
	add.n r2, r0 // EX3 // not available in EX1 due to predication

	add.w r3, r2, r0 // r0 in EX2, r2 in EX3
	mov.n r11, r11
```

predicated out instructions might cause less stalls (by following instructions) due to pipeline stage contention, than when allowed to retire.


## scalar floating point

cannot dual issue two fp arithmetic/move instructions (`vmov`, `vadd` etc.)

can dual issue fp arithmetic/move instructions with fp loads/stores only. (except `vldr.64`)

cannot dual issue double precision arithmetic even with integer instructions (e.g. `vadd.f64`, double moves can)

mixing with vector instructions is not recommended as the scheduling gets highly non obvious and weird
(it's most tolerable to vloating point moves, but still weird, scatter/gather indices are extra sensitive)

## MVE

("scalar" means integer instructions, e.g. `add`, `uxtb`)

### overall

vector instructions can't be dual issed with "slot 0" scalar instructions

### moves

"proper" moving of 2 "vector lanes" to scalar (`vmov r0, r1, q0[2], q0[0]`) has 3 cycle latency into EX3 stage
(4 into EX2, 5 into EX1) both scalar outputs have same latency, prior vector computations are not influencing latency

vloating point move of double to two scalar (`vmov.64 r0,r1, d0`) has 1 cycle latency into EX3 (2 into EX2, 3 into EX1)
- can move from destination of immediately preceeding vector instruction
- can't be overlpped with preceeding vector load/store (unlike "proper" moves)
- can't use odd `d` registers if overlapping with previous vector insn
- theoretically should be a B group but overlaps with both A and B instructions (unlike "proper" move)
- there is additional 1 cycle of latency if both output registers are to be used by both instructions in following pair
(not applied if both registers are used by one instruction) If the stall happens due to the violation of
that extra latency, then this rule resets and applies to the following cycle. (e.g. 6 consecutive violating 
pairs give 6 cycles of penalty)

```
	mov.n r10, r10
	veor q0, q1, q2 // can't be vector load in this cycle

	vmov.64 r0,r1, d0 // can't use odd d reg
	mov.n r11, r11
	
	add.n r2, r0 // EX3
	mov.n r11, r11
```

```
	mov.n r10, r10
	veor q0, q1, q2 // can't be vector load in this cycle

	vmov.64 r0,r1, d0 // can't use odd d reg
	mov.n r11, r11

	vmov.64 r2,r3, d1
	add.n r0, r1 // EX3

	add.n r2, r3 // EX3
	mov.n r11, r11
```

"proper" moving of 2 scalar regs to 2 "vector lanes" (`vmov q0[2], q0[0], r0, r1`) consume both inputs in EX3 stage

vloating point move of two scalars into double (`vmov.64 d0, r0,r1`) consume its operands in EX3 stage
- can't be overlpped with preceeding vector load/store (unlike "proper" moves)
- can't use odd `d` registers if overlapping with previous vector insn
- overlaps like A/B group instructions (similarly to "proper" equivalent)
- (there might be some anomalies still)

```
	mov.n r10, r10
	veor q1, q2, q3

	add.w r1, r2, r3 // EX1 EX2
	vmov.64 d0, r0,r1 // can't use odd d reg

	vmov.64 d1, r2,r3
	mov.n r11, r11

	veor q0, q4, q0
	mov.n r11, r11
```

can't forward `ldr` result into vector or vloating point moves in 0 cycles. (theoretically: AGU in EX1, DATA in EX2, consume in EX3)

### vector predication

### vector loads/stores

basic vector load/store instructions can dual issue with scalar, only from younger slot

scalar instructions can't be issued a cycle after vector loads/stores (during its second cycle aka "beat")

```
	mov.n r10, r10
	vldrw.u32 q0, [r12]

	veor q1, q2, q3 
	//mov.n r11, r11 // can't issue any scalar here
```

vector store to vector load latency is 2 cycles (if not gathering from store "tick 1" into load "tick0")

```
	mov.n r10, r10
	vstrw.u32 q0, [r12]

	veor q1, q2, q3 // no scalar

	mov.n r10, r10
	vldrw.u32 q7, [r12]

	veor q1, q2, q3 // no scalar
```

deinterleaving loads (`vld40.32`, `vld41.8` etc.) behave like regular loads but have 4 cycle result latency (all loaded registers have same latency)

### vector scatter/gather

scatter/gather doesn't support unaligned access

can't dual issue with scalar instructions at all

can't overlap (not pipelined) into the following cycle (any insn), resulting in e.g. 1 extra
stall cycle in word/doubleword scatter/gather.

cannot overlap with prior vector loads/stores (e.g. `vldrw.32 q0, [r12, #0]` into `vstrw.32 q1, [r11, q3]`)

non widening byte/half gather/scatter stalls pipeline for additional transfers (2 per cycle) 

byte scatter (`vstrb.8`) has total of 47 cycles of stall overhead at 16 byte stride (11 at 4 byte stride and 7 at 1 byte stride)

pseudorandom byte permutation within e.g. 16byte (32 for half) area or 0 stride scatter, has the same timmings as unit stride
(unlike e.g. 16 stride)

vector load to scatter/gather indices latency is 4 cycles

vector ALU (e.g. bitwise) to scatter/gather indices latency is 3 cycles

## other optimization tips

some instructions have issuing limitations so you may want to replace them with other equivalents:


| <img width=290/> offending instruction | <img width=290/> more efficient equivalent | notes |
|----------------------------|----------------------------------------------|------------------------|
| `uxtb{.n} r0, r1`          | `ubfx r0, r1, #0, #8`<br />`and r0, r1, #0xff` | no `.n` equivalent, `ubfx` can execute in EX1, `and` can execute in EX3 |
| `uxtb{.n} r0, r1`          | `sbfx r0, r1, #0, #8` | no `.n` equivalent |
| `{s,u}xtb r0, r1, ror #{8,16}` | `{s,u}bfx r0, r1, #{8,16}, #8` | |
| `{s,u}xtb r0, r1, ror #24`     | `{a,l}srs{.n} r0, r1, #24`<br />`{s,u}bfx r0, r1, #24, #8` | |
| `{s,u}xtab r0, r1, r2, ror #24` | `add r0, r1, r2, {a,l}sr #24` | |
| `{s,u}xtah r0, r1, r2, ror #16` | `add r0, r1, r2, {a,l}sr #16` | |
| `ldm`(`pop`)<br />`stm`(`push`)| sequence of `ldrd`<br />sequence of `strd` | load/store double can dual issue with other instructions per 64bits of transferred data |
| `vldm`(`vpop`)<br />`vstm`(`vpush`) | sequence of `vldr.64` (scalar)<br />sequence of `vstr.64` (scalar) | can easily dual issue with other scalar instructions (fixed point)<br /> you can use vector loads/stores but those impose limitations on issuing scalar instructions (use only in vector dominated prologues/epilogues) |
