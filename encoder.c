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

#ifndef ENCODER_32BIT_ACCUMULATOR
	ISR(ENCODER_INTERRUPT_VECT, ISR_NAKED)
	{
		asm volatile("\n\t"                      /* 4 ISR entry */
		
			"push  r16 \n\t"                             /* 2 */
			"push  r28 \n\t"                            /* 2 */
			"push  r29 \n\t"                            /* 2 */
		
			"lds   r28, EncoderSteps \n\t"		        /* 2 */
			"lds   r29, EncoderSteps+1 \n\t"            /* 2 */
		
		#if defined(__AVR_ATtiny2313__)||defined(__AVR_ATtiny2313A__)
			
			"in    r16, __SREG__ \n\t"                   /* 1 */
			
			"sbis	%M[Input_Port], %M[Input_Pin] \n\t" /* 1/2 */
			"sbiw	r28, 0x02 \n\t"                     /* 2 */
			"adiw	r28, 0x01 \n\t"                     /* 2 */
			
			"out   __SREG__ , r16 \n\t"                  /* 1 */
		#else
			// no SREG affected, weird but works
			"sbis	%M[Input_Port], %M[Input_Pin] \n\t" /* 1/2 */
			"ld 	r16, -Y \n\t"                     /* 2 */
		
			"sbic	%M[Input_Port], %M[Input_Pin] \n\t" /* 1/2 */
			"ld 	r16, Y+ \n\t"                      /* 2 */
		#endif
		
			"sts   EncoderSteps+1, r29 \n\t"             /* 2 */
			"sts   EncoderSteps, r28 \n\t"               /* 2 */
		
			"pop   r29 \n\t"                            /* 2 */
			"pop   r28 \n\t"                            /* 2 */
			"pop   r16 \n\t"                             /* 2 */

			"reti \n\t"                            /* 4 ISR return */
		
			: /* output operands */
		
			: /* input operands */
			[Input_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELB_PORT))),
			[Input_Pin]    "M"    (ENCODER_CHANNELB_PIN)
			/* no clobbers */
		);
	}

#else //ENCODER_32BIT_ACCUMULATOR

	ISR(ENCODER_INTERRUPT_VECT, ISR_NAKED)
	{
		asm volatile("\n\t"                      /* 4 ISR entry */
		
			"push  r16 \n\t"                             /* 2 */
			"in    r16, __SREG__ \n\t"                   /* 1 */
			
			"push  r24 \n\t"                            /* 2 */
			"push  r25 \n\t"                            /* 2 */
			"push  r26 \n\t"                            /* 2 */
			"push  r27 \n\t"                            /* 2 */
		
			"lds   r24, EncoderSteps \n\t"		        /* 2 */
			"lds   r25, EncoderSteps+1 \n\t"            /* 2 */
			"lds   r26, EncoderSteps+2 \n\t"            /* 2 */
			"lds   r27, EncoderSteps+3 \n\t"            /* 2 */
		
			"sbis	%M[Input_Port], %M[Input_Pin] \n\t" /* 1/2 */
			"rjmp	ENC_EXEC2CASE_%= \n\t"              /* 2 */
			
			"adiw	r24, 0x01 \n\t"                     /* 2 */
			"sts   EncoderSteps, r24\n\t"              /* 2 */

			"clr	r24 \n\t"                       /* 1 */
			"adc	r26, r24 \n\t"                       /* 1 */
			"adc	r27, r24 \n\t"                       /* 1 */
			"rjmp ENC_EXIT_%= \n\t"                     /* 2 */

		"ENC_EXEC2CASE_%=: "
			"sbiw	r24, 0x01 \n\t"                     /* 2 */
			"sts   EncoderSteps, r24\n\t"              /* 2 */

			"clr	r24 \n\t"                       /* 1 */
			"sbc	r26, r24 \n\t"                       /* 1 */
			"sbc	r27, r24 \n\t"                       /* 1 */

		"ENC_EXIT_%=: "
			"sts   EncoderSteps+3, r27\n\t"            /* 2 */
			"sts   EncoderSteps+2, r26\n\t"            /* 2 */
			"sts   EncoderSteps+1, r25\n\t"            /* 2 */
			//"sts   EncoderSteps, r24\n\t"              /* 2 */
		
			"pop   r27 \n\t"                           /* 2 */
			"pop   r26 \n\t"                           /* 2 */
			"pop   r25 \n\t"                           /* 2 */
			"pop   r24 \n\t"                           /* 2 */
		
			"out   __SREG__ , r16 \n\t"                 /* 1 */
			"pop   r16 \n\t"                            /* 2 */

			"reti \n\t"                            /* 4 ISR return */
		
			: /* output operands */
		
			: /* input operands */
			[Input_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELB_PORT))),
			[Input_Pin]    "M"    (ENCODER_CHANNELB_PIN)
			/* no clobbers */
		);
		
	}
#endif
	
//ISR prototype

	/*ISR(INTn_vect)
	{
		register int tmp = EncoderSteps;
		
		if(___PIN(ENCODER_CHANNELB_PORT) & (1<<ENCODER_CHANNELB_PIN))
			tmp++;
		else
			tmp--;
	
		EncoderSteps = tmp;
	}*/