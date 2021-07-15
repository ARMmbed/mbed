/*************************************************************************************************/
/*!
 *  \file
 *
 *  \brief  SMP main module.
 *
 *  Copyright (c) 2010-2019 Arm Ltd. All Rights Reserved.
 *
 *  Copyright (c) 2019-2021 Packetcraft, Inc.
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/*************************************************************************************************/
#ifndef SMP_MAIN_H
#define SMP_MAIN_H

#include "sec_api.h"
#include "wsf_timer.h"
#include "l2c_api.h"
#include "dm_api.h"
#include "smp_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
  Macros
**************************************************************************************************/

/* State machine table constants */
#define SMP_SM_POS_EVENT          0       /* Column position for event */
#define SMP_SM_POS_NEXT_STATE     1       /* Column position for next state */
#define SMP_SM_POS_ACTION         2       /* Column position for action */
#define SMP_SM_NUM_COLS           3       /* Number of columns in state table */
#define SMP_STATE_TBL_COMMON_MAX  5       /* Number of entries in common state table */

/* Position of parameters in pairing request/response */
#define SMP_IO_POS                1
#define SMP_OOB_POS               2
#define SMP_AUTHREQ_POS           3
#define SMP_MAXKEY_POS            4
#define SMP_IKEYDIST_POS          5
#define SMP_RKEYDIST_POS          6

/* Position of parameters in public key message */
#define SMP_PUB_KEY_X_POS         1
#define SMP_PUB_KEY_Y_POS         33

/* LESC Authorization Types */
#define SMP_AUTH_TYPE_UNKNOWN     0
#define SMP_AUTH_TYPE_JUST_WORKS  1
#define SMP_AUTH_TYPE_OOB         2
#define SMP_AUTH_TYPE_PASSKEY     3
#define SMP_AUTH_TYPE_NUM_COMP    4

/*! smpi_sm state machine states */
enum
{
  SMPI_SM_ST_IDLE,                        /*!< Idle */
  SMPI_SM_ST_PAIR_RSP,                    /*!< Wait for pairing response */
  SMPI_SM_ST_PIN,                         /*!< Wait for PIN */
  SMPI_SM_ST_CNF_CALC_1,                  /*!< Wait for confirm calc 1 */
  SMPI_SM_ST_CNF_CALC_2,                  /*!< Wait for confirm calc 2 */
  SMPI_SM_ST_PAIR_CNF,                    /*!< Wait for pairing confirm */
  SMPI_SM_ST_PAIR_RAND,                   /*!< Wait for pairing random */
  SMPI_SM_ST_CNF_VER_CALC_1,              /*!< Wait for confirm verify calc 1 */
  SMPI_SM_ST_CNF_VER_CALC_2,              /*!< Wait for confirm verify calc 2 */
  SMPI_SM_ST_STK_CALC,                    /*!< Wait for STK calc */
  SMPI_SM_ST_ENCRYPT,                     /*!< Wait for link encryption */
  SMPI_SM_ST_KEY_DIST,                    /*!< Key distribution */
  SMPI_SM_ST_ATTEMPTS,                    /*!< Repeated attempts */
  SMPI_SM_ST_RSP_TO                       /*!< Response timeout state */
};

/*! smpr_sm state machine states */
enum
{
  SMPR_SM_ST_IDLE,                        /*!< Idle */
  SMPR_SM_ST_API_PAIR_REQ,                /*!< Wait for API pairing request */
  SMPR_SM_ST_API_PAIR_RSP,                /*!< Wait for API pairing response */
  SMPR_SM_ST_PIN_PAIR_1,                  /*!< Wait for PIN or pairing confirm 1 */
  SMPR_SM_ST_PIN_PAIR_2,                  /*!< Wait for PIN or pairing confirm 2 */
  SMPR_SM_ST_CNF_CALC_1,                  /*!< Wait for confirm calc 1 */
  SMPR_SM_ST_CNF_CALC_2,                  /*!< Wait for confirm calc 2 */
  SMPR_SM_ST_PAIR_RAND,                   /*!< Wait for pairing random */
  SMPR_SM_ST_CNF_VER_CALC_1,              /*!< Wait for confirm verify calc 1 */
  SMPR_SM_ST_CNF_VER_CALC_2,              /*!< Wait for confirm verify calc 2 */
  SMPR_SM_ST_STK_CALC,                    /*!< Wait for STK calc */
  SMPR_SM_ST_ENCRYPT,                     /*!< Wait for link encryption */
  SMPR_SM_ST_KEY_DIST,                    /*!< Key distribution */
  SMPR_SM_ST_ATTEMPTS,                    /*!< Repeated attempts */
  SMPR_SM_ST_RSP_TO                       /*!< Response timeout state */
};

/*! smpi_sc_sm state machine states */
enum
{
  SMPI_SC_SM_ST_IDLE,                        /*!< Idle */
  SMPI_SC_SM_ST_PAIR_RSP,                    /*!< Wait for pairing response */
  SMPI_SC_SM_ST_MODE_SELECT,                 /*!< Select the security mode (LESC or Legacy) */
  SMPI_SC_SM_ST_LESC_PIN,                    /*!< Wait for PIN in LESC */
  SMPI_SC_SM_ST_PUB_KEY,                     /*!< Wait for public key from peer */
  SMPI_SC_SM_ST_AUTH_SELECT,                 /*!< Select the type of LESC pairing (Just Works, Passkey, or OOB) */

  SMPI_SC_SM_ST_JWNC_WAIT_CNF,               /*!< Wait for confirm in Just Works/Numeric Comparison Pairing */
  SMPI_SC_SM_ST_JWNC_RAND,                   /*!< Wait for rand in Just Works/Numeric Comparison Pairing */
  SMPI_SC_SM_ST_JWNC_CHECK_1,                /*!< Calculate confirm value in Just Works/Numeric Comparison Pairing */
  SMPI_SC_SM_ST_JWNC_CHECK_2,                /*!< Calculate user validate value in Just Works/Numeric Comparison Pairing */
  SMPI_SC_SM_ST_JWNC_WAIT_USER,              /* Wait for user to validate the confirm value */

  SMPI_SC_SM_ST_PK_KEYPRESS,                 /*!< Process a keypress command in passkey pairing */
  SMPI_SC_SM_ST_PK_CALC,                     /*!< Calculate the confirm in passkey pairing */
  SMPI_SC_SM_ST_PK_CNF,                      /*!< Wait for confirm in passkey pairing */
  SMPI_SC_SM_ST_PK_RAND,                     /*!< Wait for rand in passkey pairing */
  SMPI_SC_SM_ST_PK_CHECK,                    /*!< Check the conform value in passkey pairing */
  SMPI_SC_SM_ST_PK_REPEAT,                   /*!< Repeat or complete the passkey pairing */

  SMPI_SC_SM_ST_OOB_SEND_RAND,               /*!< Send the rand in OOB pairing */
  SMPI_SC_SM_ST_OOB_WAIT_RAND,               /*!< Wait for a rand in OOB pairing */

  SMPI_SC_SM_ST_CALC_DHKEY,                  /*!< Calculate the DHKEY shared secret */
  SMPI_SC_SM_ST_CALC_F5_TKEY,                /*!< Calculate the DHKEY T Key */
  SMPI_SC_SM_ST_CALC_F5_MACKEY,              /*!< Calculate the DHKEY MAC Key */
  SMPI_SC_SM_ST_CALC_F5_LTK,                 /*!< Calculate the DHKEY LTK Key */
  SMPI_SC_SM_ST_CALC_F6_EA,                  /*!< Calculate the DHKEY Ea Key */
  SMPI_SC_SM_ST_CALC_F6_EB,                  /*!< Calculate the DHKEY Eb Key */
  SMPI_SC_SM_ST_VERIFY_DH_CHECK,             /*!< Verify the DHKEY check value */

  SMPI_SC_SM_ST_LEGACY_PIN,                  /*!< Wait for PIN */
  SMPI_SC_SM_ST_CNF_CALC_1,                  /*!< Wait for confirm calc 1 */
  SMPI_SC_SM_ST_CNF_CALC_2,                  /*!< Wait for confirm calc 2 */
  SMPI_SC_SM_ST_PAIR_CNF,                    /*!< Wait for pairing confirm */
  SMPI_SC_SM_ST_PAIR_RAND,                   /*!< Wait for pairing random */
  SMPI_SC_SM_ST_CNF_VER_CALC_1,              /*!< Wait for confirm verify calc 1 */
  SMPI_SC_SM_ST_CNF_VER_CALC_2,              /*!< Wait for confirm verify calc 2 */
  SMPI_SC_SM_ST_STK_CALC,                    /*!< Wait for STK calc */
  SMPI_SC_SM_ST_ENCRYPT,                     /*!< Wait for link encryption */
  SMPI_SC_SM_ST_KEY_DIST,                    /*!< Key distribution */
  SMPI_SC_SM_ST_ATTEMPTS,                    /*!< Repeated attempts */
  SMPI_SC_SM_ST_RSP_TO                       /*!< Response timeout state */
};

/*! smpr_sc_sm state machine states */
enum
{
  SMPR_SC_SM_ST_IDLE,                         /*!< Idle */
  SMPR_SC_SM_ST_API_PAIR_REQ,                 /*!< Wait for API pairing request */
  SMPR_SC_SM_ST_API_PAIR_RSP,                 /*!< Wait for API pairing response */
  SMPR_SC_SM_ST_MODE_SELECT,                  /*!< Select the security mode (LESC or Legacy) */
  SMPR_SC_SM_ST_PUB_KEY,                      /*!< Wait for public key from peer */
  SMPR_SC_SM_ST_LESC_PIN,                     /*!< Wait for pin in LESC */
  SMPR_SC_SM_ST_AUTH_SELECT,                  /*!< Select the type of LESC pairing (Just Works, Passkey, or OOB) */

  SMPR_SC_SM_ST_JWNC_SETUP,                   /*!< Prepare for Just Works/Numeric Comparison Pairing */
  SMPR_SC_SM_ST_JWNC_WAIT_RAND,               /*!< Wait for Rand in Just Works/Numeric Comparison Pairing */
  SMPR_SC_SM_ST_JWNC_CALC_G2,                 /*!< Calculate the user validate value in Just Works/Numeric Comparison Pairing */
  SMPR_SC_SM_ST_JWNC_WAIT_USER,               /*!< Wait for user to validate the confirm value */
  SMPR_SC_SM_ST_JWNC_WAIT_USER_DH_CHECK_RCVD, /*!< Wait for user to validate the confirm value with a received DH Key Check */

  SMPR_SC_SM_ST_PK_KEYPRESS,                  /*!< Process a keypress command in passkey pairing */
  SMPR_SC_SM_ST_PK_WAIT_AUTH,                 /*!< The confirm was received before the auth from the API, wait for the auth */
  SMPR_SC_SM_ST_PK_WAIT_CNF,                  /*!< Wait for the confirm command in passkey pairing */
  SMPR_SC_SM_ST_PK_CALC,                      /*!< Calculate the confirm in passkey pairing */
  SMPR_SC_SM_ST_PK_RAND,                      /*!< Send the rand command in passkey pairing */
  SMPR_SC_SM_ST_PK_CHECK,                     /*!< Check the confirm in passkey pairing */
  SMPR_SC_SM_ST_PK_REPEAT,                    /*!< Repeat or complete the passkey pairing */

  SMPR_SC_SM_ST_OOB_SEND_RAND,                /*!< Send the rand in OOB pairing */
  SMPR_SC_SM_ST_OOB_WAIT_RAND,                /*!< Wait for a rand in OOB pairing */

  SMPR_SC_SM_ST_WAIT_DH_CHECK,                /*!< Wait for the DH Check command */
  SMPR_SC_SM_ST_CALC_DHKEY,                   /*!< Calculate the DHKEY shared secret */
  SMPR_SC_SM_ST_CALC_F5_TKEY,                 /*!< Calculate the DHKEY T Key */
  SMPR_SC_SM_ST_CALC_F5_MACKEY,               /*!< Calculate the DHKEY MAC Key */
  SMPR_SC_SM_ST_CALC_F5_LTK,                  /*!< Calculate the DHKEY LTK Key */
  SMPR_SC_SM_ST_CALC_F6_EA,                   /*!< Calculate the DHKEY Ea Key */
  SMPR_SC_SM_ST_CALC_F6_EB,                   /*!< Calculate the DHKEY Eb Key */

  SMPR_SC_SM_ST_PIN_PAIR_1,                   /*!< Wait for PIN or pairing confirm 1 */
  SMPR_SC_SM_ST_PIN_PAIR_2,                   /*!< Wait for PIN or pairing confirm 2 */
  SMPR_SC_SM_ST_CNF_CALC_1,                   /*!< Wait for confirm calc 1 */
  SMPR_SC_SM_ST_CNF_CALC_2,                   /*!< Wait for confirm calc 2 */
  SMPR_SC_SM_ST_PAIR_RAND,                    /*!< Wait for pairing random */
  SMPR_SC_SM_ST_CNF_VER_CALC_1,               /*!< Wait for confirm verify calc 1 */
  SMPR_SC_SM_ST_CNF_VER_CALC_2,               /*!< Wait for confirm verify calc 2 */
  SMPR_SC_SM_ST_STK_CALC,                     /*!< Wait for STK calc */
  SMPR_SC_SM_ST_ENCRYPT,                      /*!< Wait for link encryption */
  SMPR_SC_SM_ST_KEY_DIST,                     /*!< Key distribution */
  SMPR_SC_SM_ST_ATTEMPTS,                     /*!< Repeated attempts */
  SMPR_SC_SM_ST_RSP_TO                        /*!< Response timeout state */
};

/**************************************************************************************************
  Data Types
**************************************************************************************************/

/* Data type for state machine table entry */
typedef uint8_t smpTblEntry_t[SMP_SM_NUM_COLS];

/* Pairing request/response parameters */
typedef uint8_t smpPair_t[SMP_PAIR_REQ_LEN];

/* Temporary storage structure for calculations and other data.
 * Note that keyInd overlaps b1 through b3 so these will be overwritten
 * when key exchange occurs.
 *
 * Notes on usage from start of pairing to encryption with STK:
 * buf.b1:  PIN or OOB data
 * buf.b2:  RAND from peer
 * buf.b3:  pairing cnf received from peer, then STK (if responder)
 * buf.b4:  RAND used in pairing calc
 *
 */
typedef union
{
  struct
  {
    uint8_t         b1[SMP_KEY_LEN];
    uint8_t         b2[SMP_KEY_LEN];
    uint8_t         b3[SMP_KEY_LEN];
    uint8_t         b4[SMP_KEY_LEN];
  } buf;
  dmSecKeyIndEvt_t  keyInd;
} smpScratch_t;

/* SMP message handling function type */
typedef void (*smpMsgHandler_t)(wsfMsgHdr_t *pMsg);

/* SMP connection callback type */
typedef void (*smpConnCback_t)(dmEvt_t *pDmEvt);

/* SMP data message type */
typedef struct
{
  wsfMsgHdr_t      hdr;                           /* Header structure */
  uint8_t          *pPacket;                      /* Pointer to buffer containing L2CAP packet */
} smpDataMsg_t;

/* Union of event handler data types */
typedef union
{
  wsfMsgHdr_t      hdr;                           /* Header structure */
  smpDmMsg_t       dm;                            /* Union SMP DM message data types */
  secAes_t         aes;                           /* AES Security callback parameters structure */
  smpDataMsg_t     data;                          /* SMP data message */
} smpMsg_t;

/* LE LTK Calc Scratch buffer */
typedef struct
{
  uint8_t          mac[SEC_CMAC_KEY_LEN];         /* MAC Key */
  uint8_t          ltk_t[SEC_CMAC_KEY_LEN];       /* LTK Key or T value of MAC/LTK calculation */
} smpScLtk_t;

/* ECC DH Key */
typedef struct
{
  uint8_t          pubKeyX[SMP_PUB_KEY_LEN];      /* Public Key X */
  uint8_t          pubKeyY[SMP_PUB_KEY_LEN];      /* Public Key Y */
} smpScPubKey_t;

/* LESC Scratch Buffers */
typedef struct
{
  uint8_t         Na_Ea[SMP_RAND_LEN];            /* Initiator Na or Ea */
  uint8_t         Nb_Eb[SMP_RAND_LEN];            /* Responder Nb or Eb */
  uint8_t         Ra[SMP_RAND_LEN];               /* Initiator Ra */
  uint8_t         Rb[SMP_RAND_LEN];               /* Responder Nb */
  uint8_t         PeerCb[SMP_RAND_LEN];           /* Peer Responder Confirm */
  uint8_t         LocalCb[SMP_RAND_LEN];          /* Local Confirm */
  uint8_t         PeerCa_Ea[SMP_RAND_LEN];        /* Peer Responder Confirm or DH Key Check */
} smpScScratch_t;

/* SMP LESC Control Block */
typedef struct
{
  uint8_t         lescEnabled;                    /* TRUE when LE Secure Connection pairing in use */
  uint8_t         authType;                       /* Type of authentication (Just Works, Numeric Comparison, Passkey, or OOB) */
  uint8_t         kpNotify;                       /* TRUE when Keypress notification in use */
  uint8_t         pkPos;                          /* Current passkey bit position */
  bool_t          display;                        /* Passkey display setting */
  smpScPubKey_t   *pPeerPublicKey;                /* Peer device's ECC Key */
  smpScPubKey_t   *pLocalPublicKey;               /* Local device's public ECC Key */
  uint8_t         *pPrivateKey;                   /* Local device's private ECC Key */
  smpScScratch_t  *pScratch;                      /* Scratch Buffer */
  smpScLtk_t      *pLtk;                          /* LTK calculation control */
} smpScCcb_t;

/* SMP connection control block */
typedef struct
{
  wsfTimer_t      rspTimer;                       /* Response timer */
  wsfTimer_t      waitTimer;                      /* Wait interval timer after max attempts */
  smpPair_t       pairReq;                        /* Pair request parameters */
  smpPair_t       pairRsp;                        /* Pair response parameters */
  smpScratch_t    *pScr;                          /* Pointer to scratchpad buffer */
  uint8_t         *pQueued;                       /* Pointer to queued packet buffer */
  uint16_t        handle;                         /* Connection handle */
  bool_t          initiator;                      /* TRUE if device is initiator */
  bool_t          secReq;                         /* TRUE if security request sent or received */
  bool_t          flowDisabled;                   /* TRUE if data flow disabled */
  dmConnId_t      connId;                         /* DM connection ID */
  uint8_t         state;                          /* State machine state */
  uint8_t         nextCmdCode;                    /* Command code of next expected packet */
  uint8_t         auth;                           /* Resulting authentication and bonding flags */
  uint8_t         token;                          /* AES transaction token */
  uint8_t         attempts;                       /* Failed pairing attempts */
  uint8_t         lastSentKey;                    /* Command code of last sent key */
  bool_t          keyReady;                       /* Encryption key is ready */
  smpScCcb_t      *pScCcb;                        /* LE Secure Connection control blocks */
} smpCcb_t;

/* State machine action function type */
typedef void (*smpAct_t)(smpCcb_t *pCcb, smpMsg_t *pMsg);

/* SMP process pairing function */
typedef bool_t smpProcPairing_t(smpCcb_t *pCcb, uint8_t *pOob, uint8_t *pDisplay);

/* SMP process pairing function */
typedef void smpProcAuthReq_t(smpCcb_t *pCcb, uint8_t oob, uint8_t display);

/* State machine interface type */
typedef struct
{
  smpTblEntry_t const * const *pStateTbl;   /* Pointer to state table */
  smpAct_t              const *pActionTbl;  /* Pointer to action table */
  smpTblEntry_t         const *pCommonTbl;  /* Pointer to common action table */
} smpSmIf_t;

/* SMP main control block */
typedef struct
{
  smpCcb_t          ccb[DM_CONN_MAX];     /* Legacy Connection control blocks */
  smpSmIf_t const   *pSlave;              /* Slave state machine interface */
  smpSmIf_t const   *pMaster;             /* Master state machine interface */
  wsfHandlerId_t    handlerId;            /* WSF handler ID */
  smpProcPairing_t  *procPairing;         /* Pointer to process pairing function */
  smpProcAuthReq_t  *procAuthReq;         /* Pointer to process auth request function */
  bool_t            lescSupported;        /* TRUE if LE Secure Connections is supported */
} smpCb_t;

/**************************************************************************************************
  Global Variables
**************************************************************************************************/

/* SMP packet length table */
extern const uint8_t smpPktLenTbl[];

/* Control block */
extern smpCb_t smpCb;

/**************************************************************************************************
  Function Declarations
**************************************************************************************************/

/* utility functions from smp main */
smpCcb_t *smpCcbByHandle(uint16_t handle);
smpCcb_t *smpCcbByConnId(dmConnId_t connId);
void smpCalcC1Part1(smpCcb_t *pCcb, uint8_t *pKey, uint8_t *pRand);
void smpCalcC1Part2(smpCcb_t *pCcb, uint8_t *pKey, uint8_t *pPart1);
void smpCalcS1(smpCcb_t *pCcb, uint8_t *pKey, uint8_t *pRand1, uint8_t *pRand2);
void smpGenerateLtk(smpCcb_t *pCcb);
void smpSendPkt(smpCcb_t *pCcb, uint8_t *pPkt);
bool_t smpStateIdle(smpCcb_t *pCcb);
void *smpMsgAlloc(uint16_t len);
uint8_t smpGetScSecLevel(smpCcb_t *pCcb);

/* action functions and utility functions from smp act */
void smpStartRspTimer(smpCcb_t *pCcb);
void smpActNone(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpCleanup(smpCcb_t *pCcb);
void smpActCleanup(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpSendPairingFailed(smpCcb_t *pCcb, uint8_t reason);
void smpActPairingFailed(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpActSecReqTimeout(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpActPairingCancel(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpActStorePin(smpCcb_t *pCcb, smpMsg_t *pMsg);
bool_t smpProcPairing(smpCcb_t *pCcb, uint8_t *pOob, uint8_t *pDisplay);
void smpAuthReq(smpCcb_t *pCcb, uint8_t oob, uint8_t display);
void smpActPairCnfCalc1(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpActPairCnfCalc2(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpActSendPairCnf(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpActPairCnfVerCalc1(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpActPairCnfVerCalc2(smpCcb_t *pCcb, smpMsg_t *pMsg);
bool_t smpSendKey(smpCcb_t *pCcb, uint8_t keyDist);
bool_t smpProcRcvKey(smpCcb_t *pCcb, dmSecKeyIndEvt_t *pKeyInd, uint8_t *pBuf, uint8_t keyDist);
void smpActMaxAttempts(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpActAttemptRcvd(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpActCheckAttempts(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpActNotifyDmAttemptsFailure(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpActNotifyDmRspToFailure(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpActPairingCmpl(smpCcb_t *pCcb, smpMsg_t *pMsg);
void smpSmExecute(smpCcb_t *pCcb, smpMsg_t *pMsg);

/* diagnostic functions */
uint8_t *smpEventStr(uint8_t eventId);
uint8_t *smpStateStr(uint8_t state);

/* SMP DB functions */
uint32_t SmpDbMaxAttemptReached(dmConnId_t connId);
void SmpDbPairingFailed(dmConnId_t connId);
uint32_t SmpDbGetPairingDisabledTime(dmConnId_t connId);
void SmpDbService(void);
uint8_t SmpDbGetFailureCount(dmConnId_t connId);
void SmpDbSetFailureCount(dmConnId_t connId, uint8_t count);

#ifdef __cplusplus
};
#endif

#endif /* SMP_MAIN_H */
