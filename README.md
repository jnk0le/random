This in only example code for full step and glith free quadrature encoders, thus requiring hardware debouncing for mechanical encoders (and NO, 2x10k pullups + 2x100nF is NOT correct debouncing).

Interrupt with 16 bitt accumulator takes 35 cycles in worst case. (max 571k steps/s @20MHz)
Interrupt with 32 bitt accumulator takes 58 cycles in worst case. (max 344k steps/s @20MHz)

The real allowable speed is slower due to wrap-around of steps counter if it's not being read.

