/* $Copyright: * * Copyright 1998-2000 by the Massachusetts Institute of Technology. *  * All rights reserved. *  * Export of this software from the United States of America may require a * specific license from the United States Government.  It is the * responsibility of any person or organization contemplating export to * obtain such a license before exporting. *  * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and distribute * this software and its documentation for any purpose and without fee is * hereby granted, provided that the above copyright notice appear in all * copies and that both that copyright notice and this permission notice * appear in supporting documentation, and that the name of M.I.T. not be * used in advertising or publicity pertaining to distribution of the * software without specific, written prior permission.  Furthermore if you * modify this software you must label your software as modified software * and not distribute it in such a fashion that it might be confused with * the original MIT software. M.I.T. makes no representations about the * suitability of this software for any purpose.  It is provided "as is" * without express or implied warranty. *  * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. *  * Individual source code files are copyright MIT, Cygnus Support, * OpenVision, Oracle, Sun Soft, FundsXpress, and others. *  * Project Athena, Athena, Athena MUSE, Discuss, Hesiod, Kerberos, Moira, * and Zephyr are trademarks of the Massachusetts Institute of Technology * (MIT).  No commercial use of these trademarks may be made without prior * written permission of MIT. *  * "Commercial use" means use of a name in a product or other for-profit * manner.  It does NOT prevent a commercial firm from referring to the MIT * trademarks in order to convey information (although in doing so, * recognition of their trademark status should be given). * $ *//* $Header: /cvs/repository/iservers/Libraries/passwordserver_sasl/cyrus_sasl/mac/CommonKClient/mac_kclient3/Headers/KerberosManager/KerberosManagerLib.h,v 1.1 2002/02/28 00:28:04 snsimon Exp $ */#pragma once#include <KerberosSupport/KerberosConditionalMacros.h>#if TARGET_API_MAC_OSX && TARGET_API_MAC_CARBON    #include <Carbon/Carbon.h>#elif TARGET_API_MAC_OS8 || TARGET_API_MAC_CARBON    #include <MacTypes.h>    #include <Files.h>    #include <Processes.h>#else    #error "Unknown OS"#endif#ifdef __cplusplusextern "C" {#endif#if PRAGMA_IMPORT#pragma import on#endif/* * KMAE_SendQuitApplication * * Send quit event to Kerberos Manager. * */OSStatusKMAE_SendQuitApplication (Boolean waitForAEReply);/* * KMAE_SendOpenApplication * * Send open event to Kerberos Manager. This will launch Kerberos Manager.  Kerberos * Manager could display an error dialog that requires the user's response if Kerb for * the Mac isn't installed properly, but no AE reply is sent about this. */ OSStatusKMAE_SendOpenApplication (Boolean waitForAEReply);/* * KMAE_SendOpenApplicationFSSpec * * Send open event to Kerberos Manager specified by inKMFileSpec. * This will launch Kerberos Manager.  Kerberos Manager could display * an error dialog that requires the user's response if Kerb for * the Mac isn't installed properly, but no AE reply is sent about this. */ OSStatusKMAE_SendOpenApplicationFSSpec (FSSpec *inKMFileSpec, Boolean waitForAEReply);/* * KMAE_SendLogin * * Tell Kerberos Manager to display login dialog (no AE replies right now). * Kerberos Manager will be launched if necessary. */OSStatusKMAE_SendLogin (Boolean waitForAEReply);/* * IsKerberosManagerRunning * * Determine if Kerberos Manager is running *//* * KMAE_SendLoginFSSpec * * Tell Kerberos Manager specified by inKmFileSpec to display login dialog  * (no AE replies right now) Kerberos Manager will be launched if necessary. * */OSStatusKMAE_SendLoginFSSpec (FSSpec *inKMFileSpec, Boolean waitForAEReply);/*   IsKerberosManagerRunning()   Return true if KM is running, and fills out outPSN if the pointer is non-null.   Return false if KM is not running, and outPSN is unchanged.*/BooleanIsKerberosManagerRunning (ProcessSerialNumber *outPSN);/*   FindKerberosManagerInControlPanels()      Uses IterateDirectory from MoreFiles to search the Control   Panels folder for copies of Kerberos Manager.  If it finds one, returns true and   fills out *kmSpec.  If it doesn't find one or an error occurs, returns false   and *kmSpec is unchanged. */BooleanFindKerberosManagerInControlPanels(FSSpec *kmSpec);/*   KMAE_FindTargetKerberosManager()      Searches the startup volume to find the Kerberos Manager that would receive   AppleEvents if any of the KerberosManagerLib functions that send AEs were called.   First checks to see if KM is running, and returns the FSSpec of that one if it is.   Next looks in the Control Panels Folder.  Finally it searches the drive for   a copy.      If a Kerberos Manager is found, returns true and fills out *kmSpec.    If it doesn't find one or an error occurs, returns false and *kmSpec is unchanged.      If the hard drive catalog changes during the search, continues anyway.*/BooleanKMAE_FindTargetKerberosManager(FSSpec *kmSpec);#ifdef PRAGMA_IMPORT_OFF#pragma import off#elif PRAGMA_IMPORT#pragma import reset#endif#ifdef __cplusplus}#endif/* * Constants */enum {	kKerberosManagerClass		= FOUR_CHAR_CODE ('KrbM')};enum {	kKerberosManagerSignature	= FOUR_CHAR_CODE ('KrbM')};enum {	kAELogin			= FOUR_CHAR_CODE ('Lgin')};