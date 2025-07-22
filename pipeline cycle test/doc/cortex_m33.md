# cortex-m33

uses `DWT.CYCCNT`, it must be initialized by application, otherwise will not work
```
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
```

tested on RP2350 (cm33 r1p0)

## overall



## "limited dual issue"

only two 16 bit (`.n`) instructions can dual issue (random comment on ycombinator is incorrect)

dual issue pair doesn't need to be aligned at word boundary (but the second op needs to be pre fetched)

`nop` can dual issue with arithmetic or load/store instruction (from any slot)

can't dual issue arithmetic+arithmetic or load/store + aritmetic (as described in M55 optimization guide)
Those also cannot dual issue with `nop` from "slot 1"






## load/store

load/store double execute in 2 cycles



## branching


