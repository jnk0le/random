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

### inline shifted arith

inline shifted/rotated operand needs to be available one cycle earlier than for regular ALU



### loads

word loads can be consumed by ALU in next cycle

byte/half loads can be consumed in secend cycle (1 extra cycle)


### stores

cannot dual issue two stores

can store ALU result in same cycle

can store `ldr`,`ldrh`,`ldrb` result in same cycle


### load/store pair/multiple (incl pushpop)

cannot dual issue with preceeding or folowing instructions

takes `ceil(regnum/2)` cycles to execute


### branching

TBD

### fpu





