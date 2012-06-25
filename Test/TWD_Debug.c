/*
 * TWD_Debug.c
 *
 * Copyright(c) 1998 - 2009 Texas Instruments. All rights reserved.      
 * All rights reserved.                                                  
 *                                                                       
 * Redistribution and use in source and binary forms, with or without    
 * modification, are permitted provided that the following conditions    
 * are met:                                                              
 *                                                                       
 *  * Redistributions of source code must retain the above copyright     
 *    notice, this list of conditions and the following disclaimer.      
 *  * Redistributions in binary form must reproduce the above copyright  
 *    notice, this list of conditions and the following disclaimer in    
 *    the documentation and/or other materials provided with the         
 *    distribution.                                                      
 *  * Neither the name Texas Instruments nor the names of its            
 *    contributors may be used to endorse or promote products derived    
 *    from this software without specific prior written permission.      
 *                                                                       
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS   
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT     
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT  
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT      
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT   
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "tidef.h"
#include "TWDriver.h"
#include "rxXfer_api.h"
#include "report.h"
#include "osApi.h"
#include "eventMbox_api.h"
#include "CmdQueue_api.h"
#include "CmdMBox_api.h"
#include "FwEvent_api.h"
#include "fwDebug_api.h"
#include "CmdBld.h"


/* Phony address used by host access */
#define BB_REGISTER_ADDR_BASE              0x820000

/* Used for TWD Debug Tests */
typedef enum
{
/*
 * General
 */
/*	0x00	*/	TWD_PRINT_HELP,
/*	0x01	*/	TWD_PRINT_SYS_INFO,
/*	0x02	*/	TWD_SET_GENERIC_ADDR,
/*	0x03	*/	TWD_READ_MEM,
/*	0x04	*/	TWD_WRITE_MEM,

/*	0x05	*/	TWD_PRINT_ISTART,

/*	0x06	*/	TWD_PRINT_MBOX_QUEUE_INFO,
/*	0x07	*/	TWD_PRINT_MBOX_PRINT_CMD,
/*	0x08	*/	TWD_MAILBOX_HISTORY_PRINT,

/*	0x09	*/	TWD_MAC_REG,
/*	0x0A	*/	TWD_SET_ARM_CLOCK,
/*	0x0B	*/	TWD_SET_MAC_CLOCK,

/*
 * Rx
 */
/*	0x0C	*/	TWD_PRINT_RX_INFO,
/*	0x0D	*/	TWD_CLEAR_RX_INFO,

/*
 * Acx
 */
/*	0x0E	*/	TWD_PRINT_ACX_MAP,
/*	0x0F	*/	TWD_PRINT_ACX_STAT,

/*
 * General Debug
 */
/*	0x10	*/	TWD_PWR_SV_DBG,

/*	0x11	*/	TWD_PRINT_LIST_REGS_THROG_MBOX,
/*	0x12	*/	TWD_PRINT_LIST_MEM_THROG_MBOX,
/*	0x13	*/	TWD_SET_MAC_REGISTER_THROG_MBOX,
/*	0x14	*/	TWD_SET_PHY_REGISTER_THROG_MBOX,
/*	0x15	*/	TWD_SET_MEMORY_THROG_MBOX,

/*
 * Recover Debug
 */
/*	0x16	*/	TWD_CHECK_HW,
/*	0x17	*/	TWD_PRINT_HW_STATUS,

/*
 * Event MailBox
 */
/*	0x18	*/	TWD_PRINT_EVENT_MBOX_INFO,
/*	0x19	*/	TWD_PRINT_EVENT_MBOX_MASK,
/*	0x1A	*/	TWD_PRINT_EVENT_MBOX_UNMASK,

/*
 * Other
 */
TWD_PRINT_FW_EVENT_INFO,
TWD_PRINT_TW_IF_INFO,
TWD_PRINT_MBOX_INFO,
TWD_FORCE_TEMPLATES_RATES,

				TWD_DEBUG_TEST_MAX = 0xFF	/* mast be last!!! */

} TWD_DebugTest_e;


/* Used for Memory or Registers reading/writing*/
typedef enum
{
   TNETW_INTERNAL_RAM,
   TNETW_MAC_REGISTERS,
   TNETW_PHY_REGISTERS

} readWrite_MemoryType_e;
    
static void TWD_PrintMemRegsCB (TI_HANDLE hTWD, TI_UINT32 cmdCbStatus)
{
    TTwd    *pTWD = (TTwd *)hTWD;    
    TI_UINT32   result;

    if (cmdCbStatus != TI_OK)
    {
        return;
    }

    result = (((TI_UINT32)pTWD->tPrintRegsBuf.addr) & 0xFFFF0000);
    
    switch (result)
    {
        case REGISTERS_BASE:
                break;

        case BB_REGISTER_ADDR_BASE:
                break;

        default: /* Memory*/
                break;
    }
}


static void TWD_PrintMemRegs (TI_HANDLE hTWD, TI_UINT32 address, TI_UINT32 len, readWrite_MemoryType_e memType)
{
    TTwd  *pTWD = (TTwd *)hTWD;    
    ReadWriteCommand_t  AcxCmd_ReadMemory; 
    ReadWriteCommand_t* pCmd = &AcxCmd_ReadMemory;

    os_memoryZero (pTWD->hOs, (void *)pCmd, sizeof (*pCmd));
    
    switch (memType)
    {
        case TNETW_INTERNAL_RAM:
            pCmd->addr = (TI_UINT32)ENDIAN_HANDLE_LONG (address);
            pCmd->size = ENDIAN_HANDLE_LONG (len);
            break;
             
        case TNETW_MAC_REGISTERS:
            pCmd->addr = (TI_UINT32)ENDIAN_HANDLE_LONG (((address&0xFFFF) | REGISTERS_BASE));
            pCmd->size = 4;
            break;
    
        case TNETW_PHY_REGISTERS:
            pCmd->addr = (TI_UINT32)ENDIAN_HANDLE_LONG (((address&0xFFFF) | BB_REGISTER_ADDR_BASE));
            pCmd->size = 4;
            break;
    
        default:
            return;
    }
    
    os_memoryZero (pTWD->hOs, (void *)&pTWD->tPrintRegsBuf, sizeof(pTWD->tPrintRegsBuf));
    
    cmdQueue_SendCommand (pTWD->hCmdQueue, 
                      CMD_READ_MEMORY, 
                      (char *)pCmd, 
                      sizeof(*pCmd), 
                      (void *)TWD_PrintMemRegsCB, 
                      hTWD, 
                      &pTWD->tPrintRegsBuf);
}

static TI_STATUS TWD_PrintMemoryMapCb (TI_HANDLE hTWD, TI_STATUS status, void *pData)
{
    return TI_OK;
}


/****************************************************************************
 *                      TWD_PrintMemoryMap ()
 ****************************************************************************
 * DESCRIPTION: Print some of the MemoryMap information element fields
 *
 * INPUTS:
 *          HwMboxConfig_T* pHwMboxConfig pointer to the acx mailbox
 *
 * OUTPUT:  None
 *
 * RETURNS: None
 ****************************************************************************/
static void TWD_PrintMemoryMap (TI_HANDLE hTWD)
{
    TTwd *pTWD = (TTwd *)hTWD;    

    TWD_ItrMemoryMap (pTWD, &pTWD->MemMap, (void *)TWD_PrintMemoryMapCb, hTWD);
}


/****************************************************************************
 *                      TWD_StatisticsReadCB ()
 ****************************************************************************
 * DESCRIPTION: Interrogate Statistics from the wlan hardware
 *
 * INPUTS:  None
 *
 * OUTPUT:  None
 *
 * RETURNS: TI_OK or TI_NOK
 ****************************************************************************/
static TI_STATUS TWD_StatisticsReadCB (TI_HANDLE hTWD, TI_UINT16 MboxStatus, ACXStatistics_t* pElem)
{
    if (MboxStatus != TI_OK)
    {
        return TI_NOK;
    }

    /* 
     *  Handle FW statistics endianess
     *  ==============================
     */

    /* Ring */
    pElem->ringStat.numOfTxProcs       = ENDIAN_HANDLE_LONG(pElem->ringStat.numOfTxProcs);
    pElem->ringStat.numOfPreparedDescs = ENDIAN_HANDLE_LONG(pElem->ringStat.numOfPreparedDescs);
    pElem->ringStat.numOfTxXfr         = ENDIAN_HANDLE_LONG(pElem->ringStat.numOfTxXfr);
    pElem->ringStat.numOfTxDma         = ENDIAN_HANDLE_LONG(pElem->ringStat.numOfTxDma);
    pElem->ringStat.numOfTxCmplt       = ENDIAN_HANDLE_LONG(pElem->ringStat.numOfTxCmplt);
    pElem->ringStat.numOfRxProcs       = ENDIAN_HANDLE_LONG(pElem->ringStat.numOfRxProcs);
    pElem->ringStat.numOfRxData        = ENDIAN_HANDLE_LONG(pElem->ringStat.numOfRxData);

    /* Debug */
    pElem->debug.debug1                = ENDIAN_HANDLE_LONG(pElem->debug.debug1);
    pElem->debug.debug2                = ENDIAN_HANDLE_LONG(pElem->debug.debug2);
    pElem->debug.debug3                = ENDIAN_HANDLE_LONG(pElem->debug.debug3);
    pElem->debug.debug4                = ENDIAN_HANDLE_LONG(pElem->debug.debug4);
    pElem->debug.debug5                = ENDIAN_HANDLE_LONG(pElem->debug.debug5);
    pElem->debug.debug6                = ENDIAN_HANDLE_LONG(pElem->debug.debug6);

    /* Isr */
    pElem->isr.IRQs                    = ENDIAN_HANDLE_LONG(pElem->isr.IRQs);

    /* Rx */
    pElem->rx.RxOutOfMem               = ENDIAN_HANDLE_LONG(pElem->rx.RxOutOfMem            );
    pElem->rx.RxHdrOverflow            = ENDIAN_HANDLE_LONG(pElem->rx.RxHdrOverflow         );
    pElem->rx.RxHWStuck                = ENDIAN_HANDLE_LONG(pElem->rx.RxHWStuck             );
    pElem->rx.RxDroppedFrame           = ENDIAN_HANDLE_LONG(pElem->rx.RxDroppedFrame        );
    pElem->rx.RxCompleteDroppedFrame   = ENDIAN_HANDLE_LONG(pElem->rx.RxCompleteDroppedFrame);
    pElem->rx.RxAllocFrame             = ENDIAN_HANDLE_LONG(pElem->rx.RxAllocFrame          );
    pElem->rx.RxDoneQueue              = ENDIAN_HANDLE_LONG(pElem->rx.RxDoneQueue           );
    pElem->rx.RxDone                   = ENDIAN_HANDLE_LONG(pElem->rx.RxDone                );
    pElem->rx.RxDefrag                 = ENDIAN_HANDLE_LONG(pElem->rx.RxDefrag              );
    pElem->rx.RxDefragEnd              = ENDIAN_HANDLE_LONG(pElem->rx.RxDefragEnd           );
    pElem->rx.RxMic                    = ENDIAN_HANDLE_LONG(pElem->rx.RxMic                 );
    pElem->rx.RxMicEnd                 = ENDIAN_HANDLE_LONG(pElem->rx.RxMicEnd              );
    pElem->rx.RxXfr                    = ENDIAN_HANDLE_LONG(pElem->rx.RxXfr                 );
    pElem->rx.RxXfrEnd                 = ENDIAN_HANDLE_LONG(pElem->rx.RxXfrEnd              );
    pElem->rx.RxCmplt                  = ENDIAN_HANDLE_LONG(pElem->rx.RxCmplt               );
    pElem->rx.RxPreCmplt               = ENDIAN_HANDLE_LONG(pElem->rx.RxPreCmplt            );
    pElem->rx.RxCmpltTask              = ENDIAN_HANDLE_LONG(pElem->rx.RxCmpltTask           );
    pElem->rx.RxPhyHdr                 = ENDIAN_HANDLE_LONG(pElem->rx.RxPhyHdr              );
    pElem->rx.RxTimeout                = ENDIAN_HANDLE_LONG(pElem->rx.RxTimeout             );

    /* Tx */
    pElem->tx.numOfTxTemplatePrepared  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxTemplatePrepared);
	pElem->tx.numOfTxDataPrepared  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxDataPrepared);
	pElem->tx.numOfTxTemplateProgrammed = ENDIAN_HANDLE_LONG(pElem->tx.numOfTxTemplateProgrammed);
	pElem->tx.numOfTxDataProgrammed  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxDataProgrammed);
	pElem->tx.numOfTxBurstProgrammed  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxBurstProgrammed);
	pElem->tx.numOfTxStarts  			= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxStarts);
	pElem->tx.numOfTxImmResp  			= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxImmResp);
	pElem->tx.numOfTxStartTempaltes  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxStartTempaltes);
	pElem->tx.numOfTxStartIntTemplate  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxStartIntTemplate);
	pElem->tx.numOfTxStartFwGen  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxStartFwGen);
	pElem->tx.numOfTxStartData  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxStartData);
	pElem->tx.numOfTxStartNullFrame  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxStartNullFrame);
	pElem->tx.numOfTxExch  				= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxExch);
	pElem->tx.numOfTxRetryTemplate  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxRetryTemplate);
	pElem->tx.numOfTxRetryData  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxRetryData);
	pElem->tx.numOfTxExchPending  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxExchPending);
	pElem->tx.numOfTxExchExpiry  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxExchExpiry);
	pElem->tx.numOfTxExchMismatch  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxExchMismatch);
	pElem->tx.numOfTxDoneTemplate  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxDoneTemplate);
	pElem->tx.numOfTxDoneData  			= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxDoneData);
	pElem->tx.numOfTxDoneIntTemplate  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxDoneIntTemplate);
	pElem->tx.numOfTxPreXfr  			= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxPreXfr);
	pElem->tx.numOfTxXfr  				= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxXfr);
	pElem->tx.numOfTxXfrOutOfMem  		= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxXfrOutOfMem);
	pElem->tx.numOfTxDmaProgrammed  	= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxDmaProgrammed);
	pElem->tx.numOfTxDmaDone  			= ENDIAN_HANDLE_LONG(pElem->tx.numOfTxDmaDone);

    /* Dma */
    pElem->dma.RxDMAErrors             = ENDIAN_HANDLE_LONG(pElem->dma.RxDMAErrors);
    pElem->dma.TxDMAErrors             = ENDIAN_HANDLE_LONG(pElem->dma.TxDMAErrors);

    /* Wep */
    pElem->wep.WepAddrKeyCount         = ENDIAN_HANDLE_LONG(pElem->wep.WepAddrKeyCount);
    pElem->wep.WepDefaultKeyCount      = ENDIAN_HANDLE_LONG(pElem->wep.WepDefaultKeyCount);
    pElem->wep.WepKeyNotFound          = ENDIAN_HANDLE_LONG(pElem->wep.WepKeyNotFound);
    pElem->wep.WepDecryptFail          = ENDIAN_HANDLE_LONG(pElem->wep.WepDecryptFail);
    
    /* AES */
    pElem->aes.AesEncryptFail          = ENDIAN_HANDLE_LONG(pElem->aes.AesEncryptFail);     
    pElem->aes.AesDecryptFail          = ENDIAN_HANDLE_LONG(pElem->aes.AesDecryptFail);     
    pElem->aes.AesEncryptPackets       = ENDIAN_HANDLE_LONG(pElem->aes.AesEncryptPackets);  
    pElem->aes.AesDecryptPackets       = ENDIAN_HANDLE_LONG(pElem->aes.AesDecryptPackets);  
    pElem->aes.AesEncryptInterrupt     = ENDIAN_HANDLE_LONG(pElem->aes.AesEncryptInterrupt);
    pElem->aes.AesDecryptInterrupt     = ENDIAN_HANDLE_LONG(pElem->aes.AesDecryptInterrupt);

    /* Events */
    pElem->event.calibration           = ENDIAN_HANDLE_LONG(pElem->event.calibration);
    pElem->event.rxMismatch            = ENDIAN_HANDLE_LONG(pElem->event.rxMismatch); 
    pElem->event.rxMemEmpty            = ENDIAN_HANDLE_LONG(pElem->event.rxMemEmpty); 

    /* PS */
    pElem->pwr.MissingBcnsCnt          	= ENDIAN_HANDLE_LONG(pElem->pwr.MissingBcnsCnt);
    pElem->pwr.RcvdBeaconsCnt          	= ENDIAN_HANDLE_LONG(pElem->pwr.RcvdBeaconsCnt);
    pElem->pwr.ConnectionOutOfSync     	= ENDIAN_HANDLE_LONG(pElem->pwr.ConnectionOutOfSync);
    pElem->pwr.ContMissBcnsSpread[0]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[0]);
    pElem->pwr.ContMissBcnsSpread[1]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[1]);
    pElem->pwr.ContMissBcnsSpread[2]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[2]);
    pElem->pwr.ContMissBcnsSpread[3]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[3]);
    pElem->pwr.ContMissBcnsSpread[4]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[4]);
    pElem->pwr.ContMissBcnsSpread[5]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[5]);
    pElem->pwr.ContMissBcnsSpread[6]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[6]);
    pElem->pwr.ContMissBcnsSpread[7]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[7]);
    pElem->pwr.ContMissBcnsSpread[8]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[8]);
    pElem->pwr.ContMissBcnsSpread[9]   	= ENDIAN_HANDLE_LONG(pElem->pwr.ContMissBcnsSpread[9]);

    pElem->ps.psPollTimeOuts           	= ENDIAN_HANDLE_LONG(pElem->ps.psPollTimeOuts);
    pElem->ps.upsdTimeOuts             	= ENDIAN_HANDLE_LONG(pElem->ps.upsdTimeOuts);
    pElem->ps.upsdMaxAPturn            	= ENDIAN_HANDLE_LONG(pElem->ps.upsdMaxAPturn); 
    pElem->ps.psPollMaxAPturn          	= ENDIAN_HANDLE_LONG(pElem->ps.psPollMaxAPturn);
    pElem->ps.psPollUtilization        	= ENDIAN_HANDLE_LONG(pElem->ps.psPollUtilization);
    pElem->ps.upsdUtilization          	= ENDIAN_HANDLE_LONG(pElem->ps.upsdUtilization);

	pElem->rxFilter.arpFilter		   	= ENDIAN_HANDLE_LONG(pElem->rxFilter.arpFilter);
	pElem->rxFilter.beaconFilter	   	= ENDIAN_HANDLE_LONG(pElem->rxFilter.beaconFilter);
	pElem->rxFilter.dataFilter		   	= ENDIAN_HANDLE_LONG(pElem->rxFilter.dataFilter);
	pElem->rxFilter.dupFilter		   	= ENDIAN_HANDLE_LONG(pElem->rxFilter.dupFilter);
	pElem->rxFilter.MCFilter		   	= ENDIAN_HANDLE_LONG(pElem->rxFilter.MCFilter);
	pElem->rxFilter.ibssFilter		   	= ENDIAN_HANDLE_LONG(pElem->rxFilter.ibssFilter);

	pElem->radioCal.calStateFail	   	= ENDIAN_HANDLE_LONG(pElem->radioCal.calStateFail);
	pElem->radioCal.initCalTotal	   	= ENDIAN_HANDLE_LONG(pElem->radioCal.initCalTotal);
	pElem->radioCal.initRadioBandsFail 	= ENDIAN_HANDLE_LONG(pElem->radioCal.initRadioBandsFail);
	pElem->radioCal.initRxIqMmFail		= ENDIAN_HANDLE_LONG(pElem->radioCal.initRxIqMmFail);
	pElem->radioCal.initSetParams		= ENDIAN_HANDLE_LONG(pElem->radioCal.initSetParams);
	pElem->radioCal.initTxClpcFail		= ENDIAN_HANDLE_LONG(pElem->radioCal.initTxClpcFail);
	pElem->radioCal.tuneCalTotal		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneCalTotal);
	pElem->radioCal.tuneDrpwChanTune	= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwChanTune);
	pElem->radioCal.tuneDrpwLnaTank		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwLnaTank);
	pElem->radioCal.tuneDrpwPdBufFail	= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwPdBufFail);
	pElem->radioCal.tuneDrpwRTrimFail	= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwRTrimFail);
	pElem->radioCal.tuneDrpwRxDac		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwRxDac);
	pElem->radioCal.tuneDrpwRxIf2Gain	= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwRxIf2Gain);
	pElem->radioCal.tuneDrpwRxTxLpf		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwRxTxLpf);
	pElem->radioCal.tuneDrpwTaCal		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwTaCal);
	pElem->radioCal.tuneDrpwTxMixFreqFail = ENDIAN_HANDLE_LONG(pElem->radioCal.tuneDrpwTxMixFreqFail);
	pElem->radioCal.tuneRxAnaDcFail		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneRxAnaDcFail);
	pElem->radioCal.tuneRxIqMmFail		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneRxIqMmFail);
	pElem->radioCal.tuneTxClpcFail		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneTxClpcFail);
	pElem->radioCal.tuneTxIqMmFail		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneTxIqMmFail);
	pElem->radioCal.tuneTxLOLeakFail	= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneTxLOLeakFail);
	pElem->radioCal.tuneTxPdetFail		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneTxPdetFail);
	pElem->radioCal.tuneTxPPAFail		= ENDIAN_HANDLE_LONG(pElem->radioCal.tuneTxPPAFail);


    /* 
     *  Print FW statistics 
     *  ===================
     */
    return TI_OK;  
}

TI_STATUS TWD_Debug (TI_HANDLE hTWD, TI_UINT32 funcType, void *pParam)
{
    TTwd *pTWD 					= (TTwd *)hTWD;    
    TI_UINT32 GenericVal;
	TFwDebugParams* pMemDebug	= (TFwDebugParams*)pParam;

    static TI_UINT32 GenericAddr;

	/* check paramemters validity */
	if (pMemDebug == NULL)
	{	
		return TI_NOK;
	}

    switch (funcType)
    {
	case TWD_PRINT_SYS_INFO:
		break;

	case TWD_SET_GENERIC_ADDR:
		/* check paramemters validity */
		if (pParam == NULL)
		{	
			return TI_NOK;
		}
		GenericAddr = *(TI_UINT32 *)pParam;
        break;

	case TWD_READ_MEM:		

		/* check paramemters validity */
		if (pMemDebug == NULL)
		{
			return TI_NOK;
		}
  
		/* validate length */
		*(TI_UINT32*)&pMemDebug->length = 4;		

		/* If Address in valid Memory area and there is enough space for Length to R/W */
		if (TWD_isValidMemoryAddr(hTWD, pMemDebug) == TI_TRUE)
		{		

			/* Init buf before reading */
			os_memorySet(pTWD->hOs, (void*)pMemDebug->UBuf.buf8, 0, 4);
			if ( TWD_readMem (hTWD, pMemDebug, NULL, NULL) != TI_OK )
			{
				return TI_NOK;
			}
		}

		else if (TWD_isValidRegAddr(hTWD, pMemDebug) == TI_TRUE)
		{
			/* Init buf before reading */
			*(TI_UINT32*)&pMemDebug->UBuf.buf32 = 0;

			if ( TWD_readMem (hTWD, pMemDebug, NULL, NULL) != TI_OK )
			{
				return TI_NOK;
			}

		}

		/* address Not in valid Area */ 
		else
		{	
			return TI_NOK;
		}

		/* print read memory */
		{
		}

		break;

	case TWD_WRITE_MEM:		

		/* check paramemters validity */
		if (pMemDebug == NULL)
		{			
			return TI_NOK;
		}

		/* validate length */
		*(TI_UINT32*)&pMemDebug->length = 4;		

		/* If Address in valid Memory area and there is enough space for Length to R/W */
		if (TWD_isValidMemoryAddr(hTWD, pMemDebug) == TI_TRUE)
		{
			return ( TWD_writeMem (hTWD, pMemDebug, NULL, NULL) );
		}
		
		else if (TWD_isValidRegAddr(hTWD, pMemDebug) == TI_TRUE)
		{		
			return ( TWD_writeMem (hTWD, pMemDebug, NULL, NULL) );
		}
		/* address Not in valid Area */ 

		else
		{
			return TI_NOK;
		}

		break;

         /*  HAL Control functions */

	case TWD_PRINT_MBOX_QUEUE_INFO:
		cmdQueue_Print (pTWD->hCmdQueue);         
        break;
            
	case TWD_PRINT_MBOX_PRINT_CMD:
		/* check paramemters validity */
		if (pParam == NULL)
		{			
			return TI_NOK;
		}
		cmdQueue_PrintHistory (pTWD->hCmdQueue, *(int *)pParam);            
        break;

	case TWD_PRINT_EVENT_MBOX_INFO:
		eventMbox_Print (pTWD->hEventMbox);         
        break;
        
	case TWD_PRINT_EVENT_MBOX_MASK:
		/* check paramemters validity */
		if (pParam == NULL)
		{		
			return TI_NOK;
		}
		if ( eventMbox_MaskEvent (pTWD->hEventMbox, *(int *)pParam, NULL, NULL) == TI_NOK )
		{
			return(TI_NOK);
		}
        break;
        
	case TWD_PRINT_EVENT_MBOX_UNMASK:
		/* check paramemters validity */
		if (pParam == NULL)
		{
			return TI_NOK;
		}
		if ( eventMbox_UnMaskEvent (pTWD->hEventMbox, *(int *)pParam, NULL, NULL) == TI_NOK )
		{	
			return(TI_NOK);
		}
        break;

	case TWD_PRINT_ISTART:
		{
		}
        break;

	case TWD_PRINT_LIST_REGS_THROG_MBOX:
		{
			int i;
            TI_UINT32 RegAddr;
    
			RegAddr = *(TI_UINT32 *)pParam;
    
			for (i = 0; i < 8; i++, RegAddr += 16)
            {
				TWD_PrintMemRegs (hTWD, RegAddr +  0, 4, TNETW_MAC_REGISTERS);
                TWD_PrintMemRegs (hTWD, RegAddr +  4, 4, TNETW_MAC_REGISTERS);
				TWD_PrintMemRegs (hTWD, RegAddr +  8, 4, TNETW_MAC_REGISTERS);
				TWD_PrintMemRegs (hTWD, RegAddr + 12, 4, TNETW_MAC_REGISTERS);
			}
		}
        break;
    
	case TWD_PRINT_LIST_MEM_THROG_MBOX:
		/* check paramemters validity */
		if (pParam == NULL)
		{			
			return TI_NOK;
		}
		TWD_PrintMemRegs (hTWD, *(TI_UINT32*)pParam, 256, TNETW_INTERNAL_RAM);
        break;
                  
	case TWD_SET_MAC_CLOCK:
		/* check paramemters validity */
		if (pParam == NULL)
		{		
			return TI_NOK;
		}

		GenericVal = *(TI_UINT32*)pParam;
        TWD_CfgMacClock (hTWD, GenericVal);
        break;

#if defined(TNETW1150)
	case TWD_SET_ARM_CLOCK:
		/* check paramemters validity */
		if (pParam == NULL)
		{			
			return TI_NOK;
		}

		GenericVal = *(TI_UINT32*)pParam;
        TWD_ArmClockSet (hTWD, GenericVal);
        break;
#endif

	/*
    * Rx functions 
    */
#ifdef TI_DBG
    case TWD_PRINT_RX_INFO:
		rxXfer_PrintStats (pTWD->hRxXfer);  
        break;

	case TWD_CLEAR_RX_INFO:
		rxXfer_ClearStats (pTWD->hRxXfer);  
        break;

#endif /* TI_DBG */

	/*
    * Acx functions 
    */
    case TWD_PRINT_ACX_MAP:
		TWD_PrintMemoryMap (hTWD);           
        break;
            
	case TWD_PRINT_ACX_STAT:
		TWD_ItrStatistics (hTWD, (void*)TWD_StatisticsReadCB, hTWD, (void *)&pTWD->acxStatistic);          
        break;
            			
	/*
	* General functions 
    */
    case TWD_PRINT_HELP:

        break;
       
	case TWD_PRINT_FW_EVENT_INFO:
        fwEvent_PrintStat(pTWD->hFwEvent);
        break;
	case TWD_PRINT_TW_IF_INFO:
        twIf_PrintQueues(pTWD->hTwIf);
        break;
	case TWD_PRINT_MBOX_INFO:
        cmdMbox_PrintInfo(pTWD->hCmdMbox);
        break;

    /*
    * Recovery functions 
    */       
	case TWD_CHECK_HW:
		{
			int Stt;

            Stt = TWD_CmdHealthCheck (hTWD);
		}
        break;

	case TWD_MAILBOX_HISTORY_PRINT:
#ifdef TI_DBG
		/* check paramemters validity */
		if (pParam == NULL)
		{		
			return TI_NOK;
		}

        cmdQueue_PrintHistory (pTWD->hCmdQueue, *(int *)pParam);
#endif
        break;

	case TWD_FORCE_TEMPLATES_RATES:
		if (pParam == NULL)
		{
			return TI_NOK;
		}
		cmdBld_DbgForceTemplatesRates (pTWD->hCmdBld, *(TI_UINT32 *)pParam);
        break;


	default:
        break;

	} /* switch (funcType) */

    return TI_OK;
} 

