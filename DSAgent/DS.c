/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2001 Apple Computer, Inc. All Rights
 * Reserved. This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License'). You may not use this file
 * except in compliance with the License. Please obtain a copy of the
 * License at http:www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT. Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * DS.c
 * DirectoryService agent for lookupd
 */

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <NetInfo/system_log.h>
#include <NetInfo/dsutil.h>
#include <NetInfo/DynaAPI.h>
#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesConst.h>
#include <DirectoryService/DirServicesTypes.h>
#include <DirectoryService/DirServicesUtils.h>

#define DefaultTimeToLive 300

/*
 * This table MUST be kept up to date with the NetInfo plug-in
 * in Directory Services (number is 75 in CNiPlugIn.cpp)
 */

#define kAttrConsts 71

static const char *sAttrMap[kAttrConsts][2] =
{
			{ kDSNAttrRecordName,				"name" },
 			{ kDS1AttrDistinguishedName,		"realname" },
			{ kDS1AttrPasswordPlus,				"passwd" },
			{ kDS1AttrPassword,					"passwd" }, /* needed when retrieving all so */
															/* that standard type received gets */
															/* correctly mapped */
			{ kDS1AttrUniqueID,					"uid" },
			{ kDS1AttrPrimaryGroupID,			"gid" },
			{ kDS1AttrUserShell,				"shell" },
			{ kDS1AttrNFSHomeDirectory,			"home" },
			{ kDSNAttrAuthenticationAuthority,	"authentication_authority" },
			{ kDSNAttrHomeDirectory,			"home_loc" },
			{ kDS1StandardAttrHomeLocOwner,		"home_loc_owner" },
			{ kDS1AttrHomeDirectoryQuota,		"homedirectoryquota" },
			{ kDS1AttrPicture,					"picture" },
			{ kDS1AttrInternetAlias,			"InetAlias" },
			{ kDS1AttrMailAttribute,			"applemail" },
			{ kDS1AttrAuthenticationHint,		"hint" },
			{ kDS1AttrRARA,						"RARA" },
			{ kDS1AttrGeneratedUID,				"GeneratedUID" },
			{ kDSNAttrGroupMembership,			"users" },
			{ kDSNAttrEMailAddress,				"mail" },
			{ kDSNAttrURL,						"URL" },
			{ kDSNAttrURLForNSL,				"URL" },
			{ kDSNAttrMIME,						"mime" },
			{ kDSNAttrHTML,						"htmldata" },
			{ kDSNAttrNBPEntry,					"NBPEntry" },
			{ kDSNAttrDNSName,					"dnsname" },
			{ kDSNAttrIPAddress,				"IP_Address" },
			{ kDS1AttrENetAddress,				"en_address" },
			{ kDSNAttrComputers,				"computers" },
			{ kDS1AttrMCXFlags,					"mcx_flags" },
			{ kDS1AttrMCXSettings,				"mcx_settings" },
			{ kDS1AttrPrintServiceInfoText,		"PrintServiceInfoText" },
			{ kDS1AttrPrintServiceInfoXML,		"PrintServiceInfoXML" },
			{ kDS1AttrPrintServiceUserData,		"appleprintservice" },
			{ kDS1AttrVFSType,					"vfstype" },
			{ kDS1AttrVFSPassNo,				"passno" },
			{ kDS1AttrVFSDumpFreq,				"dump_freq" },
			{ kDS1AttrVFSLinkDir,				"dir" },
			{ kDSNAttrVFSOpts,					"opts" },
			{ kDS1AttrAliasData,				"alias_data" },
			{ kDSNAttrPhoneNumber,				"phonenumber" },
			{ kDS1AttrCapabilities,				"capabilities" },
			{ kDSNAttrProtocols,				"protocols" },
			{ kDSNAttrMember,					"users" },
			{ kDS1AttrComment, 					"comment" },
			{ kDS1AttrAdminStatus, 				"AdminStatus" },
			{ kDS1AttrAdminLimits,				"admin_limits" },
 			{ kDS1AttrPwdAgingPolicy, 			"PwdAgingPolicy" },
			{ kDS1AttrChange, 					"change" },
			{ kDS1AttrExpire, 					"expire" },
			{ kDSNAttrGroup,					"groups" },
			{ kDS1AttrFirstName,				"firstname" },
			{ kDS1AttrMiddleName,				"middlename" },
			{ kDS1AttrLastName,					"lastname" },
			{ kDSNAttrAreaCode ,				"areacode" },
			{ kDSNAttrAddressLine1,				"address1" },
			{ kDSNAttrAddressLine2,				"address2" },
			{ kDSNAttrAddressLine3,				"address3" },
			{ kDSNAttrCity,						"city" },
			{ kDSNAttrState,					"state" },
			{ kDSNAttrPostalCode,				"zip" },
			{ kDSNAttrOrganizationName,			"orgname" },
			{ kDS1AttrSetupOccupation,			"occupation" },
			{ kDS1AttrSetupLocation,			"location" },
			{ kDS1AttrSetupAdvertising,			"spam" },
			{ kDS1AttrSetupAutoRegister,		"autoregister" },
			{ kDS1AttrPresetUserIsAdmin,		"preset_user_is_admin" },
			{ kDS1AttrPasswordServerLocation,	"passwordserverlocation" },
			{ kDSNAttrBootParams,				"bootparams" },
			{ kDSNAttrNetGroups,				"netgroups" },
			{ kDSNAttrRecordAlias, 				"RecordAlias" }
};

/* Static globals */

/* Static consts */
static const unsigned long kBuffSize = 2048;  //starting size for the data buffer which can grow to accomodate returns

typedef struct
{
	int gDSRunState;
	time_t gSeconds;
	tDirReference gDirRef;
	tDirNodeReference gNodeRef;
	int gTimeToLive;
	dynainfo *dyna;
} agent_private;

static int
doWeUseDS()
{
	struct stat statResult;
	
	/*
	 * ONLY if custom search policy is set then use DirectoryService
	 * This will go away very shortly ie. always use DS will be the norm
	 */
	if (stat("/Library/Preferences/DirectoryService/.DSRunningSP3", &statResult) == 0)
	{
		return 1;
	}
	
	return 0;
}

static int
canWeWork(agent_private *ap)
{
	tDirStatus status = eDSNoErr;
	tDataList *pDataList = NULL;
	tDataBuffer *pDataBuff = NULL;
	tDirReference dirRef = 0;
	tDirNodeReference nodeRef = 0;
	unsigned long count = 0;
	time_t timenow = 0;

	timenow = time(NULL);

	/* Is it time to check or continue with past state */
	if (ap->gSeconds > timenow)
	{
		/* Continuing with current state */
		/* If DS was not running then return */
		if (!ap->gDSRunState)
		{
			return 0;
		}
		/* Else DS is running so we continue below to see if we have proper references */
	}
	else
	{
		/* Time to check */
		/* Don't bother if DS isn't running or DS should NOT be re-started */
		if (doWeUseDS() == 0)
		{
			ap->gDSRunState = 0;
	
			/* Set the time to the future */
			ap->gSeconds = timenow + 30;

			return 0;
		}
		/* Else DS is running so we continue below to get the proper references */
	}
	
	ap->gDSRunState = 1;

	/* Set the time to the future */
	ap->gSeconds = timenow + 30;

	/* verify that we have a valid dir ref otherwise get a new one */
	if ((ap->gDirRef == 0) || (dsVerifyDirRefNum(ap->gDirRef) != eDSNoErr))
	{
		ap->gDirRef = 0;
		ap->gNodeRef = 0;

		/* Open DirectoryService */
		status = dsOpenDirService(&dirRef);
		if (status != eDSNoErr)
		{
			system_log(LOG_DEBUG, "-- DS: %d: *** Error *** %s: %s() error = %d.", __LINE__ , "cww", "dsOpenDirService failed", status);
			ap->gDSRunState = 0;
			return 0;
		}
		else
		{
			ap->gDirRef = dirRef;
		}
	}

	/* verify that we have a valid search node ref otherwise we get a new one */
	if (ap->gNodeRef == 0)
	{
		/* Allocate the data buffer to be used here - 512 chars max for search node name */
		pDataBuff = dsDataBufferAllocate(ap->gDirRef, 512);
		if (pDataBuff == NULL)
		{
			dsCloseDirService(ap->gDirRef);
			ap->gDirRef = 0;
			ap->gDSRunState = 0;
			return 0;
		}

		/* Get the search node */
		status = dsFindDirNodes(ap->gDirRef, pDataBuff, NULL, eDSSearchNodeName, &count, NULL);

		/* if error or expecting only one search node returned */
		if ((status != eDSNoErr) || (count != 1))
		{
			system_log(LOG_DEBUG, "-- DS: %d: *** Error *** %s: %s() error = %d.", __LINE__, "cww", "dsFindDirNodes can't get search node", status);
			dsDataBufferDeAllocate(ap->gDirRef, pDataBuff);
			dsCloseDirService(ap->gDirRef);
			ap->gDirRef = 0;
			ap->gDSRunState = 0;
			return 0;
		}

		/* Allocate the tDataList to retrieve the search node name */
		pDataList = dsDataListAllocate(ap->gDirRef);
		if (pDataList == NULL)
		{
			dsDataBufferDeAllocate(ap->gDirRef, pDataBuff);
			dsCloseDirService(ap->gDirRef);
			ap->gDirRef = 0;
			ap->gDSRunState = 0;
			return 0;
		}

		/* Now get the search node name so we can open it - index is one based */
		status = dsGetDirNodeName(ap->gDirRef, pDataBuff, 1, &pDataList);
		if (status != eDSNoErr)
		{
			dsDataBufferDeAllocate(ap->gDirRef, pDataBuff);
			dsDataListDeallocate(ap->gDirRef, pDataList);
			free(pDataList);
			dsCloseDirService(ap->gDirRef);
			ap->gDirRef = 0;
			ap->gDSRunState = 0;
			return 0;
		}

		/* Open the search node */
		status = dsOpenDirNode(ap->gDirRef, pDataList, &nodeRef);
		dsDataListDeallocate(ap->gDirRef, pDataList);
		free(pDataList);

		if (status == eDSNoErr)
		{
			system_log(LOG_DEBUG, "-- DS: %d: %s: %s.", __LINE__, "cww", "Search node opened");
			ap->gNodeRef = nodeRef;
		}
		else
		{
			dsDataBufferDeAllocate(ap->gDirRef, pDataBuff);
			dsCloseDirService(ap->gDirRef);
			ap->gDirRef = 0;
			ap->gDSRunState = 0;
			return 0;
		}

		/* Deallocate the temp buff */
		dsDataBufferDeAllocate(ap->gDirRef, pDataBuff);
	}

	/*
	 * if DS is running then we use it since in the case of NetInfo default DS can resolve aliases and
	 * lookupd will have consulted NetInfo directly already and not found anything if it makes it here
	 */
	system_log(LOG_DEBUG, "-- DS: %d: %s: ap= %lu, gDirRef= %lu, gNodeRef= %lu.", __LINE__, "canWeWork", (unsigned long)ap, ap->gDirRef, ap->gNodeRef);
	ap->gDSRunState = 1;
	return 1;
}

static char *
mapDSAttrToNetInfoType(const char *inAttrType)
{
	int i = 0;
	char *outResult = NULL;
	unsigned long uiStdLen = strlen(kDSStdAttrTypePrefix); 

	if (inAttrType == NULL) return NULL;

	if (strncmp(inAttrType, kDSStdAttrTypePrefix, uiStdLen) == 0)
	{
		for (i = 0; i < kAttrConsts; i++)
		{
			if (strcmp(inAttrType, sAttrMap[i][0]) == 0)
			{
				outResult = (char *)malloc(strlen(sAttrMap[i][1]) + 1);
				strcpy(outResult, sAttrMap[i][1]);
				break;
			}
		}
	}

	return outResult;
}

static char *
mapNetInfoAttrToDSType(const char *inAttrType)
{
	int i = 0;
	char *outResult = NULL;

	if (inAttrType == NULL) return NULL;

	for (i = 0; i < kAttrConsts; i++)
	{
		if (strcmp(inAttrType, sAttrMap[i][1]) == 0)
		{
			outResult = (char *)malloc(strlen(sAttrMap[i][0]) + 1);
			strcpy(outResult, sAttrMap[i][0]);
			break;
		}
	}

	return outResult;
}

static char *
mapNetInfoRecToDSType(LUCategory inCategory)
{
	switch (inCategory)
	{
		case LUCategoryUser: return(kDSStdRecordTypeUsers);
		case LUCategoryGroup: return(kDSStdRecordTypeGroups);
		case LUCategoryHost: return NULL;
		case LUCategoryNetwork: return NULL;
		case LUCategoryService: return NULL;
		case LUCategoryProtocol: return NULL;
		case LUCategoryRpc: return NULL;
		case LUCategoryMount: return(kDSStdRecordTypeMounts);
		case LUCategoryPrinter: return NULL;
		case LUCategoryBootparam: return NULL;
		case LUCategoryBootp: return NULL;
		case LUCategoryAlias: return NULL;
		case LUCategoryNetDomain: return NULL;
		case LUCategoryEthernet: return NULL;
		case LUCategoryNetgroup: return NULL;
		case LUCategoryInitgroups: return NULL;
		case LUCategoryHostServices: return NULL;
		default:
			system_log(LOG_DEBUG, "-- DS: %d: %s: *** Warning *** Unmapped category: %d", __LINE__, "mapNetInfoRecToDSType", inCategory);
			return NULL;
	}

	return NULL;
}

static dsrecord *
dsrecordFromDS(agent_private *ap, tDataBuffer *buf, int which)
{
	dsrecord *item;
	dsdata *d;
	dsattribute *a;
	tDirStatus status;
	tAttributeListRef attrListRef;
	tRecordEntry *pRecEntry;
	tAttributeEntry *pAttrEntry;
	tAttributeValueListRef valueRef;
	tAttributeValueEntry *pValueEntry;
	char *pNIKey;
	int i, j;

	attrListRef = 0;
	pRecEntry = NULL;

	status = dsGetRecordEntry(ap->gNodeRef, buf, which, &attrListRef, &pRecEntry);
	if (status != eDSNoErr) return NULL;

	item = dsrecord_new();

	for (i = 1; i <= pRecEntry->fRecordAttributeCount; i++)
	{
		pAttrEntry = NULL;
		valueRef = 0;

		status = dsGetAttributeEntry(ap->gNodeRef, buf, attrListRef, i, &valueRef, &pAttrEntry);
		if (status != eDSNoErr) continue;
	
		pNIKey = mapDSAttrToNetInfoType(pAttrEntry->fAttributeSignature.fBufferData);
		if (pNIKey == NULL)
		{
			dsCloseAttributeValueList(valueRef);
			dsDeallocAttributeEntry(ap->gDirRef, pAttrEntry);
			continue;
		}

		d = cstring_to_dsdata(pNIKey);
		a = dsattribute_new(d);
		dsdata_release(d);
		free(pNIKey);

		dsrecord_append_attribute(item, a, SELECT_ATTRIBUTE);

		for (j = 1; j <= pAttrEntry->fAttributeValueCount; j++)
		{
			pValueEntry = NULL;
			status = dsGetAttributeValue(ap->gNodeRef, buf, j, valueRef, &pValueEntry);
			if (status != eDSNoErr) continue;

			d = cstring_to_dsdata(pValueEntry->fAttributeValueData.fBufferData);
			dsattribute_append(a, d);

			dsDeallocAttributeValueEntry(ap->gDirRef, pValueEntry);
		}

		dsattribute_release(a);

		dsCloseAttributeValueList(valueRef);
		dsDeallocAttributeEntry(ap->gDirRef, pAttrEntry);
	}

	dsCloseAttributeList(attrListRef);
	dsDeallocRecordEntry(ap->gDirRef, pRecEntry);

	return item;
}

static void
add_validation(dsrecord *r, int ttl)
{
	dsdata *d;
	dsattribute *a;
	time_t best_before;
	char str[32];

	if (r == NULL) return;

	d = cstring_to_dsdata("_lookup_validation");
	dsrecord_remove_key(r, d, SELECT_ATTRIBUTE);

	a = dsattribute_new(d);
	dsrecord_append_attribute(r, a, SELECT_ATTRIBUTE);

	dsdata_release(d);

	best_before = time(0) + ttl;
	sprintf(str, "%lu", best_before);

	d = cstring_to_dsdata(str);
	dsattribute_append(a, d);
	dsdata_release(d);

	dsattribute_release(a);
}

u_int32_t
DS_query(void *c, dsrecord *pattern, dsrecord **list)
{
	agent_private *ap;
	u_int32_t cat;
	dsattribute *a;
	dsdata *k;
	dsrecord *lastrec;
	dsrecord *item = NULL;
	int match;
	tDirStatus status;
	char *pDSRecType, *catname;
	int i, idx;
	unsigned long ulRecCount = 0;
	tDataList *pRecName = NULL, *pRecType = NULL, *pAttrType = NULL;
	tContextData pContext = NULL;
	tDataBuffer *pDataBuffer;
	int searchOnName = 0; /* if 1 we use dsGetRecordList else we use dsDoAttributeValueSearch */
	tDataNode *pAttrSearchType = NULL, *pAttrSearchValue = NULL;

	if (c == NULL) return 1;
	if (pattern == NULL) return 1;
	if (list == NULL) return 1;

	ap = (agent_private *)c;

	*list = NULL;
	lastrec = NULL;

	k = cstring_to_dsdata(CATEGORY_KEY);
	a = dsrecord_attribute(pattern, k, SELECT_META_ATTRIBUTE);
	dsdata_release(k);

	if (a == NULL) return 1;
	if (a->count == 0) return 1;
	dsrecord_remove_attribute(pattern, a, SELECT_META_ATTRIBUTE);

	catname = dsdata_to_cstring(a->value[0]);
	if (catname == NULL)
	{
		dsattribute_release(a);
		return 1;
	}

	cat = atoi(catname);
	dsattribute_release(a);

	pDSRecType = mapNetInfoRecToDSType(cat);
	if (pDSRecType == NULL) return 1;

	/* we actually need to check DS (in our possible return 1 series)last since other conditions will drop us out */
	/* of the search (like we don't service certain record types) before we grab a DS FW mutex ie. in dsVerifyRefNum */
	if (canWeWork(ap) == 0) return 1;

	k = cstring_to_dsdata(SINGLE_KEY);
	a = dsrecord_attribute(pattern, k, SELECT_META_ATTRIBUTE);
	dsdata_release(k);
	if (a != NULL)
	{
		dsrecord_remove_attribute(pattern, a, SELECT_META_ATTRIBUTE);
		dsattribute_release(a);
		/* only retrieve a single record for this search */
		/* therefore preset the record count to 1 */
		ulRecCount = 1;
	}

	if (pattern->count == 0)
        searchOnName = 1;
    
	/* check if the "name" is in the pattern for our search */
	for (i = 0; i < pattern->count; i++)
	{
		a = pattern->attribute[i];
		if (a != NULL)
		{
			if (a->key != NULL)
			{
				if (a->key->data != NULL)
				{
					system_log(LOG_DEBUG, "-- DS: %d: %s: ap= %lu search on attr type = %s.", __LINE__, "DS_query", (unsigned long)ap, a->key->data);
					if (strcmp(a->key->data,"name") == 0)
					{
						if (a->value != NULL)
						{
							searchOnName = 1;
							pRecName = dsDataListAllocate(ap->gDirRef);
							/* for (idx = 0; idx < a->count; idx++) */
							if (a->count > 0)
							{
								idx = 0;
								/* don't need all the values provided here since we do an exact match later */
								/* then each of the values must be present - so we need search only on the first */
								/* we keep the code here in case the logic of multiple values becomes OR not the current AND */
								if (dsdata_to_cstring(a->value[idx]) != NULL)
								{
									dsAppendStringToListAlloc(ap->gDirRef, pRecName, a->value[idx]->data);
									system_log(LOG_DEBUG, "-- DS: %d: %s: ap= %lu search on attr type value = %s.", __LINE__, "DS_query", (unsigned long)ap, a->value[idx]->data);
								}
							}
						}
						break;
					}
				}
			}
		}
	}
	
	if (searchOnName == 0) /* if not searching on name */
	{
		/* grab first pattern for our search call to DS */
		for (i = 0; i < pattern->count; i++)
		{
			a = pattern->attribute[i];
			if (a != NULL)
			{
				if (a->key != NULL)
				{
					if (a->key->data != NULL)
					{
						if (a->value != NULL)
						{
							/* for (idx = 0; idx < a->count; idx++) */
							if (a->count > 0)
							{
								idx = 0;
								/* can't use all the values provided here but do an exact match later */
								/* then each of the values must be present - so we need search only on the first */
								/* we keep the code here in case the logic of multiple values becomes OR not the current AND */
								if (dsdata_to_cstring(a->value[idx]) != NULL)
								{
									system_log(LOG_DEBUG, "-- DS: %d: %s: ap= %lu search on attr type value = %s of type = %s.", __LINE__, "DS_query", (unsigned long)ap, a->value[idx]->data, a->key->data);
									pAttrSearchValue = dsDataNodeAllocateString(ap->gDirRef, a->value[idx]->data);
									if (pAttrSearchValue == NULL)
									{
										return 1;
									}
								}
							}
							/* grab the attr type as well - make sure it is a DS type*/
							pAttrSearchType = dsDataNodeAllocateString(ap->gDirRef, mapNetInfoAttrToDSType(a->key->data));
							if (pAttrSearchType == NULL)
							{
								if (pAttrSearchValue == NULL)
								{
									dsDataNodeDeAllocate(ap->gDirRef, pAttrSearchValue);
								}
								return 1;
							}
							break;
						}
					}
				}
			}
		}
	}
	
	/* check that we are looking for recName */
	if (pRecName != NULL)
	{
		/* case where "name" was found in pattern but can't extract string out of value */
		if (dsDataListGetNodeCount(pRecName) == 0)
		{
			pRecName = dsBuildListFromStrings(ap->gDirRef, kDSRecordsAll, NULL);
		}
	}
	else if (searchOnName == 1)
	{
		/* case where no "name" pattern was provided for match */
		if (pattern->count == 0)
		{
			pRecName = dsBuildListFromStrings(ap->gDirRef, kDSRecordsAll, NULL);
		}
		else
		{
			/* this means we have already built the args for dsDoAttrValueSearch instead of dsGetRecordList */
		}
	}

	/* And all attributes since this is required by the cache agent for subsequent calls */
	if (searchOnName == 1)
	{
		pAttrType = dsBuildListFromStrings(ap->gDirRef, kDSAttributesStandardAll, NULL);
		if (pAttrType == NULL)
		{
			dsDataListDeallocate(ap->gDirRef, pRecName);
			free(pRecName);
			return 1;
		}
	}

	/* Of this record type */
	pRecType = dsBuildListFromStrings(ap->gDirRef, pDSRecType, NULL);
	if (pDSRecType != NULL)
	{
		/* DON'T free this since it is a constant returned from mapNetInfoRecToDSType */
		pDSRecType = NULL;
	}

	if (pRecType == NULL)
	{
		dsDataListDeallocate(ap->gDirRef, pAttrType);
		free(pAttrType);

		dsDataListDeallocate(ap->gDirRef, pRecName);
		free(pRecName);

		return 1;
	}

	pDataBuffer = dsDataBufferAllocate(ap->gDirRef, kBuffSize);
	if (pDataBuffer == NULL)
	{
		dsDataListDeallocate(ap->gDirRef, pAttrType);
		free(pAttrType);

		dsDataListDeallocate(ap->gDirRef, pRecName);
		free(pRecName);

		dsDataListDeallocate(ap->gDirRef, pRecType);
		free(pRecType);

		return 1;
	}

	do
	{
		do 
		{
			if (searchOnName == 1)
			{
				system_log(LOG_DEBUG, "-- DS: %d: %s: ap= %lu dsGetRecordList.", __LINE__, "DS_query", (unsigned long)ap);
				status = dsGetRecordList(ap->gNodeRef, pDataBuffer, pRecName, eDSExact, pRecType, pAttrType, 0, &ulRecCount, &pContext);
			}
			else
			{
				system_log(LOG_DEBUG, "-- DS: %d: %s: ap= %lu dsDoAttributeValueSearch.", __LINE__, "DS_query", (unsigned long)ap);
				status = dsDoAttributeValueSearch(ap->gNodeRef, pDataBuffer, pRecType, pAttrSearchType, eDSExact, pAttrSearchValue, &ulRecCount, &pContext);
			}

			if (status == eDSBufferTooSmall)
			{
				unsigned long bufSize = pDataBuffer->fBufferSize;
				dsDataBufferDeAllocate(ap->gDirRef, pDataBuffer);
				pDataBuffer = NULL;
				pDataBuffer = dsDataBufferAllocate(ap->gDirRef, bufSize * 2);
			}
		} while (status == eDSBufferTooSmall);
		
		/* If the node ref is invalid then reset it to zero */
		if (status == eDSInvalidNodeRef) ap->gNodeRef = 0;			

		/* Check for error and at least one record found. */
		if ((status == eDSNoErr) && (ulRecCount != 0))
		{				
			for (i = 1; i <= ulRecCount; i++)
			{
				item = dsrecordFromDS(ap, pDataBuffer, i);
				if (item != NULL)
				{
					match = dsrecord_match(item, pattern);
					if (match == 1)
					{
						add_validation(item, ap->gTimeToLive);

						if (*list == NULL) *list = dsrecord_retain(item);
						else lastrec->next = dsrecord_retain(item);

						lastrec = item;
					}
					dsrecord_release(item);
				}
			}
		}
	} while ((status == eDSNoErr) && (pContext != NULL));

	if (pRecType != NULL)
	{
		dsDataListDeallocate(ap->gDirRef, pRecType);
		free(pRecType);
	}

	if (pAttrType != NULL)
	{
		dsDataListDeallocate(ap->gDirRef, pAttrType);
		free(pAttrType);
	}

	if (pRecName != NULL)
	{
		dsDataListDeallocate(ap->gDirRef, pRecName);
		free(pRecName);
	}

	if (pDataBuffer != NULL)
	{
		dsDataBufferDeAllocate(ap->gDirRef, pDataBuffer);
		pDataBuffer = NULL;
	}

	if (pAttrSearchType != NULL)
	{
		dsDataNodeDeAllocate(ap->gDirRef, pAttrSearchType);
		pAttrSearchType = NULL;
	}
	
	if (pAttrSearchValue != NULL)
	{
		dsDataNodeDeAllocate(ap->gDirRef, pAttrSearchValue);
		pAttrSearchValue = NULL;
	}
	
	return 0;

}

u_int32_t
DS_new(void **c, char *args, dynainfo *d)
{
	agent_private *ap;
	dsrecord *r;
	dsattribute *a;
	dsdata *x;
	int status, didSetTTL;

	if (c == NULL) return 1;

	ap = (agent_private *)malloc(sizeof(agent_private));
	*c = ap;

	/* Can DS do the work or are we a noop */
	ap->gDSRunState = 0;

	/* How long before we check our run state */
	ap->gSeconds = 0; 

	/* DS reference and search node reference are globals since DS FW is single threaded */
	ap->gDirRef = 0;
	ap->gNodeRef = 0;

	ap->dyna = d;

	system_log(LOG_DEBUG, "Allocated DS 0x%08x\n", (int)ap);

	ap->gTimeToLive = DefaultTimeToLive;

	r = NULL;
	didSetTTL = 0;

	if (ap->dyna != NULL)
	{
		if (ap->dyna->dyna_config_agent != NULL)
		{
			status = (ap->dyna->dyna_config_agent)(ap->dyna, -1, &r);
			if (status == 0)
			{
				x = cstring_to_dsdata("TimeToLive");
				a = dsrecord_attribute(r, x, SELECT_ATTRIBUTE);
				dsdata_release(x);
				if (a != NULL)
				{
					x = dsattribute_value(a, 0);
					if (x != NULL)
					{
						ap->gTimeToLive = atoi(dsdata_to_cstring(x));
						dsdata_release(x);
						didSetTTL = 1;
					}
					dsattribute_release(a);
				}
				dsrecord_release(r);
			}
		}

		if ((didSetTTL == 0) && (ap->dyna->dyna_config_global != NULL))
		{
			status = (ap->dyna->dyna_config_global)(ap->dyna, -1, &r);
			if (status == 0)
			{
				x = cstring_to_dsdata("TimeToLive");
				a = dsrecord_attribute(r, x, SELECT_ATTRIBUTE);
				dsdata_release(x);
				if (a != NULL)
				{
					x = dsattribute_value(a, 0);
					if (x != NULL)
					{
						ap->gTimeToLive = atoi(dsdata_to_cstring(x));
						dsdata_release(x);
						didSetTTL = 1;
					}
					dsattribute_release(a);
				}
				dsrecord_release(r);
			}
		}
	}

	return 0;
}

u_int32_t
DS_free(void *c)
{
	agent_private *ap;

	if (c == NULL) return 0;

	ap = (agent_private *)c;

	system_log(LOG_DEBUG, "Deallocated DS 0x%08x\n", (int)ap);

	dsCloseDirNode(ap->gNodeRef);
	dsCloseDirService(ap->gDirRef);
	
	free(ap);
	c = NULL;

	return 0;
}

u_int32_t
DS_validate(void *c, char *v)
{
	u_int32_t t;

	if (v == NULL) return 0;

	t = atoi(v);

	if (time(0) > t) return 0;

	return 1;
}
