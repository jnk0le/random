This is just example of low-level encoder driver (no support for multiple encoders "per-lib" yet).
Code can be extracted and reused to create multiple instances working simultaneously.
This "library" is intended to work especially with high speed rotary encoders (the ones used in motor applications), and requires already debounced signals on the inputs.

## Setup
- define all used input pins in header file
	* channel A input pin have to be used as an interrupt trigger source
- define used counting mode and interrupt vector name in header file
- modify encoder_init() function with init sequence corresponding to used interrupt vector and AVR reg definitions.
- define other switches if needed

### Notes
- Currently only X1 and X2 counting modes are implemented (switchable by ENCODER_USE_X2_MODE macro).
	* x1 -- one edge on one channel
	* x2 -- both edges on one channel
	* x4 -- both edges on both channels
- The biggest drawback of the X1 mode is that, the counter will "run away" if the encoder is held (therefore bouncing) at the trigger point. 
- ISR overhead can be further reduced by using globally reserved lower registers or GPIOR's for the counter. 
- All defined inputs have to be mappable in lower IO address space.

## ISR timmings

- Inputs have to be sampled within 1/4 of the step period (total execution time can be longer).
- All timmings are given for no jitter (completing execution of the ongoing instruction), same edge to interrupt delay as edge to IO register (which may be different) and 4 cycle isr entry/exit.

| accumulator  | mode | total cycles (worst case) | cycles to sample | size in bytes | maximum peak rate at 16MHz | 
| --- | --- | --- | --- | --- | --- |
| 16 bit | X1 | 35 | 16 | 40 | 250k steps/s |
| 16 bit | X2 | 37/39 | 18/20 | 50 | 432/400k half steps/s |
| 32 bit | X1 | 54 | 24 | 78 | 166k steps/s |
| 32 bit | X2 | 57 | 27 | 88 | 280k half steps/s |

If ENCODER_OPTIMIZE_MORE is defined:

| accumulator  | mode | total cycles (worst case) | cycles to sample | size in bytes | maximum peak rate at 16MHz | 
| --- | --- | --- | --- | --- | --- |
| 16 bit | X1 | 31 | 5 | 72 | 516k steps/s |
| 16 bit | X2 | 34 | 7 | 82 | 470k half steps/s |
| 32 bit | X1 | 41 | 5 | 112 | 390k steps/s |
| 32 bit | X2 | 44 | 7 | 122 | 363k half steps/s |

### LGT8XM timmings

- Inputs have to be sampled within 1/4 of the step period (total execution time can be longer).
- All timmings are given for no jitter (completing execution of the ongoing instruction) and same edge to interrupt delay as edge to IO register (which may be different).

| accumulator  | mode | total cycles (worst case) | cycles to sample | size in bytes | maximum peak rate at 16MHz | 
| --- | --- | --- | --- | --- | --- |
| 16 bit | X1 | 25 | 13 | 40 | 307k steps/s |
| 16 bit | X2 | 26 | 14 | 50 | 571k half steps/s |
| 32 bit | X1 | 40 | 19 | 78 | 210k steps/s |
| 32 bit | X2 | 43 | 21 | 88 | 372k half steps/s |

If ENCODER_OPTIMIZE_MORE is defined:

| accumulator  | mode | total cycles (worst case) | cycles to sample | size in bytes | maximum peak rate at 16MHz | 
| --- | --- | --- | --- | --- | --- |
| 16 bit | X1 | 24 | 5 | 72 | 640k steps/s |
| 16 bit | X2 | 27 | 7 | 82 | 592k half steps/s |
| 32 bit | X1 | 34 | 5 | 112 | 470k steps/s |
| 32 bit | X2 | 37 | 7 | 122 | 432k half steps/s |

## Todo
- something for shared pcint vectors
- circuits / debouncing
- memory mapped IO's
- xmega, tiny817
- optimize interrupts code