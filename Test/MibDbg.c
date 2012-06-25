/*
 * MibDbg.c
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
/*                                                                         */
/*    MODULE:   MibDbg.c                                                 */
/*    PURPOSE:  MIB debug implementation                                    */
/*                                                                         */
/***************************************************************************/

#include "MibDbg.h"
#include "TWDriver.h"
#include "report.h"
#include "osApi.h"

/*
 ***********************************************************************
 *	Internal functions definitions
 ***********************************************************************
 */

static void mibDbgGetArpIpTable(TI_HANDLE hTWD)
{
	TI_STATUS status = TI_OK;
    TMib mib;
    TI_INT32 i;

	/* init */
	mib.aMib = MIB_arpIpAddressesTable;
	mib.aData.ArpIpAddressesTable.FilteringEnable = 0;
	for ( i = 0 ; i < IP_V4_ADDR_LEN ; i++ )
	{
		mib.aData.ArpIpAddressesTable.addr[i] = 0;
	}

	status = TWD_ReadMib(hTWD,NULL,NULL,(void*)&mib);
	if (status != TI_OK)
	{
		return;
	}
}

static void mibDbgGetGroupAddressTable(TI_HANDLE hTWD)
{
	TI_STATUS status = TI_OK;
    TMib mib;
    TI_INT32 i,j;

	/* init */
	mib.aMib 										= MIB_dot11GroupAddressesTable;
	mib.aData.GroupAddressTable.bFilteringEnable 	= 0;
	mib.aData.GroupAddressTable.nNumberOfAddresses 	= 0;
    for ( i = 0 ; i < MIB_MAX_MULTICAST_GROUP_ADDRS ; i++ )
	{
		for ( j = 0 ; j < MAC_ADDR_LEN ; j++ )
		{
			mib.aData.GroupAddressTable.aGroupTable[i][j]	= 0;
		}
	}

	status = TWD_ReadMib(hTWD,NULL,NULL,(void*)&mib);
	if (status != TI_OK)
	{
		return;
	}
}

static void mibDbgGetCounterTable(TI_HANDLE hTWD)
{
	TI_STATUS status = TI_OK;
    TMib mib;

	/* init */
	mib.aMib 								= MIB_countersTable;
	mib.aData.CounterTable.FCSErrorCount	= 0;
	mib.aData.CounterTable.PLCPErrorCount	= 0;
	mib.aData.CounterTable.SeqNumMissCount	= 0;

	status = TWD_ReadMib(hTWD,NULL,NULL,(void*)&mib);
	if (status != TI_OK)
	{
		return;
	}

}

static void mibDbgModifyCtsToSelf(TI_HANDLE hTWD, void* pParam)
{
	TI_STATUS status = TI_OK;
    TMib mib;

	if (pParam == NULL) 
	{
		return;
	}

	/* init */
	mib.aMib					= MIB_ctsToSelf;
	mib.aData.CTSToSelfEnable 	= 0;
	mib.aData.CTSToSelfEnable 	= *(TI_UINT32*)pParam;

	status = TWD_WriteMib(hTWD, &mib);
	if (status != TI_OK)
	{
		return;
	}
}

static void mibDbgGetCtsToSelf(TI_HANDLE hTWD)
{
	TI_STATUS status = TI_OK;
    TMib mib;

	/* init */
	mib.aMib					= MIB_ctsToSelf;
	mib.aData.CTSToSelfEnable 	= 0;

	status = TWD_ReadMib(hTWD,NULL,NULL,(void*)&mib);
	if (status != TI_OK)
	{
		return;
	}
}

static void mibDbgSetMaxRxLifetime(TI_HANDLE hTWD, void* pParam)
{
	TI_STATUS status = TI_OK;
    TMib mib;

	if (pParam == NULL) 
	{
		return;
	}

	/* init */
	mib.aMib 						= MIB_dot11MaxReceiveLifetime;
	mib.aData.MaxReceiveLifeTime 	= *(TI_UINT32*)pParam;

	status = TWD_WriteMib(hTWD, &mib);
	if (status != TI_OK)
	{
		return;
	}
}

/*
 ***********************************************************************
 *	External functions definitions
 ***********************************************************************
 */
void mibDbgPrintFunctions(void)
{
}

void MibDebugFunction(TI_HANDLE hTWD ,TI_UINT32 funcType, void* pParam)
{
	if (hTWD == NULL) 
	{
		return;
	}

	switch (funcType)
	{
	case DBG_MIB_PRINT_HELP:
		mibDbgPrintFunctions();
		break;
	case DBG_MIB_GET_ARP_TABLE:
		mibDbgGetArpIpTable(hTWD);
		break;
	case DBG_MIB_GET_GROUP_ADDRESS_TABLE:
		mibDbgGetGroupAddressTable(hTWD);
		break;
	case DBG_MIB_GET_COUNTER_TABLE:
		mibDbgGetCounterTable(hTWD);
		break;
	case DBG_MIB_MODIFY_CTS_TO_SELF:
		mibDbgModifyCtsToSelf(hTWD, pParam);
		break;
	case DBG_MIB_GET_CTS_TO_SELF:
		mibDbgGetCtsToSelf(hTWD);
		break;
	case DBG_MIB_SET_MAX_RX_LIFE_TIME:
		mibDbgSetMaxRxLifetime(hTWD, pParam);
		break;
	default:
		break;
	}
}
