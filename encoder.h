#ifndef ENCODER_H_
#define ENCODER_H_

//#define ENCODER_32BIT_ACCUMULATOR
//#define ENCODER_REVERSE_DIRECTION // reverse counting direction


//#define INTERRUPT SOURCE ????
//#define

#define ENCODER_CHANNELB_PORT B // A,B,C,D ... port naming 
#define ENCODER_CHANNELB_PIN 0 // 1,2,3,4 ... pin naming


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


	void encoder_init(void);

#ifndef ENCODER_32BIT_ACCUMULATOR
	
	extern volatile int16_t EncoderSteps;
	
	int16_t EncoderReadDiff(void);

#else //32 bit accumulator
	
	extern volatile int32_t EncoderSteps;
	
	int32_t EncoderReadDiff(void);

#endif

	

#endif /* ENCODER_H_ */