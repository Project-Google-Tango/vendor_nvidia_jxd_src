================================================
User documentation for using OpenMax Tracing
================================================

Tracing may be enabled for for applications using OpenMax by placing a file
named 'NvxTrace.ini' in the same directory as the app is run from.  The level
of tracing may be adjusted on a per-component group (i.e., audio decoder,
video decoder, video renderer) basis, and several different types of tracing
can be selected.

For example :

f log.txt
{All
enable = 1
AllTypes  = 0
Error     = 1
Warning   = 1
Buffer    = 0
Info      = 1
Worker    = 0
State     = 1
CallGraph = 0
}
{VideoDecoder
enable = 1
Buffer = 1
}

In this example, all components have error, warning, info, and state change
tracing enabled, and the output will be sent to 'log.txt'.  The video decoder
component (h264/mp4/etc) will also have buffer passing tracing enabled.  

If the first line is left off, output will go to stdout.

Valid component types are: Core, Scheduler, FileReader, VideoDecoder,
ImageDecoder, AudioDecoder, VideoRenderer, VideoScheduler, AudioMixer,
VideoPostProcessor, ImagePostProcessor, Camera, AudioRecorder, VideoEncoder,
ImageEncoder, AudioEncoder, FileWriter, Clock, Generic, Undesignated, and,
finally, All.

Valid trace levels are: Error, Warning, Buffer, Info, Worker, State,
CallGraph, and AllTypes.


==================================================
Developer documentation for using OpenMax Tracing
==================================================

Steps To use Trace

1. Information about enums used for tracing

a) ENvxtObjectType
   Contains all the trace types.

b) ENvxtObjectType
   Contains all the object types.

****************************************************************

2. Call trace from the required files.

Following are the examples where TRace is called for different tracetypes.

pThis->eObjectType in all the trace calls is the object -id from enum
ENvxtObjectType.  So before using the trace in OpenMax , you will have to set
this object id after the component has been created in your component.
  
For Example:

pThis->eObjectType = NVXT_RAW_READ;

a) NVXT_NONE 

b) NVXT_ERROR   
NvxtTrace(NVXT_ERROR,pNvComp->eObjectType, "ERROR at %s,
%d\n", __FILE__, __LINE__); 

This trace gives the exact line number and the filename at which the error
occured.

c) NVXT_WARNING   
NvxtTrace(NVXT_WARNING,pNvComp->eObjectType, WARNING at %s, %d\n", __FILE__,
__LINE__); 

This trace gives the exact line number and the filename at which the warning
occured.

d) NVXT_INFO    
NvxtTrace(NVXT_INFO,pThis->eObjectType,"\n--NvxFileReaderSetParameter fname is
%s",pState->pFilename); 

This trace is used to display any important information.

e) NVXT_BUFFER   
NvxtTrace(NVXT_BUFFER,pThis->pNvComp->eObjectType,"+NvxPortDeliverFullBuffer");

This trace gives information about anything that is buffer related.

f) NVXT_WORKER   
NvxtTrace(NVXT_WORKER,NVXT_SCHEDULER, "NvxWorkerResume");

This trace gives information about the worker functions.

g) NVXT_CALLGRAPH 
NvxtTrace(NVXT_CALLGRAPH,pThis->eObjectType,"\n%s","--NvxImageFileReaderInit");

This trace type gives information about the sequence of calls .	

h)  NVXT_CALLTRACE 
NvxtTrace(NVXT_CALLTRACE ,pThis->eObjectType,"\n%s","--NvxImageFileReaderInit");

This trace gives detailed information about the program execution at the point
where trace is called.  The value of certain  variables or return values can
be displayed.  The line number and filename where the trace is called can also
be displayed.

    
****************************************************************

3)  The following functions are called from Nvxcore.c to set the "log" destination and to set the trace settings for the objecttypes.


a)  NvxtSetDestination(NVXT_LOGFILE,"tracelog.txt");
NVXT_LOGFILE will redirect the output to the logfile.  NVXT_STDOUT to redirect
output to the standard output.  2nd parameter is the log-filename.

b)  NvxtConfigReader("NvxTrace.ini");
NvxTrace.ini file has the trace settings.  Refer NvxTrace.ini (checked in
under ReaderWriterContentPipeTest) as a example.

NvxTrace.ini for example will look like :

   f log.txt	   
   {FileReader
   enable = 1
   AllTypes  = 0
   Error     = 1
   Warning   = 1
   Worker    = 1
   CallGraph = 1
   }

f log.txt

The tracelog filename can also be specified from the ini file as above.

The tracetypes are pretty much self explanatory.

The first string after the '{' is the objecttype.

If enable = 0 the values of the individual tracetypes will not matter during
trace calls.

if AllTypes  = 1 , then all tracetypes are set to 1
if AllTypes  = 0 , then all tracetypes are set to 0

c) NvxtConfigWriter("NvxTraceOut.ini");
This will dump the existing trace settings in the file NvxTraceOut.ini


****************************************************************

4) Default Trace settings.

a) If the ".ini" file is not found the following message will be displayed.

    "Failed to open the trace config file"

b) If setdestination is not called and if no trace filename is specified from
the NvxTRace.ini file then in that case the output trace log generated will be
redirected to standard output.

d) If while parsing the  ini file 'f' is encountered but with no arguments in
that case the trace logfile with the default filename is created for writing
the trace logs.

c) By default all the traces for all the objects are disabled.

d) The tracelog will always be created at the location from where the
executable is run.

****************************************************************

5) How to add different tracing implementation to the existing trace
interface.  To achieve this the four functions of the  following prtotypes
will have to be implemented.

   void (*NvxtTraceInitFunction)();    
   void (*NvxtPrintfFunction)(OMX_U32 , OMX_U32 , OMX_U8 *, ...); 
   void (*NvxtFlushFunction)(); 
   void (*NvxtTraceDeInitFunction)(); 

These new defined functions should override the default implementation but calling NvxtOverrideTraceImplementation().


****************************************************************

6) In addition to this new tracetypes and object types can be added to the
existing trace functionality. 
