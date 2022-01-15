/*!
 * \file pipetest.h
 * \brief pipeline testing templates
 *
 * \author jnk0le <jnk0le@hotmail.com>
 * \copyright CC0 License
 * \date 26 Jul 2021
 */

#ifndef PIPETEST_H_
#define PIPETEST_H_

#include <stdint.h>

#ifdef __cplusplus
	extern "C"{
#endif

	/*!
	 * \brief cortex-m0 dedicated template fore pipeline testing
	 * \return number of cycles the code took to run
	 */
	uint32_t CM0_pipetest_tmpl(void);

	/*!
	 * \brief cortex-m0+ dedicated template fore pipeline testing
	 * \return number of cycles the code took to run
	 */
	//uint32_t CM0p_pipetest_tmpl(void);

	/*!
	 * \brief cortex-m3 dedicated template fore pipeline testing
	 * \return number of cycles the code took to run
	 */
	uint32_t CM3_pipetest_tmpl(void);

	/*!
	 * \brief cortex-m4 dedicated template fore pipeline testing
	 * \return number of cycles the code took to run
	 */
	//uint32_t CM4_pipetest_tmpl(void);

	/*!
	 * \brief cortex-m7 dedicated template fore pipeline testing
	 * \return number of cycles the code took to run
	 */
	uint32_t CM7_pipetest_tmpl(void);


#ifdef __cplusplus
	}
#endif

#endif /* PIPETEST_H_ */
