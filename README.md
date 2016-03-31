This in only example code for full step and glith free quadrature encoders, thus requiring hardware debouncing for mechanical encoders (and no, 2x10k pullups + 2x100nF is NOT correct debouncing).

Interrupt execution time (worst case):
- 16 bit accumulator - 35 cycles (max 571k steps/s @20MHz)
- 16 bit accumulator in GPIOR - 31 cycles (max 645k steps/s @20MHz)
- 32 bit accumulator - 55 cycles (max 363k steps/s @20MHz)
- 32 bit accumulator in GPIOR - 49 cycles (max 408k steps/s @20MHz)

The real allowable speed is slower due need of reading steps counter before it wrap-around.

#notes
- If encoder is standstill at trigger edge it will count only up/down in one direction in case of shifting around edge point.
- GPIOR storage mode with 16 bit accumulator uses two upper io registers leaving GPIOR0 for other purposes.