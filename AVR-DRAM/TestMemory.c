#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>

#include "usart.h"

//#define DRAMTEST_VERBOSE

uint8_t QuickTest(uint32_t addr); // true if corruption
uint32_t WalkingOneTest(uint32_t memsize);
uint32_t OwnAddressTest(uint32_t memsize);
uint32_t FpmTest(uint32_t memsize, uint8_t pattern_A, uint8_t pattern_B);
uint32_t RefreshTest(uint32_t memsize, uint8_t pattern, uint16_t delay_sec);

void main(void)
{
	uart_init(BAUD_CALC(115200));
	
	RefreshTimerInt(void);
	MemoryInit(void);

	sei();

	

	while (1)
	{

	}
}

uint8_t QuickTest(uint32_t addr)
{
	bool ThisByteIsCorrupted = 0;
	uint8_t tmp;
	
	for (uint8_t j = 0; j < 9; j++)
	{
		DramWrite(addr, (1 << j));

		tmp = DramRead(addr);

		if (tmp != (1 << j))
		{
			ThisByteIsCorrupted = 1;
			//verbose?
		}
	}

	for (uint8_t j = 0; j < 8; j++)
	{
		DramWrite(addr, (1 << (7 - j)));

		tmp = DramRead(addr);

		if (tmp != (1 << (7 - j)))
		{
			ThisByteIsCorrupted = 1;
			//verbose?
		}
	}

	return ThisByteIsCorrupted
}

uint32_t WalkingOneTest(uint32_t memsize) 
{
	uint32_t corrupted_bytes = 0;
	uint8_t tmp;

	for (uint32_t i = 0; i < memsize; i++)
	{
		if (i % 16 < 8)	
			DramWrite(i, (1 << i % 8));
		else
			DramWrite(i, (1 << (7 - (i % 8))));
	}

	
	for (uint32_t i = 0; i < memsize; i++)
	{
		tmp = DramRead(i);
		
		if (i % 16 < 8)
			if (tmp != (1 << i % 8))
			{
				corrupted_bytes++;
				//verbose ??
			}
		else
			if (tmp != (1 << (7 - (i % 8))))
			{
				corrupted_bytes++;
				//verbose ??
			}
	}

	return corrupted_bytes;
}

uint32_t OwnAddressTest(uint32_t memsize)
{
	uint32_t corrupted_bytes = 0;
	uint8_t tmp;
	
	for (uint32_t i = 0; i < memsize; i++)
	{
		DramWrite(i, i);
	}

	for (uint32_t i = 0; i < memsize; i++)
	{
		tmp = DramRead(i);

		if (tmp != (uint8_t)i)
		{
			corrupted_bytes++;
			//verbose ???
		}
	}
	
	return corrupted_bytes;
}

uint32_t FpmTest(uint32_t memsize, uint8_t pattern_A, uint8_t pattern_B)
{
	uint32_t corrupted_bytes = 0;
	uint8_t TT[256];

	for (uint16_t i = 0; i < 256; i++)
	{
		if (i % 2 == 0) 
			TT[i] = pattern_A;
		else 
			TT[i] = pattern_B;
	}

	for (uint32_t i = 0; i < memsize; i += 256)
	{
		DramPageWrite(i, 256, TT);
	}

	for (uint32_t i = 0; i < memsize; i += 256)
	{
		DramPageRead(i, 256, TT);

		for (uint16_t i = 0; i < 256; i++)
		{
			if (i % 2 == 0)
			{
				if (TT[i] != pattern_A)
				{
					corrupted_bytes++;
					//verbose ??
				}

			}
			else
			{
				if (TT[i] != pattern_B)
				{
					corrupted_bytes++;
					//verbose ??
				}

			}
		}

	}


	return corrupted_bytes;
}

uint32_t RefreshTest(uint32_t memsize, uint8_t pattern, uint16_t delay_sec)
{
	uint32_t corrupted_bytes = 0;
	uint8_t tmp;

	for (uint32_t i = 0; i < memsize; i++)
	{
		DramWrite(i, pattern);
	}
	
	for (uint16_t i = 0; i < delay_sec; i++)
	{
		_delay_ms(1000);
	}


	for (uint32_t i = 0; i < memsize; i++)
	{
		tmp = DramRead(i);

		if (tmp != pattern)
		{
			corrupted_bytes++;
			//verbose???
		}
	}

	return corrupted_bytes;
}

// todo

// fpm test AA/55
// refresh test ff/00
// verbose corruptions