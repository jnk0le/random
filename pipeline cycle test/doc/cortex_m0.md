# cortex m0(+)

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

`add r12, r12` can perform left shift by one in upper registers

[CM0??, stm32f0??] pc-rel loads, from waitstated (prefetched, typically flash) memory, are cycle faster if executed
as first instruction in aligned pair. Can be repeated. 
Additionally second to first is also possible at similar penalty. (probably strongly dependent on implemented prefetcher)
```
.balign 4
	nop
	ldr r0, =0x11223344
	ldr r1, =0x22334455
	nop
```
