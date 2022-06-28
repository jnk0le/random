# DRAM LIBRARY FOR AVR

- linear addressing
- can work from single 8 bit port + 5 control lines
- support for EDO read sequences 
- requires 8 bit (also 2x4, 4x2, 8x1) memories with CBR refresh capability

## Setup
Before setting up memory several conditions must be met:

- define DRAM_ADDRESS_PINS to value corresponding with used memory (eg. 12 for 16MB simms - 2^2n)
- define DRAM_INIT_SEQUENCE_CYCLES to value given by the datasheet of used memory
- define all control pins and full 8 bit port for data operation 
- define DRAM_EDO_MODE if memory is EDO type
- OE signal have to be tied to ground

## Optional
- modify RefreshTimerInt() with corresponding DRAM_REFRESH_INTERRUPT macro for most optimal refresh peroid corresponding to datasheet of given memory
(example code uses timer 0 with 256 prescaler and overflow interrupt which should be safe but not optimal)
- in case of using EDO memory (especially with FPM-read sequence), to prevent possible shot throught, address lines have to be separated from data lines (define DRAM_SEPARATE_L_ADDR)
- if you prefer to have one latch less at a cost of half of the another port (pins 0,1,2 up to DRAM_ADDRESS_PINS - 8) define DRAM_SEPARATE_H_ADDR macro
- if memory doesn't want to work correctly add some delays to DRAM_t*_WAITSTATE or define DRAM_FORCE_SLOW_STROBES macro
- All parallel ports/pins can be reused for other purposes (data, address, and latches (ADDRH latch may need modification in code)), in this case all pins have to be set back to their output states before entering Dram* functions.