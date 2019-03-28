/**************************************************************************/
/* Copyright (c) Samsung Electronics Co., Ltd. All rights reserved.       */
/*                                                                        */
/* -INSTRUCTIONS-                                                         */
/* THIS SOFTWARE IS A CONFIDENTIAL STUFFS AND PROPRIETARY OF SAMSUNG ELEC */
/* TRONICS CO.,LTD. SO YOU SHALL NOT DISCLOSE THIS SOFTWARE OTHER COMPANY */
/* OR PERSONS WITHOUT PERMISSION OF SAMSUNG AND SHALL USE THIS SOFTWARE   */
/* ONLY IN ACCORDANCE WITH THE LICENSE AGREEMENT OF SAMSUNG.              */
/* SAMSUNG MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY   */
/* OF THE SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED  */
/* TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR */
/* PURPOSE, OR NON-INFRINGEMENT. SAMSUNG SHALL NOT BE LIABLE FOR ANY      */
/* DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR        */
/* DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.                         */
/**************************************************************************/

#ifndef MB_CMD_RNG_H
#define MB_CMD_RNG_H

/*************** Include Files ********************************************/
#include "sss_common.h"

/*************** Assertions ***********************************************/

/*************** Definitions / Macros *************************************/
// ======================================
// Mailbox Command
// ======================================
#define FUNC_RNG        (0x00016101)
#define FUNC_TRNG       (0x00026101)
#define FUNC_RNG_KAT    (0x00A06101)

/*************** New Data Types (Basic Data Types)  ***********************/

/*************** New Data Types *******************************************/

/*************** Constants ************************************************/

/*************** Variable declarations ************************************/

/*************** Functions  ***********************************************/

unsigned int mb_generate_random_number(stOCTET_STRING *pstRandom, unsigned int request_byte_len);
unsigned int mb_generate_raw_random(stOCTET_STRING *pstRandom, unsigned int request_byte_len);
unsigned int mb_KAT_RNG(void);


#endif  /* MB_CMD_RNG_H */

