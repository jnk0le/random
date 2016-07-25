#ifndef ENCODER_H_
#define ENCODER_H_

//#define ENCODER_GPIOR_STORAGE // save couple of cpu cycles on loading/storing step counter to GPIOR instead of SRAM 

//#define ENCODER_32BIT_ACCUMULATOR
//#define ENCODER_REVERSE_DIRECTION // reverse counting direction

#define ENCODER_CHANNELB_PORT B // A,B,C,D ... port naming 
#define ENCODER_CHANNELB_PIN 0 // 1,2,3,4 ... pin naming

#define ENCODER_INTERRUPT_VECT INT1_vect

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

#ifdef ENCODER_32BIT_ACCUMULATOR
	#define enc_step_t int32_t
#else
	#define enc_step_t int16_t
#endif

	void encoder_init(void);

	enc_step_t EncoderReadDiff(void); // clears counter after reading
	enc_step_t EncoderRead(void);

#endif /* ENCODER_H_ */