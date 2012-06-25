/*
 * CmdQueue.c
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


/** \file  CmdQueue.c
 *  \brief Handle the wlan command queue
 *
 *  \see CmdQueue.h, CmdQueue_api.h, CmdMBox.c
 */


#define __FILE_ID__  FILE_ID_97
#include "tidef.h"
#include "osApi.h"
#include "report.h"
#include "TwIf.h"
#include "public_commands.h"
#include "CmdQueue_api.h"
#include "CmdMBox_api.h"
#include "CmdQueue.h"

/*****************************************************************************
 **         Internal functions prototypes                                   **
 *****************************************************************************/

static TI_STATUS    cmdQueue_SM (TI_HANDLE hCmdQueue, ECmdQueueSmEvents event);
static TI_STATUS    cmdQueue_Push (TI_HANDLE  hCmdQueue, 
                                   Command_e  cmdType,
                                   TI_UINT8   *pParamsBuf, 
                                   TI_UINT32  uParamsLen,
                                   void       *fCb, 
                                   TI_HANDLE  hCb, 
                                   void       *pCb);
#ifdef TI_DBG
static void         cmdQueue_PrintQueue(TCmdQueue  *pCmdQueue);
#endif /* TI_DBG */




/*
 * \brief	Create the TCmdQueue object
 * 
 * \param  hOs  - OS module object handle
 * \return Handle to the created object
 * 
 * \par Description
 * Calling this function creates a CmdQueue object
 * 
 * \sa cmdQueue_Destroy
 */
TI_HANDLE cmdQueue_Create (TI_HANDLE hOs)
{
    TCmdQueue  *pCmdQueue;

    pCmdQueue = os_memoryAlloc (hOs, sizeof(TCmdQueue));
    if (pCmdQueue == NULL)
    {
        return NULL;
    }

    /* reset control module control block */
    os_memoryZero (hOs, pCmdQueue, sizeof(TCmdQueue));
    pCmdQueue->hOs = hOs;
    
    return pCmdQueue;   
}


/*
 * \brief	Destroys the cmdQueue object
 * 
 * \param  hCmdMbox  - The object to free
 * \return TI_OK
 * 
 * \par Description
 * Calling this function destroys the cmdQueue object
 * 
 * \sa cmdQueue_Create
 */
TI_STATUS cmdQueue_Destroy (TI_HANDLE hCmdQueue)
{
    TCmdQueue* pCmdQueue = (TCmdQueue*)hCmdQueue;

    /* Free context */
    os_memoryFree (pCmdQueue->hOs, pCmdQueue, sizeof(TCmdQueue));

	return TI_OK;
}


/*
 * \brief	Configure the CmdQueue object
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \param  hCmdMbox  - Handle to CmdMbox
 * \param  hReport - Handle to report module
 * \param  hTwIf  - Handle to TwIf
 * \param  hTimer  - Handle to os timer
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa
 */
TI_STATUS cmdQueue_Init   (TI_HANDLE hCmdQueue, 
                           TI_HANDLE hCmdMbox, 
                           TI_HANDLE hReport, 
                           TI_HANDLE hTwIf,
                           TI_HANDLE hTimer)
{
    TCmdQueue* pCmdQueue = (TCmdQueue*) hCmdQueue;

    pCmdQueue->head = 0;
    pCmdQueue->tail = 0;
    pCmdQueue->uNumberOfCommandInQueue = 0;
    pCmdQueue->uMaxNumberOfCommandInQueue = 0;
    pCmdQueue->state = CMDQUEUE_STATE_IDLE;

    pCmdQueue->fCmdCompleteCb = NULL;
    pCmdQueue->hCmdCompleteCb = NULL;
    pCmdQueue->fFailureCb = NULL;
    pCmdQueue->hFailureCb = NULL;

    pCmdQueue->hReport = hReport;
    pCmdQueue->hCmdMBox = hCmdMbox;
    pCmdQueue->hTwIf = hTwIf;
    pCmdQueue->bErrorFlag = TI_FALSE;
    pCmdQueue->bMboxEnabled = TI_FALSE;
    pCmdQueue->bAwake = TI_FALSE;

    /* Configure Command Mailbox */
    cmdMbox_Init (hCmdMbox, hReport, hTwIf,
                  hTimer, hCmdQueue,
                  cmdQueue_Error);

    /*
     * NOTE: don't set uNumberOfRecoveryNodes = 0; 
     *       its value is used by recovery process
     */

    return TI_OK;
}


/*
 * \brief	Configure the CmdQueue object
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \param  eCmdQueueEvent - The event that triggered the SM
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Handles the CmdQueue SM.
 * 
 * \sa cmdQueue_Push, cmdQueue_ResultReceived
 */
static TI_STATUS cmdQueue_SM (TI_HANDLE hCmdQueue, ECmdQueueSmEvents eCmdQueueEvent)
{   
    TCmdQueue     *pCmdQueue = (TCmdQueue*)hCmdQueue;
    TI_BOOL        bBreakWhile = TI_FALSE;
    TI_STATUS      rc = TI_OK, status;
    TCmdQueueNode *pHead;
    TI_UINT32      uReadLen, uWriteLen;

    while(!bBreakWhile)
    {
        switch (pCmdQueue->state)
        {
            case CMDQUEUE_STATE_IDLE:
                switch(eCmdQueueEvent)
                {
                    case CMDQUEUE_EVENT_RUN:
                        pCmdQueue->state = CMDQUEUE_STATE_WAIT_FOR_COMPLETION;

                        pHead = &pCmdQueue->aCmdQueue[pCmdQueue->head];
                
                        #ifdef TI_DBG
                            pCmdQueue->uCmdSendCounter++;
                        #endif 

                        /* 
                         * if bAwake is true, then we reached here because there were more commands 
                         * in the queue after sending a previous command.
                         * There is no need to send another awake command to TwIf. 
                         */
                        if (pCmdQueue->bAwake == TI_FALSE)
                        {
                            /* Keep the device awake for the entire Cmd transaction */
                            twIf_Awake(pCmdQueue->hTwIf);
                            pCmdQueue->bAwake = TI_TRUE;
                        }

                        if (pHead->cmdType == CMD_INTERROGATE)
                        {
                            uWriteLen = CMDQUEUE_INFO_ELEM_HEADER_LEN;
                            /* Will be updated by CmdMbox to count the status response */
                            uReadLen = pHead->uParamsLen;
                        }
                        else if(pHead->cmdType == CMD_TEST)
                        {
                            /* CMD_TEST has configure & interrogate abillities together */
                            uWriteLen = pHead->uParamsLen;
                            /* Will be updated by CmdMbox to count the status response */
                            uReadLen = pHead->uParamsLen;
                        }
                        else /* CMD_CONFIGURE or others */
                        {
                            uWriteLen = pHead->uParamsLen;
                            /* Will be updated by CmdMbox to count the status response */
                            uReadLen = 0;

                        }
                        /* send the command to TNET */
                        rc = cmdMbox_SendCommand (pCmdQueue->hCmdMBox, 
                                              pHead->cmdType, 
                                              pHead->aParamsBuf, 
                                              uWriteLen,
                                              uReadLen);

                        bBreakWhile = TI_TRUE;

                        /* end of CMDQUEUE_EVENT_RUN */
                        break;

                    default:
                        bBreakWhile = TI_TRUE;
                        rc =  TI_NOK;

                        break;
                }
                break;
            
            case CMDQUEUE_STATE_WAIT_FOR_COMPLETION:
                switch(eCmdQueueEvent)
                {
                    case CMDQUEUE_EVENT_RUN:
                        /* We are in the middle of other command transaction so there is nothing top be done */
                        bBreakWhile = TI_TRUE;
                        rc = TXN_STATUS_PENDING;
                        break;

                    case CMDQUEUE_EVENT_COMPLETE:
                        {
                            Command_e cmdType;
                            TI_UINT16        uParam;
                            void *fCb, *hCb, *pCb;
                            CommandStatus_e cmdStatus;

                            pHead = &pCmdQueue->aCmdQueue[pCmdQueue->head];
            
                            /* Keep callback parameters in temporary variables */
                            cmdType = pHead->cmdType;
                            uParam  = *(TI_UINT16 *)pHead->aParamsBuf;
                            fCb = pHead->fCb;
                            hCb = pHead->hCb;
                            pCb = pHead->pInterrogateBuf;
                            
                            /* 
                             * Delete the command from the queue before calling a callback 
                             * because there may be nested calls inside a callback
                             */
                            pCmdQueue->head ++;
                            if (pCmdQueue->head >= CMDQUEUE_QUEUE_DEPTH)
                                pCmdQueue->head = 0;                
                            pCmdQueue->uNumberOfCommandInQueue --;                
                
                        #ifdef TI_DBG
                            pCmdQueue->uCmdCompltCounter++;
                        #endif 

                            /* Read the latest command return status */
                            status = cmdMbox_GetStatus (pCmdQueue->hCmdMBox, &cmdStatus);
                            if (status != TI_OK)
                            {
                                if (cmdStatus == CMD_STATUS_REJECT_MEAS_SG_ACTIVE)
                                {
                                    /* return reject status in the callback */
                                    status = SG_REJECT_MEAS_SG_ACTIVE;
                                    pCmdQueue->bErrorFlag = TI_FALSE;
                                }
                                else
                                {
                                    pCmdQueue->bErrorFlag = TI_TRUE;
                                }
                            }
                            else
                            {
                                pCmdQueue->bErrorFlag = TI_FALSE;
                            }

                            /* If the command had a CB, then call it with the proper results buffer */
                            if (fCb)
                            {   
                                if (pCb)
                                {
                                    /* If pInterrogateBuf isn't NULL we need to copy the results */
                                    cmdMbox_GetCmdParams(pCmdQueue->hCmdMBox, pCb);
                                    /* Call the CB with the result buffer and the returned status */
                                    ((TCmdQueueInterrogateCb)fCb) (hCb, status, pCb); 
                                }
                                else
                                {
                                    /* Call the CB with only the returned status */
                                    ((TCmdQueueCb)fCb) (hCb, status);
                                }
                            }
                            else
                            {
                                /* Call the generic callback */
                                if (pCmdQueue->fCmdCompleteCb)
                                {
                                    pCmdQueue->fCmdCompleteCb (pCmdQueue->hCmdCompleteCb, cmdType, uParam, status);
                                }
                            }

                            /* Check if there are any more commands in queue */
                            if (pCmdQueue->uNumberOfCommandInQueue > 0)               
                            {
                                /* If queue isn't empty, send the next command */
                                pCmdQueue->state = CMDQUEUE_STATE_IDLE;
                                eCmdQueueEvent = CMDQUEUE_EVENT_RUN;
                            }
                            else	
                            {   
                                /* If queue is empty, we can permit TwIf to send sleep a command if neccesary */
                                twIf_Sleep(pCmdQueue->hTwIf);
                                pCmdQueue->bAwake = TI_FALSE;
                                pCmdQueue->state = CMDQUEUE_STATE_IDLE;

                                bBreakWhile = TI_TRUE;
                            }
                        /* end of CMDQUEUE_EVENT_COMPLETE */
                        }            
                        break;

                    default:
                        bBreakWhile = TI_TRUE;
                        rc =  TI_NOK;

                        break;

                /* end of switch event */
                } 
                break;
        /* end of switch state */
        }
    /* end of while */
    }

    return rc;
}


/*
 * \brief	Sends the command to the cmdMbox
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \param  eMboxCmdType - The command type
 * \param  pMboxBuf - The command itself (parameters)
 * \param  uParamsLen - The command's length
 * \param  fCb - The command's Cb function
 * \param  hCb - The command's Cb handle
 * \param  pCb - Pointer to the results buffer (for interrogate commands)
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Pushes the command to the command queue, which triggers the 
 * CmdQueue SM.
 * 
 * \sa cmdQueue_Push
 */
TI_STATUS cmdQueue_SendCommand (TI_HANDLE  hCmdQueue, 
                            Command_e  eMboxCmdType, 
                            void      *pMboxBuf, 
                            TI_UINT32  uParamsLen, 
                            void      *fCb, 
                            TI_HANDLE  hCb, 
                            void      *pCb)
{
    TCmdQueue *pCmdQueue = (TCmdQueue*)hCmdQueue;
    TI_STATUS  status;

    if (pCmdQueue->bErrorFlag) 
        return TI_NOK;

    status = cmdQueue_Push (pCmdQueue, 
                            eMboxCmdType,
                            (TI_UINT8*)pMboxBuf, 
                            uParamsLen,
                            fCb, 
                            hCb, 
                            (TI_UINT8*)pCb); 

    return RC_CONVERT (status);
}


/*
 * \brief	Push the command Node to the Queue with its information element parameter
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \param  cmdType - The command type
 * \param  pParamsBuf - The command itself (parameters)
 * \param  uParamsLen - The command's length
 * \param  fCb - The command's Cb function
 * \param  hCb - The command's Cb handle
 * \param  pCb - Pointer to the results buffer (for interrogate commands)
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * 
 * \sa cmdQueue_SendCommand, cmdQueue_SM
 */
static TI_STATUS cmdQueue_Push (TI_HANDLE  hCmdQueue, 
                         Command_e  cmdType,
                         TI_UINT8   *pParamsBuf, 
                         TI_UINT32  uParamsLen,
                         void       *fCb, 
                         TI_HANDLE  hCb, 
                         void       *pCb)
{
    TCmdQueue *pCmdQueue = (TCmdQueue*)hCmdQueue;

    /* If command type is NOT CMD_INTERROGATE, enter Push only if Mailbox is enabled */
    if (!pCmdQueue->bMboxEnabled) 
        return TI_OK;

    #ifdef TI_DBG
        /*
         * Check if Queue is Full
         */
        if (pCmdQueue->uNumberOfCommandInQueue == CMDQUEUE_QUEUE_DEPTH)
        	{

            	return  TI_NOK;
        	}
    #endif /* TI_DBG*/

    /* Initializes the last Node in the Queue with the arrgs */
    pCmdQueue->aCmdQueue[pCmdQueue->tail].cmdType   = cmdType;
    pCmdQueue->aCmdQueue[pCmdQueue->tail].uParamsLen = uParamsLen;
    pCmdQueue->aCmdQueue[pCmdQueue->tail].fCb = fCb;
    pCmdQueue->aCmdQueue[pCmdQueue->tail].hCb = hCb;   

    os_memoryCopy (pCmdQueue->hOs, 
                   pCmdQueue->aCmdQueue[pCmdQueue->tail].aParamsBuf, 
                   pParamsBuf, 
                   uParamsLen);
    
    pCmdQueue->aCmdQueue[pCmdQueue->tail].pInterrogateBuf = (TI_UINT8 *)pCb;
            
    /* Advance the queue tail*/
    pCmdQueue->tail++;
    if (pCmdQueue->tail == CMDQUEUE_QUEUE_DEPTH)
        pCmdQueue->tail = 0;
    
    /* Update counters */
    pCmdQueue->uNumberOfCommandInQueue++;

    #ifdef TI_DBG    
        if (pCmdQueue->uMaxNumberOfCommandInQueue < pCmdQueue->uNumberOfCommandInQueue)
        {
            pCmdQueue->uMaxNumberOfCommandInQueue = pCmdQueue->uNumberOfCommandInQueue;     
        }
    #endif /* TI_DBG*/

    /* If queue has only one command trigger the send command from queue */  
    if (pCmdQueue->uNumberOfCommandInQueue == 1)
	{
        return cmdQueue_SM (pCmdQueue, CMDQUEUE_EVENT_RUN);
	}
	else
    {
        return TI_OK;            
    }
}


/*
 * \brief	Notify the CmdQueue SM on the result received.
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \return TI_OK on success or TI_NOK on failure
 * 
 * \par Description
 * Call the CmdQueue SM with CMDQUEUE_EVENT_COMPLETE
 * 
 * \sa cmdQueue_SM
 */
TI_STATUS  cmdQueue_ResultReceived(TI_HANDLE hCmdQueue)
{
    TCmdQueue *pCmdQueue = (TCmdQueue*)hCmdQueue;

    return cmdQueue_SM (pCmdQueue, CMDQUEUE_EVENT_COMPLETE);
}


/*
 * \brief	Prepere the command queue for recovery.
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \return TI_OK
 * 
 * \par Description
 * Copy the queue nodes to a recovery list, in order handle 
 * the commands CB's after recovery has finished
 * 
 * \sa cmdQueue_EndReconfig
 */
TI_STATUS cmdQueue_Restart (TI_HANDLE hCmdQueue)
{
    TCmdQueue* pCmdQueue = (TCmdQueue*)   hCmdQueue;
    TI_UINT32  uCurrentCmdIndex;
    TI_UINT32  first  = pCmdQueue->head;
    TCmdQueueNode *pHead;
    TCmdQueueRecoveryNode *pRecoveryNode;
    
    /* 
     * Stop the SM
    */
    pCmdQueue->state = CMDQUEUE_STATE_IDLE;
    pCmdQueue->bAwake = TI_FALSE;

    
	/*
     * Save The Call Back Function in the Queue in order the return them after the recovery 
     * with an error status 
	*/ 

	/* Clean The Command Call Back Counter */ 
    pCmdQueue->uNumberOfRecoveryNodes = 0;
    pRecoveryNode = &pCmdQueue->aRecoveryQueue[pCmdQueue->uNumberOfRecoveryNodes];

    for (uCurrentCmdIndex = 0; 
         uCurrentCmdIndex < pCmdQueue->uNumberOfCommandInQueue; 
         uCurrentCmdIndex++)
    {
        pHead  =  &pCmdQueue->aCmdQueue[first];

        if (pHead->fCb != NULL)
        { 
            /*Copy the interrogate CB and the interrogate data buffer pointer */
            pRecoveryNode->fCb = pHead->fCb;
            pRecoveryNode->hCb = pHead->hCb;
            pRecoveryNode->pInterrogateBuf = pHead->pInterrogateBuf;
            pCmdQueue->uNumberOfRecoveryNodes++;
            pRecoveryNode = &pCmdQueue->aRecoveryQueue[pCmdQueue->uNumberOfRecoveryNodes];
        }       
        first++;
        if (first == CMDQUEUE_QUEUE_DEPTH)
            first = 0;
    }

    /*
     * Init the queue
     */
    pCmdQueue->head = 0;
    pCmdQueue->tail = 0;
    pCmdQueue->uNumberOfCommandInQueue = 0;

    return TI_OK;
}


/*
 * \brief	Call the stored CB to end the recovery of the MBox queue
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \return TI_OK
 * 
 * \par Description
 * Call the stored CB's with an error status 
 * 
 * \sa cmdQueue_StartReconfig
 */
TI_STATUS cmdQueue_EndReconfig (TI_HANDLE hCmdQueue)
{
    TCmdQueue* pCmdQueue = (TCmdQueue*)   hCmdQueue;
    TI_UINT32  uCbIndex;
    TCmdQueueRecoveryNode *pHead;

    for (uCbIndex = 0; uCbIndex < pCmdQueue->uNumberOfRecoveryNodes; uCbIndex++)
    {
        pHead  =  &pCmdQueue->aRecoveryQueue[uCbIndex];

        if (pHead->pInterrogateBuf)
        {
            ((TCmdQueueInterrogateCb)pHead->fCb)(pHead->hCb, CMD_STATUS_FW_RESET, pHead->pInterrogateBuf);
        }
        else
        {
            ((TCmdQueueCb)pHead->fCb)(pHead->hCb, CMD_STATUS_FW_RESET);
        }
    }

    pCmdQueue->uNumberOfRecoveryNodes = 0;
	
	return TI_OK;
}


/*
 * \brief	Register for a call back to be called when Command Complete occured and the CmdMboxCB was NULL
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \param  fCb - The command's Cb function
 * \param  hCb - The command's Cb handle
 * \return TI_OK
 * 
 * \par Description
 * 
 * \sa
 */
TI_STATUS cmdQueue_RegisterCmdCompleteGenericCb (TI_HANDLE hCmdQueue, void *fCb, TI_HANDLE hCb)
{
    TCmdQueue* pCmdQueue = (TCmdQueue*)hCmdQueue;
	
    if (fCb == NULL || hCb == NULL)
	{
		return TI_NOK;
	}

    pCmdQueue->fCmdCompleteCb = (TCmdQueueGenericCb)fCb;
    pCmdQueue->hCmdCompleteCb = hCb;

	return TI_OK;
}


/*
 * \brief	Register for a call back to be called when an Error (Timeout) occurs
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \param  fCb - The command's Cb function
 * \param  hCb - The command's Cb handle
 * \return TI_OK
 * 
 * \par Description
 * 
 * \sa
 */
TI_STATUS cmdQueue_RegisterForErrorCb (TI_HANDLE hCmdQueue, void *fCb, TI_HANDLE hCb)
{
    TCmdQueue* pCmdQueue = (TCmdQueue*)hCmdQueue;
	
    if (fCb == NULL || hCb == NULL)
	{
		return TI_NOK;
	}

    pCmdQueue->hFailureCb = hCb;
    pCmdQueue->fFailureCb = (TCmdQueueCb)fCb;

	return TI_OK;
}


/*
 * \brief	Enables the CmdMbox (on exit from init mode)
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \return TI_OK
 * 
 * \par Description
 * 
 * \sa cmdQueue_DisableMbox
 */
TI_STATUS cmdQueue_EnableMbox (TI_HANDLE hCmdQueue)
{
    TCmdQueue* pCmdQueue = (TCmdQueue*)hCmdQueue;

    pCmdQueue->bMboxEnabled = TI_TRUE;

    return TI_OK;
}


/*
 * \brief	Disables the CmdMbox (when stopping the driver)
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \return TI_OK
 * 
 * \par Description
 * 
 * \sa cmdQueue_EnableMbox
 */
TI_STATUS cmdQueue_DisableMbox (TI_HANDLE hCmdQueue)
{
    TCmdQueue* pCmdQueue = (TCmdQueue*)hCmdQueue;

    pCmdQueue->bMboxEnabled = TI_FALSE;

    return TI_OK;
}


/*
 * \brief	Called when a command timeout occur
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \return TI_OK
 * 
 * \par Description
 * 
 * \sa cmdQueue_Init, cmdMbox_TimeOut
 */
TI_STATUS cmdQueue_Error (TI_HANDLE hCmdQueue, TI_UINT32 command, TI_UINT32 status, void *param)
{
    TCmdQueue* pCmdQueue = (TCmdQueue*)hCmdQueue;     

    if (status != CMD_STATUS_UNKNOWN_CMD && status != CMD_STATUS_UNKNOWN_IE)
    {
        /* preform Recovery */
        if (pCmdQueue->fFailureCb)
        {
            pCmdQueue->fFailureCb (pCmdQueue->hFailureCb, TI_NOK);
        }
    }

    return TI_OK;
}


/*
 * \brief	Returns maximum number of commands (ever) in TCmdQueue queue
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \return maximum number of commands (ever) in mailbox queue
 * 
 * \par Description
 * Used for debugging purposes
 *
 * \sa cmdQueue_Error
 */
TI_UINT32 cmdQueue_GetMaxNumberOfCommands (TI_HANDLE hCmdQueue)
{
    TCmdQueue* pCmdQueue = (TCmdQueue*)hCmdQueue;

    return pCmdQueue->uMaxNumberOfCommandInQueue;
}



/********************************************************************************
*                              DEBUG  FUNCTIONS           	          			*
*********************************************************************************/

#ifdef TI_DBG

/*
 * \brief	Print the command queue & statistics
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \return void
 * 
 * \par Description
 * Used for debugging purposes
 *
 * \sa cmdQueue_PrintQueue
 */
void cmdQueue_Print (TI_HANDLE hCmdQueue)
{
    TCmdQueue* pCmdQueue = (TCmdQueue*)hCmdQueue; 

    cmdQueue_PrintQueue (pCmdQueue);
}


/*
 * \brief	Print the command queue
 * 
 * \param  pCmdQueue - Pointer to TCmdQueue
 * \return void
 * 
 * \par Description
 * Used for debugging purposes
 *
 * \sa cmdQueue_Print, cmdQueue_GetCmdString, cmdQueue_GetIEString
 */
static void cmdQueue_PrintQueue (TCmdQueue *pCmdQueue)
{
    TI_UINT32 uCurrentCmdIndex;
    TI_UINT32 first = pCmdQueue->head;
    TCmdQueueNode* pHead;
    TI_UINT32 NumberOfCommand = pCmdQueue->uNumberOfCommandInQueue;

    for(uCurrentCmdIndex = 0 ; uCurrentCmdIndex < NumberOfCommand ; uCurrentCmdIndex++)
    {
        pHead = &pCmdQueue->aCmdQueue[first];

        first++;
        if (first == CMDQUEUE_QUEUE_DEPTH)
        {
            first = 0;
        }
    } 
}


/*
 * \brief	print the last uNumOfCmd commands
 * 
 * \param  hCmdQueue - Handle to CmdQueue
 * \param  uNumOfCmd - Number of commands to print
 * \return void
 * 
 * \par Description
 * Used for debugging purposes
 *
 * \sa cmdQueue_Error
 */
void cmdQueue_PrintHistory (TI_HANDLE hCmdQueue, TI_UINT32 uNumOfCmd)
{
    TCmdQueue* pCmdQueue = (TCmdQueue*)hCmdQueue; 
    TI_UINT32 uCurrentCmdIndex;
    TI_UINT32 first  = pCmdQueue->head;
    TCmdQueueNode* pHead;

    
    for (uCurrentCmdIndex = 0; uCurrentCmdIndex < uNumOfCmd; uCurrentCmdIndex++)
    {
        pHead  =  &pCmdQueue->aCmdQueue[first];

        if (first == 0)
        {
            first = CMDQUEUE_QUEUE_DEPTH - 1;
        }
		else
        {
			first--;
        }
	}
}

#endif  /* TI_DBG */


