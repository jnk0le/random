/*!
 * \file sseg_test.cpp
 * \brief
 *
 * \author Jan Oleksiewicz <jnk0le@hotmail.com>
 * \license SPDX-License-Identifier: MIT
 * \date 9 oct 2022
 */

#include <stdio.h>
#include <stm32f0xx.h>

#include "sseg.hpp"
#include "sseg_test.hpp"

//common cathode

// common - active low (drive by emitter follower pnp)
// CA1 - PF1
// CA2 - PF0
// CA3 - PA9
// CA4 - PB1

// segment - active high (drive directly)
// seg A - PA4
// seg B - PA2
// seg C - PA6
// seg D - PA5
// seg E - PA1
// seg F - PA3
// seg G - PA7
// seg DP - PA0


using segment_config = jnk0le::sseg::PinConfig<false, GPIOA_BASE, 4, 2, 6, 5, 1, 3, 7, 0>;
using common_config = jnk0le::sseg::CommonConfigScattered<true, GPIOF_BASE,1, GPIOF_BASE,0, GPIOA_BASE,9, GPIOB_BASE,1>;
//using common_simple = jnk0le::sseg::CommonConfig<true, GPIOB_BASE, 2, 3, 4, 5>;

jnk0le::sseg::Display<segment_config, common_config, true> displ;

void sseg_init_all()
{
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN | RCC_AHBENR_GPIOFEN;

	//common
	GPIOB->MODER |= (0b01 << GPIO_MODER_MODER1_Pos);
	GPIOF->MODER |= (0b01 << GPIO_MODER_MODER0_Pos) | (0b01 << GPIO_MODER_MODER1_Pos);
	GPIOA->MODER |= (0b01 << GPIO_MODER_MODER9_Pos);

	//initialize to disabled state (common scattered will blink first digit on all columns on startup otherwise)
	GPIOB->BSRR = GPIO_BSRR_BS_1;
	GPIOF->BSRR = GPIO_BSRR_BS_0 | GPIO_BSRR_BS_1;
	GPIOA->BSRR = GPIO_BSRR_BS_9;

	//segment
	GPIOA->MODER |= (0b01 << GPIO_MODER_MODER4_Pos)
					|(0b01 << GPIO_MODER_MODER2_Pos)
					|(0b01 << GPIO_MODER_MODER6_Pos)
					|(0b01 << GPIO_MODER_MODER5_Pos)
					|(0b01 << GPIO_MODER_MODER1_Pos)
					|(0b01 << GPIO_MODER_MODER3_Pos)
					|(0b01 << GPIO_MODER_MODER7_Pos)
					|(0b01 << GPIO_MODER_MODER0_Pos);

	GPIOA->OSPEEDR |= (0b11 << GPIO_OSPEEDR_OSPEEDR4_Pos)
					|(0b11 << GPIO_OSPEEDR_OSPEEDR2_Pos)
					|(0b11 << GPIO_OSPEEDR_OSPEEDR6_Pos)
					|(0b11 << GPIO_OSPEEDR_OSPEEDR5_Pos)
					|(0b11 << GPIO_OSPEEDR_OSPEEDR1_Pos)
					|(0b11 << GPIO_OSPEEDR_OSPEEDR3_Pos)
					|(0b11 << GPIO_OSPEEDR_OSPEEDR7_Pos)
					|(0b11 << GPIO_OSPEEDR_OSPEEDR0_Pos);

	//48 mhz
	RCC->APB2ENR |= RCC_APB2ENR_TIM16EN;

	TIM16->DIER = TIM_DIER_UIE;

	TIM16->ARR = 47999; // 1khz isr rate

	TIM16->CR1 = TIM_CR1_CEN;

	NVIC_EnableIRQ(TIM16_IRQn);
}

void sseg_test_loop()
{
	static int n = -150;
	displ.writeNumber(n++, 0);

	displ.insertDot(3);

	for(volatile int i = 0; i<500000; i++);

}

extern "C" void TIM16_IRQHandler()
{
	TIM16->SR = 0; //bits are rc_w0, write ~UIF (1 to others)

	displ.defaultIrqHandler();
}

void init_clocks() // 48mhz hsi
{
	FLASH->ACR |= FLASH_ACR_PRFTBE_Msk | (FLASH_ACR_LATENCY_Msk & 0b001); // 1ws

	RCC->CFGR = RCC_CFGR_PLLMUL12; // 0 on reset, hsi/2, hsi selected

	RCC->CR |= RCC_CR_PLLON;

	while(!(RCC->CR & RCC_CR_PLLRDY));

	RCC->CFGR |= RCC_CFGR_SW_PLL;
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

int main()
{
	init_clocks();

	sseg_init_all();

	while(1)
	{
		sseg_test_loop();
	}
}
