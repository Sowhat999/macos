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

#include "IOPMUSB99.h"

#define number_of_power_states 3

static IOPMPowerState ourPowerStates[number_of_power_states] = {
{1,0,0,0,0,0,0,0,0,0,0,0},
    {1,IOPMPowerOn,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0},
    {1,IOPMPowerOn,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
};

#define super IOService
OSDefineMetaClassAndStructors(IOPMUSB99,IOService)


// **********************************************************************************
// start
//
// **********************************************************************************
bool IOPMUSB99::start ( IOService * nub )
{
    super::start(nub);
    PMinit();

    registerPowerDriver(this,ourPowerStates,number_of_power_states);

    changePowerStateTo( number_of_power_states-1);			// clamp power on

    setProperty ("IOClass", "IOPMUSB99");

    return true;
}


// **********************************************************************************
// setPowerState
//
// **********************************************************************************
IOReturn IOPMUSB99::setPowerState ( long powerStateOrdinal, IOService* whatDevice)
{
  return IOPMAckImplied;
}


// **********************************************************************************
// initialPowerStateForDomainState
//
// this is incomplete pending completed design of root domain states
// **********************************************************************************
unsigned long IOPMUSB99::initialPowerStateForDomainState ( IOPMPowerFlags powerFlags)
{
    if ( powerFlags & IOPMPowerOn ) {
        return  number_of_power_states-1;
    }
    return  0;
}
