/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <IOKit/IOLib.h>
#include <IOKit/platform/ApplePlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLocks.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/graphics/IOGraphicsPrivate.h>
#include <IOKit/graphics/IOGraphicsInterfaceTypes.h>
#include <IOKit/ndrvsupport/IONDRVFramebuffer.h>
#include <IOKit/pci/IOAGPDevice.h>
#include <IOKit/assert.h>

#include <libkern/c++/OSContainers.h>
#include <string.h>

#include "IONDRV.h"
#include "IONDRVI2CInterface.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn _IONDRVLibrariesInitialize( IOService * provider );

#define IONDRVCHECK	0
#define IONDRVI2CLOG	0

#define kFirstDepth	kDepthMode1

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct IONDRVFramebufferPrivate {
    IOOptionBits	displayConnectFlags;
    unsigned int 	ackConnectChange:1;
    unsigned int 	postWakeProbe:1;
    unsigned int 	pad1:30;
    UInt32		desiredGammaWidth;
    UInt32		desiredGammaCount;
};


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOATINDRV : public IONDRVFramebuffer
{
    OSDeclareDefaultStructors(IOATINDRV)

public:
    virtual IOReturn getStartupDisplayMode( IODisplayModeID * displayMode,
                            IOIndex * depth );
    virtual IODeviceMemory * findVRAM( void );

};

class IOATI128NDRV : public IOATINDRV
{
    OSDeclareDefaultStructors(IOATI128NDRV)

public:
    virtual void flushCursor( void );
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOBootNDRV : public IONDRV
{
    OSDeclareDefaultStructors(IOBootNDRV)

private:
    enum { kIOBootNDRVDisplayMode = 100 };

    void *	fAddress;
    UInt32	fRowBytes;
    UInt32	fWidth;
    UInt32	fHeight;
    UInt32	fBitsPerPixel;

public:

    static IONDRV * fromRegistryEntry( IORegistryEntry * regEntry );

    virtual void free( void );

    virtual IOReturn getSymbol( const char * symbolName,
				IOLogicalAddress * address );

    virtual const char * driverName( void );

    virtual IOReturn doDriverIO( UInt32 commandID, void * contents,
				 UInt32 commandCode, UInt32 commandKind );

private:

    static bool getUInt32Property( IORegistryEntry * regEntry, const char * name, 
                                    UInt32 * result );
    IOReturn doControl( UInt32 code, void * params );
    IOReturn doStatus( UInt32 code, void * params );

};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* generic nub for multihead devices */

class IONDRVDevice : public IOPlatformDevice
{
    OSDeclareDefaultStructors(IONDRVDevice)

public:
    virtual void joinPMtree( IOService * driver );
};


#undef super
#define super IOPlatformDevice

OSDefineMetaClassAndStructors(IONDRVDevice, IOPlatformDevice)

void IONDRVDevice::joinPMtree( IOService * driver )
{
    IOService * realDevice;
    realDevice = OSDynamicCast( IOService, getParentEntry(gIODTPlane) );
    if( realDevice)
        realDevice->addPowerChild(driver);
    else
        super::joinPMtree( driver );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct _VSLService {
    class IONDRVFramebuffer *	framebuffer;
    IOSelect			type;
    IOFBInterruptProc  		handler;
    OSObject *			target;
    void *			ref;
    _VSLService *		next;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// IONDRVFramebuffer has three power states: sleep, doze, wake.

enum {
    kNDRVFramebufferSleepState		= 0,
    kNDRVFramebufferDozeState		= 1,
    kNDRVFramebufferWakeState		= 2,
    kIONDRVFramebufferPowerStateCount	= 3,
    kIONDRVFramebufferPowerStateMax	= kIONDRVFramebufferPowerStateCount - 1
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOFramebuffer

OSDefineMetaClassAndStructors(IONDRVFramebuffer, IOFramebuffer)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOService * IONDRVFramebuffer::probe( IOService * 	provider,
                                        SInt32 *	score )
{
    IOService *		inst = this;
    IOService *		newInst = 0;
    const char *	name;

    if( !super::probe( provider, score ))
	return( 0 );

    if( 0 != provider->getProperty(kIONDRVIgnoreKey))
        return( 0 );

    if( getProperty(gIONameMatchedKey)) {
        // matched 
        provider->setProperty(kIONDRVForXKey, this, sizeof(this) );

        // temporary for in-kernel acceleration
        name = provider->getName();
        if( (0 == strncmp("ATY,Rage128", name, strlen("ATY,Rage128")))
         || (0 == strncmp("ATY,RageM3", name, strlen("ATY,RageM3"))))
            newInst = new IOATI128NDRV;
        else if( 0 == strncmp("ATY,", name, strlen("ATY,")))
            newInst = new IOATINDRV;

	if( newInst) {
            if( ! newInst->init( inst->getPropertyTable())) {
                newInst->release();
                newInst = 0;
            }
	    inst = newInst;
	}
    }

    return( inst );
}

IOReturn IONDRVFramebuffer::setProperties( OSObject * properties )
{
    OSDictionary *	dict;
    OSData *		data;
    IOReturn		kr = kIOReturnUnsupported;

    if( !(dict = OSDynamicCast( OSDictionary, properties)))
	return( kIOReturnBadArgument);

    if( (data = OSDynamicCast(OSData,
		dict->getObject("driver,AAPL,MacOS,PowerPC")))) {

        if( ndrvState)
            return( kIOReturnStillOpen );

	if( ndrv)
            ndrv->release();
	nub->setProperty("driver,AAPL,MacOS,PowerPC", data);
	nub->removeProperty("AAPL,ndrvInst");
        ndrv = IOPEFNDRV::fromRegistryEntry( nub, &_undefinedSymbolHandler, (void *) this );
	if( ndrv)
            setName( ndrv->driverName());
	kr = kIOReturnSuccess;
    }

    return( kr );
}

bool IONDRVFramebuffer::start( IOService * provider )
{
    bool		ok = false;
    IOService *		parent = 0;
    OSData *		data;

    if( 0 == getProperty(gIONameMatchedKey)) {

        // !matched 
        IORegistryIterator * iter;
        IORegistryEntry *    next;
        IOService *	     nub;
        OSArray *	     toDo = 0;
        bool		     firstLevel;

        if( 0 == provider->getProperty("AAPL,ndrv-dev")) {

            provider->setProperty("AAPL,ndrv-dev", kOSBooleanTrue);
            iter = IORegistryIterator::iterateOver( provider, gIODTPlane, 0 );
            toDo = OSArray::withCapacity(2);

            if( iter && toDo) {
                bool haveDoneLibInit = false;
                UInt32 index = 0;
                do { 
                    while( (next = (IORegistryEntry *) iter->getNextObject())) {

                        firstLevel = (provider == next->getParentEntry(gIODTPlane));
                        if( firstLevel) {
                            data = OSDynamicCast( OSData, next->getProperty("device_type"));
                            if( !data || (0 != strcmp("display", (char *) data->getBytesNoCopy())))
                                continue;
                
                            if( !haveDoneLibInit) {
                                haveDoneLibInit = (kIOReturnSuccess == _IONDRVLibrariesInitialize(provider));
                                if( !haveDoneLibInit)
                                    continue;
                            }
                            next->setProperty( kIOFBDependentIDKey, (UInt64) (UInt32) provider, 64 );
                            next->setProperty( kIOFBDependentIndexKey, index, 32 );
                            index++;
                        }

                        toDo->setObject( next );
                        iter->enterEntry();
                    }

                } while( iter->exitEntry());
            }
            if( iter)
                iter->release();
            
            if( toDo) {
                for( unsigned int i = 0;
                     (next = (IORegistryEntry *) toDo->getObject(i));
                     i++) {
                    nub = new IONDRVDevice;
                    if( !nub)
                        continue;
                    if( !nub->init( next, gIODTPlane )) {
                        nub->free();
                        nub = 0;
                        continue;
                    }
                    nub->attach( getPlatform() );
                    nub->registerService();
                }
                toDo->release();
            }
        }
        return( false );
    }

    do {
        cachedVDResolution.csDisplayModeID = kDisplayModeIDInvalid;

        __private = IONew( IONDRVFramebufferPrivate, 1 );
        if( !__private)
            continue;
        bzero( __private, sizeof(IONDRVFramebufferPrivate) );

   	nub = provider;
        ndrv = IOPEFNDRV::fromRegistryEntry( provider, &_undefinedSymbolHandler, (void *) this );
        if( ndrv)
            setName( ndrv->driverName());
        consoleDevice = (0 != provider->getProperty("AAPL,boot-display"));
        powerState = kIONDRVFramebufferPowerStateMax;

        if( 0 == nub->getDeviceMemoryCount()) {
            parent = OSDynamicCast( IOService, nub->getParentEntry(gIODTPlane));
            if( parent) {
                parent->getResources();
                OSArray * array = parent->getDeviceMemory();
                array->retain();
                nub->setDeviceMemory( array);
                array->release();
            }
        }
        if( parent)
            device = parent;
        else
            device = nub;

        RegEntryID regEntry;
        MAKE_REG_ENTRY( regEntry, nub);
        nub->setProperty( "AAPL,RegEntryID", regEntry, sizeof(regEntry) );

        if( false == super::start( nub ))
	    continue;

        OSObject * obj;
        if( (obj = nub->getProperty( kIOFBDependentIDKey )))
            setProperty( kIOFBDependentIDKey, obj );
        if( (obj = nub->getProperty( kIOFBDependentIndexKey )))
            setProperty( kIOFBDependentIndexKey, obj );

        platformSleep = (false == getPlatform()->hasPrivPMFeature( kPMHasLegacyDesktopSleepMask )
                      && (false == getPlatform()->hasPMFeature( kPMCanPowerOffPCIBusMask )));

        IOOptionBits flags = getPMRootDomain()->getSleepSupported();
        getPMRootDomain()->setSleepSupported(flags & ~kFrameBufferDeepSleepSupported);

        // default flags can be overriden
        accessFlags = 0;
        if(0 == strncmp("3Dfx", provider->getName(), strlen("3Dfx")))
            accessFlags |= kFramebufferDisableAltivecAccess;

	if( (data = OSDynamicCast( OSData, provider->getProperty(kIOFBHostAccessFlagsKey))))
            accessFlags = *((UInt32 *) data->getBytesNoCopy());

	ok = true;			// Success

    } while( false);
    
    return( ok);
}

bool IONDRVFramebuffer::isConsoleDevice( void )
{
    return( consoleDevice );
}

IOReturn IONDRVFramebuffer::enableController( void )
{
    IOReturn		err;
    const char *	logname;

    logname = getProvider()->getName();

    if( 0 == strcmp( "control", logname))
        waitForService( resourceMatching( "IOiic0" ));

    do {

        getProvider()->setProperty("AAPL,ndrv-dev", kOSBooleanTrue);
	
        // wait for accelerator module, display parameter drivers
        // device->waitQuiet();
        // find out about onboard audio/video jack state
        // OSObject * notify =
        addNotification( gIOPublishNotification,
                         resourceMatching(kAppleAudioVideoJackStateKey), 
                         _videoJackStateChangeHandler, this, 0 );

	err = checkDriver();

        if( err) {
            IOLog("%s: Not usable\n", logname );
            if( err == -999)
                IOLog("%s: driver incompatible.\n", logname );
            continue;
        }
        UInt32 isOnline = true;
        if( (kIOReturnSuccess != getAttributeForConnection( 0, kConnectionEnable, &isOnline ))
          || isOnline || true) {

            getCurrentConfiguration();

	    if( !device->getProperty("IONVRAMProperty"))
		setStartupDisplayMode( currentDisplayMode, currentDepth );
        }
        online = isOnline;
        vramMemory = findVRAM();

        UInt8 probeType[32];
        if( (nub != device) && (noErr == _doStatus( this, cscProbeConnection, &probeType ))) {
            setProperty( kIOFBProbeOptionsKey, kIOFBUserRequestProbe, 32);
            supportsProbe = true;
        }

	OSData * data;
	if( (data = OSDynamicCast( OSData, device->getProperty(kIOAGPCommandValueKey))))
            accessFlags |= (kIOAGPFastWrite & (*((UInt32 *) data->getBytesNoCopy())))
			? kFramebufferAGPFastWriteAccess : 0;

        // initialize power management of the device
        initForPM();
	device->setProperty(kIOPMIsPowerManagedKey, true);

    } while( false);

    return( err);
}

bool IONDRVFramebuffer::_videoJackStateChangeHandler( void * target, void * ref,
                                                        IOService * resourceService )
{
    IONDRVFramebuffer * self = (IONDRVFramebuffer *) target;
    IOReturn		err;
    UInt32		jackData;

    OSObject * jackValue = resourceService->getProperty(kAppleAudioVideoJackStateKey);
    if( !jackValue)
        return( true );

    jackData = (jackValue == kOSBooleanTrue);
    IOLog(kAppleAudioVideoJackStateKey " %ld\n", jackData);

    self->nub->setProperty( kAppleAudioVideoJackStateKey, &jackData, sizeof(jackData) );   
    resourceService->removeProperty(kAppleAudioVideoJackStateKey);

    if( self->avJackState != jackData) {
        if( !self->captured) {
            err = self->doControl( cscProbeConnection, 0 );
        }
        self->avJackState = jackData;
    }

    return( true );
}

IOReturn IONDRVFramebuffer::_probeAction( IONDRVFramebuffer * self, IOOptionBits options )
{
    IOReturn err = kIOReturnSuccess;

    if( self->captured) {
        err = kIOReturnBusy;

    } else if( options & kIOFBUserRequestProbe) {

        if( !self->supportsProbe) 
            err = kIOReturnUnsupported;

        else do {

            OSNumber * num = OSDynamicCast(OSNumber, self->getProperty(kIOFBDependentIndexKey));
            if( num && (0 != num->unsigned32BitValue()))
                continue;

	    err = self->_doControl( self, cscProbeConnection, 0 );
	    IONDRVFramebuffer * other;
	    if( (other = OSDynamicCast( IONDRVFramebuffer, self->nextDependent))) {
		other->_doControl( other, cscProbeConnection, 0 );
	    }

        } while( false );

    } else if( options & kIOFBForceReadEDID) {

        if( !self->forceReadEDID) {

            _VSLService *	service;
            IOFBInterruptProc	proc;

            self->forceReadEDID = 1;

            for( service = self->vslServices;
                 service && (kIOFBConnectInterruptType != service->type);
                 service = service->next ) {}
            if( service && (proc = service->handler))
                (*proc) (service->target, service->ref);
        }
        err = kIOReturnSuccess;

    } else
        err = kIOReturnBadArgument;

    return( err );
}

IOReturn IONDRVFramebuffer::requestProbe( IOOptionBits options )
{
    IOReturn	 err;
    IOWorkLoop * wl;

    if( (wl = getWorkLoop())) {
        err = wl->runAction( (IOWorkLoop::Action) &_probeAction, this, (void *) options );
    } else
        err = kIOReturnNotReady;

    return( err );
}

IODeviceMemory * IONDRVFramebuffer::getVRAMRange( void )
{
    if( vramMemory)
	vramMemory->retain();

    return( vramMemory );
}

const IOTVector * IONDRVFramebuffer::_undefinedSymbolHandler( void * self, 
                            const char * libraryName, const char * symbolName )
{
    return( ((IONDRVFramebuffer *)self)->undefinedSymbolHandler( libraryName, symbolName) );
}

const IOTVector * IONDRVFramebuffer::undefinedSymbolHandler( const char * libraryName,
                                                       const char * symbolName )
{
    return( 0 );
}

void IONDRVFramebuffer::free( void )
{
    if( __private) {
        IODelete( __private, IONDRVFramebufferPrivate, 1 );
        __private = 0;
    }
    super::free();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IONDRVFramebuffer::registerForInterruptType( IOSelect interruptType,
                        IOFBInterruptProc proc, OSObject * target, void * ref,
			void ** interruptRef )

{
    _VSLService *	service;
    IOReturn		err;

    if( (interruptType == kIOFBVBLInterruptType)
        && (getProvider()->getProperty("Ignore VBL")))
	return( kIOReturnUnsupported );

    for( service = vslServices;
	 service && (service->type != interruptType);
	 service = service->next ) {}

    if( service) {

	if( service->handler)
	    err = kIOReturnBusy;

	else {
	    service->target	= target;
	    service->ref	= ref;
	    service->handler	= proc;
	    *interruptRef	= service;
	    err			= kIOReturnSuccess;
	}

    } else
	err = kIOReturnNoResources;

    return( err );
}

IOReturn IONDRVFramebuffer::unregisterInterrupt( void * interruptRef )
{
    _VSLService * service = (_VSLService *) interruptRef;

    service->handler = 0;

    return( kIOReturnSuccess );
}

IOReturn IONDRVFramebuffer::setInterruptState( void * interruptRef, 
						UInt32 state )
{
    return( kIOReturnUnsupported );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

//// VSL calls

OSStatus IONDRVFramebuffer::VSLNewInterruptService(
                                        void * entryID,
                                        IOSelect serviceType,
                                        _VSLService ** vslService )
{
    IORegistryEntry *	regEntry;
    IONDRVFramebuffer *	fb;
    _VSLService *	service;
    IOReturn		err = kIOReturnSuccess;

    REG_ENTRY_TO_OBJ( (const RegEntryID *) entryID, regEntry)

    fb = OSDynamicCast( IONDRVFramebuffer,
		regEntry->getChildEntry( gIOServicePlane ));
    assert( fb );

    if( fb) {
	service = IONew( _VSLService, 1 );

	if( service) {
            service->framebuffer	= fb;
            service->type		= serviceType;
	    service->handler		= 0;
            service->next = fb->vslServices;
            fb->vslServices = service;

            *vslService = service;

	} else
	    err = kIOReturnNoMemory;

    } else
	err = kIOReturnBadArgument;

    return( err );
}

OSStatus IONDRVFramebuffer::VSLDisposeInterruptService(_VSLService * vslService)
{
    IONDRVFramebuffer *	fb;
    _VSLService * 	next;
    _VSLService * 	prev;

    if( vslService) {

	fb = vslService->framebuffer;

        prev = fb->vslServices;
	if( prev == vslService)
	    fb->vslServices = vslService->next;
	else {
	    while( ((next = prev->next) != vslService) && next)
		prev = next;
	    if( next)
		prev->next = vslService->next;
	}

	IODelete( vslService, _VSLService, 1 );
    }

    return( kIOReturnSuccess );
}

OSStatus IONDRVFramebuffer::VSLDoInterruptService( _VSLService * vslService )
{
    IOFBInterruptProc	proc;

    if( vslService) {

        if( kIOFBConnectInterruptType == vslService->type)
            vslService->framebuffer->__private->ackConnectChange = true;

	if( (proc = vslService->handler))
	    (*proc) (vslService->target, vslService->ref);
    }

    return( kIOReturnSuccess );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

struct _VSLCursorRef {
    IOFramebuffer *	framebuffer;
    void * 		cursorImage;
};

Boolean IONDRVFramebuffer::VSLPrepareCursorForHardwareCursor(
                                        void * cursorRef,
                                        IOHardwareCursorDescriptor * hwDesc,
                                        IOHardwareCursorInfo * hwCursorInfo )
{
    _VSLCursorRef *	cursor = (_VSLCursorRef *) cursorRef;
    bool		ok;

    if( hwCursorInfo->colorMap)
        hwCursorInfo->colorMap += 1;
    ok = cursor->framebuffer->convertCursorImage(
		cursor->cursorImage, hwDesc, hwCursorInfo );
    if( hwCursorInfo->colorMap)
        hwCursorInfo->colorMap -= 1;

    return( ok );
}

IOReturn IONDRVFramebuffer::setCursorImage( void * cursorImage )
{
    _VSLCursorRef		cursorRef;
    VDSetHardwareCursorRec	setCursor;
    IOReturn			err;

    if( 0 == powerState)
        return( kIOReturnSuccess );
    if( !online)
        return( kIOReturnSuccess );

    cursorRef.framebuffer = this;
    cursorRef.cursorImage = cursorImage;

    setCursor.csCursorRef = (void *) &cursorRef;
    setCursor.csReserved1 = 0;
    setCursor.csReserved2 = 0;

    err = _doControl( this, cscSetHardwareCursor, &setCursor );

    mirrorSWCursor = (mirrored && !mirrorPrimary && (kIOReturnSuccess != err));
    if( mirrorSWCursor)
        err = kIOReturnSuccess;

    return( err );
}

IOReturn IONDRVFramebuffer::setCursorState( SInt32 x, SInt32 y, bool visible )
{
    VDDrawHardwareCursorRec	drawCursor;
    IOReturn			err;

    if( mirrorSWCursor || !online)
        return( kIOReturnSuccess );

    if( 0 == powerState)
        return( kIOReturnSuccess );

    if( 0 == OSIncrementAtomic( &ndrvEnter))
    {

        drawCursor.csCursorX 	= x;
        drawCursor.csCursorY 	= y;
        drawCursor.csCursorVisible 	= visible;
        drawCursor.csReserved1 	= 0;
        drawCursor.csReserved2 	= 0;

        err = _doControl( this, cscDrawHardwareCursor, &drawCursor );

    } else {
	err = kIOReturnBusy;
    }

    OSDecrementAtomic( &ndrvEnter );

    return( err );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IONDRVFramebuffer::doDriverIO( UInt32 commandID, void * contents,
                                        UInt32 commandCode, UInt32 commandKind )
{
    IOReturn err;

    if( kIONDRVInitializeCommand == commandCode) {

        if( !ndrv) {
            ndrv = IOBootNDRV::fromRegistryEntry( nub );
            if( ndrv)
                setName( ndrv->driverName());
        }
    }

    if( ndrv) {
        OSIncrementAtomic( &ndrvEnter );
        err = ndrv->doDriverIO( commandID, contents, commandCode, commandKind );
        OSDecrementAtomic( &ndrvEnter );
    } else
        err = kIOReturnUnsupported;

    return( err );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IONDRVFramebuffer::_doControl( IONDRVFramebuffer * self, UInt32 code, void * params )
{
    IOReturn			err;
    IONDRVControlParameters	pb;

#if IONDRVCHECK
    IOWorkLoop * wl;
    if( !ml_at_interrupt_context() && (wl = self->getWorkLoop()) && !wl->inGate())
	DEBG("control not on gate: %ld\n", code);
#endif

    if( self->ndrvState == 0)
	return( kIOReturnNotOpen);

    pb.code = code;
    pb.params = params;

    err = self->doDriverIO( /*ID*/ (UInt32) &pb, &pb,
                     kIONDRVControlCommand, kIONDRVImmediateIOCommandKind );

    return( err );
}

IOReturn IONDRVFramebuffer::_doStatus( IONDRVFramebuffer * self, UInt32 code, void * params )
{
    IOReturn			err;
    IONDRVControlParameters	pb;

    if( self->ndrvState == 0)
	return( kIOReturnNotOpen);

#if IONDRVCHECK
    IOWorkLoop * wl;
    if( !ml_at_interrupt_context() && (wl = self->getWorkLoop()) && !wl->inGate())
	DEBG("status not on gate: %ld\n", code);
#endif

    pb.code = code;
    pb.params = params;

    err = self->doDriverIO( /*ID*/ (UInt32) &pb, &pb,
                      kIONDRVStatusCommand, kIONDRVImmediateIOCommandKind );

    return( err );
}

IOReturn IONDRVFramebuffer::extControl( OSObject * owner, void * code, void * params )
{
    IONDRVFramebuffer * self = (IONDRVFramebuffer *) owner;
    IOReturn		err;

    if( self->powerState)
	err = _doControl( self, (UInt32) code, params );
    else
	err = kIOReturnNotReady;

    return( err );
}

IOReturn IONDRVFramebuffer::extStatus( OSObject * owner, void * code, void * params )
{
    IONDRVFramebuffer * self = (IONDRVFramebuffer *) owner;
    IOReturn		err;

    if( self->powerState)
	err = _doStatus( self, (UInt32) code, params );
    else
	err = kIOReturnNotReady;

    return( err );
}

IOReturn IONDRVFramebuffer::doControl( UInt32 code, void * params )
{
    return( getWorkLoop()->runAction( (IOWorkLoop::Action) &extControl, this, (void *) code, params));
}

IOReturn IONDRVFramebuffer::doStatus( UInt32 code, void * params )
{
    return( getWorkLoop()->runAction( (IOWorkLoop::Action) &extStatus, this, (void *) code, params));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IONDRVFramebuffer::checkDriver( void )
{
    OSStatus			err = noErr;
    IONDRVControlParameters	pb;
    VDClutBehavior		clutSetting;
    VDGammaRecord		gammaRec;
    VDSwitchInfoRec		switchInfo;
    VDPageInfo			pageInfo;

    if( ndrvState == 0) {

        do {
            err = doDriverIO( 0, nub,
                              kIONDRVInitializeCommand, kIONDRVImmediateIOCommandKind );
            if( err)
                continue;
            err = doDriverIO( 0, &pb,
                              kIONDRVOpenCommand, kIONDRVImmediateIOCommandKind );
        } while( false );
        if( err)
            return( err);

        // allow calls to ndrv
        ndrvState = 2;
        
        IONDRVI2CInterface::create( this );

#if IONDRVI2CLOG
        do {
            VDCommunicationInfoRec	commInfo;
        
            bzero( &commInfo, sizeof( commInfo));
            commInfo.csBusID = kVideoDefaultBus;

            err = _doStatus( this, cscGetCommunicationInfo, &commInfo );
            IOLog("%s: cscGetCommunicationInfo: ", getName());
            if( kIOReturnSuccess != err) {
                IOLog("fails with %ld\n", err);
                continue;
            }
            IOLog("csBusType %lx, csMinBus %lx, csMaxBus %lx\n"
                    "csSupportedTypes %lx, csSupportedCommFlags %lx\n",
                    commInfo.csBusType,
                    commInfo.csMinBus, commInfo.csMaxBus,
                    commInfo.csSupportedTypes, commInfo.csSupportedCommFlags);

            if( commInfo.csSupportedTypes & (1<<kVideoDDCciReplyType) )
                IOLog("supports kVideoDDCciReplyType, ");
            if( commInfo.csSupportedTypes & (1<<kVideoSimpleI2CType) ) {
                IOLog("supports kVideoSimpleI2CType");
                VDCommunicationRec	comm;
                UInt8			edidData[132];
                UInt8			edidRequest[2];
        
                memset( edidData, 0xAA, sizeof( edidData));

                edidRequest[0]		= 0;
                edidRequest[1]		= 0;
                
                comm.csBusID		= kVideoDefaultBus;
                comm.csCommFlags	= 0;
                comm.csMinReplyDelay	= 0;
                comm.csReserved2	= 0;
        
                comm.csSendAddress	= 0xA0;
                comm.csSendType		= kVideoSimpleI2CType;
                comm.csSendBuffer	= &edidRequest[0];
                comm.csSendSize		= 0x01;
        
                comm.csReplyAddress	= 0xA1;
                comm.csReplyType	= kVideoSimpleI2CType;
                comm.csReplyBuffer	= &edidData[0];
                comm.csReplySize	= 128;
        
                comm.csReserved3	= 0;
                comm.csReserved4	= 0;
                comm.csReserved5	= 0;
                comm.csReserved6	= 0;

                do {
                    err = _doControl( this, cscDoCommunication, &comm );
                    IOLog("\nI2C read block[%x](%ld)\n", edidRequest[0], err);
                    if( kIOReturnSuccess != err)
                        break;
                    IOLog("    0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
                    for( int i = 0; i < 128; i++) {
                        if( 0 == (i & 15))
                            IOLog("\n%02x: ", i);
                        IOLog("%02x ", edidData[i]);
                    }
                    IOLog("\n");
                    if( edidRequest[0] || (0 == edidData[126]))
                        break;
                    edidRequest[0] = 0x80;
                } while( true );
            }
        
        } while( false );
#endif /* IONDRVI2CLOG */

        // duplicate QD InitGDevice
        pageInfo.csMode = switchInfo.csMode;
        pageInfo.csData = 0;
        pageInfo.csPage = 0;
        _doControl( this, cscGrayPage, &pageInfo);

        clutSetting = kSetClutAtSetEntries;
        lastClutSetting = clutSetting;
        _doControl( this, cscSetClutBehavior, &clutSetting);

        do {
            VDDisplayTimingRangeRec	rangeRec;
            VDScalerInfoRec		scalerRec;

            bzero( &rangeRec, sizeof( rangeRec));
            rangeRec.csRangeSize = sizeof( rangeRec);
            err = _doStatus( this, cscGetTimingRanges, &rangeRec );
            if( kIOReturnSuccess == err)
                setProperty( kIOFBTimingRangeKey, &rangeRec, sizeof( rangeRec));

            bzero( &scalerRec, sizeof( scalerRec));
            scalerRec.csScalerInfoSize = sizeof( scalerRec);
            err = _doStatus( this, cscGetScalerInfo, &scalerRec );
            if( kIOReturnSuccess == err)
                setProperty( kIOFBScalerInfoKey, &scalerRec, sizeof( scalerRec));

        } while( false );

        VDGetGammaListRec scan;
        GammaTbl *	  table;

        err = _doStatus( this, cscGetGamma, &gammaRec );
        table = (GammaTbl *) gammaRec.csGTable;
        if( table && (err == kIOReturnSuccess)) {
            __private->desiredGammaWidth = table->gDataWidth;
            __private->desiredGammaCount = table->gDataCnt;
        } else {
            __private->desiredGammaWidth = 8;
            __private->desiredGammaCount = (1UL << 8);
	}

	bool setGamma = true;
	for( scan.csPreviousGammaTableID = kGammaTableIDFindFirst;
                ;
                scan.csPreviousGammaTableID = scan.csGammaTableID) {

	    VDRetrieveGammaRec	get;
	    char		name[ 64 ];

	    scan.csGammaTableName = name;
	    err = _doStatus( this, cscGetGammaInfoList, &scan);
	    if( err || (scan.csGammaTableID == (GammaTableID) kGammaTableIDNoMoreTables))
		break;

	    table = (GammaTbl *) IOMalloc(scan.csGammaTableSize);
	    if( 0 == table)
		continue;
	    get.csGammaTableID  = scan.csGammaTableID;
	    get.csGammaTablePtr = table;
	    
	    err = _doStatus( this, cscRetrieveGammaTable, &get );
	    if( noErr == err) {
		DEBG("gamma %s: %d * %d\n", scan.csGammaTableName, table->gDataWidth, table->gDataCnt);
		if( ((UInt32) table->gDataWidth) > __private->desiredGammaWidth) {
		    __private->desiredGammaWidth = table->gDataWidth;
		    __private->desiredGammaCount = table->gDataCnt;
		}
		if( setGamma) {
		    gammaRec.csGTable = (Ptr) table;
		    _doControl( this, cscSetGamma, &gammaRec );
		}
		setGamma = false;
	    }

	    IOFree( table, scan.csGammaTableSize);
	}

        setProperty("IOFBGammaWidth", __private->desiredGammaWidth, 32);
        setProperty("IOFBGammaCount", __private->desiredGammaCount, 32);
    }
    return( noErr);
}


UInt32 IONDRVFramebuffer::iterateAllModes( IODisplayModeID * displayModeIDs )
{
    VDResolutionInfoRec	info;
    UInt32		num = 0;

    info.csPreviousDisplayModeID = kDisplayModeIDFindFirstResolution;

    while( 
 	   (noErr == _doStatus( this, cscGetNextResolution, &info))
	&& ((SInt32) info.csDisplayModeID > 0) ) {

	    if( displayModeIDs)
		displayModeIDs[ num ] = info.csDisplayModeID;

	    info.csPreviousDisplayModeID = info.csDisplayModeID;
	    num++;
    }

    if( detailedTimings) {
        IOItemCount	count, i;

        count = detailedTimings->getCount();
        if( displayModeIDs) {
            for( i = 0; i < count; i++)
		displayModeIDs[ num + i ] = kDisplayModeIDReservedBase + i;
        }
        num += count;
    }

    return( num);
}

IOReturn IONDRVFramebuffer::getResInfoForArbMode( IODisplayModeID modeID,
                                                IODisplayModeInformation * info )
{
    VDVideoParametersInfoRec	pixelParams;
    VPBlock			pixelInfo;
    VDDetailedTimingRec *	detailed;
    IOIndex			depth;
    IOReturn			err;

    err = validateDisplayMode( modeID, 0, &detailed );

    for( depth = -1; err == kIOReturnSuccess; ) {
        pixelParams.csDisplayModeID = modeID;
        pixelParams.csDepthMode = ++depth + kFirstDepth;
        pixelParams.csVPBlockPtr = &pixelInfo;
        err = _doStatus( this, cscGetVideoParameters, &pixelParams );
    }

    if( depth) {
        info->maxDepthIndex	= depth - 1;
        if( detailed) {
	    IODetailedTimingInformationV2 * desc = (IODetailedTimingInformationV2 *) detailed;
	    if( desc->horizontalScaled && desc->verticalScaled) {
		info->nominalWidth	= desc->horizontalScaled;
		info->nominalHeight	= desc->verticalScaled;
	    } else {
		info->nominalWidth	= detailed->csHorizontalActive;
		info->nominalHeight	= detailed->csVerticalActive;
	    }
            info->refreshRate	= detailed->csPixelClock * 65536ULL / 
                                ((detailed->csVerticalActive + detailed->csVerticalBlanking)
                                * (detailed->csHorizontalActive + detailed->csHorizontalBlanking));
        } else {
            info->nominalWidth	= pixelInfo.vpBounds.right;
            info->nominalHeight	= pixelInfo.vpBounds.bottom;
            info->refreshRate	= 0;
        }

        err = kIOReturnSuccess;
    }

    return( err );
}

IOReturn IONDRVFramebuffer::getResInfoForMode( IODisplayModeID modeID,
                                                IODisplayModeInformation * info )
{

    bzero( info, sizeof( *info));

    if( (UInt32) modeID >= (UInt32) kDisplayModeIDReservedBase)
        return( getResInfoForArbMode( modeID, info ));

    // unfortunately, there is no "kDisplayModeIDFindSpecific"
    if( cachedVDResolution.csDisplayModeID != (UInt32) modeID) {

        // try the next after cached mode
        cachedVDResolution.csPreviousDisplayModeID = cachedVDResolution.csDisplayModeID;
        if( (noErr != _doStatus( this, cscGetNextResolution, &cachedVDResolution))
         || (cachedVDResolution.csDisplayModeID != (UInt32) modeID) ) {
    
            // else full blown iterate
            cachedVDResolution.csPreviousDisplayModeID = kDisplayModeIDFindFirstResolution;
            while(
                (noErr == _doStatus( this, cscGetNextResolution, &cachedVDResolution))
             && (cachedVDResolution.csDisplayModeID != (UInt32) modeID) 
             && ((SInt32) cachedVDResolution.csDisplayModeID > 0)) {
        
                cachedVDResolution.csPreviousDisplayModeID = cachedVDResolution.csDisplayModeID;
            }
        }
    }

    if( cachedVDResolution.csDisplayModeID != (UInt32) modeID) {
        cachedVDResolution.csDisplayModeID = kDisplayModeIDInvalid;
        return( kIOReturnUnsupportedMode);

    } else {

	info->maxDepthIndex	= cachedVDResolution.csMaxDepthMode - kFirstDepth;
	info->nominalWidth	= cachedVDResolution.csHorizontalPixels;
	info->nominalHeight	= cachedVDResolution.csVerticalLines;
	info->refreshRate	= cachedVDResolution.csRefreshRate;

	return( noErr);
    }
}

enum {
    kModePreflight = 1,
    kDisplayModeIDPreflight = kDisplayModeIDReservedBase + 1000
};

IOReturn IONDRVFramebuffer::setDetailedTiming(
            IODisplayModeID mode, IOOptionBits options,
            void * _desc, IOByteCount descripSize )
{
    IOReturn		  	    err;
    VDResolutionInfoRec	  	    info;
    IODetailedTimingInformationV2 * desc = (IODetailedTimingInformationV2 *)_desc;
    VDDetailedTimingRec	  	    look;
    VDDetailedTimingRec	  	    newTiming;
    VDSwitchInfoRec		    switchInfo;
    IOIndex		  	    index;
    UInt32		            checkCurrent;
    UInt32		            checkBoot;
    bool		            notPreflight = (0 == (options & kModePreflight));
    bool		            hasScale;

    index = mode - kDisplayModeIDReservedBase;

    // current must be ok
    if( (mode == currentDisplayMode)
      && notPreflight
      && (detailedTimingsCurrent[index] == detailedTimingsSeed))
        return( kIOReturnSuccess );

    err = _doStatus( this, cscGetCurMode, &switchInfo );
    if( (err == noErr) && (switchInfo.csData == (UInt32) kDisplayModeIDBootProgrammable))
        checkBoot = (UInt32) kDisplayModeIDBootProgrammable;
    else
        checkBoot = 0xffffffff;
    checkCurrent = (UInt32) currentDisplayMode;

    bzero( &look, sizeof( VDDetailedTimingRec) );
    look.csTimingSize = sizeof( VDDetailedTimingRec);

    // look for a programmable
    for(
       info.csPreviousDisplayModeID = kDisplayModeIDFindFirstProgrammable;
       (noErr == (err = _doStatus( this, cscGetNextResolution, &info)));
       info.csPreviousDisplayModeID = info.csDisplayModeID) {

        if( (SInt32) info.csDisplayModeID < 0) {
            err = kIOReturnNoResources;
            break;
        }

        look.csDisplayModeID = info.csDisplayModeID;
	err = _doStatus( this, cscGetDetailedTiming, &look );
        if( err != kIOReturnSuccess)
            continue;

        // don't toss current
        if( look.csDisplayModeAlias == checkCurrent) {
            checkCurrent = 0xffffffff;
            continue;
        }
        // don't toss kDisplayModeIDBootProgrammable if the driver is in that mode
        if( look.csDisplayModeAlias == checkBoot) {
            checkBoot = 0xffffffff;
            continue;
        }

        // see if already set to the right timing
        if( (look.csDisplayModeAlias == (UInt32) mode)
         && (look.csDisplayModeState == kDMSModeReady)
         && (notPreflight)
         && (detailedTimingsCurrent[index] == detailedTimingsSeed))
            break;

        // set it free
        if( look.csDisplayModeState != kDMSModeFree) {
            look.csDisplayModeID    = info.csDisplayModeID;
            look.csDisplayModeAlias = 0;
            look.csDisplayModeState = kDMSModeFree;
            err = _doControl( this, cscSetDetailedTiming, &look );
            if( err != kIOReturnSuccess)
                continue;
        }

        // set it
        hasScale = (desc->horizontalScaled && desc->verticalScaled);

        newTiming = *((VDDetailedTimingRec *) desc);
	newTiming.csDisplayModeID    = info.csDisplayModeID;
        newTiming.csDisplayModeAlias = mode;
        newTiming.csDisplayModeSeed  = look.csDisplayModeSeed;
        newTiming.csDisplayModeState = hasScale ? kDMSModeNotReady : kDMSModeReady;
        err = _doControl( this, cscSetDetailedTiming, &newTiming );

        if( hasScale && (kIOReturnSuccess == err)) {
            VDScalerRec	scaler;

            // set scale
            bzero( &scaler, sizeof( scaler));
            scaler.csScalerSize	      = sizeof( scaler);
            scaler.csScalerFlags      = desc->scalerFlags;
            scaler.csHorizontalPixels = desc->horizontalScaled;
            scaler.csVerticalPixels   = desc->verticalScaled;
            scaler.csDisplayModeID    = info.csDisplayModeID;
            scaler.csDisplayModeSeed  = newTiming.csDisplayModeSeed;
            scaler.csDisplayModeState = kDMSModeReady;

            err = _doControl( this, cscSetScaler, &scaler );

            newTiming.csDisplayModeSeed  = scaler.csDisplayModeSeed;
        }

        if( kIOReturnSuccess == err) {
            err = _doStatus( this, cscGetDetailedTiming, &newTiming );

            if( !notPreflight && (kIOReturnSuccess == err)) {
		desc->signalConfig 		= newTiming.csSignalConfig;
		desc->signalLevels 		= newTiming.csSignalLevels;

		desc->pixelClock 		= newTiming.csPixelClock;
		desc->minPixelClock 		= newTiming.csMinPixelClock;
		desc->maxPixelClock 		= newTiming.csMaxPixelClock;

		desc->horizontalActive 		= newTiming.csHorizontalActive;
		desc->horizontalBlanking 	= newTiming.csHorizontalBlanking;
		desc->horizontalSyncOffset 	= newTiming.csHorizontalSyncOffset;
		desc->horizontalSyncPulseWidth 	= newTiming.csHorizontalSyncPulseWidth;

		desc->verticalActive 		= newTiming.csVerticalActive;
		desc->verticalBlanking 		= newTiming.csVerticalBlanking;
		desc->verticalSyncOffset 	= newTiming.csVerticalSyncOffset;
		desc->verticalSyncPulseWidth 	= newTiming.csVerticalSyncPulseWidth;

		desc->horizontalBorderLeft 	= newTiming.csHorizontalBorderLeft;
		desc->horizontalBorderRight	= newTiming.csHorizontalBorderRight;
		desc->verticalBorderTop		= newTiming.csVerticalBorderTop;
		desc->verticalBorderBottom	= newTiming.csVerticalBorderBottom;

		desc->horizontalSyncConfig	= newTiming.csHorizontalSyncConfig;
		desc->horizontalSyncLevel	= newTiming.csHorizontalSyncLevel;
		desc->verticalSyncConfig	= newTiming.csVerticalSyncConfig;
		desc->verticalSyncLevel		= newTiming.csVerticalSyncLevel;
	    }
        }
        if( notPreflight && (kIOReturnSuccess == err))
            detailedTimingsCurrent[index] = detailedTimingsSeed;

        break;
    }

    return( err );
}

IOReturn IONDRVFramebuffer::validateDisplayMode(
            IODisplayModeID _mode, IOOptionBits flags,
            VDDetailedTimingRec ** detailed )
{
    UInt32		mode = _mode;
    IOReturn		err = kIOReturnSuccess;
    OSData *		data;
    const void *	bytes;

    if( detailed)
        *detailed = (VDDetailedTimingRec *) 0;

    if( mode >= (UInt32) kDisplayModeIDReservedBase) do {

        if( mode == (UInt32) kDisplayModeIDBootProgrammable)
            continue;

        err = kIOReturnBadArgument;
        if( !detailedTimings)
            continue;

        data = OSDynamicCast( OSData, detailedTimings->getObject(
                                        mode - kDisplayModeIDReservedBase));
        if( !data)
            continue;

        bytes = data->getBytesNoCopy();
        err = setDetailedTiming( mode, 0, (void *) bytes, data->getLength() );
        if( err != kIOReturnSuccess)
            continue;

        if( detailed)
            *detailed = (VDDetailedTimingRec *) bytes;

    } while( false );

    return( err );
}

void IONDRVFramebuffer::getCurrentConfiguration( void )
{
    IOReturn		err;
    VDSwitchInfoRec	switchInfo;
    VDGrayRecord	grayRec;

    grayRec.csMode = 0;			// turn off luminance map
    err = _doControl( this, cscSetGray, &grayRec );
    // driver refused => mono display
    grayMode = ((noErr == err) && (0 != grayRec.csMode));
#if 0
    VDPageInfo		pageInfo;
    if( noErr == _doStatus( this, cscGetMode, &pageInfo )) {
        _doControl( this, cscSetMode, &pageInfo);
        _doControl( this, cscGrayPage, &pageInfo);
    }
#endif

    err = _doStatus( this, cscGetCurMode, &switchInfo );
    if( err == noErr) {
        currentDisplayMode	= switchInfo.csData;
        currentDepth		= switchInfo.csMode - kFirstDepth;
        currentPage		= switchInfo.csPage;
	if( 0 == (physicalFramebuffer = pmap_extract( kernel_pmap,
		((vm_address_t) switchInfo.csBaseAddr) ))) {
	    physicalFramebuffer = (UInt32) switchInfo.csBaseAddr;
	}
    } else
	IOLog("%s: cscGetCurMode failed\n", nub->getName());
}

IODeviceMemory * IONDRVFramebuffer::makeSubRange( 
	IOPhysicalAddress	start,
	IOPhysicalLength	length ) 
{
    IODeviceMemory *	mem = 0;
    UInt32		numMaps, i;
    IOService *		device;

    device = nub;
    numMaps = device->getDeviceMemoryCount();

    for( i = 0; (!mem) && (i < numMaps); i++) {
	mem = device->getDeviceMemoryWithIndex(i);
	if( !mem)
	    continue;
	mem = IODeviceMemory::withSubRange( mem,
			start - mem->getPhysicalAddress(), length );
    }
    if( !mem)
	mem = IODeviceMemory::withRange( start, length );

    return( mem );
}

IODeviceMemory * IONDRVFramebuffer::getApertureRange( IOPixelAperture aper )
{
    IOReturn			err;
    IOPixelInformation		info;
    IOByteCount			bytes;

    err = getPixelInformation( currentDisplayMode, currentDepth, aper,
                                &info );
    if( err)
	return( 0 );

    bytes = (info.bytesPerRow * info.activeHeight) + 128;

    return( makeSubRange( physicalFramebuffer, bytes ));
}

IODeviceMemory * IONDRVFramebuffer::findVRAM( void )
{
    IODeviceMemory *	mem = 0;
    IOPhysicalAddress	vramBase;
    enum {		kMinimumVRAMLength = 512*1024 };
    IOByteCount		length = kMinimumVRAMLength, vramLength = 16*1024*1024;
    OSData *		prop;

    prop = OSDynamicCast( OSData, nub->getProperty("AAPL,vram-memory"));
    if( prop && (prop->getLength() >= (2 * sizeof(IOByteCount)))) {
        IOByteCount * lengths;

        lengths = (IOByteCount *) prop->getBytesNoCopy();
        length = lengths[1];
        vramBase = lengths[0];

    } else {
        vramBase = physicalFramebuffer;
        prop = OSDynamicCast( OSData, nub->getProperty("VRAM,memsize"));
        if( prop)
            length = *((IOByteCount *) prop->getBytesNoCopy());
	if( length < kMinimumVRAMLength)
	    prop = 0;
    }

    if( prop) {
        length = (length + (vramBase & 0xffff) + 0xffff) & 0xffff0000;
        vramBase &= 0xffff0000;

        for( vramLength = page_size; (vramLength < length) && vramLength; vramLength <<= 1)
            {}
        if( !vramLength)
            vramLength = length;
    
        mem = makeSubRange( vramBase, vramLength );

    } else {

        for( UInt32 i = 0; (mem = nub->getDeviceMemoryWithIndex(i)); i++) {
            if( (physicalFramebuffer >= mem->getPhysicalAddress())
             && (physicalFramebuffer < (mem->getPhysicalAddress() + mem->getLength())))
                break;
        }
        if( mem) {
            vramBase = mem->getPhysicalAddress();
            vramLength = mem->getLength();
            mem->retain();
        } else
            mem = makeSubRange( vramBase, vramLength );
    }

    IOLog("%s: vram [%08lx:%08lx]\n", nub->getName(), vramBase, vramLength);
    return( mem );
}

const char * IONDRVFramebuffer::getPixelFormats( void )
{
    static const char * ndrvPixelFormats =
        IO1BitIndexedPixels "\0"
        IO2BitIndexedPixels "\0"
        IO4BitIndexedPixels "\0"
        IO8BitIndexedPixels "\0"
        IO16BitDirectPixels "\0"
        IO32BitDirectPixels "\0"
        "\0";

    return( ndrvPixelFormats);
}

IOItemCount IONDRVFramebuffer::getDisplayModeCount( void )
{
    return( iterateAllModes( 0 ));
}

IOReturn IONDRVFramebuffer::getDisplayModes( IODisplayModeID * allDisplayModes )
{
    iterateAllModes( allDisplayModes );
    return( kIOReturnSuccess );
}

IOReturn IONDRVFramebuffer::validateDetailedTiming(
            void * desc, IOByteCount descripSize )
{
    IOReturn err;

    err = setDetailedTiming( kDisplayModeIDPreflight,
                             kModePreflight, desc, descripSize);

    return( err );
}

IOReturn IONDRVFramebuffer::setDetailedTimings( OSArray * array )
{
    IOReturn	err;
    UInt32 *	newCurrent;
    IOItemCount	newCount;

    if( !array) {
        if( detailedTimings) {
            IODelete( detailedTimingsCurrent, UInt32, detailedTimings->getCount());
            detailedTimingsCurrent = 0;
        }
        removeProperty( kIOFBDetailedTimingsKey );
        detailedTimings = 0;
        detailedTimingsSeed++;
        return( kIOReturnSuccess );
    }

    newCount = array->getCount();
    newCurrent = IONew(UInt32, newCount);
    if( newCurrent) {
        if( detailedTimings)
            IODelete( detailedTimingsCurrent, UInt32, detailedTimings->getCount());
        detailedTimingsCurrent = newCurrent;
        bzero( newCurrent, newCount * sizeof( UInt32));
        setProperty( kIOFBDetailedTimingsKey, array );	// retains
        detailedTimings = array;
        detailedTimingsSeed++;

        if( currentDisplayMode == kDisplayModeIDBootProgrammable) {
            VDDetailedTimingRec			look;
            VDScalerRec				scaler;
            IODetailedTimingInformationV2 *	detailed;
            OSData *				data;
            IODisplayModeID			newDisplayMode;
            bool				bootScaled;

            newDisplayMode = currentDisplayMode;

            bzero( &look, sizeof( VDDetailedTimingRec) );
            look.csTimingSize = sizeof( VDDetailedTimingRec);
            look.csDisplayModeID = kDisplayModeIDBootProgrammable;
            err = _doStatus( this, cscGetDetailedTiming, &look );

            bzero( &scaler, sizeof( VDScalerRec) );
            scaler.csScalerSize = sizeof( VDScalerRec);
            scaler.csDisplayModeID = kDisplayModeIDBootProgrammable;
            bootScaled = (noErr == _doStatus( this, cscGetScaler, &scaler ));

            if( kIOReturnSuccess == err) {
              for( int i = 0;
                 (data = OSDynamicCast( OSData, detailedTimings->getObject(i)));
                 i++) {

                    detailed = (IODetailedTimingInformationV2 *) data->getBytesNoCopy();
                    if( (detailed->horizontalActive != look.csHorizontalActive)
                     || (detailed->verticalActive != look.csVerticalActive))
                        continue;

                    if( bootScaled
                     && ((detailed->horizontalScaled != scaler.csHorizontalPixels)
                      || (detailed->horizontalScaled != scaler.csHorizontalPixels)
                      || (detailed->verticalScaled   != scaler.csVerticalPixels)
                      || (detailed->scalerFlags      != scaler.csScalerFlags)))
                        continue;

                    newDisplayMode = i + kDisplayModeIDReservedBase;
                    break;
                }
            }
        
            if( newDisplayMode != currentDisplayMode) {
                err = validateDisplayMode( newDisplayMode, 0, 0 );
                currentDisplayMode = newDisplayMode;
            }
        }

        err = kIOReturnSuccess;
    } else
        err = kIOReturnNoMemory;

    return( err );
}

IOReturn IONDRVFramebuffer::getInformationForDisplayMode(
		IODisplayModeID displayMode, IODisplayModeInformation * info )
{
    IOReturn			err;

    err = getResInfoForMode( displayMode, info );
    if( err)
        err = kIOReturnUnsupportedMode;

    return( err );
}


UInt64 IONDRVFramebuffer::getPixelFormatsForDisplayMode(
		IODisplayModeID /* displayMode */, IOIndex depthIndex )
{
    return( 1 << depthIndex );
}

IOReturn IONDRVFramebuffer::getPixelInformation(
	IODisplayModeID displayMode, IOIndex depth,
	IOPixelAperture aperture, IOPixelInformation * info )
{
    SInt32			err;
    VDVideoParametersInfoRec	pixelParams;
    VPBlock			pixelInfo;

    bzero( info, sizeof( *info));

    if( aperture)
        return( kIOReturnUnsupportedMode);

    err = validateDisplayMode( displayMode, 0, 0 );
    if( err)
        return( err );

    do {
    	pixelParams.csDisplayModeID = displayMode;
	pixelParams.csDepthMode = depth + kFirstDepth;
	pixelParams.csVPBlockPtr = &pixelInfo;
	err = _doStatus( this, cscGetVideoParameters, &pixelParams );
	if( err)
	    continue;

	info->flags		= accessFlags;

	info->activeWidth	= pixelInfo.vpBounds.right;
	info->activeHeight	= pixelInfo.vpBounds.bottom;
	info->bytesPerRow       = pixelInfo.vpRowBytes & 0x7fff;
	info->bytesPerPlane	= pixelInfo.vpPlaneBytes;
	info->bitsPerPixel 	= pixelInfo.vpPixelSize;

        switch( pixelInfo.vpPixelSize / 8 ) {

          default:
            pixelInfo.vpPixelSize = 8;
          case 0:
          case 1:
            strncpy( info->pixelFormat, "PPPPPPPP", pixelInfo.vpPixelSize);
            info->pixelType = kIOCLUTPixels;
            info->componentMasks[0] = (1 << pixelInfo.vpPixelSize) - 1;
            info->bitsPerPixel = pixelInfo.vpPixelSize;
            info->componentCount = 1;
            info->bitsPerComponent = pixelInfo.vpPixelSize;
            break;

          case 2:
            strcpy( info->pixelFormat, "-RRRRRGGGGGBBBBB");
            info->pixelType = kIORGBDirectPixels;
            info->componentMasks[0] = 0x7c00;
            info->componentMasks[1] = 0x03e0;
            info->componentMasks[2] = 0x001f;
            info->bitsPerPixel = 16;
            info->componentCount = 3;
            info->bitsPerComponent = 5;
            break;

          case 4:
            strcpy( info->pixelFormat, "--------RRRRRRRRGGGGGGGGBBBBBBBB");
            info->pixelType = kIORGBDirectPixels;
            info->componentMasks[0] = 0x00ff0000;
            info->componentMasks[1] = 0x0000ff00;
            info->componentMasks[2] = 0x000000ff;
            info->bitsPerPixel = 32;
            info->componentCount = 3;
            info->bitsPerComponent = 8;
            break;
        }

    } while( false);

    return( err);
}

IOReturn IONDRVFramebuffer::getTimingInfoForDisplayMode(
		IODisplayModeID displayMode, IOTimingInformation * info )
{
    VDTimingInfoRec		timingInfo;
    
    OSStatus			err;

    err = validateDisplayMode( displayMode, 0, 0 );
    if( err)
        return( err );

    timingInfo.csTimingMode = displayMode;
    // in case the driver doesn't do it:
    timingInfo.csTimingFormat = kDeclROMtables;
    err = _doStatus( this, cscGetModeTiming, &timingInfo);
    if( err == noErr) {
	if( timingInfo.csTimingFormat == kDeclROMtables)
	    info->appleTimingID = timingInfo.csTimingData;
	else
	    info->appleTimingID = timingInvalid;

        if( info->flags & kIODetailedTimingValid) {
            VDDetailedTimingRec	* look = (VDDetailedTimingRec *) &info->detailedInfo.v2;
            VDScalerRec		  scaler;

            bzero( look, sizeof( VDDetailedTimingRec) );
            look->csTimingSize = sizeof( VDDetailedTimingRec);
            look->csDisplayModeID = displayMode;
            err = _doStatus( this, cscGetDetailedTiming, look );
            if( kIOReturnSuccess != err)
                info->flags &= ~kIODetailedTimingValid;
            else {

                bzero( &info->detailedInfo.v2.__reservedA[0], sizeof( info->detailedInfo.v2.__reservedA));
                bzero( &info->detailedInfo.v2.__reservedB[0], sizeof( info->detailedInfo.v2.__reservedB));

                bzero( &scaler, sizeof( VDScalerRec) );
                scaler.csScalerSize = sizeof( VDScalerRec);
                scaler.csDisplayModeID = displayMode;
                err = _doStatus( this, cscGetScaler, &scaler );
                if( kIOReturnSuccess == err) {
                    info->flags |= kIOScalingInfoValid;
                    info->detailedInfo.v2.scalerFlags      = scaler.csScalerFlags;
                    info->detailedInfo.v2.horizontalScaled = scaler.csHorizontalPixels;
                    info->detailedInfo.v2.verticalScaled   = scaler.csVerticalPixels;
                } else {
                    info->detailedInfo.v2.scalerFlags      = 0;
                    info->detailedInfo.v2.horizontalScaled = 0;
                    info->detailedInfo.v2.verticalScaled   = 0;
                }
            }
        }

	return( kIOReturnSuccess);
    }

    return( kIOReturnUnsupportedMode);
}

IOReturn IONDRVFramebuffer::getCurrentDisplayMode( 
				IODisplayModeID * displayMode, IOIndex * depth )
{
    if( displayMode)
	*displayMode = currentDisplayMode;
    if( depth)
	*depth = currentDepth;

    return( kIOReturnSuccess);
}

IOReturn IONDRVFramebuffer::setDisplayMode( IODisplayModeID displayMode, IOIndex depth )
{
    SInt32		err;
    VDSwitchInfoRec	switchInfo, oldSwitchInfo;
    VDPageInfo		pageInfo;

    if( 0 == powerState)
        return( kIOReturnNotReady );

#if 0
    if( !online) {
        VDTimingInfoRec		timingInfo;

        timingInfo.csTimingMode   = displayMode;
        timingInfo.csTimingFormat = kDeclROMtables;
        err = _doStatus( this, cscGetModeTiming, &timingInfo);

        if( (noErr != err)
          || (kDeclROMtables != timingInfo.csTimingFormat)
          || (timingApple_0x0_0hz_Offline != timingInfo.csTimingData) ) {
            IOLog("%s: offline\n", nub->getName());
            return( kIOReturnSuccess );
        }
    }
#endif

    err = validateDisplayMode( displayMode, 0, 0 );
    if( err)
        return( err );

    err = _doStatus( this, cscGetCurMode, &oldSwitchInfo );

    switchInfo.csData = displayMode;
    switchInfo.csMode = depth + kFirstDepth;
    switchInfo.csPage = 0;
    err = _doControl( this, cscSwitchMode, &switchInfo);
    if(err)
	IOLog("%s:%d: cscSwitchMode(%08lx) %d\n", nub->getName(),
                online, displayMode, (int) err);

    // duplicate QD InitGDevice
    pageInfo.csMode = switchInfo.csMode;
    pageInfo.csData = 0;
    pageInfo.csPage = 0;
    _doControl( this, cscSetMode, &pageInfo);

#if 0
    if( mirrored) {
        VDGammaRecord	gammaRec;
        OSStatus		gammaErr;
    
        gammaErr = _doStatus( this, cscGetGamma, &gammaRec );
        if( noErr == gammaErr)
            _doControl( this, cscSetGamma, &gammaRec );
        else
            _doControl( this, cscGrayPage, &pageInfo);
    } else
#endif
        _doControl( this, cscGrayPage, &pageInfo);

    getCurrentConfiguration();

    return( err);
}

IOReturn IONDRVFramebuffer::setStartupDisplayMode(
			IODisplayModeID displayMode, IOIndex depth )
{
    SInt32		err;
    VDSwitchInfoRec	switchInfo;

    err = validateDisplayMode( displayMode, 0, 0 );
    if( err)
        return( err );

    switchInfo.csData = displayMode;
    switchInfo.csMode = depth + kFirstDepth;
    err = _doControl( this, cscSavePreferredConfiguration, &switchInfo);
    return( err);
}

IOReturn IONDRVFramebuffer::getStartupDisplayMode(
				IODisplayModeID * displayMode, IOIndex * depth )
{
    SInt32		err;
    VDSwitchInfoRec	switchInfo;

    err = _doStatus( this, cscGetPreferredConfiguration, &switchInfo);
    if( err == noErr) {
	*displayMode	= switchInfo.csData;
	*depth		= switchInfo.csMode - kFirstDepth;
    }
    return( err);
}

IOReturn IONDRVFramebuffer::setApertureEnable( IOPixelAperture /* aperture */,
						IOOptionBits /* enable */ )
{
    return( kIOReturnSuccess);
}

IOReturn IONDRVFramebuffer::setCLUTWithEntries(
			IOColorEntry * colors, UInt32 index, UInt32 numEntries,
			IOOptionBits options )
{
    IOReturn		err;
    VDSetEntryRecord	setEntryRec;
    VDClutBehavior	clutSetting;
    VDGrayRecord	grayRec;

    if( 0 == powerState)
        return( kIOReturnSuccess );

    if( options & kSetCLUTWithLuminance)
        grayRec.csMode = 1;		// turn on luminance map
    else
        grayRec.csMode = 0;		// turn off luminance map

    if( grayRec.csMode != lastGrayMode) {
	_doControl( this, cscSetGray, &grayRec);
	lastGrayMode = grayRec.csMode;
    }

    if( options & kSetCLUTImmediately)
        clutSetting = kSetClutAtSetEntries;
    else
        clutSetting = kSetClutAtVBL;

    if( clutSetting != lastClutSetting) {
	_doControl( this, cscSetClutBehavior, &clutSetting);
	lastClutSetting = clutSetting;
    }

    if( options & kSetCLUTByValue)
        setEntryRec.csStart = -1;
    else
        setEntryRec.csStart = index;

    setEntryRec.csTable = (ColorSpec *) colors;
    setEntryRec.csCount = numEntries - 1;
    err = _doControl( this, cscSetEntries, &setEntryRec);

    return( err);
}

IOReturn IONDRVFramebuffer::setGammaTable( UInt32 channelCount, UInt32 origDataCount,
                                            UInt32 dataWidth, void * data )
{
    IOReturn		err = kIOReturnUnsupported;
    VDGammaRecord	gammaRec;
    UInt32		dataCount = origDataCount;
    UInt32		expandCount = 0;
    struct GammaTbl {
        short gVersion;		/*gamma version number*/
        short gType;		/*gamma data type*/
        short gFormulaSize;	/*Formula data size */
        short gChanCnt;		/*number of channels of data */
        short gDataCnt;		/*number of values/channel */
        short gDataWidth;	/*bits/corrected value */
				/* (data packed to next larger byte size) */
        UInt8 gFormulaData[0];	/* data for formulas followed by gamma values */
    };
    GammaTbl * 	table = NULL;
    IOByteCount	dataLen = 0;
    UInt32      tryWidth;

    if( 0 == powerState)
        return( kIOReturnSuccess );
    if( !data)
        return( kIOReturnBadArgument );

    if( (dataCount < __private->desiredGammaCount) && (dataWidth >= __private->desiredGammaWidth)) {
	expandCount = __private->desiredGammaCount / dataCount;
	if( (expandCount * dataCount) == __private->desiredGammaCount) {
	    dataCount = __private->desiredGammaCount;
	    expandCount--;
	} else
	    expandCount = 0;
    }

    dataLen = (dataWidth + 7) / 8;
    dataLen *= dataCount * channelCount;
    table = (GammaTbl *) IOMalloc( dataLen + sizeof( struct GammaTbl));
    if( NULL == table)
        return( kIOReturnNoMemory);

    table->gVersion	= 0;
    table->gType	= 0;
    table->gFormulaSize	= 0;
    table->gChanCnt	= channelCount;
    table->gDataCnt	= dataCount;

    for( tryWidth = __private->desiredGammaWidth;
         tryWidth;
         ) {

        if( tryWidth > dataWidth)
            continue;
        if( !expandCount && (tryWidth == dataWidth))
            bcopy( data, table->gFormulaData, dataLen);
        else {

            UInt32 pin, pt5, value;

            pin = (1 << tryWidth) - 1;
            pt5 = (1 << (dataWidth - tryWidth - 1));

            for( UInt32 in = 0, out = 0; out < (dataCount * channelCount);) {

                value = (((UInt16 *) data)[in++] + pt5) >> (dataWidth - tryWidth);
                if( value > pin)
                    value = pin;
		for( UInt32 i = 0; i <= expandCount; i++) {
		    if( tryWidth <= 8)
			((UInt8 *) &table->gFormulaData[0])[out++] = (value & 0xff);
		    else
			((UInt16 *) &table->gFormulaData[0])[out++] = value;
		}
            }
        }
	table->gDataWidth = tryWidth;
        gammaRec.csGTable = (Ptr) table;
        err = _doControl( this, cscSetGamma, &gammaRec );

	if( kIOReturnSuccess != err)
	    DEBG("Gamma(%ld, %ld, %ld) set: %d\n", channelCount, dataCount, tryWidth, err);

        if( kIOReturnSuccess == err)
            break;
	else if (tryWidth != 8) {
	    dataCount = origDataCount;
	    expandCount = 0;
	} else
	    break;
    }

    if( table)
        IOFree( table, dataLen + sizeof( struct GammaTbl));

    return( err);
}

static IOReturn REG( void *entryID, IOService ** result )
{

    REG_ENTRY_TO_PT( entryID, regEntry );
    *result = OSDynamicCast( IOService, regEntry );

    return(  *result ? kIOReturnSuccess : kIOReturnBadArgument );
}

IOReturn IONDRVFramebuffer::mirrorInfo( UInt32 index )
{
    IOReturn		err = kIOReturnSuccess;
    VDMirrorRec		mirror;
    IOService *		device;
    IONDRVFramebuffer * fb;
    const OSSymbol *	sym = OSSymbol::withCStringNoCopy("IOFramebuffer");

    bzero( &mirror, sizeof( mirror));
    mirror.csMirrorSize = sizeof(VDMirrorRec);

    fb = this;
    do {
        err = fb->_doStatus( this, cscGetMirror, &mirror );
        if( err)
            continue;

        setProperty("IOFBMirrorCapabilities",
                        mirror.csMirrorFeatures, sizeof( mirror.csMirrorFeatures));

        err = REG( &mirror.csMirrorResultID, &device );
        if( err)
            continue;

        fb = OSDynamicCast( IONDRVFramebuffer, device->getClientWithCategory( sym ));
        if( !fb)
            continue;

        IOLog("%s, %s: %08lx, %08lx\n", device->getName(), fb->getName(),
                mirror.csMirrorFeatures, mirror.csMirrorSupportedFlags );

        if( this == fb)
            break;

    } while( false );

    sym->release();

    return( err );
}

IOReturn IONDRVFramebuffer::setMirror( IONDRVFramebuffer * other )
{
    IOReturn	        err = kIOReturnSuccess;
    IONDRVFramebuffer * next;
    VDMirrorRec	        mirror;

    if( mirrored == (other != 0))
        return( kIOReturnSuccess );

    if( !nextDependent)
        return( kIOReturnUnsupported );
    if( other && (other != nextDependent))
        return( kIOReturnUnsupported );

    do {
        bzero( &mirror, sizeof( mirror));
        mirror.csMirrorSize = sizeof(VDMirrorRec);
        mirror.csMirrorFlags = 0
                            | kMirrorCanMirrorMask
                            | kMirrorAreMirroredMask
                            | kMirrorUnclippedMirrorMask
                            | kMirrorClippedMirrorMask
                            | kMirrorHAlignCenterMirrorMask
                            | kMirrorVAlignCenterMirrorMask;
        MAKE_REG_ENTRY( mirror.csMirrorResultID, 0 );
        if( 0 == other) {
            MAKE_REG_ENTRY( mirror.csMirrorRequestID, 0 );
        } else {
            MAKE_REG_ENTRY( mirror.csMirrorRequestID, other->getProvider() );
        }
        err = _doControl( this, cscSetMirror, &mirror );
        if( err)
            continue;

        mirrored = (other != 0);
        if( (next = OSDynamicCast( IONDRVFramebuffer, nextDependent)))
            next->setMirror( (other != 0) ? this : 0 );

    } while( false );

    return( err );
}


IOReturn IONDRVFramebuffer::setAttribute( IOSelect attribute, UInt32 _value )
{
    IOReturn		err = kIOReturnSuccess;
    IONDRVFramebuffer *	other = 0;
    UInt32 *		data = (UInt32 *) _value;
    UInt32		value;

    switch( attribute ) {

        case kIOPowerAttribute:
            err = ndrvSetPowerState( _value );
	    break;

        case kIOSystemPowerAttribute:
	    break;

	case kIOMirrorAttribute:

            do {
                value = data[0] & 1;
                if( value) {
                    other = OSDynamicCast( IONDRVFramebuffer, (OSObject *) data[1] );
                    if( !other) {
                        err = kIOReturnBadArgument;
                        continue;
                    }
                }

                err = setMirror( other );
                if( kIOReturnSuccess != err)
                    continue;

                OSNumber * num = OSDynamicCast(OSNumber, getProperty(kIOFBDependentIndexKey));
                mirrorPrimary = mirrored && (!num || (1 == num->unsigned32BitValue()));

                (void) setDisplayMode( currentDisplayMode, currentDepth );

            } while( false );

	    break;

	default:
	    err = super::setAttribute( attribute, _value );
    }

    return( err );
}

IOReturn IONDRVFramebuffer::getAttribute( IOSelect attribute, UInt32 * value )
{
    IOReturn			err = kIOReturnSuccess;
    VDSupportsHardwareCursorRec	hwCrsrSupport;
    VDMirrorRec			mirror;
    IONDRVFramebuffer *		other;

    switch( attribute ) {

	case kIOHardwareCursorAttribute:

	    *value = ((kIOReturnSuccess ==
			_doStatus( this, cscSupportsHardwareCursor, &hwCrsrSupport))
                    && true && (hwCrsrSupport.csSupportsHardwareCursor));
	    break;

	case kIOMirrorAttribute:

            bzero( &mirror, sizeof( mirror));
            mirror.csMirrorSize = sizeof(VDMirrorRec);

            other = OSDynamicCast( IONDRVFramebuffer, (OSObject *) value[0] );
            if( other)  {
                MAKE_REG_ENTRY( mirror.csMirrorRequestID, other->getProvider() );
            } else {
                MAKE_REG_ENTRY( mirror.csMirrorRequestID, 0 );
            }
            err = _doStatus( this, cscGetMirror, &mirror );
            value[0] = mirror.csMirrorSupportedFlags;

            if( kIOReturnSuccess == err) {

		DEBG("IOFB: csMirrorSupportedFlags %08lx, csMirrorFeatures %08lx\n",
		     mirror.csMirrorSupportedFlags, mirror.csMirrorFeatures);

                OSNumber * num = OSDynamicCast(OSNumber, getProperty(kIOFBDependentIndexKey));
                if( !num || (1 == num->unsigned32BitValue()))
                    value[0] |= kIOMirrorIsPrimary;

                if( kMirrorClippedMirrorMask & mirror.csMirrorSupportedFlags)
                    value[0] |= kIOMirrorHWClipped;

                do {
                    VDVideoParametersInfoRec	pixelParams;
                    VPBlock			pixelInfo;
                    IODisplayModeInformation	modeInfo;
                    IOReturn			err;
                
                    pixelParams.csDisplayModeID = currentDisplayMode;
                    pixelParams.csDepthMode = currentDepth + kFirstDepth;
                    pixelParams.csVPBlockPtr = &pixelInfo;
                    err = _doStatus( this, cscGetVideoParameters, &pixelParams );
                    if( err)
                        continue;
                    err = getResInfoForMode( currentDisplayMode, &modeInfo );
                    if( err)
                        continue;
                    if( (modeInfo.nominalWidth != (UInt32) pixelInfo.vpBounds.right)
                     || (modeInfo.nominalHeight != (UInt32) pixelInfo.vpBounds.bottom))
                        value[0] |= kIOMirrorHWClipped;

                } while( false );

		DEBG("IOFB: kIOMirrorAttribute %08lx\n", value[0]);

                err = kIOReturnSuccess;
            }
	    break;

        case kIOMirrorDefaultAttribute:
          {
            IORegistryEntry   * entry;
            IORegistryEntry   * root;
            IONDRVFramebuffer * next;
            OSNumber *	        num;
            OSData *	        data = 0;
            enum { kIOMirrorHint = 0x10000 };

            num = OSDynamicCast(OSNumber, getProperty("graphic-options"));
            if( num)
                value[0] = num->unsigned32BitValue();
            else {
                value[0] = 0;

                data = OSDynamicCast(OSData, nub->getProperty("graphic-options"));
                if( !data && (root = IORegistryEntry::fromPath("/", gIODTPlane))) {
                    data = OSDynamicCast(OSData, root->getProperty("graphic-options"));
                    root->release();
                }

                if( data)
                    value[0] = *((UInt32 *) data->getBytesNoCopy());
                else if( (entry = IORegistryEntry::fromPath("mac-io/battery", gIODTPlane))
                        || (entry = IORegistryEntry::fromPath("mac-io/via-pmu/battery", gIODTPlane))) {
        
                    value[0] &= ~kIOMirrorDefault;
                    entry->release();
                }
                setProperty("graphic-options", value[0], 32);
            }

            if( online
              && (kIOMirrorDefault & value[0])
              && (next = OSDynamicCast(IONDRVFramebuffer, nextDependent))
              && next->getOnlineState())
                value[0] |= kIOMirrorHint;
            err = kIOReturnSuccess;
            break;
          }

	case kIOVRAMSaveAttribute:

	    *value = (online && (!mirrored || mirrorPrimary));
	    break;

	default:
	    err = super::getAttribute( attribute, value );
    }

    return( err );
}

UInt32 IONDRVFramebuffer::getConnectionCount( void )
{
    VDMultiConnectInfoRec	multiConnection;

    multiConnection.csDisplayCountOrNumber = kGetConnectionCount;
    if( noErr == _doStatus( this, cscGetMultiConnect, &multiConnection))
        return( multiConnection.csDisplayCountOrNumber );
    else
        return( 1 );
}

/*
    File:	DDCPowerOnOffUtils.c <CS3>
*/

enum{
    kVCPSendSize			= 8,
    kVCPReplySize			= 64,
    kI2CDisplayWriteAddress		= 0x6E,
    kI2CDisplayReadAddress		= 0x6F,
    // Messed up specification says checksum should be calculated with ACCESS.bus value of 50.
    kI2CDisplayReadHostCheckSumAddress	= 0x50,
    // Messed up specification says checksum should be calculated with ACCESS.bus value of 50.
    kI2CDisplayReadHostAddress		= 0x51,

    kI2CVCPGetCode			= 0x01,
    kI2CVCPGetLength			= 0x82,
    kI2CVCPGetMessageSize		= 0x05,

    kI2CVCPReplyLength			= 0x88,
    kI2CNullReplyLength			= 0x80,
    kI2CNullReplyCheckSum		= 0xBE,

    kI2CVCPSetCode			= 0x03,
    kI2CVCPSetLength			= 0x84,
    kI2CVCPReplyCode			= 0x02,
            
    kDDCPowerOn				= 0x01,
    kDDCPowerOff			= 0x04
};
enum {
    kBasicI2CCommTransactionsMask = ( (1<<kVideoNoTransactionType) | (1<<kVideoSimpleI2CType)
                                    | (1<<kVideoDDCciReplyType) )
};

void IONDRVFramebuffer::displayI2CPower( bool enable )
{
    VDCommunicationRec	i2CRecord;
    VDCommunicationInfoRec i2cInfoRecord;
    Byte		sendBuffer[8];
    Byte		replyBuffer[kVCPReplySize];
    UInt32		supportedCommFlags = 0;
    // Don't do it if we're told it's not supported
    bool		setThisDisplay = true;

    // 
    // Some displays (like Fiji) do not support the reading
    // of the current power state.  Others (like Mitsubishi
    // Diamond Pro 710) report that they do not support 
    // power management calls.
    //
    // I'll avoid sending the power command only in the 
    // case that I get a valid reply that does says 
    // it does not support the power selector.
    //

    bzero( &i2cInfoRecord, sizeof(i2cInfoRecord) );
    if( noErr != _doStatus( this, cscGetCommunicationInfo, &i2cInfoRecord))
        return;
    if( kBasicI2CCommTransactionsMask != (i2cInfoRecord.csSupportedTypes & kBasicI2CCommTransactionsMask))
        return;

    supportedCommFlags = i2cInfoRecord.csSupportedCommFlags;
    bzero( &i2CRecord, sizeof(i2CRecord) );
    bzero( &sendBuffer, sizeof(sendBuffer) );
    bzero( &replyBuffer, sizeof(replyBuffer) );

    sendBuffer[0]	= kI2CDisplayReadHostAddress;
    sendBuffer[1]	= kI2CVCPGetLength;
    sendBuffer[2]	= kI2CVCPGetCode;		// GetVCP command
    sendBuffer[3]	= 0xD6;
    sendBuffer[4]	= kI2CDisplayWriteAddress ^
                            sendBuffer[0] ^ sendBuffer[1] ^
                            sendBuffer[2] ^ sendBuffer[3];

    i2CRecord.csBusID		= kVideoDefaultBus;
    i2CRecord.csSendType	= kVideoSimpleI2CType;
    i2CRecord.csSendAddress	= kI2CDisplayWriteAddress;
    i2CRecord.csSendBuffer	= &sendBuffer;
    i2CRecord.csSendSize	= 7;
    i2CRecord.csReplyType	= kVideoDDCciReplyType;
    i2CRecord.csReplyAddress	= kI2CDisplayReadAddress;
    i2CRecord.csReplyBuffer	= &replyBuffer;
    i2CRecord.csReplySize	= kVCPReplySize;

    if( supportedCommFlags & kVideoReplyMicroSecDelayMask )
    {
        // We know some displays are slow, this is an important call to get right
	i2CRecord.csCommFlags	|= kVideoReplyMicroSecDelayMask;
        // 50 milliseconds should be enough time for the display to respond.
	i2CRecord.csMinReplyDelay = 50 * 1000;
    }

    if( (noErr == _doControl( this, cscDoCommunication, &i2CRecord))
      && (kI2CDisplayWriteAddress == replyBuffer[0])
      && (kI2CVCPReplyLength == replyBuffer[1])
      && (kI2CVCPReplyCode == replyBuffer[2])) {
	Byte checkSum = kI2CDisplayReadHostCheckSumAddress ^	// host address
            replyBuffer[0] ^	// source address
            replyBuffer[1] ^	// message length (0x88)
            replyBuffer[2] ^	// VCP type code
            replyBuffer[3] ^	// result code
            replyBuffer[4] ^	// VCP op code
            replyBuffer[5] ^	// VCP type code
            replyBuffer[6] ^	// Max value MSB
            replyBuffer[7] ^	// Max value LSB
            replyBuffer[8] ^	// Current value MSB
            replyBuffer[9];	// Current value LSB

        if( (checkSum == replyBuffer[10]) &&		// Did the check sum match AND
                        (0 != replyBuffer[3]))		// Are we not supposed to support this feature?
            setThisDisplay = false;			// Don't do it if we're told it's not supported
    }
        
    if( setThisDisplay) {

        bzero( &i2CRecord, sizeof(i2CRecord) );
        bzero( &sendBuffer, sizeof(sendBuffer) );
        bzero( &replyBuffer, sizeof(replyBuffer) );

        sendBuffer[0]	= kI2CDisplayReadHostAddress;
        sendBuffer[1]	= kI2CVCPSetLength;
        sendBuffer[2]	= kI2CVCPSetCode;			// SetVCP command
        sendBuffer[3]	= 0xD6;
        sendBuffer[4]	= 0;					// MSB
        sendBuffer[5]	= enable ? kDDCPowerOn : kDDCPowerOff;	// LSB
        sendBuffer[6]	= kI2CDisplayWriteAddress ^
                            sendBuffer[0] ^ sendBuffer[1] ^
                            sendBuffer[2] ^ sendBuffer[3] ^
                            sendBuffer[4] ^ sendBuffer[5];

        i2CRecord.csBusID		= kVideoDefaultBus;
        i2CRecord.csSendAddress		= kI2CDisplayWriteAddress;
        i2CRecord.csSendType		= kVideoSimpleI2CType;
        i2CRecord.csSendBuffer		= &sendBuffer;
        i2CRecord.csSendSize		= 7;
        i2CRecord.csReplyType		= kVideoNoTransactionType;
        i2CRecord.csReplyAddress	= 0;
        i2CRecord.csReplyBuffer		= 0;
        i2CRecord.csReplySize		= 0;

        if( supportedCommFlags & kVideoReplyMicroSecDelayMask) {
            // We know some displays are slow, this is an important call to get right
            i2CRecord.csCommFlags |= kVideoReplyMicroSecDelayMask;
            // 50 milliseconds should be enough time for the display to respond.
            i2CRecord.csMinReplyDelay	= 50 * 1000;
        }

        _doControl( this, cscDoCommunication, &i2CRecord);
    }
}

bool IONDRVFramebuffer::getOnlineState( void )
{
    return( online );
}

IOReturn IONDRVFramebuffer::ndrvSetFeature( UInt32 feature, UInt32 value )
{
    IOReturn err;
    VDConfigurationRec configRec;

    bzero( &configRec, sizeof( configRec));
    configRec.csConfigFeature = feature;
    err = _doStatus( this, cscGetFeatureConfiguration, &configRec );

    DEBG("%s: cscGetFeatureConfiguration(%d), %08lx %08lx\n", nub->getName(), err, 
	    configRec.csConfigSupport, configRec.csConfigValue);

    if( (kIOReturnSuccess == err) && (configRec.csConfigSupport & kDVIPowerSwitchSupportMask)) {
	configRec.csConfigFeature = feature;
	configRec.csConfigValue   = value;
	err = _doControl( this, cscSetFeatureConfiguration, &configRec );

	DEBG("%s: cscSetFeatureConfiguration(%d) %08lx\n", nub->getName(), err, configRec.csConfigValue);
    }

    return( err );
}

IOReturn IONDRVFramebuffer::setConnectionFlags( void )
{

    shouldDoI2CPower |= (0 != ( kIOI2CPowerEnableFlag & __private->displayConnectFlags));

    return( kIOReturnSuccess );
}

IOReturn IONDRVFramebuffer::setAttributeForConnection( IOIndex connectIndex,
                                                        IOSelect attribute, UInt32 info )
{
    IOReturn		err;
    VDSyncInfoRec	syncInfo;

    switch( attribute ) {

        case kConnectionPower:
            err = kIOReturnSuccess;
            break;

        case kConnectionSyncEnable:
            syncInfo.csMode = (UInt8) (info >> 8);
            syncInfo.csFlags = (UInt8) (info & 0xFF);
            _doControl( this, cscSetSync, &syncInfo);
            err = kIOReturnSuccess;
            break;

        case kConnectionFlags:
            __private->displayConnectFlags |= info;
            err = setConnectionFlags();
            break;

        default:
            err = super::setAttributeForConnection( connectIndex,
					attribute, info );
            break;
    }

    return( err );
}

bool IONDRVFramebuffer::searchOfflineMode( IODisplayModeID * offlineMode )
{
    VDResolutionInfoRec	info;
    VDTimingInfoRec	timingInfo;
    bool		ret = false;

    info.csPreviousDisplayModeID = kDisplayModeIDFindFirstResolution;
    while( 
        (noErr == _doStatus( this, cscGetNextResolution, &info))
        && ((SInt32) info.csDisplayModeID > 0) ) {
    
        timingInfo.csTimingMode   = info.csDisplayModeID;
        timingInfo.csTimingFormat = kDeclROMtables;
        ret = _doStatus( this, cscGetModeTiming, &timingInfo);

        if( (noErr == ret)
          && (kDeclROMtables == timingInfo.csTimingFormat)
          && (timingApple_0x0_0hz_Offline == timingInfo.csTimingData) 
          && (kDisplayModeSafeFlag & timingInfo.csTimingFlags) ) {
            if( offlineMode)
                *offlineMode = info.csDisplayModeID;
            ret = true;
            break;
        }
        info.csPreviousDisplayModeID = info.csDisplayModeID;
    }

    return( ret );
}

IOReturn IONDRVFramebuffer::processConnectChange( UInt32 * value )
{
    IOReturn ret;
    UInt32   connectEnabled;

    if( 0 == *value) {
        getAttributeForConnection( 0, kConnectionEnable, &connectEnabled );
        return( kIOReturnSuccess );
    }

    if( !__private->ackConnectChange)
        return( kIOReturnSuccess );

    DEBG("conn, ");

    deliverFramebufferNotification( kIOFBNotifyDisplayModeWillChange );

    ret = getAttributeForConnection( 0, kConnectionEnable, &connectEnabled );

    setDetailedTimings( 0 );
    removeProperty( kIOFBConfigKey );
    __private->displayConnectFlags = 0;

    do {
        VDScalerInfoRec		scalerRec;

        removeProperty( kIOFBScalerInfoKey );
        bzero( &scalerRec, sizeof( scalerRec));
        scalerRec.csScalerInfoSize = sizeof( scalerRec);
        ret = _doStatus( this, cscGetScalerInfo, &scalerRec );
        if( kIOReturnSuccess == ret)
            setProperty( kIOFBScalerInfoKey, &scalerRec, sizeof( scalerRec));

    } while( false );

    if( mirrored)
        setMirror( 0 );

    IODisplayModeID offlineMode;
    if( !online
     && searchOfflineMode(&offlineMode)
     && (offlineMode != currentDisplayMode)) {
        setDisplayMode( offlineMode, currentDepth );
    }

    __private->ackConnectChange = false;

    ret = kIOReturnSuccess;

    return( ret );
}

IOReturn IONDRVFramebuffer::getAttributeForConnection( IOIndex connectIndex,
                                         IOSelect attribute, UInt32  * value )
{
    IOReturn		ret;
    VDSyncInfoRec	syncInfo;

    switch( attribute ) {

        case kConnectionSyncFlags:
            // find out current state of sync lines
            syncInfo.csMode = 0x00;
            _doStatus( this, cscGetSync, &syncInfo);
            *value = syncInfo.csMode;
            ret = kIOReturnSuccess;
            break;

        case kConnectionSyncEnable:
            // what are the sync-controlling capabilities of the ndrv?
            syncInfo.csMode = 0xFF;
            _doStatus( this, cscGetSync, &syncInfo);
            *value = (UInt32) syncInfo.csMode;
            ret = kIOReturnSuccess;
            break;

        case kConnectionSupportsHLDDCSense:
        case kConnectionSupportsAppleSense:
            ret = kIOReturnSuccess;
            break;

        case kConnectionFlags:
            VDMultiConnectInfoRec multiConnect;
            if( connectIndex == 0 )
                ret = _doStatus( this, cscGetConnection, &multiConnect.csConnectInfo);
            else {
                multiConnect.csDisplayCountOrNumber = connectIndex;
                ret = _doStatus( this, cscGetMultiConnect, &multiConnect);
            }
            if( kIOReturnSuccess == ret)
                *value = multiConnect.csConnectInfo.csConnectFlags;
            else
                *value = 0;
            break;

        case kConnectionEnable:

            if( connectIndex == 0 )
                ret = _doStatus( this, cscGetConnection, &multiConnect.csConnectInfo);
            else {
                multiConnect.csDisplayCountOrNumber = connectIndex;
                ret = _doStatus( this, cscGetMultiConnect, &multiConnect);
            }
            if( (kIOReturnSuccess == ret) 
             && ((1 << kConnectionInactive) & multiConnect.csConnectInfo.csConnectFlags)) {
                online = false;
                *value = online;
                break;
            }

            online = !searchOfflineMode( 0 );
            *value = online;
            ret = kIOReturnSuccess;
            break;

        case kConnectionPostWake:
	    if( __private->postWakeProbe) {
		DEBG("%s: postWakeProbe\n", nub->getName());
		ret = _doControl( this, cscProbeConnection, 0 );
		__private->postWakeProbe = false;
	    } else
		ret = kIOReturnSuccess;
            break;

        case kConnectionChanged:

            ret = processConnectChange(value);
            IONDRVFramebuffer * other;
            if( (other = OSDynamicCast( IONDRVFramebuffer, nextDependent)))
                other->processConnectChange(value);
            break;

        default:
            ret = super::getAttributeForConnection( connectIndex,
				attribute, value );
            break;
    }

    return( ret );
}

IOReturn IONDRVFramebuffer::getAppleSense( IOIndex  connectIndex,
                                            UInt32 * senseType,
                                            UInt32 * primary,
                                            UInt32 * extended,
                                            UInt32 * displayType )
{
    OSStatus		err;
    VDMultiConnectInfoRec	multiConnect;
    UInt32			sense, extSense;

    if( connectIndex == 0 )
        err = _doStatus( this, cscGetConnection, &multiConnect.csConnectInfo);
    else {
        multiConnect.csDisplayCountOrNumber = connectIndex;
        err = _doStatus( this, cscGetMultiConnect, &multiConnect);
    }
    if( err)
	return( err);

    if( (primary || extended)
     && (0 == ((1<<kReportsTagging) & multiConnect.csConnectInfo.csConnectFlags)))

	err = kIOReturnUnsupported;

    else {

        sense 		= multiConnect.csConnectInfo.csConnectTaggedType;
        extSense 	= multiConnect.csConnectInfo.csConnectTaggedData;
	// bug fixes for really old ATI driver
        if( sense == 0) {
            if( extSense == 6) {
                sense          	= kRSCSix;
                extSense        = kESCSixStandard;
            } else if( extSense == 4) {
                sense		= kRSCFour;
                extSense        = kESCFourNTSC;
            }
        }
        if( primary)
            *primary = sense;
        if( extended)
            *extended = extSense;
        if( senseType)
            *senseType = (0 != ((1<<kTaggingInfoNonStandard) & multiConnect.csConnectInfo.csConnectFlags));
        if( displayType)
            *displayType = multiConnect.csConnectInfo.csDisplayType;
    }
    return( err);
}

IOReturn IONDRVFramebuffer::connectFlags( IOIndex /* connectIndex */,
                             IODisplayModeID displayMode, IOOptionBits * flags )
{
    VDTimingInfoRec		timingInfo;
    OSStatus			err;

    timingInfo.csTimingMode = displayMode;
    // in case the driver doesn't do it:
    timingInfo.csTimingFormat = kDeclROMtables;
    err = _doStatus( this, cscGetModeTiming, &timingInfo);

    if( kDetailedTimingFormat == timingInfo.csTimingFormat)
        *flags = kDisplayModeValidFlag | kDisplayModeSafeFlag;
    else
        *flags = timingInfo.csTimingFlags;

    return( err );
}


bool IONDRVFramebuffer::hasDDCConnect( IOIndex  connectIndex )
{
    OSStatus		err;
    VDMultiConnectInfoRec	multiConnect;
    enum		{	kNeedFlags = (1<<kReportsDDCConnection)
					   | (1<<kHasDDCConnection) };
    if( connectIndex == 0 )
        err = _doStatus( this, cscGetConnection, &multiConnect.csConnectInfo);
    else {
        multiConnect.csDisplayCountOrNumber = connectIndex;
        err = _doStatus( this, cscGetMultiConnect, &multiConnect);
    }
    if( err)
        return( err);

    return( (multiConnect.csConnectInfo.csConnectFlags & kNeedFlags)
		== kNeedFlags );
}

// I2C first year for Apple displays.
// Apple monitors older than this (and Manta)
// are never called with I2C commands
enum {
    kFirstAppleI2CYear	= 1999,
    kAppleVESAVendorID	= 0x0610
};

struct EDID {
    UInt8	header[8];
    UInt8	vendorProduct[4];
    UInt8	serialNumber[4];
    UInt8	weekOfManufacture;
    UInt8	yearOfManufacture;
    UInt8	version;
    UInt8	revision;
    UInt8	displayParams[5];
    UInt8	colorCharacteristics[10];
    UInt8	establishedTimings[3];
    UInt16	standardTimings[8];
    UInt8	descriptors[4][18];
    UInt8	extension;
    UInt8	checksum;
};

static bool IsApplePowerBlock(UInt8 * theBlock)
{    
    return( theBlock &&
            0x00000000		== *(UInt32 *)&theBlock[0] &&
            0x00		== theBlock[4] &&
            0x06		== theBlock[5] &&
            0x10		== theBlock[6] );
}

IOReturn IONDRVFramebuffer::getDDCBlock( IOIndex /* connectIndex */, 
					UInt32 blockNumber,
                                        IOSelect blockType,
					IOOptionBits options,
                                        UInt8 * data, IOByteCount * length )

{
    OSStatus		err = 0;
    VDDDCBlockRec	ddcRec;
    ByteCount		actualLength = *length;

    if( forceReadEDID) {
        forceReadEDID = 0;
        options |= kDDCForceReadMask;
    }

    ddcRec.ddcBlockNumber 	= blockNumber;
    ddcRec.ddcBlockType 	= blockType;
    ddcRec.ddcFlags 		= options;

    err = _doStatus( this, cscGetDDCBlock, &ddcRec);

    if( err == noErr) {

	if( actualLength < kDDCBlockSize)
            actualLength = actualLength;
	else
            actualLength = kDDCBlockSize;
        bcopy( ddcRec.ddcBlockData, data, actualLength);
	*length = actualLength;

        if( (1 == blockNumber) && (kIODDCBlockTypeEDID == blockType)
         && (actualLength >= sizeof( EDID))) do {

            EDID * edid;
            UInt32 vendor;
            UInt32 product;

            edid = (EDID *) data;
            vendor = (edid->vendorProduct[0] << 8) | edid->vendorProduct[1];
            product = (edid->vendorProduct[3] << 8) | edid->vendorProduct[2];
            if( kAppleVESAVendorID == vendor) {
                if( (0x01F4 == product) || (0x9D02 == product) || (0x9216 == product))
                    continue;
                if( edid->yearOfManufacture && ((edid->yearOfManufacture + 1990) < kFirstAppleI2CYear))
                    continue;
            }
            shouldDoI2CPower =    (IsApplePowerBlock( &edid->descriptors[1][0])
                                || IsApplePowerBlock( &edid->descriptors[2][0])
                                || IsApplePowerBlock( &edid->descriptors[3][0]));

            err = kIOReturnSuccess;

        } while( false );
    }

    if( 1 == blockNumber)
        DEBG("%s: i2cPower %d\n", nub->getName(), shouldDoI2CPower);

    return( err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initForPM
//
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void IONDRVFramebuffer::initForPM( void )
{
    IOPMPowerState powerStates[ kIONDRVFramebufferPowerStateCount ] = {
      // version,
      // capabilityFlags, outputPowerCharacter, inputPowerRequirement,
      { 1, 0,		     0,	          0,	       0, 0, 0, 0, 0, 0, 0, 0 },
      { 1, 0,                0,           IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 1, IOPMDeviceUsable, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
      // staticPower, unbudgetedPower, powerToAttain, timeToAttain, settleUpTime, 
      // timeToLower, settleDownTime, powerDomainBudget
    };
    VDPowerStateRec sleepInfo;
    IOReturn err;
    bool dozeOnly;

    dozeOnly = getPlatform()->hasPrivPMFeature( kPMHasLegacyDesktopSleepMask );
    if( !dozeOnly && getPlatform()->hasPMFeature( kPMCanPowerOffPCIBusMask )) {
        sleepInfo.powerState = 0;
        sleepInfo.powerFlags = 0;
        sleepInfo.powerReserved1 = 0;
        sleepInfo.powerReserved2 = 0;
        // can this ndrv power off?
        err = _doStatus( this, cscGetPowerState, &sleepInfo);
        dozeOnly = ((kIOReturnSuccess != err)
                 || (0 == (kPowerStateSleepCanPowerOffMask & sleepInfo.powerFlags)));
    }

    if( OSDynamicCast(IOBootNDRV, ndrv))
        dozeOnly = true;

    if( dozeOnly) {
        powerStates[kNDRVFramebufferSleepState].capabilityFlags |= kIOPMPreventSystemSleep;
        powerStates[kNDRVFramebufferDozeState].capabilityFlags  |= kIOPMPreventSystemSleep;
        powerStates[kNDRVFramebufferWakeState].capabilityFlags  |= kIOPMPreventSystemSleep;
    }

    // register ourselves with superclass policy-maker
    registerPowerDriver( this, powerStates, kIONDRVFramebufferPowerStateCount );
    // no sleep until children
    temporaryPowerClampOn();
    // not below doze until system sleep
    changePowerStateTo( kNDRVFramebufferDozeState );
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// maxCapabilityForDomainState
//
// This simple device needs only power.  If the power domain is supplying
// power, the frame buffer can be on.  If there is no power it can only be off.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

unsigned long IONDRVFramebuffer::maxCapabilityForDomainState(
					IOPMPowerFlags domainState )
{
   if( domainState & IOPMPowerOn)
       return( kIONDRVFramebufferPowerStateMax );
   else
       return( kNDRVFramebufferSleepState );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// initialPowerStateForDomainState
//
// The power domain may be changing state.  If power is on in the new
// state, that will not affect our state at all.  If domain power is off,
// we can attain only our lowest state, which is off.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

unsigned long IONDRVFramebuffer::initialPowerStateForDomainState(
					 IOPMPowerFlags domainState )
{
   if( domainState & IOPMPowerOn)
       return( kIONDRVFramebufferPowerStateMax );
   else
       return( kNDRVFramebufferSleepState );
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// powerStateForDomainState
//
// The power domain may be changing state.  If power is on in the new
// state, that will not affect our state at all.  If domain power is off,
// we can attain only our lowest state, which is off.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

unsigned long IONDRVFramebuffer::powerStateForDomainState( 
                                            IOPMPowerFlags domainState )
{
   if( domainState & IOPMPowerOn)
       return( pm_vars->myCurrentState );
   else
       return( kNDRVFramebufferSleepState );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IONDRVFramebuffer::ndrvSetDisplayPowerState( UInt32 state )
{
    IOReturn		err;
    VDSyncInfoRec	syncInfo;

    // what are the sync-controlling capabilities of the ndrv?
    syncInfo.csMode = 0xff;
    err = _doStatus( this, cscGetSync, &syncInfo );
    if( kIOReturnSuccess == err) {
	// pick new sync state
	if( state)
	    syncInfo.csMode = kDPMSSyncOn;
	else if( syncInfo.csMode & (1<<kNoSeparateSyncControlBit))
	    syncInfo.csMode = kDPMSSyncOff;
	else if( getPlatform()->hasPMFeature( kPMHasDimSuspendSupportMask ))
	    syncInfo.csMode = kDPMSSyncOff;
	else
	    syncInfo.csMode = kDPMSSyncOff;
	syncInfo.csFlags = kDPMSSyncMask;
	_doControl( this, cscSetSync, &syncInfo);

	DEBG("%s: sync->%02x\n", nub->getName(), syncInfo.csMode);
    }

    if( shouldDoI2CPower) {
	displayI2CPower( state ? true : false );
	DEBG("%s: i2c->%02x\n", nub->getName(), state ? true : false);
    }

    err = kIOReturnSuccess;

    return( err );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

enum {
    cscSleepWake = 0x86,
    sleepWakeSig = 'slwk',
    vdSleepState = 0,
    vdWakeState  = 1
};
#pragma options align=mac68k

struct VDSleepWakeInfo {
    UInt8	csMode;
    UInt8	fill;
    UInt32	csData;
};
typedef struct VDSleepWakeInfo VDSleepWakeInfo;

#pragma options align=reset

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// ndrvSetPowerState
//
// Called by the superclass to turn the frame buffer on and off.
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IONDRVFramebuffer::ndrvSetPowerState( UInt32 newState )
{
    static const unsigned long
    // [sleep][fromState][toState]
    states[2][kIONDRVFramebufferPowerStateCount][kIONDRVFramebufferPowerStateCount] = {
     {
	{ 0, kAVPowerOff, kAVPowerOn },
	{ kAVPowerOff, 0, kAVPowerOn },
	{ kAVPowerOff, kAVPowerOff, 0 },
     }, {
	{ 0, kHardwareWakeToDoze, kHardwareWake },
	{ kHardwareSleep, 0, kAVPowerOn },
	{ kHardwareSleep, kAVPowerSuspend, 0 }
     }
    };

    IOReturn		err;
    UInt32		sleep = 0;
    UInt32		ndrvPowerState;
    UInt32		oldState;
    IOIndex		postEvent = 0;
    IOAGPDevice *	agpDev;
    IODTPlatformExpert * pe;

    if( newState == powerState)
        return( kIOReturnSuccess );

    if( newState > kIONDRVFramebufferPowerStateMax)
        newState = kIONDRVFramebufferPowerStateMax;

    oldState = powerState;

    if( kIONDRVFramebufferPowerStateMax == oldState) {
        super::handleEvent( kIOFBNotifyWillPowerOff );
        postEvent = kIOFBNotifyDidPowerOff;
	ndrvSetDisplayPowerState( false );
    } else if( kIONDRVFramebufferPowerStateMax == newState) {
        super::handleEvent( kIOFBNotifyWillPowerOn );
        postEvent = kIOFBNotifyDidPowerOn;
    }

    if( kNDRVFramebufferSleepState == newState) {

        IOMemoryDescriptor * vram;
        if( (vram = getVRAMRange())) {
            vram->redirect( kernel_task, true );
            vram->release();
        }
	// tell accelerators to protect HW also
        super::handleEvent( kIOFBNotifyWillSleep, (void *) true );
    }

    if( platformSleep
     && (pe = OSDynamicCast(IODTPlatformExpert, getPlatform()))
     && (pe->getChipSetType() < kChipSetTypeCore99)) {

        VDSleepWakeInfo sleepInfo;

        ndrvPowerState = newState ? vdWakeState : vdSleepState;

        err = _doStatus( this, cscSleepWake, &sleepInfo);

        powerState = newState;

        if( (kIOReturnSuccess == err) && (sleepWakeSig == sleepInfo.csData)
            && (ndrvPowerState != sleepInfo.csMode)) {
            sleepInfo.csMode = ndrvPowerState;

            err = _doControl( this, cscSleepWake, &sleepInfo);
        }

    } else {

        VDPowerStateRec	sleepInfo;

        sleepInfo.powerState = 0;
        sleepInfo.powerFlags = 0;
        sleepInfo.powerReserved1 = 0;
        sleepInfo.powerReserved2 = 0;
    
        err = _doStatus( this, cscGetPowerState, &sleepInfo);

        if( (kIOReturnSuccess == err)
        && ((kPowerStateSleepCanPowerOffMask & sleepInfo.powerFlags)
        || platformSleep))
            sleep = 1;

        ndrvPowerState = states[sleep][oldState][newState];

        if( (kHardwareWakeToDoze == ndrvPowerState)
         && (0 == (kPowerStateSleepWaketoDozeMask & sleepInfo.powerFlags)))
            ndrvPowerState = kHardwareWake;

        else if( kAVPowerSuspend == ndrvPowerState) {

            if( (kPowerStateSupportsReducedPower1BitMask
               | kPowerStateSupportsReducedPower2BitMask
               | kPowerStateSupportsReducedPower3BitMask) & sleepInfo.powerFlags)
                ndrvPowerState = kAVPowerOff;

            else if( false == getPlatform()->hasPMFeature( kPMHasDimSuspendSupportMask ))
                ndrvPowerState = kAVPowerStandby;
        }
    

        DEBG("ndrv %ld powerFlags %08lx, state->%02lx\n",
                    newState, sleepInfo.powerFlags, ndrvPowerState);
    
        powerState = newState;
    
        if( (kIOReturnSuccess != err) || (sleepInfo.powerState != ndrvPowerState)) {
            sleepInfo.powerState = ndrvPowerState;
            sleepInfo.powerFlags = 0;
            sleepInfo.powerReserved1 = kPowerStateSleepWakeNeedsProbeMask;
            sleepInfo.powerReserved2 = 0;

            err = _doControl( this, cscSetPowerState, &sleepInfo);

	    DEBG("ndrv done powerFlags %08lx\n", sleepInfo.powerFlags);

//	    __private->postWakeProbe = (0 != (kPowerStateSleepWakeNeedsProbeMask & sleepInfo.powerFlags));
        }
    }

    agpDev = OSDynamicCast(IOAGPDevice, device);

    if( kNDRVFramebufferSleepState == oldState) {

        UInt32 isOnline, wasOnline = online;
        if( kIOReturnSuccess != getAttributeForConnection( 0,
                                            kConnectionEnable, &isOnline ))
            isOnline = true;
        if( isOnline != wasOnline) {
            online = isOnline;
//            vramMemory = findVRAM();
            if( isOnline)
                getCurrentConfiguration();
        }
        if( agpDev)
            agpDev->resetAGP();
    }

    if( kNDRVFramebufferSleepState == oldState) {
        IOMemoryDescriptor * vram;
        if( (vram = getVRAMRange())) {
            vram->redirect( kernel_task, false );
            vram->release();
        }
	// tell accelerators to unprotect HW also
        super::handleEvent( kIOFBNotifyDidWake, (void *) true );
    }

    if( postEvent) {
        super::handleEvent( postEvent );
	if( kIOFBNotifyDidPowerOn == postEvent) {
	    ndrvSetDisplayPowerState( true );
	}
    }

    if( true && (newState > oldState)) {
	IONDRVFramebuffer * other;
	if( (other = OSDynamicCast( IONDRVFramebuffer, nextDependent))) {
	    other->ndrvSetPowerState( newState );
	}
    }

    if( (kNDRVFramebufferSleepState == oldState) && !__private->ackConnectChange)
        setConnectionFlags();

    return( kIOReturnSuccess );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// ATI patches.

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IONDRVFramebuffer

OSDefineMetaClassAndStructors(IOATINDRV, IONDRVFramebuffer)
OSDefineMetaClassAndStructors(IOATI128NDRV, IOATINDRV)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IOATINDRV::getStartupDisplayMode(
				IODisplayModeID * displayMode, IOIndex * depth )
{
    UInt16 *		nvram;
    OSData *		prop;

    prop = OSDynamicCast( OSData, nub->getProperty("Sime"));
    if( prop) {
	nvram = (UInt16 *) prop->getBytesNoCopy();
	*displayMode = nvram[ 0 ];	// 1 is physDisplayMode
	*depth = nvram[ 2 ] - kFirstDepth;
        return( kIOReturnSuccess);
    } else
        return(super::getStartupDisplayMode( displayMode, depth));
}

IODeviceMemory * IOATINDRV::findVRAM( void )
{
    OSData *		 prop;
    IOByteCount *	 lengths;
    IOMemoryDescriptor * mem;
    UInt32		 data[2];

    if( nub->getProperty("AAPL,vram-memory"))
	return( super::findVRAM());
    prop = OSDynamicCast( OSData, nub->getProperty("ATY,memsize"));
    if( !prop)
	return( super::findVRAM());
    mem = nub->getDeviceMemoryWithIndex(0);
    if( !mem)
	return( super::findVRAM());

    lengths = (IOByteCount *) prop->getBytesNoCopy();
    if( prop->getLength() > sizeof(IOByteCount)) {

        OSNumber * num = OSDynamicCast(OSNumber, getProperty(kIOFBDependentIndexKey));

        data[1] = lengths[1];
        data[0] = (UInt32) mem->getPhysicalAddress();
        if( !num || (0 == num->unsigned32BitValue()))
            data[0] += (mem->getLength() >> 1);
    } else {

        data[1] = lengths[0];
        data[0] = physicalFramebuffer & 0xffff0000;
    }

    nub->setProperty("AAPL,vram-memory", data, sizeof(data));

    return( super::findVRAM());
}

static int g128ExtraCurs = 8;
static int g128DeltaCurs = 0x25c0;

void IOATI128NDRV::flushCursor( void )
{
    volatile UInt32 *	fb;
    UInt32		x;
    int			i;

    fb = (volatile UInt32 *) frameBuffer;
    for( i = 0; i < g128ExtraCurs; i++) {
	x += *(fb++);
	fb += g128DeltaCurs;
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IONDRV
OSDefineMetaClassAndStructors(IOBootNDRV, IONDRV)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOBootNDRV::getUInt32Property( IORegistryEntry * regEntry, const char * name, 
                                  UInt32 * result )
{
    OSData * data;

    data = OSDynamicCast(OSData, regEntry->getProperty(name));
    if( data)
        *result = *((UInt32 *) data->getBytesNoCopy());

    return( data != 0 );
}

IONDRV * IOBootNDRV::fromRegistryEntry( IORegistryEntry * regEntry )
{
    IOBootNDRV * inst;
    IOBootNDRV * result = 0;

    do {
    
        inst = new IOBootNDRV;
        if( !inst)
            continue;
        if( !inst->init())
            continue;
        if( !getUInt32Property(regEntry, "address", (UInt32 *) &inst->fAddress))
            continue;
        if( !getUInt32Property(regEntry, "linebytes", &inst->fRowBytes))
            continue;
        if( !getUInt32Property(regEntry, "width", &inst->fWidth))
            continue;
        if( !getUInt32Property(regEntry, "height", &inst->fHeight))
            continue;
        if( !getUInt32Property(regEntry, "depth", &inst->fBitsPerPixel))
            continue;

        result = inst;

    } while( false );

    if( !result && regEntry->getProperty("AAPL,boot-display")) {
        PE_Video	bootDisplay;
        UInt32		bpp;

        IOService::getPlatform()->getConsoleInfo( &bootDisplay);

        inst->fAddress	    = (void *) bootDisplay.v_baseAddr;
        inst->fRowBytes	    = bootDisplay.v_rowBytes;
        inst->fWidth	    = bootDisplay.v_width;
        inst->fHeight	    = bootDisplay.v_height;
        bpp = bootDisplay.v_depth;
        if( bpp == 15)
            bpp = 16;
        else if( bpp == 24)
            bpp = 32;
        inst->fBitsPerPixel = bpp;

        result = inst;
    }

    if( inst && !result)
        inst->release();

    return( result );
}

void IOBootNDRV::free( void )
{
    super::free();
}

IOReturn IOBootNDRV::getSymbol( const char * symbolName,
				IOLogicalAddress * address )
{
    return( kIOReturnUnsupported );
}

const char * IOBootNDRV::driverName( void )
{
    return( ".Display_boot" );
}

IOReturn IOBootNDRV::doDriverIO( UInt32 commandID, void * contents,
                               UInt32 commandCode, UInt32 commandKind )
{
    IONDRVControlParameters * pb = (IONDRVControlParameters *) contents;
    IOReturn	ret;

    switch( commandCode ) {

        case kIONDRVInitializeCommand:
        case kIONDRVOpenCommand:
            ret = kIOReturnSuccess;
            break;

        case kIONDRVControlCommand:
            ret = doControl( pb->code, pb->params );
            break;
        case kIONDRVStatusCommand:
            ret = doStatus( pb->code, pb->params );
            break;

        default:
            ret = kIOReturnUnsupported;
            break;
    }

    return( ret );
}

IOReturn IOBootNDRV::doControl( UInt32 code, void * params )
{
    IOReturn		ret;
    
    switch( code ) {

        case cscSetEntries:
        case cscSetGamma:
            ret = kIOReturnSuccess;
            break;

        default:
            ret = kIOReturnUnsupported;
            break;
    }

    return( ret );
}

IOReturn IOBootNDRV::doStatus( UInt32 code, void * params )
{
    IOReturn		ret;
    
    switch( code ) {

        case cscGetCurMode:
          {
            VDSwitchInfoRec * switchInfo = (VDSwitchInfoRec *) params;

            switchInfo->csData     = kIOBootNDRVDisplayMode;
            switchInfo->csMode     = kDepthMode1;
            switchInfo->csPage     = 1;
            switchInfo->csBaseAddr = (Ptr) fAddress;
            ret = kIOReturnSuccess;
          }
          break;

        case cscGetNextResolution:
          {
            VDResolutionInfoRec * resInfo = (VDResolutionInfoRec *) params;

            if( (kDisplayModeIDFindFirstResolution == (SInt32) resInfo->csPreviousDisplayModeID)
             || (kDisplayModeIDCurrent == (SInt32) resInfo->csPreviousDisplayModeID)) {
                resInfo->csDisplayModeID 	= kIOBootNDRVDisplayMode;
                resInfo->csMaxDepthMode		= kDepthMode1;
                resInfo->csHorizontalPixels	= fWidth;
                resInfo->csVerticalLines	= fHeight;
                resInfo->csRefreshRate		= 0 << 16;
                ret = kIOReturnSuccess;

            } else if( kIOBootNDRVDisplayMode == resInfo->csPreviousDisplayModeID) {
                resInfo->csDisplayModeID = kDisplayModeIDNoMoreResolutions;
                ret = kIOReturnSuccess;

            } else {
                resInfo->csDisplayModeID = kDisplayModeIDInvalid;
                ret = kIOReturnBadArgument;
            }
          }
          break;

        case cscGetVideoParameters:
          {
            VDVideoParametersInfoRec * pixelParams = (VDVideoParametersInfoRec *) params;

            if( (kIOBootNDRVDisplayMode != pixelParams->csDisplayModeID)
             || (kDepthMode1 != pixelParams->csDepthMode)) {
                ret = kIOReturnBadArgument;
                break;
            }
            VPBlock *	pixelInfo = pixelParams->csVPBlockPtr;

            pixelInfo->vpBounds.left	= 0;
            pixelInfo->vpBounds.top	= 0;
            pixelInfo->vpBounds.right	= fWidth;
            pixelInfo->vpBounds.bottom	= fHeight;
            pixelInfo->vpRowBytes	= fRowBytes;
            pixelInfo->vpPlaneBytes	= 0;
            pixelInfo->vpPixelSize	= fBitsPerPixel;
            ret = kIOReturnSuccess;
          }
          break;

        case cscGetModeTiming:
          {
            VDTimingInfoRec * timingInfo = (VDTimingInfoRec *) params;

            if( kIOBootNDRVDisplayMode != timingInfo->csTimingMode) {
                ret = kIOReturnBadArgument;
                break;
            }
            timingInfo->csTimingFormat = kDeclROMtables;
            timingInfo->csTimingFlags  = kDisplayModeValidFlag | kDisplayModeSafeFlag;
            ret = kIOReturnSuccess;
          }
          break;

        default:
            ret = kIOReturnUnsupported;
            break;
    }

    return( ret );
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSMetaClassDefineReservedUsed(IONDRVFramebuffer, 0);

OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 1);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 2);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 3);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 4);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 5);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 6);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 7);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 8);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 9);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 10);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 11);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 12);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 13);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 14);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 15);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 16);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 17);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 18);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 19);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 20);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 21);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 22);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 23);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 24);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 25);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 26);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 27);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 28);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 29);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 30);
OSMetaClassDefineReservedUnused(IONDRVFramebuffer, 31);
