/*
 * qosMngrDbg.c
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


/***************************************************************************/
/*																									*/
/*		MODULE:																	*/
/*    PURPOSE:										*/
/*																			*/
/***************************************************************************/
#include "tidef.h"
#include "osApi.h"
#include "qosMngr.h"
#include "report.h"
#include "paramOut.h"
#include "qosMngrDbg.h"

void printQosMngrParams(TI_HANDLE hQosMngr);
void printQosDbgFunction(TI_HANDLE hQosMngr);


void qosMngrDebugFunction(TI_HANDLE hQosMngr, TI_UINT32 funcType, void *pParam)
{
	switch (funcType)
	{
	case DBG_QOS_MNGR_PRINT_HELP:
		printQosDbgFunction(hQosMngr);
		break;

	case DBG_QOS_MNGR_PRINT_PARAMS:
		printQosMngrParams(hQosMngr);
		break;

	default:
		break;
	}


}
void printQosDbgFunction(TI_HANDLE hQosMngr)
{
}

void printQosMngrParams(TI_HANDLE hQosMngr)
{
	qosMngr_t *pQosMngr = (qosMngr_t *)hQosMngr;
    EQosProtocol protocol = QOS_NONE;


	switch(pQosMngr->activeProtocol){
	case QOS_WME:
		protocol = QOS_WME;

		break;
	case QOS_NONE:
		break;

    default:
		break;

	}
}

