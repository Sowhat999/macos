# This script will generate the AppleEvents interface for Python.
# It uses the "bgen" package to generate C code.
# It execs the file aegen.py which contain the function definitions
# (aegen.py was generated by aescan.py, scanning the <AppleEvents.h> header file).


from macsupport import *


AEArrayType = Type("AEArrayType", "c")
AESendMode = Type("AESendMode", "l")
AESendPriority = Type("AESendPriority", "h")
AEInteractAllowed = Type("AEInteractAllowed", "b")
AEReturnID = Type("AEReturnID", "h")
AETransactionID = Type("AETransactionID", "l")



AEEventClass = OSTypeType('AEEventClass')
AEEventID = OSTypeType('AEEventID')
AEKeyword = OSTypeType('AEKeyword')
DescType = OSTypeType('DescType')


AEDesc = OpaqueType('AEDesc')
AEDesc_ptr = OpaqueType('AEDesc')

AEAddressDesc = OpaqueType('AEAddressDesc', 'AEDesc')
AEAddressDesc_ptr = OpaqueType('AEAddressDesc', 'AEDesc')

AEDescList = OpaqueType('AEDescList', 'AEDesc')
AEDescList_ptr = OpaqueType('AEDescList', 'AEDesc')

AERecord = OpaqueType('AERecord', 'AEDesc')
AERecord_ptr = OpaqueType('AERecord', 'AEDesc')

AppleEvent = OpaqueType('AppleEvent', 'AEDesc')
AppleEvent_ptr = OpaqueType('AppleEvent', 'AEDesc')


class EHType(Type):
	def __init__(self, name = 'EventHandler', format = ''):
		Type.__init__(self, name, format)
	def declare(self, name):
		Output("AEEventHandlerUPP %s__proc__ = upp_GenericEventHandler;", name)
		Output("PyObject *%s;", name)
	def getargsFormat(self):
		return "O"
	def getargsArgs(self, name):
		return "&%s" % name
	def passInput(self, name):
		return "%s__proc__, (long)%s" % (name, name)
	def passOutput(self, name):
		return "&%s__proc__, (long *)&%s" % (name, name)
	def mkvalueFormat(self):
		return "O"
	def mkvalueArgs(self, name):
		return name
	def cleanup(self, name):
		Output("Py_INCREF(%s); /* XXX leak, but needed */", name)

class EHNoRefConType(EHType):
	def passInput(self, name):
		return "upp_GenericEventHandler"

EventHandler = EHType()
EventHandlerNoRefCon = EHNoRefConType()


IdleProcPtr = FakeType("upp_AEIdleProc")
AEIdleUPP = IdleProcPtr
EventFilterProcPtr = FakeType("(AEFilterUPP)0")
AEFilterUPP = EventFilterProcPtr
NMRecPtr = FakeType("(NMRecPtr)0")
EventHandlerProcPtr = FakeType("upp_GenericEventHandler")
AEEventHandlerUPP = EventHandlerProcPtr
AlwaysFalse = FakeType("0")


AEFunction = OSErrFunctionGenerator
AEMethod = OSErrMethodGenerator


includestuff = includestuff + """
#ifdef WITHOUT_FRAMEWORKS
#include <AppleEvents.h>
#include <AEObjects.h>
#else
#include <Carbon/Carbon.h>
#endif

#ifdef USE_TOOLBOX_OBJECT_GLUE
extern PyObject *_AEDesc_New(AEDesc *);
extern int _AEDesc_Convert(PyObject *, AEDesc *);

#define AEDesc_New _AEDesc_New
#define AEDesc_Convert _AEDesc_Convert
#endif

static pascal OSErr GenericEventHandler(); /* Forward */

AEEventHandlerUPP upp_GenericEventHandler;

static pascal Boolean AEIdleProc(EventRecord *theEvent, long *sleepTime, RgnHandle *mouseRgn)
{
	if ( PyOS_InterruptOccurred() )
		return 1;
#if !TARGET_API_MAC_OSX
	if ( PyMac_HandleEvent(theEvent) < 0 ) {
		PySys_WriteStderr("Exception in user event handler during AE processing\\n");
		PyErr_Clear();
	}
#endif
	return 0;
}

AEIdleUPP upp_AEIdleProc;
"""

finalstuff = finalstuff + """
#if UNIVERSAL_INTERFACES_VERSION >= 0x0340
typedef long refcontype;
#else
typedef unsigned long refcontype;
#endif

static pascal OSErr
GenericEventHandler(const AppleEvent *request, AppleEvent *reply, refcontype refcon)
{
	PyObject *handler = (PyObject *)refcon;
	AEDescObject *requestObject, *replyObject;
	PyObject *args, *res;
	if ((requestObject = (AEDescObject *)AEDesc_New((AppleEvent *)request)) == NULL) {
		return -1;
	}
	if ((replyObject = (AEDescObject *)AEDesc_New(reply)) == NULL) {
		Py_DECREF(requestObject);
		return -1;
	}
	if ((args = Py_BuildValue("OO", requestObject, replyObject)) == NULL) {
		Py_DECREF(requestObject);
		Py_DECREF(replyObject);
		return -1;
	}
	res = PyEval_CallObject(handler, args);
	requestObject->ob_itself.descriptorType = 'null';
	requestObject->ob_itself.dataHandle = NULL;
	replyObject->ob_itself.descriptorType = 'null';
	replyObject->ob_itself.dataHandle = NULL;
	Py_DECREF(args);
	if (res == NULL)
		return -1;
	Py_DECREF(res);
	return noErr;
}
"""

initstuff = initstuff + """
	upp_AEIdleProc = NewAEIdleUPP(AEIdleProc);
#if UNIVERSAL_INTERFACES_VERSION >= 0x03400
	upp_GenericEventHandler = NewAEEventHandlerUPP(&GenericEventHandler);
#else
	upp_GenericEventHandler = NewAEEventHandlerUPP(GenericEventHandler);
#endif
	PyMac_INIT_TOOLBOX_OBJECT_NEW(AEDesc *, AEDesc_New);
	PyMac_INIT_TOOLBOX_OBJECT_CONVERT(AEDesc, AEDesc_Convert);
"""

module = MacModule('_AE', 'AE', includestuff, finalstuff, initstuff)

class AEDescDefinition(GlobalObjectDefinition):

	def __init__(self, name, prefix = None, itselftype = None):
		GlobalObjectDefinition.__init__(self, name, prefix or name, itselftype or name)
		self.argref = "*"

	def outputFreeIt(self, name):
		Output("AEDisposeDesc(&%s);", name)

	def outputGetattrHook(self):
		Output("""
if (strcmp(name, "type") == 0)
	return PyMac_BuildOSType(self->ob_itself.descriptorType);
if (strcmp(name, "data") == 0) {
	PyObject *res;
#if !TARGET_API_MAC_CARBON
	char state;
	state = HGetState(self->ob_itself.dataHandle);
	HLock(self->ob_itself.dataHandle);
	res = PyString_FromStringAndSize(
		*self->ob_itself.dataHandle,
		GetHandleSize(self->ob_itself.dataHandle));
	HUnlock(self->ob_itself.dataHandle);
	HSetState(self->ob_itself.dataHandle, state);
#else
	Size size;
	char *ptr;
	OSErr err;
	
	size = AEGetDescDataSize(&self->ob_itself);
	if ( (res = PyString_FromStringAndSize(NULL, size)) == NULL )
		return NULL;
	if ( (ptr = PyString_AsString(res)) == NULL )
		return NULL;
	if ( (err=AEGetDescData(&self->ob_itself, ptr, size)) < 0 )
		return PyMac_Error(err);	
#endif
	return res;
}
if (strcmp(name, "__members__") == 0)
	return Py_BuildValue("[ss]", "data", "type");
""")


aedescobject = AEDescDefinition('AEDesc')
module.addobject(aedescobject)

functions = []
aedescmethods = []

execfile('aegen.py')
##execfile('aedatamodelgen.py')

for f in functions: module.add(f)
for f in aedescmethods: aedescobject.add(f)

SetOutputFileName('_AEmodule.c')
module.generate()
