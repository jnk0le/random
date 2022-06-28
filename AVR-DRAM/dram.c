#include <avr/io.h> 
#include <avr/interrupt.h>
#include <util/atomic.h>

#include "dram.h"

void RefreshTimerInt(void)
{
	// timer 0 overflow used for example
	// refresh period have to match with datasheet of the used memory (4-64 ms)
	
	TCCR0B |= (1<<CS02); // 256 // 4 ms refresh period at 16 MHz
	TIMSK0 |= (1<<TOIE0); // overflow
	// fix compiler errors here 
}

void MemoryInit(void)
{	
#if defined(DRAM_LARGE_MEMORY_MODE) && defined(DRAM_SEPARATE_H_ADDR)
	___DDR(ADDRH_PORT) |= ((1<<(DRAM_ADDRESS_PINS-8)) - 1); 
#endif

#ifdef DRAM_SEPARATE_L_ADDR
	___DDR(ADDRL_PORT) = 0xff;
#endif
	
	// can be merged into one call if using same port 
	___DDR(RAS_PORT) |= (1<<RAS_PIN);
	___DDR(CAS_PORT) |= (1<<CAS_PIN);
	___DDR(WE_PORT) |= (1<<WE_PIN);
	//___DDR(OE_PORT) |= (1<<OE_PIN);
	
#ifndef DRAM_SEPARATE_L_ADDR
	___DDR(LA1_PORT) |= (1<<LA1_PIN);
#endif
	
#ifndef DRAM_SEPARATE_H_ADDR
	___DDR(LA2_PORT) |= (1<<LA2_PIN);
#endif
	
	//OE_HI;
	//WE_HI;
	CAS_HI;
	RAS_HI;
	
	uint8_t i = (DRAM_INIT_SEQUENCE_CYCLES - 1);
	do
	{
		CAS_LO; // CAS lo
		RAS_LO; // RAS lo
		
		asm volatile(DRAM_REFRESH_tRAS_WAITSTATE);
			
		CAS_HI; // CAS hi
		RAS_HI;	// RAS hi
	} while(i--);
	
	___DDR(DATA_PORT) = 0xff; // memory should be in a known state now
}

#ifdef DRAM_LARGE_MEMORY_MODE

	uint8_t DramDirectRead(uint16_t row, uint16_t column)
	{
		uint8_t tmp;
		
		WE_HI;
		//OE_LO;
		
		LA1_HI;
		LA2_HI;
	
	#ifdef DRAM_SEPARATE_H_ADDR
		register uint8_t rtmp;
	#endif
		
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
		#if defined(DRAM_SEPARATE_H_ADDR)
			rtmp = ___PORT(ADDRH_PORT); // save port content
			rtmp &= ~( (1<<(DRAM_ADDRESS_PINS-8)) - 1);
			___PORT(ADDRH_PORT) = rtmp | ((row >> 8) & ((1<<(DRAM_ADDRESS_PINS-8)) - 1));
		#else
			___PORT(ADDRH_PORT) = (row >> 8); // & ( (1<<(DRAM_ADDRESS_PINS-8)) - 1);
			LA2_LO;
		#endif
			
			___PORT(ADDRL_PORT) = (uint8_t)row;

			RAS_LO;

		#if defined(DRAM_SEPARATE_H_ADDR)
			___PORT(ADDRH_PORT) = rtmp | ((column >> 8) & ((1<<(DRAM_ADDRESS_PINS-8)) - 1));
		#else
			LA2_HI;
			___PORT(ADDRH_PORT) = (column >> 8); // & ( (1<<(DRAM_ADDRESS_PINS-8)) - 1);
			LA2_LO;
		#endif
		
			___PORT(ADDRL_PORT) = (uint8_t)column;
			LA1_LO; // lock cas address
			
			___DDR(DATA_PORT) = 0x00; //set port to input
			
			CAS_LO;
			
			___PORT(DATA_PORT) = 0x00; // clear pullups for the next latch-up and give 1 additional delay cycle
			DRAM_tCAC_WAITSTATE;
			
		#ifndef DRAM_EDO_MODE
			asm volatile("nop"::); // valid data have to appear on the port before half of the last delay clock cycle - strongly Tcac dependent
			tmp = ___PIN(DATA_PORT);
		#endif

			CAS_HI;
		#ifdef DRAM_EDO_MODE
			tmp = ___PIN(DATA_PORT);
		#endif
			
			RAS_HI;
		}

		___DDR(DATA_PORT) = 0xff; // set back to output state
		
		return tmp;
	}
	
	void DramDirectWrite(uint16_t row, uint16_t column, uint8_t dat)
	{
		//OE_HI;
		WE_LO; 
	
		LA1_HI;
		LA2_HI;
		
	#ifdef DRAM_SEPARATE_H_ADDR
		register uint8_t rtmp;
	#endif
		
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
		#if defined(DRAM_SEPARATE_H_ADDR)
			rtmp = ___PORT(ADDRH_PORT);
			rtmp &= ~( (1<<(DRAM_ADDRESS_PINS-8)) - 1);
			___PORT(ADDRH_PORT) = rtmp | ((row >> 8) & ((1<<(DRAM_ADDRESS_PINS-8)) - 1));
		#else
			___PORT(ADDRH_PORT) = (row >> 8); // & ((1<<(DRAM_ADDRESS_PINS-8)) - 1));
			LA2_LO;
		#endif
		
			___PORT(ADDRL_PORT) = (uint8_t)row;
		
			RAS_LO;

		#if defined(DRAM_SEPARATE_H_ADDR)
			___PORT(ADDRH_PORT) = rtmp | ((column >> 8) & ((1<<(DRAM_ADDRESS_PINS-8)) - 1));
		#else
			LA2_HI;
			___PORT(ADDRH_PORT) = (column >> 8); // & ((1<<(DRAM_ADDRESS_PINS-8)) - 1));
			LA2_LO;
		#endif
			
			___PORT(ADDRL_PORT) = (uint8_t)column;
			LA1_LO; // lock cas address
		
			___PORT(DATA_PORT) = dat;

			CAS_LO;
		
			DRAM_tCAS_WAITSTATE;

			CAS_HI;
			RAS_HI;
		}
		
	}

	void DramDirectPageRead(uint16_t row, uint16_t column, uint16_t count, uint8_t *Dst)
	{
		WE_HI;
		//OE_LO;
		
		LA1_HI;
		LA2_HI;
		
	#ifdef DRAM_SEPARATE_H_ADDR
		register uint8_t rtmp;
	#endif
	
	#ifdef DRAM_SEPARATE_L_ADDR
		___DDR(DATA_PORT) = 0x00;
	#endif
		
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
		#if defined(DRAM_SEPARATE_H_ADDR)
			rtmp = ___PORT(ADDRH_PORT);
			rtmp &= ~( (1<<(DRAM_ADDRESS_PINS-8)) - 1);
			___PORT(ADDRH_PORT) = rtmp | ((row >> 8) & ((1<<(DRAM_ADDRESS_PINS-8)) - 1));
		#else
			___PORT(ADDRH_PORT) = (row >> 8); // & ((1<<(DRAM_ADDRESS_PINS-8)) - 1));
			LA2_LO;
		#endif
		
			___PORT(ADDRL_PORT) = (uint8_t)row;
			
			RAS_LO;
			
			uint16_t i = 0;
			
			//for(uint16_t i = 0; i < count; i++)
			do 
			{
				LA1_HI;
			#if defined(DRAM_SEPARATE_H_ADDR)
				___PORT(ADDRH_PORT) = rtmp | ((column >> 8) & ((1<<(DRAM_ADDRESS_PINS-8)) - 1));
			#else
				LA2_HI;
				___PORT(ADDRH_PORT) = (column >> 8); // & ((1<<(DRAM_ADDRESS_PINS-8)) - 1));
				LA2_LO;
			#endif
				
				___PORT(ADDRL_PORT) = (uint8_t)column++;
				LA1_LO; // lock cas address
			
			#ifndef DRAM_SEPARATE_L_ADDR
				___DDR(DATA_PORT) = 0x00;
			#endif
				
				CAS_FAST_TOG_L;
				
				___PORT(DATA_PORT) = 0x00; //clear pullups for the next latch-up and give 1 additional delay cycle
				DRAM_tCAC_WAITSTATE;
			
			#ifndef DRAM_EDO_MODE
				asm volatile("nop"::); // valid data have to appear on the port before half of the last delay clock cycle - strongly Tcac dependent
				Dst[i++] = ___PIN(DATA_PORT);
			#endif
				
				CAS_FAST_TOG_H;
			
			#ifdef DRAM_EDO_MODE
				Dst[i++] = ___PIN(DATA_PORT);
			#endif
				
			#ifndef DRAM_SEPARATE_L_ADDR
				___DDR(DATA_PORT) = 0xff; // set back to output
			#endif
			} while(--count);
			
			RAS_HI;
			
		#ifdef DRAM_SEPARATE_L_ADDR
			___DDR(DATA_PORT) = 0xff; // set back to output
		#endif
		}
		
	}
	
	void DramDirectPageWrite(uint16_t row, uint16_t column, uint16_t count, uint8_t *Dst)
	{
		//OE_HI;
		WE_LO;
		
		LA1_HI;
		LA2_HI;
		
	#ifdef DRAM_SEPARATE_H_ADDR
		register uint8_t rtmp;
	#endif
		
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
		#if defined(DRAM_SEPARATE_H_ADDR)
			rtmp = ___PORT(ADDRH_PORT);
			rtmp &= ~( (1<<(DRAM_ADDRESS_PINS-8)) - 1);
			___PORT(ADDRH_PORT) = rtmp | ((row >> 8) & ((1<<(DRAM_ADDRESS_PINS-8)) - 1));
		#else
			___PORT(ADDRH_PORT) = (row >> 8); // & ((1<<(DRAM_ADDRESS_PINS-8)) - 1));
			LA2_LO;
		#endif
			___PORT(ADDRL_PORT) = (uint8_t)row;
			
			RAS_LO;
			
			uint16_t i = 0;
			
			//for(uint16_t i = 0; i < count; i++)
			do
			{
				LA1_HI;
			#if defined(DRAM_SEPARATE_H_ADDR)
				___PORT(ADDRH_PORT) = rtmp | ((column >> 8) & ((1<<(DRAM_ADDRESS_PINS-8)) - 1));
			#else
				LA2_HI;
				___PORT(ADDRH_PORT) = (column >> 8); // & ((1<<(DRAM_ADDRESS_PINS-8)) - 1));
				LA2_LO;
			#endif
				
				___PORT(ADDRL_PORT) = (uint8_t)column++;
				LA1_LO; // lock cas address
				
				___PORT(DATA_PORT) = Dst[i++]; 
				
				CAS_FAST_TOG_L;
				
				DRAM_tCAS_WAITSTATE;
				
				CAS_FAST_TOG_H;
			} while(--count);
			
			RAS_HI;
		}
	}

#else // 64k memory
	
	uint8_t DramRead(uint16_t addr)
	{
		uint8_t tmp;
	
		WE_HI;
		//OE_LO;
	
		LA1_HI;
	
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			// The row address must be applied to the address input pins on the memory device for the prescribed
			// amount of time before RAS goes low and held after RAS goes low.
		
			___PORT(ADDRL_PORT) = (addr >> 8);
		
			// RAS must go from high to low and remain low.
		
			RAS_LO;

			// A column address must be applied to the address input pins on the memory device for the prescribed amount of time and held after CAS goes low.

			___PORT(ADDRL_PORT) = (uint8_t)addr;
			LA1_LO; // lock cas address
		
			___DDR(DATA_PORT) = 0x00; //set port to input

			// CAS must switch from high to low and remain low.

			CAS_LO;

			// Data appears at the data output pins of the memory device. The time at which the data appears depends on when RAS , CAS and OE went low, and when the address is supplied.
		
			___PORT(DATA_PORT) = 0x00; //clear pullups for the next latch-up and give 1 additional delay cycle
			DRAM_tCAC_WAITSTATE;
		
		#ifndef DRAM_EDO_MODE
			asm volatile("nop"::); // valid data have to appear on the port before half of the last delay clock cycle - strongly Tcac dependent 
			tmp = ___PIN(DATA_PORT); 
		#endif
			// Before the read cycle can be considered complete, CAS and RAS must return to their inactive states.

			CAS_HI;
		#ifdef DRAM_EDO_MODE
			tmp = ___PIN(DATA_PORT);
		#endif
			
			RAS_HI;
		}

		___DDR(DATA_PORT) = 0xff; // set back to output state

		return tmp;
	}

	void DramWrite(uint16_t addr, uint8_t dat)
	{
		//OE_HI;
		WE_LO; 
	
		LA1_HI;
	
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			// The row address must be applied to the address input pins on the memory device for the prescribed amount of time before RAS goes low and be held for a period of time.
		
			___PORT(ADDRL_PORT) = (addr >> 8);
		
			// RAS must go from high to low.

			RAS_LO;

			// A column address must be applied to the address input pins on the memory device for the prescribed amount of time after RAS goes low and before CAS goes low and held for the prescribed time.
		
			___PORT(ADDRL_PORT) = (uint8_t)addr;
			LA1_LO; // lock cas address
		
			// Data must be applied to the data input pins the prescribed amount of time before CAS goes low  and held.
		
			___PORT(DATA_PORT) = dat;
		
			// CAS must switch from high to low.

			CAS_LO;
		
			DRAM_tCAS_WAITSTATE;

			// Before the write cycle can be considered complete, CAS and RAS must return to their inactive states.
		
			CAS_HI;
			RAS_HI;
		}
	}
	
	void DramPageRead(uint16_t addr, uint8_t count, uint8_t *Dst)
	{
		WE_HI;
		//OE_LO;
		
	#ifdef DRAM_SEPARATE_L_ADDR
		___DDR(DATA_PORT) = 0x00; //set port to input
	#endif
		
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			___PORT(ADDRL_PORT) = (addr >> 8);
			
			RAS_LO;
			
			uint16_t i = 0; // uint8_t generates weird code
		
			do
			{
				LA1_HI;
				___PORT(ADDRL_PORT) = (uint8_t)addr++;
				LA1_LO; // lock cas address
				
			#ifndef DRAM_SEPARATE_L_ADDR
				___DDR(DATA_PORT) = 0x00; //set port to input
			#endif
				
				CAS_FAST_TOG_L;
				
				___PORT(DATA_PORT) = 0x00; //clear pullups for the next latch-up and give 1 additional delay cycle
				DRAM_tCAC_WAITSTATE;
				
			#ifndef DRAM_EDO_MODE
				asm volatile("nop"::); // valid data have to appear on the port before half of the last delay clock cycle - strongly Tcac dependent
				Dst[i++] = ___PIN(DATA_PORT);
			#endif
				
				CAS_FAST_TOG_H;
				
			#ifdef DRAM_EDO_MODE
				Dst[i++] = ___PIN(DATA_PORT);
			#endif
				
			#ifndef DRAM_SEPARATE_L_ADDR
				___DDR(DATA_PORT) = 0xff; // set back to output
			#endif
			} while(--count);
			
			RAS_HI;
			
		#ifdef DRAM_SEPARATE_L_ADDR
			___DDR(DATA_PORT) = 0xff; // set back to output
		#endif
		}
		
	}
	
	void DramPageWrite(uint16_t addr, uint8_t count, uint8_t *Dst)
	{
		//OE_HI;
		WE_LO;
		
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			___PORT(ADDRL_PORT) = (addr >> 8);
			
			RAS_LO;
			
			uint16_t i = 0; // uint8_t generates weird code
			
			do
			{
				LA1_HI;
				___PORT(ADDRL_PORT) = (uint8_t)addr++;
				LA1_LO; // lock cas address
				
				___PORT(DATA_PORT) = Dst[i++];
				
				CAS_FAST_TOG_L;
				
				DRAM_tCAS_WAITSTATE;
				
				CAS_FAST_TOG_H;
			} while(--count);
			
			RAS_HI;
		}
		
	}
		
#endif // !DRAM_LARGE_MEMORY_MODE

#ifndef DRAM_FAST_TOGGLE
	ISR(DRAM_REFRESH_INTERRUPT) // CAS before RAS refresh
	{
	#if DRAM_REFRESH_CYCLES == 256
		uint8_t i = (DRAM_REFRESH_CYCLES - 1);
	#else
		uint16_t i = (DRAM_REFRESH_CYCLES - 1);
	#endif
		do
		{
			CAS_FAST_TOG_L;
			RAS_FAST_TOG_L;
	
			asm volatile(DRAM_REFRESH_tRAS_WAITSTATE);
	
			CAS_FAST_TOG_H;
			RAS_FAST_TOG_H;
		} while(i--);
	}
#else
	ISR(DRAM_REFRESH_INTERRUPT, ISR_NAKED)
	{
		asm volatile("\n\t"
			"push r0 \n\t"
			"in r0, __SREG__\n\t"
			
			"push r16 \n\t"
			"push r17 \n\t"
			"push r18 \n\t"
		
		#if (DRAM_REFRESH_CYCLES == 4096)
			"clh \n\t" // clear H to make 9 bit counter
		#elif (DRAM_REFRESH_CYCLES > 4096)
			"push r19 \n\t"
			"ldi r19, %M[refresh_multipler] \n\t"
		#endif
			
			"ldi r16, %M[ras_tog_mask] \n\t"
			"ldi r17, %M[cas_tog_mask] \n\t"
			"ldi r18, 0x00 \n\t"
		
		"DRAM_REFRESH_LOOP_%=:"
			"out %M[cas_PIN_input_reg], r17 \n\t"
			"out %M[ras_PIN_input_reg], r16 \n\t"
			DRAM_REFRESH_tRAS_WAITSTATE
			"out %M[cas_PIN_input_reg], r17 \n\t"
			"out %M[ras_PIN_input_reg], r16 \n\t"
		
		#if (DRAM_REFRESH_CYCLES >= 512)
			DRAM_REFRESH_tRP_WAITSTATE
			"out %M[cas_PIN_input_reg], r17 \n\t"
			"out %M[ras_PIN_input_reg], r16 \n\t"
			DRAM_REFRESH_tRAS_WAITSTATE
			"out %M[cas_PIN_input_reg], r17 \n\t"
			"out %M[ras_PIN_input_reg], r16 \n\t"
		#endif
		#if (DRAM_REFRESH_CYCLES >= 1024)
			DRAM_REFRESH_tRP_WAITSTATE
			"out %M[cas_PIN_input_reg], r17 \n\t"
			"out %M[ras_PIN_input_reg], r16 \n\t"
			DRAM_REFRESH_tRAS_WAITSTATE
			"out %M[cas_PIN_input_reg], r17 \n\t"
			"out %M[ras_PIN_input_reg], r16 \n\t"
			DRAM_REFRESH_tRP_WAITSTATE
			"out %M[cas_PIN_input_reg], r17 \n\t"
			"out %M[ras_PIN_input_reg], r16 \n\t"
			DRAM_REFRESH_tRAS_WAITSTATE
			"out %M[cas_PIN_input_reg], r17 \n\t"
			"out %M[ras_PIN_input_reg], r16 \n\t"
		#endif
		#if (DRAM_REFRESH_CYCLES >= 2048)
			DRAM_REFRESH_tRP_WAITSTATE
			"out %M[cas_PIN_input_reg], r17 \n\t"
			"out %M[ras_PIN_input_reg], r16 \n\t"
			DRAM_REFRESH_tRAS_WAITSTATE
			"out %M[cas_PIN_input_reg], r17 \n\t"
			"out %M[ras_PIN_input_reg], r16 \n\t"
			DRAM_REFRESH_tRP_WAITSTATE
			"out %M[cas_PIN_input_reg], r17 \n\t"
			"out %M[ras_PIN_input_reg], r16 \n\t"
			DRAM_REFRESH_tRAS_WAITSTATE
			"out %M[cas_PIN_input_reg], r17 \n\t"
			"out %M[ras_PIN_input_reg], r16 \n\t"
			DRAM_REFRESH_tRP_WAITSTATE
			"out %M[cas_PIN_input_reg], r17 \n\t"
			"out %M[ras_PIN_input_reg], r16 \n\t"
			DRAM_REFRESH_tRAS_WAITSTATE
			"out %M[cas_PIN_input_reg], r17 \n\t"
			"out %M[ras_PIN_input_reg], r16 \n\t"
			DRAM_REFRESH_tRP_WAITSTATE
			"out %M[cas_PIN_input_reg], r17 \n\t"
			"out %M[ras_PIN_input_reg], r16 \n\t"
			DRAM_REFRESH_tRAS_WAITSTATE
			"out %M[cas_PIN_input_reg], r17 \n\t"
			"out %M[ras_PIN_input_reg], r16 \n\t"
		#endif
		
			"dec r18 \n\t"
			"brne DRAM_REFRESH_LOOP_%= \n\t"
		
		#if (DRAM_REFRESH_CYCLES == 4096)
			"brhs DRAM_FINISH_LOOP_%= \n\t" // if refresh code was executed twice
			"seh \n\t"
			"rjmp DRAM_REFRESH_LOOP_%= \n\t"
		"DRAM_FINISH_LOOP_%=:"
		#elif (DRAM_REFRESH_CYCLES > 4096)
			"dec r19 \n\t"
			"brne DRAM_REFRESH_LOOP_%= \n\t"
		
			"pop r19 \n\t"
		#endif
			
			"pop r18 \n\t"
			"pop r17 \n\t"
			"pop r16 \n\t"
			
			"out __SREG__, r0 \n\t"
			"pop r0 \n\t"
			"reti \n\t"
			
			: // output operands
			: // input operands
			[ras_tog_mask]        "M" (1<<RAS_PIN),
			[cas_tog_mask]        "M" (1<<CAS_PIN),
			[ras_PIN_input_reg]   "I" (_SFR_IO_ADDR(___PIN(RAS_PORT))),
			[cas_PIN_input_reg]   "I" (_SFR_IO_ADDR(___PIN(CAS_PORT))),
			[refresh_multipler]   "M" (DRAM_REFRESH_CYCLES/(256*8))
			// no clobbers
		);
	}
#endif