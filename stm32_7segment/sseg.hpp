/*!
 * \file sseg.hpp
 * \version 0.7.0
 * \brief
 *
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

			if constexpr(DPpos < 16) // only 16 IOs per port
				tmp |= DP << 16;

			if constexpr(invert_polarity)
				tmp = (tmp >> 16) | (tmp << 16);

			return tmp;
		}

		static inline constexpr uint32_t mapToBsrrDotOn()
		{
			static_assert(DPpos < 16, "dot position is not specified");

			uint32_t tmp = DP;

			if constexpr(invert_polarity)
				tmp = (tmp >> 16) | (tmp << 16);

			return tmp;
		}

		//dot off pattern???

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

			// always turn off dot ??
			if constexpr(DPpos < 16) // only 16 IOs per port
				tmp |= DP << 16;

			if constexpr(invert_polarity)
				tmp = (tmp >> 16) | (tmp << 16);

			return tmp;
		}

		//map to asci character/glyph????

	private:
		static constexpr int A = 1 << Apos;
		static constexpr int B = 1 << Bpos;
		static constexpr int C = 1 << Cpos;
		static constexpr int D = 1 << Dpos;
		static constexpr int E = 1 << Epos;
		static constexpr int F = 1 << Fpos;
		static constexpr int G = 1 << Gpos;
		static constexpr int DP = 1 << DPpos;
	};

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

	template<class seg_config, class common_config, bool use_dot>
	class Display
	{
	public:

		Display()
		{
			for(uint32_t i = 0; i < common_config::getColumnAmount(); i++)
				disp_cache[i] = seg_config::mapToBsrrPatternOff(); // clear all to avoid ruining gpio state
		}

		void defaultIrqHandler()
		{
			common_config::turnOff(cnt);

			if(cnt == 0)
				cnt = common_config::getColumnAmount(); // 1 more than effective indexing

			cnt--;

			// put delay here in case of ghosting

			seg_config::getSegGPIO()->BSRR = disp_cache[cnt];

			common_config::turnOn(cnt);
		}

		//doesn't handle negative
		template<bool mask_leading_zero = true, typename T>
		void writeNumber(T num)
		{
			static_assert(std::is_integral_v<T>, "integers only");

			for(uint32_t i = common_config::getColumnAmount(); i>0; i--) {
				if(i < common_config::getColumnAmount() && num == 0 && mask_leading_zero)
					disp_cache[i-1] = seg_config::mapToBsrrPatternOff();
				else
					disp_cache[i-1] = segment_lut_decimal[num % 10];

				num /= 10; // mod/div emit large builtin function without hw div
				// can do fixed point arith + hw mul to get it more efficient
			}

		}

		template<bool mask_leading_zero = false, typename T>
		void writeHex(T num)
		{
			static_assert(std::is_integral_v<T>, "integers only");

			for(uint32_t i = common_config::getColumnAmount(); i>0; i--) {
				if(i < common_config::getColumnAmount() && num == 0 && mask_leading_zero)
					disp_cache[i-1] = seg_config::mapToBsrrPatternOff();
				else
					disp_cache[i-1] = segment_lut_hex[num & 0xf];

				num >>= 4;
			}

		}

		void insertDot(uint32_t n)
		{
			if(n < common_config::getColumnAmount()) // sanitize input
				disp_cache[n] |= seg_config::mapToBsrrDotOn();
		}


		//write asci

		//glyphs ????

		//single update ????

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
