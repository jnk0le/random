#ifndef ENCODER_H_
#define ENCODER_H_

#include <util/atomic.h> // inline funcs

//#define ENCODER_USE_X2_MODE // count steps on both edges of the channel A
//#define ENCODER_32BIT_ACCUMULATOR
//#define ENCODER_REVERSE_DIRECTION // reverse counting direction
#define ENCODER_OPTIMIZE_MORE // use larger and faster interrupt handlers

// channel A definitions are used in X2_MODE only
#define ENCODER_CHANNELA_PORT B // A,B,C,D ... port naming
#define ENCODER_CHANNELA_PIN 0 // 1,2,3,4 ... pin naming

#define ENCODER_CHANNELB_PORT B // A,B,C,D ... port naming 
#define ENCODER_CHANNELB_PIN 1 // 1,2,3,4 ... pin naming

#define ENCODER_INTERRUPT_VECT INT1_vect
// define interrupt vector fired on channel A transitions
// X1 mode can be used with INTn interrupts only
// X2 mode can be used with INTn or PCINT interrupts exclusively (no other inputs allowed)

//#define ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE // prematures out 4 cycles from every isr run // requires one globally reserved lower register
//#define ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE    // prematures out 6 cycles from every isr run // requires pair of globally reserved lower registers
// use globally reserved register for temporary storage in interrupts, should be combined with other interrupts for best results. 
// special care have to be taken when doing so, since those registers can still be used by other compilation units (fixable in gcc by -ffixed-n flag, where n is a suppressed register),
// precompiled libraries (vprintf, vscanf, qsort, strtod, strtol, strtoul), or even assembly hardcoded libraries (fft, aes).
// only registers r2-r7 may be used with acceptable penalty for rest of the code, since other registers might be used by gcc for eg. argument passing.

	#define ENCODER_SREG_SAVE_REG_NAME G_sreg_save // ??? // have to be redeclared under the same name if the same registers are reused in other instances
	#define ENCODER_SREG_SAVE_REG_NUM "r4"
	
	#define ENCODER_Z_SAVE_REG_NAME G_z_save // ??? // have to be redeclared under the same name if the same registers are reused in other instances
	#define ENCODER_Z_SAVE_REG_NUM "r2" // register pair

#ifndef ___DDR
	#define ___DDR(x) ___XDDR(x)
#endif
#ifndef ___XDDR
	#define ___XDDR(x) (DDR ## x)
#endif

#ifndef ___PIN
	#define ___PIN(x) ___XPIN(x)
#endif
#ifndef ___XPIN
	#define ___XPIN(x) (PORT ## x)
#endif

#if defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)&&defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE)
	#define ENCODER_REG_SAVE_LIST \
		[sreg_save] "+r" (ENCODER_SREG_SAVE_REG_NAME), \
		[z_save] "+r" (ENCODER_Z_SAVE_REG_NAME)
		
#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE)
	#define ENCODER_REG_SAVE_LIST \
		[z_save] "+r" (ENCODER_Z_SAVE_REG_NAME)
	
#elif defined(ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE)
	#define ENCODER_REG_SAVE_LIST \
		[sreg_save] "+r" (ENCODER_SREG_SAVE_REG_NAME)
#else
	#define ENCODER_REG_SAVE_LIST
#endif

#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_SREG_SAVE
	register uint8_t ENCODER_SREG_SAVE_REG_NAME asm(ENCODER_SREG_SAVE_REG_NUM); // have to be defined separately in every compilation unit
#endif

#ifdef ENCODER_USE_GLOBALLY_RESERVED_ISR_Z_SAVE
	register uint8_t ENCODER_Z_SAVE_REG_NAME asm(ENCODER_Z_SAVE_REG_NUM); // have to be defined separately in every compilation unit
#endif

void encoder_init(void);

#ifdef ENCODER_32BIT_ACCUMULATOR
	extern volatile int32_t EncoderSteps;
	
	static inline int32_t EncoderReadDiff(void) __attribute__((always_inline));
	static inline int32_t EncoderReadDiff(void) // clears counter after reading
	{
		int32_t tmp;
		
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			tmp = EncoderSteps;
			EncoderSteps = 0;
		}
		
		return tmp;
	}
	
	static inline int32_t EncoderRead(void) __attribute__((always_inline));
	static inline int32_t EncoderRead(void)
	{
		int32_t tmp;
		
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			tmp = EncoderSteps;
		}
		
		return tmp;
	}
#else
	extern volatile int16_t EncoderSteps;

	static inline int16_t EncoderReadDiff(void) __attribute__((always_inline));
	static inline int16_t EncoderReadDiff(void) // clears counter after reading
	{
		int16_t tmp;
		
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			tmp = EncoderSteps;
			EncoderSteps = 0;
		}
		
		return tmp;
	}
	
	static inline int16_t EncoderRead(void) __attribute__((always_inline));
	static inline int16_t EncoderRead(void)
	{
		int16_t tmp;
		
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			tmp = EncoderSteps;
		}
		
		return tmp;
	}
#endif

#endif /* ENCODER_H_ */