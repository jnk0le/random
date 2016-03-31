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

#ifndef ENCODER_GPIOR_STORAGE
	
	volatile enc_step_t EncoderSteps;

	enc_step_t EncoderReadDiff(void)
	{
		enc_step_t tmp;
		
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			tmp = EncoderSteps;
			EncoderSteps = 0;
		}
		
		return tmp;
	}
	
	enc_step_t EncoderRead(void)
	{
		enc_step_t tmp;
		
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			tmp = EncoderSteps;
		}
		
		return tmp;
	}

#else // ENCODER_GPIOR_STORAGE // gcc is too retarded - return value is placed temporaliry in fucking r24:r27, instead of r22:r25
	
#ifdef ENCODER_32BIT_ACCUMULATOR
	volatile uint8_t EncStep_hbyte;

	int32_t EncoderReadDiff(void) // __attribute__ ((naked))
	{
		asm volatile("\n\t"
		
			"in r0, __SREG__ \n\t"
			"cli \n\t"
		
			"in r22, %M[gpior0] \n\t"
			"in r23, %M[gpior1] \n\t"
			"in r24, %M[gpior2] \n\t"
			"lds r25, EncStep_hbyte \n\t"
		
			"out %M[gpior0], r1 \n\t"
			"out %M[gpior1], r1 \n\t"
			"out %M[gpior2], r1 \n\t"
			"sts EncStep_hbyte, r1 \n\t"
		
			"out __SREG__, r0 \n\t"
		
			"ret \n\t" // 32 bit variable is returned in r22:r25
		
			: /* output operands */
			: /* input operands */
			[gpior0]   "M"    (_SFR_IO_ADDR(GPIOR0)),
			[gpior1]   "M"    (_SFR_IO_ADDR(GPIOR1)),
			[gpior2]   "M"    (_SFR_IO_ADDR(GPIOR2))
			/* no clobbers */
		);
		
	}
	
	int32_t EncoderRead(void)
	{
		asm volatile("\n\t"
		
			"in r0, __SREG__ \n\t"
			"cli \n\t"
		
			"in r22, %M[gpior0] \n\t"
			"in r23, %M[gpior1] \n\t"
			"in r24, %M[gpior2] \n\t"
			"lds r25, EncStep_hbyte \n\t"
		
			"out __SREG__, r0 \n\t"
		
			"ret \n\t" // 32 bit variable is returned in r22:r25
		
			: /* output operands */
			: /* input operands */
			[gpior0]   "M"    (_SFR_IO_ADDR(GPIOR0)),
			[gpior1]   "M"    (_SFR_IO_ADDR(GPIOR1)),
			[gpior2]   "M"    (_SFR_IO_ADDR(GPIOR2))
			/* no clobbers */
		);
	}
	
#else // !ENCODER_32BIT_ACCUMULATOR

	int16_t EncoderReadDiff(void) // __attribute__ ((naked))
	{
		asm volatile("\n\t"
		
			"in r0, __SREG__ \n\t"
			"cli \n\t"
		
			"in r24, %M[gpior1] \n\t"
			"in r25, %M[gpior2] \n\t"
		
			"out %M[gpior1], r1 \n\t"
			"out %M[gpior2], r1 \n\t"
		
			"out __SREG__, r0 \n\t"
		
			"ret \n\t" // 16 bit variable is returned in r24:r25
		
			: /* output operands */
			: /* input operands */
			[gpior0]   "M"    (_SFR_IO_ADDR(GPIOR0)),
			[gpior1]   "M"    (_SFR_IO_ADDR(GPIOR1)),
			[gpior2]   "M"    (_SFR_IO_ADDR(GPIOR2))
			/* no clobbers */
		);
		
	}
	
	int16_t EncoderRead(void)
	{
		asm volatile("\n\t"
		
			"in r0, __SREG__ \n\t"
			"cli \n\t"
		
			"in r22, %M[gpior0] \n\t"
			"in r23, %M[gpior1] \n\t"
			"in r24, %M[gpior2] \n\t"
			"lds r25, EncStep_hbyte \n\t"
		
			"out __SREG__, r0 \n\t"
		
			"ret \n\t" // 32 bit variable is returned in r24:r25
		
			: /* output operands */
			: /* input operands */
			[gpior0]   "M"    (_SFR_IO_ADDR(GPIOR0)),
			[gpior1]   "M"    (_SFR_IO_ADDR(GPIOR1)),
			[gpior2]   "M"    (_SFR_IO_ADDR(GPIOR2))
			/* no clobbers */
		);
	}

#endif // ENCODER_32BIT_ACCUMULATOR

#endif // ENCODER_GPIOR_STORAGE

#ifndef ENCODER_32BIT_ACCUMULATOR
	ISR(ENCODER_INTERRUPT_VECT, ISR_NAKED)
	{
		asm volatile("\n\t"                      /* 4 ISR entry */
		
			"push  r0 \n\t"                             /* 2 */
			"in    r0, __SREG__ \n\t"                   /* 1 */
		
			"push  r24 \n\t"                            /* 2 */
			"push  r25 \n\t"                            /* 2 */
		
		#ifdef ENCODER_GPIOR_STORAGE
			"in    r24, %M[gpior1] \n\t"                   /* 1 */
			"in    r25, %M[gpior2] \n\t"                   /* 1 */
		#else
			"lds   r24, EncoderSteps \n\t"		        /* 2 */
			"lds   r25, EncoderSteps+1 \n\t"            /* 2 */
		#endif
		
			"sbis	%M[Input_Port], %M[Input_Pin] \n\t" /* 1/2 */
			"sbiw	r24, 0x02 \n\t"                     /* 2 */
			"adiw	r24, 0x01 \n\t"                     /* 2 */
		
		#ifdef ENCODER_GPIOR_STORAGE
			"out   %M[gpior2] , 25 \n\t"                  /* 1 */
			"out   %M[gpior1] , 24 \n\t"                  /* 1 */
		#else
			"sts   EncoderSteps+1, r25\n\t"             /* 2 */
			"sts   EncoderSteps, r24\n\t"               /* 2 */
		#endif
		
			"pop   r25 \n\t"                            /* 2 */
			"pop   r24 \n\t"                            /* 2 */
		
			"out   __SREG__ , r0 \n\t"                  /* 1 */
			"pop   r0 \n\t"                             /* 2 */

			"reti \n\t"                            /* 4 ISR return */
		
			: /* output operands */
		
			: /* input operands */
			[gpior1]   "M"    (_SFR_IO_ADDR(GPIOR1)),
			[gpior2]   "M"    (_SFR_IO_ADDR(GPIOR2)),
			[Input_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELB_PORT))),
			[Input_Pin]        "M"    (ENCODER_CHANNELB_PIN)
			/* no clobbers */
		);
	}

#else //!ENCODER_32BIT_ACCUMULATOR

	ISR(ENCODER_INTERRUPT_VECT, ISR_NAKED)
	{
		asm volatile("\n\t"                      /* 4 ISR entry */
		
			"push  r0 \n\t"                             /* 2 */
			"in    r0, __SREG__ \n\t"                   /* 1 */
			
			"push  r24 \n\t"                            /* 2 */
			"push  r25 \n\t"                            /* 2 */
			"push  r26 \n\t"                            /* 2 */
			"push  r27 \n\t"                            /* 2 */
		
		#ifdef ENCODER_GPIOR_STORAGE
			"in  r24, %M[gpior0] \n\t"                 /* 1 */
			"in  r25, %M[gpior1] \n\t"                 /* 1 */
			"in  r26, %M[gpior2] \n\t"                 /* 1 */
			"lds r27, EncStep_hbyte \n\t"                 /* 1 */
		#else
			"lds   r24, EncoderSteps \n\t"		        /* 2 */
			"lds   r25, EncoderSteps+1 \n\t"            /* 2 */
			"lds   r26, EncoderSteps+2 \n\t"            /* 2 */
			"lds   r27, EncoderSteps+3 \n\t"            /* 2 */
		#endif
		
			"sbis	%M[Input_Port], %M[Input_Pin] \n\t" /* 1/2 */
			"rjmp	ENC_EXEC2CASE_%= \n\t"              /* 2 */
			
			"adiw	r24, 0x01 \n\t"                     /* 2 */
		#ifdef ENCODER_GPIOR_STORAGE
			"out  %M[gpior0], r24 \n\t"                 /* 1 */
		#else
			"sts   EncoderSteps, r24\n\t"              /* 2 */
		#endif
			"clr	r24 \n\t"                       /* 1 */
			"adc	r26, r24 \n\t"                       /* 1 */
			"adc	r27, r24 \n\t"                       /* 1 */
			"rjmp ENC_EXIT_%= \n\t"                     /* 2 */

		"ENC_EXEC2CASE_%=: "
			"sbiw	r24, 0x01 \n\t"                     /* 2 */
		#ifdef ENCODER_GPIOR_STORAGE
			"out  %M[gpior0], r24 \n\t"                 /* 1 */
		#else
			"sts   EncoderSteps, r24\n\t"              /* 2 */
		#endif
			"clr	r24 \n\t"                       /* 1 */
			"sbc	r26, r24 \n\t"                       /* 1 */
			"sbc	r27, r24 \n\t"                       /* 1 */

		"ENC_EXIT_%=: "
		#ifdef ENCODER_GPIOR_STORAGE
			"sts  EncStep_hbyte, r27\n\t"            /* 2 */
			"out  %M[gpior2], r26 \n\t"                       /* 1 */
			"out  %M[gpior1], r25 \n\t"                       /* 1 */
			//"out  %M[gpior0], r24 \n\t"                       /* 1 */
		#else
			"sts   EncoderSteps+3, r27\n\t"            /* 2 */
			"sts   EncoderSteps+2, r26\n\t"            /* 2 */
			"sts   EncoderSteps+1, r25\n\t"            /* 2 */
			//"sts   EncoderSteps, r24\n\t"              /* 2 */
		#endif
		
			"pop   r27 \n\t"                           /* 2 */
			"pop   r26 \n\t"                           /* 2 */
			"pop   r25 \n\t"                           /* 2 */
			"pop   r24 \n\t"                           /* 2 */
		
			"out   __SREG__ , r0 \n\t"                 /* 1 */
			"pop   r0 \n\t"                            /* 2 */

			"reti \n\t"                            /* 4 ISR return */
		
			: /* output operands */
		
			: /* input operands */
			[gpior0]   "M"    (_SFR_IO_ADDR(GPIOR0)),
			[gpior1]   "M"    (_SFR_IO_ADDR(GPIOR1)),
			[gpior2]   "M"    (_SFR_IO_ADDR(GPIOR2)),
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
		
		if(___PIN(ENCODER_CHANNELB_PORT) & (1<<ENCODER_CHANNELB_PIN))
			tmp++;
		else
			tmp--;
	
		EncoderSteps = tmp;
	}*/