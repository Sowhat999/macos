/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IOKIT_IOAUDIOCONTROLUSERCLIENT_H
#define _IOKIT_IOAUDIOCONTROLUSERCLIENT_H

#include <IOKit/IOUserClient.h>

#include <IOKit/audio/IOAudioTypes.h>

class IOAudioControl;

class IOAudioControlUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOAudioControlUserClient)
    
protected:
    task_t 				clientTask;
    IOAudioControl *			audioControl;
    IOAudioNotificationMessage *	notificationMessage;

    virtual IOReturn clientClose();
    virtual IOReturn clientDied();

protected:
    struct ExpansionData { };
    
    ExpansionData *reserved;

public:
	virtual void sendChangeNotification(UInt32 notificationType);

private:
    OSMetaClassDeclareReservedUsed(IOAudioControlUserClient, 0);

    OSMetaClassDeclareReservedUnused(IOAudioControlUserClient, 1);
    OSMetaClassDeclareReservedUnused(IOAudioControlUserClient, 2);
    OSMetaClassDeclareReservedUnused(IOAudioControlUserClient, 3);
    OSMetaClassDeclareReservedUnused(IOAudioControlUserClient, 4);
    OSMetaClassDeclareReservedUnused(IOAudioControlUserClient, 5);
    OSMetaClassDeclareReservedUnused(IOAudioControlUserClient, 6);
    OSMetaClassDeclareReservedUnused(IOAudioControlUserClient, 7);
    OSMetaClassDeclareReservedUnused(IOAudioControlUserClient, 8);
    OSMetaClassDeclareReservedUnused(IOAudioControlUserClient, 9);
    OSMetaClassDeclareReservedUnused(IOAudioControlUserClient, 10);
    OSMetaClassDeclareReservedUnused(IOAudioControlUserClient, 11);
    OSMetaClassDeclareReservedUnused(IOAudioControlUserClient, 12);
    OSMetaClassDeclareReservedUnused(IOAudioControlUserClient, 13);
    OSMetaClassDeclareReservedUnused(IOAudioControlUserClient, 14);
    OSMetaClassDeclareReservedUnused(IOAudioControlUserClient, 15);

public:
    static IOAudioControlUserClient *withAudioControl(IOAudioControl *control, task_t clientTask, void *securityID, UInt32 type);

    virtual bool initWithAudioControl(IOAudioControl *control, task_t owningTask, void *securityID, UInt32 type);
    virtual void free();

    virtual IOReturn registerNotificationPort(mach_port_t port, UInt32 type, UInt32 refCon);

    virtual void sendValueChangeNotification();
};

#endif /* _IOKIT_IOAUDIOCONTROLUSERCLIENT_H */
