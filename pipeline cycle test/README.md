A small set of simple templates to do cycle accurate measurements of in-order pipelines.

For testing various instruction parallelim or dependencies.


Template files by default measure 20 cycles per loop x1000



## cortex m0

can't use `DWT.CYCCNT`

TBD

## cortex m3 and m4

TBD, needs cleanup

## cortex m7

uses `DWT.CYCCNT`

it is recommended to start with `*.w` opcodes as the short ones (even in fours) can cause half cycle slide (remove one nop before branch to confirm)

findings:

### overall

there is no `nop` elimination, they are just not creating execution stalls as things listed below

### bitfield and DSP instructions except multiplication (e.g. `uxtb`,`uxtab`,`ubfi`,`pkhbt`,`uadd8`,`qadd`,`clz`,`rev`)

cannot dual issue wrt each other

if first instruction was placed in younger or older "slot", all folowing bitfield/dsp instructions also need to be placed in the same 
"slot", distance inbetween occurences doesn't seem to matter

bitfield/dsp (e.g. `uxtb`) result cannot be used as index by load/store instructions in next cycle (1 extra cycle latency)

`rev` can't source result of another bitfield/dsp instruction from a previous cycle (`uxtb` or `uadd8` and regular ALU can)
(prob there is more such instructions)

sometimes slippery dependencies (1 or 2 cycles of loop invariant stalls)

### operand2

inline shifted/rotated (register) operand needs to be available one cycle earlier than for regular ALU


operand2 dual issuing matrix (except cmp - not tested yet)

| op1\op2    | simple constant | constant pattern | shifted constant | shifted reg | shift by constant | shift by register |
| --- | --- | --- | --- | --- | --- | --- |
| simple constant    |  +  |  +  |  +  |  +  |  +  |  +  |
| constant pattern   |  +  |  +  |  +  |  +  |  +  |  +  |
| shifted constant   |  +  |  +  |  -  |  -  |  +  |  +  |
| inline shifted reg |  +  |  +  |  -  |  -  |  +  |  +  |
| shift by constant  |  +  |  +  |  +  |  +  |  +  |  +  |
| shift by register  |  +  |  +  |  +  |  +  |  +  |  +  |

legend:

`+` - can dual issue

`-` - cannot dual issue

- simple constant - 8bit and 12bit (in add instruction) constants e.g. `add.w r0, r1, #1` and `add.w r0, r1, #0xff9` 
(if 12 bit constant can be created by shifted 8 bits, compiler might use it instead)
- constant pattern - pattern constructed from 8bit imm as `0x00XY00XY`,`0xXY00XY00` or `0xXYXYXYXY` e.g. `eor.w r6, r7, #0x1b1b1b1b`
- shifted constant - immediate constructed from shifted 8bit imm e.g. `eor r0, r1, #0x1fc`
- inline shifted reg - shift/rotate second reg operand e.g. `add.w r3, r4, r5, ror #24`
- shift by constant - simple shift reg by constant e.g. `lsr.w r2, r3, #12`
- shift by register - simple shift reg by register content e.g. `lsr.w r2, r3, r4`


### loads

word loads can be consumed by ALU in next cycle

byte/half loads can be consumed in secend cycle (1 extra cycle)


### stores

cannot dual issue two stores

can store any result in same cycle

can store `ldr`,`ldrh`,`ldrb` result in same cycle


### load/store pair/multiple (incl pushpop)

cannot dual issue with preceeding or folowing instructions

takes `ceil(regnum/2)` cycles to execute


### branching

TBD

### fpu





