/*
 * siteMgrDebug.c
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

/** \file reportReplvl.c
 *  \brief Report level implementation
 * 
 *  \see reportReplvl.h
 */

/** \file   siteMgrDebug.c 
 *  \brief  The siteMgrDebug module.
 *  
 *  \see    siteMgrDebug.h
 */

#include "tidef.h"
#include "osApi.h"
#include "paramOut.h"
#include "siteMgrDebug.h"
#include "siteMgrApi.h"
#include "siteHash.h"
#include "report.h"
#include "CmdDispatcher.h"
#include "DrvMainModules.h"
#include "sme.h"
#include "apConn.h"
#include "healthMonitor.h"
#include "conn.h"
#include "connApi.h"

#ifdef XCC_MODULE_INCLUDED
#include "XCCMngr.h"
#endif


static void printPrimarySite(siteMgr_t *pSiteMgr);

void printSiteTable(siteMgr_t *pSiteMgr, char *desiredSsid);

static void printDesiredParams(siteMgr_t *pSiteMgr, TI_HANDLE hCmdDispatch);

static void printPrimarySiteDesc(siteMgr_t *pSiteMgr, OS_802_11_BSSID *pPrimarySiteDesc);

static void setRateSet(TI_UINT8 maxRate, TRates *pRates);

void printSiteMgrHelpMenu(void);

/*	Function implementation */
void siteMgrDebugFunction (TI_HANDLE         hSiteMgr, 
                           TStadHandlesList *pStadHandles,
                           TI_UINT32         funcType, 
                           void             *pParam)
{
	siteMgr_t *pSiteMgr = (siteMgr_t *)hSiteMgr;
	paramInfo_t		param;
	TSsid			newDesiredSsid;
	TI_UINT8		value;
	OS_802_11_BSSID primarySiteDesc;
	TRates			ratesSet;


	newDesiredSsid.len = 5;
	os_memoryCopy(pSiteMgr->hOs, (void *)newDesiredSsid.str, "yaeli", 5);

	
	switch (funcType)
	{
	case SITE_MGR_DEBUG_HELP_MENU:
		printSiteMgrHelpMenu();
		break;

	case PRIMARY_SITE_DBG:
		printPrimarySite(pSiteMgr);
		break;

	case SITE_TABLE_DBG:
		printSiteTable(pSiteMgr, NULL);
		break;

	case DESIRED_PARAMS_DBG:
		printDesiredParams(pSiteMgr, pStadHandles->hCmdDispatch);
		break;

	case GET_PRIMARY_SITE_DESC_DBG:
		param.paramType = SITE_MGR_GET_SELECTED_BSSID_INFO;
		param.content.pSiteMgrPrimarySiteDesc = &primarySiteDesc;
		cmdDispatch_GetParam(pStadHandles->hCmdDispatch, &param);
		printPrimarySiteDesc(pSiteMgr, &primarySiteDesc);
		break;

	case SET_RSN_DESIRED_CIPHER_SUITE_DBG:
		param.paramType = RSN_ENCRYPTION_STATUS_PARAM;
		value = *((TI_UINT32 *)pParam);
		param.content.rsnEncryptionStatus = (ECipherSuite)value;
		cmdDispatch_SetParam(pStadHandles->hCmdDispatch, &param); 
		break;

	case GET_RSN_DESIRED_CIPHER_SUITE_DBG:
		param.paramType = RSN_ENCRYPTION_STATUS_PARAM;
		cmdDispatch_GetParam(pStadHandles->hCmdDispatch, &param);
		break;
 
	case SET_RSN_DESIRED_AUTH_TYPE_DBG:
		param.paramType = RSN_EXT_AUTHENTICATION_MODE;
		value = *((TI_UINT32 *)pParam);
		param.content.rsnDesiredAuthType = (EAuthSuite)value;
		cmdDispatch_SetParam(pStadHandles->hCmdDispatch, &param); 
		break;

	case GET_RSN_DESIRED_AUTH_TYPE_DBG:
		param.paramType = RSN_EXT_AUTHENTICATION_MODE;
		cmdDispatch_GetParam(pStadHandles->hCmdDispatch, &param);

		break;

	case GET_CONNECTION_STATUS_DBG:
		param.paramType = SME_CONNECTION_STATUS_PARAM;
		cmdDispatch_GetParam(pStadHandles->hCmdDispatch, &param);
		break;

	case SET_SUPPORTED_RATE_SET_DBG:
		param.paramType = SITE_MGR_DESIRED_SUPPORTED_RATE_SET_PARAM;
		value = *((TI_UINT32 *)pParam);
		setRateSet(value, &ratesSet);
		os_memoryCopy(pSiteMgr->hOs, &(param.content.siteMgrDesiredSupportedRateSet), &(ratesSet), sizeof(TRates));
		cmdDispatch_SetParam(pStadHandles->hCmdDispatch, &param);
		break;

	case GET_SUPPORTED_RATE_SET_DBG:
		param.paramType = SITE_MGR_DESIRED_SUPPORTED_RATE_SET_PARAM;
		cmdDispatch_GetParam(pStadHandles->hCmdDispatch, &param);
		if(param.content.siteMgrDesiredSupportedRateSet.len == 0)
		{
		}
		else 
		{	
			/* It looks like it never happens. Anyway decided to check */
            if ( param.content.siteMgrDesiredSupportedRateSet.len > DOT11_MAX_SUPPORTED_RATES )
            {
                handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
                param.content.siteMgrDesiredSupportedRateSet.len = DOT11_MAX_SUPPORTED_RATES;
            }
		}
		break;

	case SET_MLME_LEGACY_AUTH_TYPE_DBG:
		param.paramType = MLME_LEGACY_TYPE_PARAM;
		value = *((TI_UINT32 *)pParam);
		param.content.mlmeLegacyAuthType = (legacyAuthType_e)value;
		cmdDispatch_SetParam(pStadHandles->hCmdDispatch, &param); 
		break;

	case GET_MLME_LEGACY_AUTH_TYPE_DBG:
		param.paramType = MLME_LEGACY_TYPE_PARAM;
		cmdDispatch_GetParam(pStadHandles->hCmdDispatch, &param);

		break;


	case RADIO_STAND_BY_CHANGE_STATE:
		break;
		

    case PRINT_FAILURE_EVENTS:
        {

		healthMonitor_printFailureEvents (pStadHandles->hHealthMonitor);
		apConn_printStatistics(pStadHandles->hAPConnection);
        conn_ibssPrintStatistics(pStadHandles->hConn);
        if (((conn_t*)pStadHandles->hConn)->currentConnType==CONNECTION_INFRA)
        {
            switch (((conn_t*)pStadHandles->hConn)->state)
            {
            case   0:
                break;       
             case   1:
                break;             
             case   2:
                break;            
             case   3:
                break;           
             case   4:
                break;            
             case   5:
                break;        
             case   6:
                break; 
            case   7:
               break; 
            default:
                break;
            }
        }
        }
        break;

	case FORCE_HW_RESET_RECOVERY:
		break;

	case FORCE_SOFT_RECOVERY:
		break;


	case PERFORM_HEALTH_TEST:
		healthMonitor_PerformTest(pStadHandles->hHealthMonitor, TI_FALSE);	
		break;

	case PRINT_SITE_TABLE_PER_SSID:
		printSiteTable(pSiteMgr, (char*)pParam);
		break;

	case SET_DESIRED_CHANNEL:
		param.paramType = SITE_MGR_DESIRED_CHANNEL_PARAM;
		param.content.siteMgrDesiredChannel = *(TI_UINT8*)pParam;
		siteMgr_setParam(pStadHandles->hSiteMgr, &param);
		break;

	default:
		break;
	}
} 

static void printPrimarySite(siteMgr_t *pSiteMgr)
{
	siteEntry_t *pSiteEntry;
    TI_UINT8	len;
	char	ssid[MAX_SSID_LEN + 1];
	
	pSiteEntry = pSiteMgr->pSitesMgmtParams->pPrimarySite;
	
	if (pSiteEntry == NULL)
	{
		return;
	}

    len = pSiteEntry->ssid.len;
    /* It looks like it never happens. Anyway decided to check */
    if ( pSiteEntry->ssid.len > MAX_SSID_LEN )
    {
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        len = MAX_SSID_LEN;
    }
	os_memoryCopy(pSiteMgr->hOs, ssid, (void *)pSiteEntry->ssid.str, len);
	ssid[len] = '\0';

	switch (pSiteEntry->maxBasicRate)
	{
	case DRV_RATE_1M:
		break;

	case DRV_RATE_2M:
		break;

	case DRV_RATE_5_5M:
		break;

	case DRV_RATE_11M:
		break;

	case DRV_RATE_6M:
		break;

	case DRV_RATE_9M:
		break;

	case DRV_RATE_12M:
		break;

	case DRV_RATE_18M:
		break;

	case DRV_RATE_24M:
		break;

	case DRV_RATE_36M:
		break;

	case DRV_RATE_48M:
		break;

	case DRV_RATE_54M:
		break;

	default:
		break;
	}

	switch (pSiteEntry->maxActiveRate)
	{
	case DRV_RATE_1M:
		break;

	case DRV_RATE_2M:
		break;

	case DRV_RATE_5_5M:
		break;

	case DRV_RATE_11M:
		break;

	case DRV_RATE_22M:
		break;

	case DRV_RATE_6M:
		break;

	case DRV_RATE_9M:
		break;

	case DRV_RATE_12M:
		break;

	case DRV_RATE_18M:
		break;

	case DRV_RATE_24M:
		break;

	case DRV_RATE_36M:
		break;

	case DRV_RATE_48M:
		break;

	case DRV_RATE_54M:
		break;

	default:
		break;
	}
}

void printSiteTable(siteMgr_t *pSiteMgr, char *desiredSsid)
{
	TI_UINT8	i, numOfSites = 0;
	siteEntry_t *pSiteEntry;	
	char	ssid[MAX_SSID_LEN + 1];
    siteTablesParams_t      *pCurrentSiteTable = pSiteMgr->pSitesMgmtParams->pCurrentSiteTable;
    TI_UINT8                   tableIndex=2;
	
    /* It looks like it never happens. Anyway decided to check */
    if ( pCurrentSiteTable->maxNumOfSites > MAX_SITES_BG_BAND )
    {
        handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
        pCurrentSiteTable->maxNumOfSites = MAX_SITES_BG_BAND;
    }

    do
	{
        tableIndex--;
		for (i = 0; i < pCurrentSiteTable->maxNumOfSites; i++)
		{
			pSiteEntry = &(pCurrentSiteTable->siteTable[i]);
	
			if (pSiteEntry->siteType == SITE_NULL)
				continue;
            /* It looks like it never happens. Anyway decided to check */
            if ( pCurrentSiteTable->siteTable[i].ssid.len > MAX_SSID_LEN )
            {
                handleRunProblem(PROBLEM_BUF_SIZE_VIOLATION);
                pCurrentSiteTable->siteTable[i].ssid.len = MAX_SSID_LEN;
            }
			os_memoryCopy(pSiteMgr->hOs ,ssid, (void *)pCurrentSiteTable->siteTable[i].ssid.str, pCurrentSiteTable->siteTable[i].ssid.len);
			ssid[pCurrentSiteTable->siteTable[i].ssid.len] = '\0';
			
			if (desiredSsid != NULL)
			{
				int desiredSsidLength = 0;
				char * tmp = desiredSsid;

				while (tmp != '\0')
				{
					desiredSsidLength++;
					tmp++;
				}

				if (os_memoryCompare(pSiteMgr->hOs, (TI_UINT8 *)ssid, (TI_UINT8 *)desiredSsid, desiredSsidLength))
					continue;
			}
		
			switch (pCurrentSiteTable->siteTable[i].maxBasicRate)
			{
			case DRV_RATE_1M:
				break;
	
			case DRV_RATE_2M:
				break;
	
			case DRV_RATE_5_5M:
				break;
	
			case DRV_RATE_11M:
				break;
	
			case DRV_RATE_6M:
				break;
		
			case DRV_RATE_9M:
				break;
		
			case DRV_RATE_12M:
				break;
		
			case DRV_RATE_18M:
				break;
		
			case DRV_RATE_24M:
				break;
		
			case DRV_RATE_36M:
				break;
		
			case DRV_RATE_48M:
				break;
		
			case DRV_RATE_54M:
				break;

			default:
				break;
			}
	
				switch (pCurrentSiteTable->siteTable[i].maxActiveRate)
			{
			case DRV_RATE_1M:
				break;
	
			case DRV_RATE_2M:
				break;
	
			case DRV_RATE_5_5M:
				break;
	
			case DRV_RATE_11M:
				break;
	
			case DRV_RATE_22M:
				break;

			case DRV_RATE_6M:
				break;
		
			case DRV_RATE_9M:
				break;
		
			case DRV_RATE_12M:
				break;
		
			case DRV_RATE_18M:
				break;
		
			case DRV_RATE_24M:
				break;
		
			case DRV_RATE_36M:
				break;
		
			case DRV_RATE_48M:
				break;
		
			case DRV_RATE_54M:
				break;
	
			default:
				break;
			}
	
			numOfSites++;
		}
		
		   if ((pSiteMgr->pDesiredParams->siteMgrDesiredDot11Mode == DOT11_DUAL_MODE) && (tableIndex==1))
		   {   /* change site table */
			   if (pCurrentSiteTable == &pSiteMgr->pSitesMgmtParams->dot11BG_sitesTables)
				  {
                   pCurrentSiteTable = (siteTablesParams_t *)&pSiteMgr->pSitesMgmtParams->dot11A_sitesTables;
				  }
			   else
				  {
                   pCurrentSiteTable = &pSiteMgr->pSitesMgmtParams->dot11BG_sitesTables;
				  }
		   }

    } while (tableIndex>0);
}

static void printDesiredParams(siteMgr_t *pSiteMgr, TI_HANDLE hCmdDispatch)
{  
	paramInfo_t		param;

	switch (pSiteMgr->pDesiredParams->siteMgrDesiredRatePair.maxBasic)
	{
	case DRV_RATE_1M:
		break;

	case DRV_RATE_2M:
		break;

	case DRV_RATE_5_5M:
		break;

	case DRV_RATE_11M:
		break;

	case DRV_RATE_22M:
		break;

	case DRV_RATE_6M:
		break;

	case DRV_RATE_9M:
		break;

	case DRV_RATE_12M:
		break;

	case DRV_RATE_18M:
		break;

	case DRV_RATE_24M:
		break;

	case DRV_RATE_36M:
		break;

	case DRV_RATE_48M:
		break;

	case DRV_RATE_54M:
		break;

	default:
		break;
	}

	switch (pSiteMgr->pDesiredParams->siteMgrDesiredRatePair.maxActive)
	{
	case DRV_RATE_1M:
		break;

	case DRV_RATE_2M:
		break;

	case DRV_RATE_5_5M:
		break;

	case DRV_RATE_11M:
		break;

	case DRV_RATE_22M:
		break;

	case DRV_RATE_6M:
		break;

	case DRV_RATE_9M:
		break;

	case DRV_RATE_12M:
		break;

	case DRV_RATE_18M:
		break;

	case DRV_RATE_24M:
		break;

	case DRV_RATE_36M:
		break;

	case DRV_RATE_48M:
		break;

	case DRV_RATE_54M:
		break;

	default:
		break;
	}

	param.paramType = RSN_EXT_AUTHENTICATION_MODE;
	cmdDispatch_GetParam(hCmdDispatch, &param);

	param.paramType = RSN_ENCRYPTION_STATUS_PARAM;
	cmdDispatch_GetParam(hCmdDispatch, &param);
}



static void printPrimarySiteDesc(siteMgr_t *pSiteMgr, OS_802_11_BSSID *pPrimarySiteDesc)
{
	TI_UINT8 maxNumOfRates;
	char ssid[MAX_SSID_LEN + 1];

	/* SSID */
	os_memoryCopy(pSiteMgr->hOs, ssid, (void *)pPrimarySiteDesc->Ssid.Ssid, pPrimarySiteDesc->Ssid.SsidLength);
	ssid[pPrimarySiteDesc->Ssid.SsidLength] = 0;


	maxNumOfRates = sizeof(pPrimarySiteDesc->SupportedRates) / sizeof(pPrimarySiteDesc->SupportedRates[0]); 
}

static void setRateSet(TI_UINT8 maxRate, TRates *pRates)
{
	TI_UINT8 i = 0;

	switch (maxRate)
	{

	case DRV_RATE_54M:
		pRates->ratesString[i] = 108;
		i++;

	case DRV_RATE_48M:
		pRates->ratesString[i] = 96;
		i++;

	case DRV_RATE_36M:
		pRates->ratesString[i] = 72;
		i++;

	case DRV_RATE_24M:
		pRates->ratesString[i] = 48;
		i++;

	case DRV_RATE_18M:
		pRates->ratesString[i] = 36;
		i++;

	case DRV_RATE_12M:
		pRates->ratesString[i] = 24;
		i++;

	case DRV_RATE_9M:
		pRates->ratesString[i] = 18;
		i++;

	case DRV_RATE_6M:
		pRates->ratesString[i] = 12;
		i++;

	case DRV_RATE_22M:
		pRates->ratesString[i] = 44;
		i++;

	case DRV_RATE_11M:
		pRates->ratesString[i] = 22;
		pRates->ratesString[i] |= 0x80;
		i++;

	case DRV_RATE_5_5M:
		pRates->ratesString[i] = 11;
		pRates->ratesString[i] |= 0x80;
		i++;

	case DRV_RATE_2M:
		pRates->ratesString[i] = 4;
		pRates->ratesString[i] |= 0x80;
		i++;

	case DRV_RATE_1M:
		pRates->ratesString[i] = 2;
		pRates->ratesString[i] |= 0x80;
		i++;
		break;

	default:
		pRates->len = 0;
	}

	pRates->len = i;

}

void printSiteMgrHelpMenu(void)
{
}



