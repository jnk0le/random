#include <avr/interrupt.h>
#include <util/atomic.h>
#include <avr/io.h>

#include "encoder.h"

#ifdef ENCODER_32BIT_ACCUMULATOR
	volatile int32_t EncoderSteps;
#else
	volatile int16_t EncoderSteps;
#endif

// define interrupt vector fired on channel A transitions
// X1 mode can be used with INTn interrupts only
// X2 mode can be used with INTn or PCINT interrupts exclusively (no other inputs allowed)
#define ENCODER_INTERRUPT_VECT INT1_vect

	void encoder_init(void)
	{
		//___DDR(ENCODER_CHANNELA_PORT) &= ~(1<<ENCODER_CHANNELA_PIN);
		//___DDR(ENCODER_CHANNELB_PORT) &= ~(1<<ENCODER_CHANNELB_PIN);
		
	#if !defined(ENCODER_USE_X2_MODE)
		#ifdef ENCODER_REVERSE_DIRECTION
			EICRA |= (1<<ISC11)|(1<<ISC10); // interrupt on rising edge
			EIMSK |= (1<<INT1); // INT1 trigger
		#else
			EICRA |= (1<<ISC11); // interrupt on falling edge
			EIMSK |= (1<<INT1); // INT1 trigger
		#endif
	#else // ENCODER_USE_X2_MODE
		// reversing direction is done inside interrupts
		EICRA |= (1<<ISC10); // interrupt on both edges
		EIMSK |= (1<<INT1); // INT1 trigger
		
		// PCINT interrupts can be used in X2 mode only
		//PCICR |= (1<<PCIE0); // exclusive  PCINT0 trigger
		//PCMSK0 |= (1<<PCINT0); // interrupt on PB0
	#endif
	}

#if !defined(ENCODER_32BIT_ACCUMULATOR) // 16 bit accumulator
	#if !defined(ENCODER_USE_X2_MODE) // X1 counting mode
		ISR(ENCODER_INTERRUPT_VECT, ISR_NAKED)
		{
			asm volatile("\n\t"                      /* 4 ISR entry */
		
				"push  r16 \n\t"                             /* 2 */
				"push  r28 \n\t"                            /* 2 */
				"push  r29 \n\t"                            /* 2 */
		
				"lds   r28, EncoderSteps \n\t"		        /* 2 */
				"lds   r29, EncoderSteps+1 \n\t"            /* 2 */
		
				"in    r16, __SREG__ \n\t"                   /* 1 */
			
				"sbis	%M[Input_Port], %M[Input_Pin] \n\t" /* 1/2 */
				"sbiw	r28, 0x02 \n\t"                     /* 2 */
				"adiw	r28, 0x01 \n\t"                     /* 2 */
			
				"out   __SREG__, r16 \n\t"                  /* 1 */
				
// 			//#if defined(__AVR_ATtiny2313__)||defined(__AVR_ATtiny2313A__) 
//			//#else
// 				// no SREG affected, weird but works // rejected due to late sampling
// 				"sbis	%M[Input_Port], %M[Input_Pin] \n\t" /* 1/2 */
// 				"ld 	r16, -Y \n\t"                     /* 2 */
// 		
// 				"sbic	%M[Input_Port], %M[Input_Pin] \n\t" /* 1/2 */
// 				"ld 	r16, Y+ \n\t"                      /* 2 */
// 			#endif
		
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
	#else // ENCODER_USE_X2_MODE
		ISR(ENCODER_INTERRUPT_VECT, ISR_NAKED)
		{
			asm volatile("\n\t"                      /* 4 ISR entry */
		
				"push  r16 \n\t"                             /* 2 */
				"push  r28 \n\t"                            /* 2 */
				"push  r29 \n\t"                            /* 2 */
		
				"lds   r28, EncoderSteps \n\t"		        /* 2 */
				"lds   r29, EncoderSteps+1 \n\t"            /* 2 */
		
			#if defined(__AVR_ATtiny2313__)||defined(__AVR_ATtiny2313A__)
					"in    r16, __SREG__ \n\t"                    // 1/1/1/1
			
					"adiw	r28, 0x01 \n\t"                       // 3/3/3/3
			
				#ifdef	ENCODER_REVERSE_DIRECTION
					"sbis	%M[InputA_Port], %M[InputA_Pin] \n\t" // 4/4/5/5
				#else
					"sbic	%M[InputA_Port], %M[InputA_Pin] \n\t" // 4/4/5/5
				#endif
					"rjmp	ENCODER_AH_%= \n\t"                   // 6/6/-/-
				
					"sbis	%M[InputB_Port], %M[InputB_Pin] \n\t" // -/-/6/7
					"sbiw	r28, 0x02 \n\t"                       // -/-/8/-
					"rjmp	ENCODER_EXIT_%= \n\t"                 // -/-/10/9
			
				"ENCODER_AH_%=:"	
					"sbic	%M[InputB_Port], %M[InputB_Pin] \n\t" // 7/8/-/-
					"sbiw	r28, 0x02 \n\t"                       // 9/-/-/-
				
				"ENCODER_EXIT_%=:"
					"out   __SREG__ , r16 \n\t"                   // 10/9/11/10
			#else
				#ifdef ENCODER_REVERSE_DIRECTION
					"sbis	%M[InputA_Port], %M[InputA_Pin] \n\t" // 1/1/2/2
				#else
					"sbic	%M[InputA_Port], %M[InputA_Pin] \n\t" // 1/1/2/2
				#endif
					"rjmp	ENCODER_AH_%= \n\t"                   // 3/3/-/-
				
					"sbis	%M[InputB_Port], %M[InputB_Pin] \n\t" // -/-/3/4
					"rjmp	ENCODER_DEC_%= \n\t"                  // -/-/5/-
					"rjmp	ENCODER_INC_%= \n\t"                  // -/-/-/6
			
				"ENCODER_AH_%=:"
					"sbic	%M[InputB_Port], %M[InputB_Pin] \n\t" // 4/5/-/-
					"rjmp	ENCODER_INC_%= \n\t"                  // 6/-/-/-
				"ENCODER_DEC_%=:"
					"ld      r16, -Y \n\t"                        // -/7/7/-
					"rjmp	ENCODER_EXIT_%= \n\t"                 // -/9/9/-
				"ENCODER_INC_%=:"
					"ld      r16, Y+ \n\t"                        // 8/-/-/8
				"ENCODER_EXIT_%=:"                                // 8/9/9/8
			#endif
		
				"sts   EncoderSteps+1, r29 \n\t"             /* 2 */
				"sts   EncoderSteps, r28 \n\t"               /* 2 */
		
				"pop   r29 \n\t"                            /* 2 */
				"pop   r28 \n\t"                            /* 2 */
				"pop   r16 \n\t"                             /* 2 */

				"reti \n\t"                            /* 4 ISR return */
		
				: /* output operands */
		
				: /* input operands */
				[InputA_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELA_PORT))),
				[InputA_Pin]    "M"    (ENCODER_CHANNELA_PIN),
				[InputB_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELB_PORT))),
				[InputB_Pin]    "M"    (ENCODER_CHANNELB_PIN)
				
				/* no clobbers */
			);
		}
		
	#endif
#else //ENCODER_32BIT_ACCUMULATOR
	#if !defined(ENCODER_USE_X2_MODE) // X1 counting mode
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
		
				"sbic	%M[Input_Port], %M[Input_Pin] \n\t" /* 1/2 */
				"rjmp	ENC_INC_%= \n\t"              /* 2 */
			
				"subi	r24, 1 \n\t"
				"sbci	r24, 0 \n\t"
				"sbci	r24, 0 \n\t"
				"sbci	r24, 0 \n\t"
				"rjmp ENC_EXIT_%= \n\t"                     /* 2 */

			"ENC_INC_%=: "
				"subi	r24, -1 \n\t"
				"sbci	r25, -1 \n\t"
				"sbci	r26, -1 \n\t"
				"sbci	r27, -1 \n\t"

			"ENC_EXIT_%=: "
				"sts   EncoderSteps+3, r27\n\t"            /* 2 */
				"sts   EncoderSteps+2, r26\n\t"            /* 2 */
				"sts   EncoderSteps+1, r25\n\t"            /* 2 */
				"sts   EncoderSteps, r24\n\t"              /* 2 */
		
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
	#else // ENCODER_USE_X2_MODE
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
		
			#ifdef ENCODER_REVERSE_DIRECTION
				"sbis	%M[InputA_Port], %M[InputA_Pin] \n\t"
			#else
				"sbic	%M[InputA_Port], %M[InputA_Pin] \n\t"
			#endif
				"rjmp	ENCODER_AH_%= \n\t" 
				
				"sbis	%M[InputB_Port], %M[InputB_Pin] \n\t"
				"rjmp	ENCODER_DEC_%= \n\t"
				"rjmp	ENCODER_INC_%= \n\t"
				
			"ENCODER_AH_%=:"
				"sbis	%M[InputB_Port], %M[InputB_Pin] \n\t"
				"rjmp	ENCODER_INC_%= \n\t"
			                                                   // 6/5/5/6
			"ENCODER_DEC_%=:"
				"subi	r24, 1 \n\t"
				"sbci	r24, 0 \n\t"
				"sbci	r24, 0 \n\t"
				"sbci	r24, 0 \n\t"
				"rjmp	ENCODER_EXIT_%= \n\t"
			
			"ENCODER_INC_%=:"
				"subi	r24, -1 \n\t"
				"sbci	r25, -1 \n\t"
				"sbci	r26, -1 \n\t"
				"sbci	r27, -1 \n\t"
				                                              // 10/11/11/10
			"ENCODER_EXIT_%=:"
				"sts   EncoderSteps+3, r27\n\t"            /* 2 */
				"sts   EncoderSteps+2, r26\n\t"            /* 2 */
				"sts   EncoderSteps+1, r25\n\t"            /* 2 */
				"sts   EncoderSteps, r24\n\t"              /* 2 */
		
				"pop   r27 \n\t"                           /* 2 */
				"pop   r26 \n\t"                           /* 2 */
				"pop   r25 \n\t"                           /* 2 */
				"pop   r24 \n\t"                           /* 2 */
		
				"out   __SREG__ , r16 \n\t"                 /* 1 */
				"pop   r16 \n\t"                            /* 2 */

				"reti \n\t"                            /* 4 ISR return */
		
				: /* output operands */
		
				: /* input operands */
				[InputA_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELA_PORT))),
				[InputA_Pin]    "M"    (ENCODER_CHANNELA_PIN),
				[Input_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELB_PORT))),
				[Input_Pin]    "M"    (ENCODER_CHANNELB_PIN)
				/* no clobbers */
			);
		}
	#endif
#endif
	
//ISR prototype
/*
	// X1 counting mode
	ISR(INTn_vect)
	{
		register int16/32_t tmp = EncoderSteps;
		
		if(___PIN(ENCODER_CHANNELB_PORT) & (1<<ENCODER_CHANNELB_PIN))
			tmp++;
		else
			tmp--;
	
		EncoderSteps = tmp;
	}
	
	// X2 counting mode
	ISR(INTn_vect)
	{
		register int16/32_t tmp = EncoderStepsa;
		
		//if reverse ???
		if(!(___PIN(ENCODER_CHANNELA_PORT) & (1<<ENCODER_CHANNELA_PIN)))
		{
			if(___PIN(ENCODER_CHANNELB_PORT) & (1<<ENCODER_CHANNELB_PIN))
				tmp++;
			else
				tmp--;
		}
		else
		{
			if(___PIN(ENCODER_CHANNELB_PORT) & (1<<ENCODER_CHANNELB_PIN))
				tmp--;
			else
				tmp++;
		}
		
		EncoderStepsa = tmp;
	}
*/