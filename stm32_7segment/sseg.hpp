/*!
 * \file sseg.hpp
 * \version 0.9.2
 * \brief Driver for directly connected 7 segment displays
 *
 * Initialization of clocks, gpio dir, timer and interrupt handler has to be handled separately.
 *
 * \author Jan Oleksiewicz <jnk0le@hotmail.com>
 * \license SPDX-License-Identifier: MIT
 * \date 9 oct 2022
 */

#ifndef SSEG_HPP
#define SSEG_HPP

#include <stdint.h>
#include <stddef.h>
#include <array>
#include <type_traits>

#include <cmsis_device.h>

namespace jnk0le
{
namespace sseg
{

	/*!
	 * \brief maps pin mapping to 7segment patterns, shall be used to instantiate Display class
	 *
	 * Default polarity (ie. `invert_polarity == false`) is active high, suitable for e.g. common
	 * cathode displays.
	 *
	 * \tparam invert_polarity invert polarity to active low
	 * \tparam gpio_addr address of GPIO peripheral
	 * \tparam Apos pin position of A segment
	 * \tparam Bpos pin position of B segment
	 * \tparam Cpos pin position of C segment
	 * \tparam Dpos pin position of D segment
	 * \tparam Epos pin position of E segment
	 * \tparam Fpos pin position of F segment
	 * \tparam Gpos pin position of G segment
	 * \tparam DPpos pin position of DP segment, if omitted, then dot line is not used
	 */
	template <bool invert_polarity, uintptr_t gpio_addr, int Apos, int Bpos, int Cpos, int Dpos, int Epos, int Fpos, int Gpos, int DPpos = 17>
	class PinConfig
	{
	public:
		static inline constexpr GPIO_TypeDef* getSegGPIO() {
			return reinterpret_cast<GPIO_TypeDef*>(gpio_addr);
		}

		static inline constexpr uint32_t mapToBsrrPatternOff()
		{
			uint32_t tmp = (A | B | C | D | E | F | G) << 16;

			if constexpr(invert_polarity)
				tmp = (tmp >> 16) | (tmp << 16);

			// always turn off dot
			tmp |= dotOffPattern();

			return tmp;
		}

		static inline constexpr uint32_t insertDotPattern(uint32_t bsrr_pattern)
		{
			static_assert(DPpos < 16, "dot position is not specified");

			if constexpr(invert_polarity)
				bsrr_pattern &= ~DP; // clear BS bit, BR is set
			else
				bsrr_pattern |= DP;

			return bsrr_pattern;
		}

		static inline constexpr uint32_t mapToBsrrPatternMinus()
		{
			uint32_t tmp = G;
			tmp |= (A | B | C | D | E | F) << 16;

			if constexpr(invert_polarity)
				tmp = (tmp >> 16) | (tmp << 16);

			// always turn off dot
			tmp |= dotOffPattern();

			return tmp;
		}

		static inline constexpr uint32_t mapToBsrrPatternDigit(uint32_t i)
		{
			uint32_t tmp;

			switch(i)
			{
			case 0:
				tmp = A | B | C | D | E | F;
				tmp |= G << 16;
				break;
			case 1:
				tmp = B | C;
				tmp |= (A | D | E | F | G) << 16;
				break;
			case 2:
				tmp = A | B | D | E | G;
				tmp |= (C | F) << 16;
				break;
			case 3:
				tmp = A | B | C | D | G;
				tmp |= (E | F) << 16;
				break;
			case 4:
				tmp = B | C | F | G;
				tmp |= (A | D | E) << 16;
				break;
			case 5:
				tmp = A | C | D | F | G;
				tmp |= (B | E) << 16;
				break;
			case 6:
				tmp = A | C | D | E | F | G;
				tmp |= B << 16;
				break;
			case 7:
				tmp = A | B | C;
				tmp |= (D | E | F | G) << 16;
				break;
			case 8:
				tmp = A | B | C | D | E | F | G;
				break;
			case 9:
				tmp = A | B | C | D | F | G;
				tmp |= E << 16;
				break;
			case 10:
				tmp = A | B | C | E | F | G;
				tmp |= D << 16;
				break;
			case 11:
				tmp =  C | D | E | F | G;
				tmp |= (A | B) << 16;
				break;
			case 12:
				tmp = A | D | E | F;
				tmp |=  (B | C | G) << 16;
				break;
			case 13:
				tmp =  B | C | D | E | G;
				tmp |= (A | F) << 16;
				break;
			case 14:
				tmp = A | D | E | F | G;
				tmp |= (B | C) << 16;
				break;
			case 15:
				tmp = A | E | F | G;
				tmp |=  (B | C | D) << 16;
				break;
			default: // turn off all ??
				tmp = (A | B | C | D | E | F | G) << 16;
			}

			if constexpr(invert_polarity)
				tmp = (tmp >> 16) | (tmp << 16);

			// always turn off dot
			tmp |= dotOffPattern();

			return tmp;
		}

		//map to asci character/glyph????

	private:
		static inline constexpr uint32_t dotOffPattern()
		{
			uint32_t tmp;

			if constexpr(DPpos < 16) // only 16 IOs per port
			{
				if constexpr(invert_polarity) {
					// in BSRR the set function takes priority if both positions are set
					// use BR + BS combination so dot can be inserted by single masking operation
					tmp = DP | (DP << 16);
				} else {
					tmp = DP << 16;
				}
			} else {
				tmp = 0; //no dot
			}

			return tmp;
		}

		static constexpr int A = 1 << Apos;
		static constexpr int B = 1 << Bpos;
		static constexpr int C = 1 << Cpos;
		static constexpr int D = 1 << Dpos;
		static constexpr int E = 1 << Epos;
		static constexpr int F = 1 << Fpos;
		static constexpr int G = 1 << Gpos;
		static constexpr int DP = 1 << DPpos;
	};

	/*!
	 * \brief maps pin mappings to column drivers, shall be used to instantiate Display class
	 *
	 * Default polarity (`invert_polarity == false`) is active high, suitable for e.g. common
	 * cathode displays with inverting driver (e.g. mosfet or common emitter bjt).
	 *
	 * \tparam invert_polarity invert polarity to active low
	 * \tparam gpio_addr address of GPIO peripheral
	 * \tparam args pin positions for each column, starting from the most left
	 */
	template <bool invert_polarity, uintptr_t gpio_addr, uint32_t... args>
	class CommonConfig
	{
	public:
		static inline constexpr uint32_t getColumnAmount() {
			return (sizeof...(args));
		}

		static inline constexpr void turnOff([[maybe_unused]] uint32_t idx)
		{
			if constexpr(invert_polarity)
				reinterpret_cast<GPIO_TypeDef*>(gpio_addr)->BSRR = selectAllPinsMask();
			else
				reinterpret_cast<GPIO_TypeDef*>(gpio_addr)->BRR = selectAllPinsMask();
		}

		static inline constexpr void turnOn(uint32_t idx)
		{
			if constexpr(invert_polarity)
				reinterpret_cast<GPIO_TypeDef*>(gpio_addr)->BRR = static_cast<uint32_t>(column_pin_mask_lut[idx]);
			else
				reinterpret_cast<GPIO_TypeDef*>(gpio_addr)->BSRR = static_cast<uint32_t>(column_pin_mask_lut[idx]);
		}

	private:
		//0 is first instead of 1
		template<uint32_t Cpos, uint32_t... tail>
		[[gnu::always_inline]] static inline constexpr uint32_t parseArgs(uint32_t finish_cond)
		{
			if(finish_cond > getColumnAmount()) {
				__builtin_unreachable();
				return 0;
			} else if(finish_cond == 0) {
				return Cpos;
			} else {
				if constexpr((sizeof...(tail) > 0)) // avoids extra boilerplate of empty tail overload
					return parseArgs<tail...>(finish_cond-1);
				else {
					__builtin_unreachable();
					return 0;
				}
			}
		}

		static inline constexpr uint32_t selectAllPinsMask()
		{
			uint32_t tmp = 0;

			for(uint32_t i = 0; i<getColumnAmount(); i++)
				tmp |= (1 << parseArgs<args...>(i));

			return tmp;
		}

	#if __ARM_ARCH_7M__ || __ARM_ARCH_7EM__ || __riscv_zba
		typedef uint16_t pinlut_T;
	#else
		typedef uint32_t pinlut_T;
	#endif

		static inline constexpr std::array<pinlut_T, getColumnAmount()> column_pin_mask_lut = []()
		{
			std::array<pinlut_T, getColumnAmount()> arr;
			for (uint32_t i = 0; i < arr.size(); i++) {
				arr[i] = (1 << parseArgs<args...>(i));
			}

			return arr;
		}();

		static_assert((getColumnAmount() > 0), "wrong number of parameters - provide pin position for each column");
	};

	/*!
	 * \brief maps pin mappings to column drivers, shall be used to instantiate Display class
	 *
	 * Default polarity (`invert_polarity == false`) is active high, suitable for e.g. common
	 * cathode displays with inverting driver (e.g. mosfet or common emitter bjt).
	 *
	 * It is a bit less efficient than CommonConfig.
	 *
	 * \tparam invert_polarity invert polarity to active low
	 * \tparam args paired address of GPIO peripheral and pin positions for
	 * each column, starting from the most left
	 */
	template <bool invert_polarity, /*uintptr_t gpio_addr, int Cpos,*/ uint32_t... args>
	class CommonConfigScattered
	{
	public:
		static inline constexpr uint32_t getColumnAmount() {
			return (sizeof...(args))/2;
		}

		static inline constexpr void turnOff(uint32_t idx) {
			if constexpr(invert_polarity)
				reinterpret_cast<GPIO_TypeDef*>(column_gpio_addr_lut[idx])->BSRR = static_cast<uint32_t>(column_pin_mask_lut[idx]);
			else
				reinterpret_cast<GPIO_TypeDef*>(column_gpio_addr_lut[idx])->BRR = static_cast<uint32_t>(column_pin_mask_lut[idx]);
		}

		static inline constexpr void turnOn(uint32_t idx)
		{
			if constexpr(invert_polarity)
				reinterpret_cast<GPIO_TypeDef*>(column_gpio_addr_lut[idx])->BRR = static_cast<uint32_t>(column_pin_mask_lut[idx]);
			else
				reinterpret_cast<GPIO_TypeDef*>(column_gpio_addr_lut[idx])->BSRR = static_cast<uint32_t>(column_pin_mask_lut[idx]);
		}

	private:

		//0 is first instead of 1
		template<bool parse_addr, uint32_t gpio_addr, uint32_t Cpos, uint32_t... tail>
		[[gnu::always_inline]] static inline constexpr uint32_t parseArgs(uint32_t finish_cond)
		{
			if(finish_cond > getColumnAmount()) {
				__builtin_unreachable();
				return 0;
			} else if(finish_cond == 0) {
				if constexpr(parse_addr)
					return gpio_addr;
				else
					return Cpos;
			} else {
				if constexpr((sizeof...(tail) > 0)) // avoids extra boilerplate of empty tail overload
					return parseArgs<parse_addr, tail...>(finish_cond-1);
				else {
					__builtin_unreachable();
					return 0;
				}
			}
		}

		static inline constexpr std::array<uint32_t, getColumnAmount()> column_gpio_addr_lut = []()
		{
			std::array<uint32_t, getColumnAmount()> arr;
			for (uint32_t i = 0; i < arr.size(); i++) {
				arr[i] = parseArgs<true, args...>(i);
			}

			return arr;
		}();

	#if __ARM_ARCH_7M__ || __ARM_ARCH_7EM__ || __riscv_zba
		typedef uint16_t pinlut_T;
	#else
		typedef uint32_t pinlut_T;
	#endif

		static inline constexpr std::array<pinlut_T, getColumnAmount()> column_pin_mask_lut = []()
		{
			std::array<pinlut_T, getColumnAmount()> arr;
			for (uint32_t i = 0; i < arr.size(); i++) {
				arr[i] = (1 << parseArgs<false, args...>(i));
			}

			return arr;
		}();

		static_assert((getColumnAmount()%2 == 0), "wrong number of parameters - provide GPIO_BASE and pin position for each column");
		static_assert((getColumnAmount() > 0), "wrong number of parameters - provide GPIO_BASE and pin position for each column");
	};

	/*!
	 * \brief main class handling 7segment displays
	 *
	 * Display buffer is not atomically cached, no dimming
	 *
	 * \tparam seg_config Pin mapping template
	 * \tparam common_config common mapping template
	 */
	template<class seg_config, class common_config>
	class Display
	{
	public:
		Display()
		{
			for(uint32_t i = 0; i < common_config::getColumnAmount(); i++)
				disp_cache[i] = seg_config::mapToBsrrPatternOff(); // clear all to avoid ruining gpio state
		}

		/*!
		 * \brief column scan routine, must be called periodically
		 *
		 * It is recommended to use evenly spaced timer interrupt to keep
		 * all digits at the same brightness.
		 */
		void defaultIrqHandler()
		{
			common_config::turnOff(cnt);

			// cnt is not volatile but gcc emits some garbage otherwise
			// It must span no more, otherwise increases register pressure in llvm and gcc
			uint32_t cnt_tmp = cnt;

			if(cnt_tmp == 0)
				cnt_tmp = common_config::getColumnAmount(); // 1 more than effective indexing

			cnt_tmp--;

			cnt = cnt_tmp;

			// put delay here in case of ghosting

			seg_config::getSegGPIO()->BSRR = disp_cache[cnt];

			common_config::turnOn(cnt);
		}

		/*!
		 * \brief prints unsigned number on display
		 *
		 * template specialization for unsigned numbers
		 *
		 * \tparam T type of the number, deduced automatically
		 * \param num number to print on display
		 * \param right_offset offset from the right side of display, columns to the right are not blanked
		 */
		template<typename T>
		typename std::enable_if<std::is_unsigned<T>::value, void>::type writeNumber(T num, uint16_t right_offset = 0)
		{
			static_assert(std::is_integral_v<T>, "integers only");

			int start_idx = common_config::getColumnAmount() - right_offset;

			for(int i = start_idx; i>0; i--)
			{
				if(num == 0 && i != start_idx)
					disp_cache[i-1] = seg_config::mapToBsrrPatternOff();
				else
					disp_cache[i-1] = segment_lut_decimal[num % 10];

				num /= 10; // mod/div emit large builtin function without hw div
				// can do fixed point arith + hw mul to get it more efficient
			}

		}

		/*!
		 * \brief prints signed number on display
		 *
		 * template specialization for signed numbers
		 *
		 * \tparam T type of the number, deduced automatically
		 * \param num number to print on display
		 * \param right_offset offset from the right side of display, columns to the right are not blanked
		 */
		template<typename T>
		typename std::enable_if<std::is_signed<T>::value, void>::type writeNumber(T num, uint16_t right_offset = 0)
		{
			static_assert(std::is_integral_v<T>, "integers only");

			bool negative = false;

			if(num < 0) {
				num = -num;
				negative = true;
			}

			typename std::make_unsigned<T>::type unum = num; // unsigned division is smaller

			int start_idx = common_config::getColumnAmount() - right_offset;

			for(int i = start_idx; i>0; i--)
			{
				if(unum == 0 && i != start_idx)
				{
					if (negative) {
						disp_cache[i-1] = seg_config::mapToBsrrPatternMinus();
						negative = false;
					} else
						disp_cache[i-1] = seg_config::mapToBsrrPatternOff();
				}
				else
					disp_cache[i-1] = segment_lut_decimal[unum % 10];

				unum /= 10; // mod/div emit large builtin function without hw div
				// can do fixed point arith + hw mul to get it more efficient
			}
		}


		/*!
		 * \brief prints hexadecimal number
		 *
		 * \tparam print_leading_zero
		 * \tparam T type of the number, deduced automatically
		 * \param num number to print on display
		 */
		template<bool print_leading_zero = true, typename T>
		void writeHex(T num)
		{
			static_assert(std::is_integral_v<T>, "integers only");

			for(uint32_t i = common_config::getColumnAmount(); i>0; i--) {
				if(i < common_config::getColumnAmount() && num == 0 && !print_leading_zero)
					disp_cache[i-1] = seg_config::mapToBsrrPatternOff();
				else
					disp_cache[i-1] = segment_lut_hex[num & 0xf];

				num >>= 4;
			}

		}

		/*!
		 * \brief inserts dot into column
		 *
		 * Doesn't blank out currently displayed digit.
		 *
		 * \param n position to insert (from left, indexed from zero)
		 */
		void insertDot(uint32_t n)
		{
			if(n < common_config::getColumnAmount()) // sanitize input
				disp_cache[n] = seg_config::insertDotPattern(disp_cache[n]);
		}

		/*!
		 * \brief sets a single digit on display
		 *
		 * \tparam T type of the number, deduced automatically
		 * \param n position to set (from left, indexed from zero)
		 * \param num digit to set
		 */
		template<typename T>
		void setDigit(uint32_t n, T num)
		{
			// instead of sanitizing negative numbers
			typename std::make_unsigned<T>::type unum = num;

			if(n < common_config::getColumnAmount()) // sanitize input
			{
				if(unum < 10) // sanitize again...
					disp_cache[n] = segment_lut_decimal[unum];
				else
					disp_cache[n] = seg_config::mapToBsrrPatternOff();
			}
		}

		//write asci

		//glyphs ???

	private:
		volatile uint32_t disp_cache[common_config::getColumnAmount()];
		uint32_t cnt = 0; // should be accessible with short offset as the cache is usually small

		static inline constexpr std::array<uint32_t, 10> segment_lut_decimal = []()
		{
			std::array<uint32_t, 10> arr;
			for (uint32_t i = 0; i < arr.size(); i++) {
				arr[i] = seg_config::mapToBsrrPatternDigit(i);
			}

			return arr;
		}();

		//hex and decimal is not usually mixed, keep as separate
		static inline constexpr std::array<uint32_t, 16> segment_lut_hex = []()
		{
			std::array<uint32_t, 16> arr;
			for (uint32_t i = 0; i < arr.size(); i++) {
				arr[i] = seg_config::mapToBsrrPatternDigit(i);
			}

			return arr;
		}();

	};

} // namespace
} // namespace

#endif /* SSEG_HPP */
