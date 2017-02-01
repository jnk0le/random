This is just example of low-level encoder driver (no support for multiple encoders "per-lib" yet).
Code can be extracted and reused to create multiple instances working simultaneously.
This "library" is intended to work especially with high speed rotary encoders (the ones used in motor applications), and requires already debounced signals on the inputs.

###SETUP
- define all used input pins in header file
	* channel A input pin have to be used as an interrupt trigger source
- define used interrupt vector in encoder.c file
- modify encoder_init() function with init sequence corresponding to used interrupt vector
- define other switches if needed

###NOTES
- Currently only X1 and X2 counting modes are implemented (switchable by ENCODER_USE_X2_MODE macro).
	* x1 -- one edge on one channel
	* x2 -- both edges on one channel
	* x4 -- both edges on both channels
- The biggest drawback of the X1 mode is that, the counter will "run away" if the encoder is held (therefore bouncing) at the trigger point. 
- ISR overhead can be further reduced by using globally reserved lower registers or GPIOR's for the counter. 
- All defined inputs have to be mapped in lower IO address space.

###INTERRUPT TIMMINGS

- Inputs have to be sampled within 1/4 of the step period (total execution time can be longer).
- All timmings are given for no jitter (completing execution of the ongoing instruction) and 4 cycle isr entry/exit.

| accumulator  | mode | total cycles (worst case) | cycles to sample | size in bytes | maximum peak rate at 16MHz | 
| --- | --- | --- | --- | --- | --- |
| 16 bit | X1 | 35 | 15 | 40 | 267k steps/s |
| 16 bit | X2 | 37/39 | 17/20 | 50 | 216/200k half steps/s |
| 32 bit | X1 | 54 | 23 | 80 | 174k steps/s |
| 32 bit | X2 | 57 | 26 | 90 | 140k half steps/s |

If ENCODER_OPTIMIZE_MORE is defined:

| accumulator  | mode | total cycles (worst case) | cycles to sample | size in bytes | maximum peak rate at 16MHz | 
| --- | --- | --- | --- | --- | --- |
| 16 bit | X1 | 31 | 4 | 72 | 516k steps/s |
| 16 bit | X2 | 34 | 7 | 82 | 470k half steps/s |
| 32 bit | X1 | 41 | 4 | 112 | 390k steps/s |
| 32 bit | X2 | 44 | 7 | 122 | 363k half steps/s |

###TODO
- something for shared pcint vectors
- circuits / debouncing
- memory mapped IO's
- xmega