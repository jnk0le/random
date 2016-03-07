#include <avr/interrupt.h>
#include <util/atomic.h>
#include <avr/io.h>

#include "encoder.h"

	void encoder_init(void)
	{
	#ifdef ENCODER_REVERSE_DIRECTION
		EICRA |= (1<<ISC11)|(1<<ISC10); // interrupt on rising edge
		EIMSK |= (1<<INT1);
	#else
		EICRA |= (1<<ISC11); // interrupt on falling edge
		EIMSK |= (1<<INT1);
	#endif
	
	//___DDR(ENCODER_CHANNELB_PORT) &= ~(1<<ENCODER_CHANNELB_PIN);
	}

#ifndef ENCODER_32BIT_ACCUMULATOR
	volatile int16_t EncoderSteps;

	int16_t EncoderReadDiff(void)
	{
		int16_t tmp;
		
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			tmp = EncoderSteps;
			EncoderSteps = 0;
		}
		
		return tmp;
	}

	ISR(INT1_vect, ISR_NAKED)
	{
		asm volatile("\n\t"                      /* 4 ISR entry */
		
		"push  r0 \n\t"                             /* 2 */
		"in    r0, __SREG__ \n\t"                   /* 1 */
		
		"push  r24 \n\t"                            /* 2 */
		"push  r25 \n\t"                            /* 2 */
		
		"lds   r24, EncoderSteps \n\t"		        /* 2 */
		"lds   r25, EncoderSteps+1 \n\t"            /* 2 */
		 
	#if ENCODER_MOSTLY_COUNTING_DIRECTION == 0 // CW
		"sbis	%M[Input_Port], %M[Input_Pin] \n\t" /* 1/2 */
		"rjmp	ENC_EXEC2CASE_%= \n\t"              /* 2 */
		"adiw	r24, 0x01 \n\t"                     /* 2 */
	#else // CCW
		"sbic	%M[Input_Port], %M[Input_Pin] \n\t" /* 1/2 */
		"rjmp	ENC_EXEC2CASE_%= \n\t"              /* 2 */
		"sbiw	r24, 0x01 \n\t"                     /* 2 */
	#endif
	
	"ENC_EXIT_%=: "
		"sts   EncoderSteps+1, r25\n\t"            /* 2 */
		"sts   EncoderSteps, r24\n\t"              /* 2 */
		
		"pop   r25 \n\t"                           /* 2 */
		"pop   r24 \n\t"                           /* 2 */
		
		"out   __SREG__ , r0 \n\t"                 /* 1 */
		"pop   r0 \n\t"                            /* 2 */

		"reti \n\t"                            /* 4 ISR return */
	
	"ENC_EXEC2CASE_%=: "
		
	#if ENCODER_MOSTLY_COUNTING_DIRECTION == 0 // CW
		"sbiw	r24, 0x01 \n\t"                     /* 2 */
	#else // CCW
		"adiw	r24, 0x01 \n\t"                     /* 2 */
	#endif
		"rjmp ENC_EXIT_%= \n\t"                     /* 2 */
		
		: /* output operands */
		
		: /* input operands */
		[Input_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELB_PORT))),
		[Input_Pin]        "M"    (ENCODER_CHANNELB_PIN)
		/* no clobbers */
		);
	}
	
#else //32 bit accumulator
	volatile int32_t EncoderSteps;

	int32_t EncoderReadDiff(void)
	{
		int32_t tmp;
		
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			tmp = EncoderSteps;
			EncoderSteps = 0;
		}
		
		return tmp;
	}

	ISR(INT1_vect, ISR_NAKED)
	{
		asm volatile("\n\t"                      /* 4 ISR entry */
		
		"push  r0 \n\t"                             /* 2 */
		"in    r0, __SREG__ \n\t"                   /* 1 */
		
		"push  r1 \n\t"                            /* 2 */
		"clr   r1 \n\t"                            /* 2 */
		"push  r24 \n\t"                            /* 2 */
		"push  r25 \n\t"                            /* 2 */
		"push  r26 \n\t"                            /* 2 */
		"push  r27 \n\t"                            /* 2 */
		
		"lds   r24, EncoderSteps \n\t"		        /* 2 */
		"lds   r25, EncoderSteps+1 \n\t"            /* 2 */
		"lds   r26, EncoderSteps+2 \n\t"            /* 2 */
		"lds   r27, EncoderSteps+3 \n\t"            /* 2 */
		
	#if ENCODER_MOSTLY_COUNTING_DIRECTION == 0 // CW
		"sbis	%M[Input_Port], %M[Input_Pin] \n\t" /* 1/2 */
		"rjmp	ENC_EXEC2CASE_%= \n\t"              /* 2 */
		"adiw	r24, 0x01 \n\t"                     /* 2 */
		"adc	r26, r1 \n\t"                       /* 1 */
		"adc	r27, r1 \n\t"                       /* 1 */
	#else // CCW
		"sbic	%M[Input_Port], %M[Input_Pin] \n\t" /* 1/2 */
		"rjmp	ENC_EXEC2CASE_%= \n\t"              /* 2 */
		"sbiw	r24, 0x01 \n\t"                     /* 2 */
		"sbc	r26, r1 \n\t"                       /* 1 */
		"sbc	r27, r1 \n\t"                       /* 1 */
	#endif
		
		"ENC_EXIT_%=: "
		"sts   EncoderSteps+3, r27\n\t"            /* 2 */
		"sts   EncoderSteps+2, r26\n\t"            /* 2 */
		"sts   EncoderSteps+1, r25\n\t"            /* 2 */
		"sts   EncoderSteps, r24\n\t"              /* 2 */
		
		"pop   r27 \n\t"                           /* 2 */
		"pop   r26 \n\t"                           /* 2 */
		"pop   r25 \n\t"                           /* 2 */
		"pop   r24 \n\t"                           /* 2 */
		"pop   r1 \n\t"                           /* 2 */
		
		"out   __SREG__ , r0 \n\t"                 /* 1 */
		"pop   r0 \n\t"                            /* 2 */

		"reti \n\t"                            /* 4 ISR return */
		
		"ENC_EXEC2CASE_%=: "
		
	#if ENCODER_MOSTLY_COUNTING_DIRECTION == 0 // CW
		"sbiw	r24, 0x01 \n\t"                     /* 2 */
		"sbc	r26, r1 \n\t"                       /* 1 */
		"sbc	r27, r1 \n\t"                       /* 1 */
	#else // CCW
		"adiw	r24, 0x01 \n\t"                     /* 2 */
		"adc	r26, r1 \n\t"                       /* 1 */
		"adc	r27, r1 \n\t"                       /* 1 */
	#endif
		"rjmp ENC_EXIT_%= \n\t"                     /* 2 */
		
		: /* output operands */
		
		: /* input operands */
		[Input_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELB_PORT))),
		[Input_Pin]        "M"    (ENCODER_CHANNELB_PIN)
		/* no clobbers */
		);
		
	}
	
#endif

//ISR prototype

	/*ISR(INT1_vect)
	{
		register int tmp = EncoderSteps;
		
	#if ENCODER_MAIN_COUNTING_DIRECTION == 0 // CW
		if(!(___PIN(ENCODER_CHANNELB_PORT) & (1<<ENCODER_CHANNELB_PIN)))
			tmp--;
		else
			tmp++;
	#else // CCW
		if(___PIN(ENCODER_CHANNELB_PORT) & (1<<ENCODER_CHANNELB_PIN))
			tmp++;
		else
			tmp--;
	#endif
	
		EncoderSteps = tmp;
	}*/