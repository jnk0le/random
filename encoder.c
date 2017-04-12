#include <avr/interrupt.h>
#include <util/atomic.h>
#include <avr/io.h>

#include "encoder.h"

#ifdef ENCODER_32BIT_ACCUMULATOR
	volatile int32_t EncoderSteps;
#else
	volatile int16_t EncoderSteps;
#endif

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
		#if !defined(ENCODER_OPTIMIZE_MORE)
			asm volatile("\n\t"
				
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE
				"in    %[sreg_save], __SREG__ \n\t"
			#else
				"push  r16 \n\t"
				"in    r16, __SREG__ \n\t"
			#endif
				
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"movw	%[z_save], r28 \n\t"
				#else // in this case only 4 cycles are prematured out
					"mov	%A[z_save], r28 \n\t"
					"mov	%B[z_save], r29 \n\t"
				#endif
			#else
				"push  r28 \n\t"
				"push  r29 \n\t"
			#endif
				
				"lds   r28, EncoderSteps \n\t"
				"lds   r29, EncoderSteps+1 \n\t"
				
				"sbis	%M[Input_Port], %M[Input_Pin] \n\t"
				"sbiw	r28, 0x02 \n\t"
				"adiw	r28, 0x01 \n\t"
				
				"sts   EncoderSteps+1, r29 \n\t"
				"sts   EncoderSteps, r28 \n\t"
			
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"movw	r28, %[z_save] \n\t"
				#else // in this case only 4 cycles are prematured out
					"mov	r29, %B[z_save] \n\t"
					"mov	r28, %A[z_save] \n\t"
				#endif
			#else
				"pop   r29 \n\t"
				"pop   r28 \n\t"
			#endif
			
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE
				"out   __SREG__, %[sreg_save] \n\t"
			#else
				"out   __SREG__, r16 \n\t"
				"pop   r16 \n\t"
			#endif
				"reti \n\t"
		
				: // outputs
				ENCODER_REG_SAVE_LIST
				: // inputs
				[Input_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELB_PORT))),
				[Input_Pin]    "M"    (ENCODER_CHANNELB_PIN)
				// no clobbers
			);
		#else
			asm volatile("\n\t"     
				"sbis	%M[Input_Port], %M[Input_Pin] \n\t"
				"rjmp	ENC_DEC_%= \n\t" // 7
		
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"movw	%[z_save], r16 \n\t"
					"in		r16, __SREG__ \n\t"
				#else
					"in 	%A[z_save], __SREG__ \n\t"
					"mov	%B[z_save], r17 \n\t"
				#endif
			#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)
				"in		%[sreg_save], __SREG__ \n\t"
				"push	r17 \n\t"
			#else
				"push	r16 \n\t"
				"in		r16, __SREG__ \n\t"
				"push	r17 \n\t"
			#endif
		
				"lds	r17, EncoderSteps \n\t"
				"subi	r17, -1 \n\t"
				"sts	EncoderSteps, r17 \n\t"
		
				"lds	r17, EncoderSteps+1 \n\t"
				"sbci	r17, -1 \n\t"
				"sts	EncoderSteps+1, r17 \n\t"
		
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"out	__SREG__, r16 \n\t"
					"movw	r16, %[z_save] \n\t"
				#else
					"mov	r17, %B[z_save] \n\t"
					"out	__SREG__, %A[z_save] \n\t"
				#endif	
			#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)
				"pop	r17 \n\t"
				"out	__SREG__, %[sreg_save] \n\t"
			#else
				"pop	r17 \n\t"
				"out	__SREG__, r16 \n\t"
				"pop	r16 \n\t"
			#endif
				"reti \n\t"
		
			"ENC_DEC_%=:"
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"movw	%[z_save], r16 \n\t"
					"in		r16, __SREG__ \n\t"
				#else
					"in 	%A[z_save], __SREG__ \n\t"
					"mov	%B[z_save], r17 \n\t"
				#endif
			#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)
				"in		%[sreg_save], __SREG__ \n\t"
				"push	r17 \n\t"
			#else
				"push	r16 \n\t"
				"in		r16, __SREG__ \n\t"
				"push	r17 \n\t"
			#endif
	
				"lds	r17, EncoderSteps \n\t"
				"subi	r17, 1 \n\t"
				"sts	EncoderSteps, r17 \n\t" // 17
		
				"lds	r17, EncoderSteps+1 \n\t"
				"sbci	r17, 0 \n\t"
				"sts	EncoderSteps+1, r17 \n\t" // 22
		
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"out	__SREG__, r16 \n\t"
					"movw	r16, %[z_save] \n\t"
				#else
					"mov	r17, %B[z_save] \n\t"
					"out	__SREG__, %A[z_save] \n\t"
				#endif	
			#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)
				"pop	r17 \n\t"
				"out	__SREG__, %[sreg_save] \n\t"
			#else
				"pop	r17 \n\t"
				"out	__SREG__, r16 \n\t"
				"pop	r16 \n\t"
			#endif
				"reti \n\t" // 31
	
				: // outputs
				ENCODER_REG_SAVE_LIST
				: // inputs
				[Input_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELB_PORT))),
				[Input_Pin]    "M"    (ENCODER_CHANNELB_PIN)
				// no clobbers
			);
		#endif
		}
	#else // ENCODER_USE_X2_MODE
		ISR(ENCODER_INTERRUPT_VECT, ISR_NAKED)
		{
		#if !defined(ENCODER_OPTIMIZE_MORE)
			asm volatile("\n\t"
			
			#ifndef ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE
				"push  r16 \n\t"
			#endif
			
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"movw	%[z_save], r28 \n\t"
				#else
					"mov	%A[z_save], r28 \n\t"
					"mov	%B[z_save], r29 \n\t"
				#endif
			#else
				"push  r28 \n\t"
				"push  r29 \n\t"
			#endif
			
				"lds   r28, EncoderSteps \n\t"
				"lds   r29, EncoderSteps+1 \n\t"
		
			#if defined(__AVR_ATtiny2313__)||defined(__AVR_ATtiny2313A__) // add others ???
				#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE
					"in    %[sreg_save], __SREG__ \n\t"
				#else
					"in    r16, __SREG__ \n\t"                    // 1/1/1/1
				#endif
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
				#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE
					"out   __SREG__ , %[sreg_save] \n\t"
				#else
					"out   __SREG__ , r16 \n\t"                   // 10/9/11/10
				#endif
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
				#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE
					"ld      %[sreg_save], -Y \n\t"
				#else
					"ld      r16, -Y \n\t"                        // -/7/7/-     // should not hardfault on avr
				#endif
					"rjmp	ENCODER_EXIT_%= \n\t"                 // -/9/9/-
				"ENCODER_INC_%=:"
				#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE
					"ld      %[sreg_save], Y+ \n\t"
				#else
					"ld      r16, Y+ \n\t"                        // 8/-/-/8     // should not hardfault on avr
				#endif
				"ENCODER_EXIT_%=:"                                // 8/9/9/8
			#endif
		
				"sts   EncoderSteps+1, r29 \n\t"
				"sts   EncoderSteps, r28 \n\t"
		
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"movw	r28, %[z_save] \n\t"
				#else
					"mov	r29, %B[z_save] \n\t"
					"mov	r28, %A[z_save] \n\t"
				#endif
			#else
				"pop   r29 \n\t"
				"pop   r28 \n\t"
			#endif
			
			#ifndef ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE
				"pop   r16 \n\t"
			#endif	
				"reti \n\t"
		
				: // outputs
				ENCODER_REG_SAVE_LIST
				: // inputs
				[InputA_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELA_PORT))),
				[InputA_Pin]    "M"    (ENCODER_CHANNELA_PIN),
				[InputB_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELB_PORT))),
				[InputB_Pin]    "M"    (ENCODER_CHANNELB_PIN)
				
				// no clobbers
			);
		#else
			asm volatile("\n\t"
		
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
			                                                  
			"ENCODER_DEC_%=:"
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"movw	%[z_save], r16 \n\t"
					"in		r16, __SREG__ \n\t"
				#else
					"in 	%A[z_save], __SREG__ \n\t"
					"mov	%B[z_save], r17 \n\t"
				#endif
			#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)
				"in		%[sreg_save], __SREG__ \n\t"
				"push	r17 \n\t"
			#else
				"push	r16 \n\t"
				"in		r16, __SREG__ \n\t"
				"push	r17 \n\t"
			#endif
				
				"lds	r17, EncoderSteps \n\t"
				"subi	r17, 1 \n\t"
				"sts	EncoderSteps, r17 \n\t"
		
				"lds	r17, EncoderSteps+1 \n\t"
				"sbci	r17, 0 \n\t"
				"sts	EncoderSteps+1, r17 \n\t"
		
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"out	__SREG__, r16 \n\t"
					"movw	r16, %[z_save] \n\t"
				#else
					"mov	r17, %B[z_save] \n\t"
					"out	__SREG__, %A[z_save] \n\t"
				#endif	
			#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)
				"pop	r17 \n\t"
				"out	__SREG__, %[sreg_save] \n\t"
			#else
				"pop	r17 \n\t"
				"out	__SREG__, r16 \n\t"
				"pop	r16 \n\t"
			#endif
				"reti \n\t"
		
			"ENCODER_INC_%=:"
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"movw	%[z_save], r16 \n\t"
					"in		r16, __SREG__ \n\t"
				#else
					"in 	%A[z_save], __SREG__ \n\t"
					"mov	%B[z_save], r17 \n\t"
				#endif
			#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)
				"in		%[sreg_save], __SREG__ \n\t"
				"push	r17 \n\t"
			#else
				"push	r16 \n\t"
				"in		r16, __SREG__ \n\t"
				"push	r17 \n\t"
			#endif
	
				"lds	r17, EncoderSteps \n\t"
				"subi	r17, -1 \n\t"
				"sts	EncoderSteps, r17 \n\t" //20
		
				"lds	r17, EncoderSteps+1 \n\t"
				"sbci	r17, -1 \n\t"
				"sts	EncoderSteps+1, r17 \n\t" //25
		
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"out	__SREG__, r16 \n\t"
					"movw	r16, %[z_save] \n\t"
				#else
					"mov	r17, %B[z_save] \n\t"
					"out	__SREG__, %A[z_save] \n\t"
				#endif	
			#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)
				"pop	r17 \n\t"
				"out	__SREG__, %[sreg_save] \n\t"
			#else
				"pop	r17 \n\t"
				"out	__SREG__, r16 \n\t"
				"pop	r16 \n\t"
			#endif 
				"reti \n\t" // 34
	
				: // outputs
				ENCODER_REG_SAVE_LIST
				: // inputs
				[InputA_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELA_PORT))),
				[InputA_Pin]    "M"    (ENCODER_CHANNELA_PIN),
				[InputB_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELB_PORT))),
				[InputB_Pin]    "M"    (ENCODER_CHANNELB_PIN)
	
				// no clobbers
			);	
		#endif
		}
		
	#endif
#else //ENCODER_32BIT_ACCUMULATOR
	#if !defined(ENCODER_USE_X2_MODE) // X1 counting mode
		ISR(ENCODER_INTERRUPT_VECT, ISR_NAKED)
		{
		#if !defined(ENCODER_OPTIMIZE_MORE)
			asm volatile("\n\t"
			
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE
				"in		%[sreg_save], __SREG__ \n\t"
			#else
				"push	r16 \n\t"
				"in		r16, __SREG__ \n\t"
			#endif
			
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"movw	%[z_save], r24 \n\t"
				#else
					"mov	%A[z_save], r24 \n\t"
					"mov	%B[z_save], r25 \n\t"
				#endif
			#else
				"push	r24 \n\t"
				"push	r25 \n\t"
			#endif
				"push	r26 \n\t"
				"push	r27 \n\t"
		
				"lds	r24, EncoderSteps \n\t"
				"lds	r25, EncoderSteps+1 \n\t"
				"lds	r26, EncoderSteps+2 \n\t"
				"lds	r27, EncoderSteps+3 \n\t"
		
				"sbic	%M[Input_Port], %M[Input_Pin] \n\t"
				"rjmp	ENC_INC_%= \n\t"
			
			#ifdef __AVR_HAVE_MOVW__
				"sbiw	r24, 1 \n\t"
			#else
				"subi	r24, 1 \n\t"
				"sbci	r25, 0 \n\t"
			#endif
				"sbci	r26, 0 \n\t"
				"sbci	r27, 0 \n\t"
				"rjmp	ENC_EXIT_%= \n\t"

			"ENC_INC_%=: "
				"subi	r24, -1 \n\t"
				"sbci	r25, -1 \n\t"
				"sbci	r26, -1 \n\t"
				"sbci	r27, -1 \n\t"

			"ENC_EXIT_%=: "
				"sts	EncoderSteps+3, r27 \n\t"
				"sts	EncoderSteps+2, r26 \n\t"
				"sts	EncoderSteps+1, r25 \n\t"
				"sts	EncoderSteps, r24 \n\t"
		
				"pop	r27 \n\t"
				"pop	r26 \n\t"
				
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"movw	r24, %[z_save] \n\t"
				#else
					"mov	r25, %B[z_save] \n\t"
					"mov	r24, %A[z_save] \n\t"
				#endif
			#else
				"pop	r25 \n\t"
				"pop	r24 \n\t"
			#endif
			
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE
				"out   __SREG__, %[sreg_save] \n\t"
			#else
				"out   __SREG__, r16 \n\t"
				"pop   r16 \n\t"
			#endif
				"reti \n\t"
		
				: // outputs
				ENCODER_REG_SAVE_LIST
				: // inputs
				[Input_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELB_PORT))),
				[Input_Pin]    "M"    (ENCODER_CHANNELB_PIN)
				// no clobbers
			);
		#else
			asm volatile("\n\t"     
				"sbis	%M[Input_Port], %M[Input_Pin] \n\t"
				"rjmp	ENC_DEC_%= \n\t"
		
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"movw	%[z_save], r16 \n\t"
					"in		r16, __SREG__ \n\t"
				#else
					"in 	%A[z_save], __SREG__ \n\t"
					"mov	%B[z_save], r17 \n\t"
				#endif
			#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)
				"in		%[sreg_save], __SREG__ \n\t"
				"push	r17 \n\t"
			#else
				"push	r16 \n\t"
				"in		r16, __SREG__ \n\t"
				"push	r17 \n\t"
			#endif
		
				"lds	r17, EncoderSteps \n\t"
				"subi	r17, -1 \n\t"
				"sts	EncoderSteps, r17 \n\t"
				
				"lds	r17, EncoderSteps+1 \n\t"
				"sbci	r17, -1 \n\t"
				"sts	EncoderSteps+1, r17 \n\t"
		
				"lds	r17, EncoderSteps+2 \n\t"
				"sbci	r17, -1 \n\t"
				"sts	EncoderSteps+2, r17 \n\t"
		
				"lds	r17, EncoderSteps+3 \n\t"
				"sbci	r17, -1 \n\t"
				"sts	EncoderSteps+3, r17 \n\t"
		
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"out	__SREG__, r16 \n\t"
					"movw	r16, %[z_save] \n\t"
				#else
					"mov	r17, %B[z_save] \n\t"
					"out	__SREG__, %A[z_save] \n\t"
				#endif	
			#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)
				"pop	r17 \n\t"
				"out	__SREG__, %[sreg_save] \n\t"
			#else
				"pop	r17 \n\t"
				"out	__SREG__, r16 \n\t"
				"pop	r16 \n\t"
			#endif
				"reti \n\t"
		
			"ENC_DEC_%=:"
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"movw	%[z_save], r16 \n\t"
					"in		r16, __SREG__ \n\t"
				#else
					"in 	%A[z_save], __SREG__ \n\t"
					"mov	%B[z_save], r17 \n\t"
				#endif
			#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)
				"in		%[sreg_save], __SREG__ \n\t"
				"push	r17 \n\t"
			#else
				"push	r16 \n\t"
				"in		r16, __SREG__ \n\t"
				"push	r17 \n\t"
			#endif
	
				"lds	r17, EncoderSteps \n\t"
				"subi	r17, 1 \n\t"
				"sts	EncoderSteps, r17 \n\t"
		
				"lds	r17, EncoderSteps+1 \n\t"
				"sbci	r17, 0 \n\t"
				"sts	EncoderSteps+1, r17 \n\t"
		
				"lds	r17, EncoderSteps+2 \n\t"
				"sbci	r17, 0 \n\t"
				"sts	EncoderSteps+2, r17 \n\t"
		
				"lds	r17, EncoderSteps+3 \n\t"
				"sbci	r17, 0 \n\t"
				"sts	EncoderSteps+3, r17 \n\t" //32
		
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"out	__SREG__, r16 \n\t"
					"movw	r16, %[z_save] \n\t"
				#else
					"mov	r17, %B[z_save] \n\t"
					"out	__SREG__, %A[z_save] \n\t"
				#endif	
			#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)
				"pop	r17 \n\t"
				"out	__SREG__, %[sreg_save] \n\t"
			#else
				"pop	r17 \n\t"
				"out	__SREG__, r16 \n\t"
				"pop	r16 \n\t"
			#endif
				"reti \n\t" //41
	
				: // outputs
				ENCODER_REG_SAVE_LIST
				: // inputs
				[Input_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELB_PORT))),
				[Input_Pin]    "M"    (ENCODER_CHANNELB_PIN)
				// no clobbers
			);
		#endif
		}
	#else // ENCODER_USE_X2_MODE
		ISR(ENCODER_INTERRUPT_VECT, ISR_NAKED)
		{
		#if !defined(ENCODER_OPTIMIZE_MORE)
			asm volatile("\n\t"
			
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE
				"in		%[sreg_save], __SREG__ \n\t"
			#else
				"push	r16 \n\t"
				"in		r16, __SREG__ \n\t"
			#endif
			
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"movw	%[z_save], r24 \n\t"
				#else
					"mov	%A[z_save], r24 \n\t"
					"mov	%B[z_save], r25 \n\t"
				#endif
			#else
				"push  r24 \n\t"
				"push  r25 \n\t"
			#endif
				"push  r26 \n\t"
				"push  r27 \n\t"
		
				"lds   r24, EncoderSteps \n\t"
				"lds   r25, EncoderSteps+1 \n\t"
				"lds   r26, EncoderSteps+2 \n\t"
				"lds   r27, EncoderSteps+3 \n\t"
		
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
			#ifdef __AVR_HAVE_MOVW__
				"sbiw	r24, 1 \n\t"
			#else
				"subi	r24, 1 \n\t"
				"sbci	r25, 0 \n\t"
			#endif
				"sbci	r26, 0 \n\t"
				"sbci	r27, 0 \n\t"
				"rjmp	ENCODER_EXIT_%= \n\t"
			
			"ENCODER_INC_%=:"
				"subi	r24, -1 \n\t"
				"sbci	r25, -1 \n\t"
				"sbci	r26, -1 \n\t"
				"sbci	r27, -1 \n\t"
				                                              // 10/11/11/10
			"ENCODER_EXIT_%=:"
				"sts   EncoderSteps+3, r27\n\t"
				"sts   EncoderSteps+2, r26\n\t"
				"sts   EncoderSteps+1, r25\n\t"
				"sts   EncoderSteps, r24\n\t"
		
				"pop   r27 \n\t"
				"pop   r26 \n\t"
				
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"movw	r24, %[z_save] \n\t"
				#else
					"mov	r25, %B[z_save] \n\t"
					"mov	r24, %A[z_save] \n\t"
				#endif
			#else
				"pop   r25 \n\t"
				"pop   r24 \n\t"
			#endif
		
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE
				"out   __SREG__, %[sreg_save] \n\t"
			#else
				"out   __SREG__, r16 \n\t"
				"pop   r16 \n\t"
			#endif
				"reti \n\t"
		
				: // outputs
				ENCODER_REG_SAVE_LIST
				: // inputs
				[InputA_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELA_PORT))),
				[InputA_Pin]    "M"    (ENCODER_CHANNELA_PIN),
				[InputB_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELB_PORT))),
				[InputB_Pin]    "M"    (ENCODER_CHANNELB_PIN)
				// no clobbers
			);
		#else
			asm volatile("\n\t"      
		
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
				"rjmp	ENCODER_INC_%= \n\t" //10
			                                                  
			"ENCODER_DEC_%=:"
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"movw	%[z_save], r16 \n\t"
					"in		r16, __SREG__ \n\t"
				#else
					"in 	%A[z_save], __SREG__ \n\t"
					"mov	%B[z_save], r17 \n\t"
				#endif
			#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)
				"in		%[sreg_save], __SREG__ \n\t"
				"push	r17 \n\t"
			#else
				"push	r16 \n\t"
				"in		r16, __SREG__ \n\t"
				"push	r17 \n\t"
			#endif
	
				"lds	r17, EncoderSteps \n\t"
				"subi	r17, 1 \n\t"
				"sts	EncoderSteps, r17 \n\t"
		
				"lds	r17, EncoderSteps+1 \n\t"
				"sbci	r17, 0 \n\t"
				"sts	EncoderSteps+1, r17 \n\t"
		
				"lds	r17, EncoderSteps+2 \n\t"
				"sbci	r17, 0 \n\t"
				"sts	EncoderSteps+2, r17 \n\t"
		
				"lds	r17, EncoderSteps+3 \n\t"
				"sbci	r17, 0 \n\t"
				"sts	EncoderSteps+3, r17 \n\t"
		
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"out	__SREG__, r16 \n\t"
					"movw	r16, %[z_save] \n\t"
				#else
					"mov	r17, %B[z_save] \n\t"
					"out	__SREG__, %A[z_save] \n\t"
				#endif	
			#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)
				"pop	r17 \n\t"
				"out	__SREG__, %[sreg_save] \n\t"
			#else
				"pop	r17 \n\t"
				"out	__SREG__, r16 \n\t"
				"pop	r16 \n\t"
			#endif
				"reti \n\t"
		
			"ENCODER_INC_%=:"
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"movw	%[z_save], r16 \n\t"
					"in		r16, __SREG__ \n\t"
				#else
					"in 	%A[z_save], __SREG__ \n\t"
					"mov	%B[z_save], r17 \n\t"
				#endif
			#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)
				"in		%[sreg_save], __SREG__ \n\t"
				"push	r17 \n\t"
			#else
				"push	r16 \n\t"
				"in		r16, __SREG__ \n\t"
				"push	r17 \n\t"
			#endif
	
				"lds	r17, EncoderSteps \n\t"
				"subi	r17, -1 \n\t"
				"sts	EncoderSteps, r17 \n\t"
		
				"lds	r17, EncoderSteps+1 \n\t"
				"sbci	r17, -1 \n\t"
				"sts	EncoderSteps+1, r17 \n\t"
		
				"lds	r17, EncoderSteps+2 \n\t"
				"sbci	r17, -1 \n\t"
				"sts	EncoderSteps+2, r17 \n\t"
		
				"lds	r17, EncoderSteps+3 \n\t"
				"sbci	r17, -1 \n\t"
				"sts	EncoderSteps+3, r17 \n\t" //35
		
			#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
				#ifdef __AVR_HAVE_MOVW__
					"out	__SREG__, r16 \n\t"
					"movw	r16, %[z_save] \n\t"
				#else
					"mov	r17, %B[z_save] \n\t"
					"out	__SREG__, %A[z_save] \n\t"
				#endif	
			#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)
				"pop	r17 \n\t"
				"out	__SREG__, %[sreg_save] \n\t"
			#else
				"pop	r17 \n\t"
				"out	__SREG__, r16 \n\t"
				"pop	r16 \n\t"
			#endif
				"reti \n\t" //44
	
				: // outputs
				ENCODER_REG_SAVE_LIST
				: // inputs
				[InputA_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELA_PORT))),
				[InputA_Pin]    "M"    (ENCODER_CHANNELA_PIN),
				[InputB_Port]   "M"    (_SFR_IO_ADDR(___PIN(ENCODER_CHANNELB_PORT))),
				[InputB_Pin]    "M"    (ENCODER_CHANNELB_PIN)
	
				// no clobbers
			);
		#endif
		}
	#endif
#endif
	
//ISR prototypes
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