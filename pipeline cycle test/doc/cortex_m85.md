# cortex-m85

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

## overall/scalar ALU

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


## "slippery condition"

Observable as half cycle execution slide, which results in 1 cycle of loop invariant stall
(if no further stall is hit) due to tripple issue across branch.\
Obsrvation of a skewed pipeline is a false positive caused by this.

can be caused by:
- issuing instruction pairs that can't be dual issued (`uxtb`+`uxtb`, `ldrd`+`ldrd` etc.)
- certain prohibited forwardings (e.g. EX2 to EX2 or EX3 to (nonexistent) EX4) and some rare exceptions

## scalar multiplication, MAC

mul/MAC instructions execute throughout EX2 and EX3 satges (MUL + ACC)

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


## branching

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

## HW loop (`WLS`/`LE`)

`le` instruction behaves like regular branch instruction with 4-5 cycles of "misprediction penalty".
Meaning that it is executed every round.\
Net gain is however positive due to one less instruction (e.g. `cmp`) in inner loops.

In a nested loop scenario, `le` instruction doesn't show any "misprediction penalty". There is also
around 30 accumulated loop invariant cycles of penealty. (which includes outer loop mispredict penalty)

## scalar floating point

cannot dual issue two fp arithmetic/move instructions (`vmov`, `vadd` etc.)

can dual issue fp arithmetic/move instructions with fp loads/stores only. (except `vldr.64`)

cannot dual issue double precision arithmetic even with integer instructions (`vadd.f64`, double moves can)

## MVE overall

("scalar" means integer instructions, e.g. `add`, `uxtb`)

## vector loads/stores

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

## other optimization tips


some instructions have issuing limitations so you may want to replace them with other equivalents:


| <img width=200/> offending instruction | <img width=200/> more efficient equivalent | notes |
|----------------------------|----------------------------------------------|------------------------|
| `uxtb{.n} r0, r1`          | `ubfx r0, r1, #0, #8`<br />`and r0, r1, #0xff` | no `.n` equivalent, `ubfx` can execute in EX1, `and` can execute in EX3 |
| `uxtb r0, r1, ror #8`      | `ubfx r0, r1, #8, #8` | |
| `uxtb r0, r1, ror #16`     | `ubfx r0, r1, #16, #8` | |
| `uxtb r0, r1, ror #24`     | `lsrs{.n} r0, r1, #24`<br />`ubfx r0, r1, #24, #8` | |
| `ldm`(`pop`)<br />`stm`(`push`)| sequence of `ldrd`<br />sequence of `strd` | load/store double can dual issue with other instructions per 64bits of transferred data |
| `vldm`(`vpop`)<br />`vstm`(`vpush`) | sequence of `vldr.64` (scalar)<br />sequence of `vstr.64` (scalar) | can dual issue with other scalar instructions (fixed point)<br /> you can use vector loads/stores but those impose limitations on issuing scalar instructions (use only in vector dominated prologues/epilogues) |
