#ifndef ENCODER_H_
#define ENCODER_H_

#define ENCODER_USE_X2_MODE // count steps on edges of the 
//#define ENCODER_32BIT_ACCUMULATOR
//#define ENCODER_REVERSE_DIRECTION // reverse counting direction 

// channel A definitions are used in X2_MODE only
#define ENCODER_CHANNELA_PORT B // A,B,C,D ... port naming
#define ENCODER_CHANNELA_PIN 0 // 1,2,3,4 ... pin naming

#define ENCODER_CHANNELB_PORT B // A,B,C,D ... port naming 
#define ENCODER_CHANNELB_PIN 1 // 1,2,3,4 ... pin naming

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

#include <util/atomic.h>

void encoder_init(void);

#ifdef ENCODER_32BIT_ACCUMULATOR
	extern volatile int32_t EncoderSteps;
	
	inline int32_t EncoderReadDiff(void) // clears counter after reading
	{
		int32_t tmp;
		
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			tmp = EncoderSteps;
			EncoderSteps = 0;
		}
		
		return tmp;
	}
	
	inline int32_t EncoderRead(void)
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

	inline int16_t EncoderReadDiff(void) // clears counter after reading
	{
		int16_t tmp;
		
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			tmp = EncoderSteps;
			EncoderSteps = 0;
		}
		
		return tmp;
	}
	
	inline int16_t EncoderRead(void)
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