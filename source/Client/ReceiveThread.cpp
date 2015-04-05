#include "ReceiveThread.h"
#include "../Buffer/MessageQueue.h"
#include "../Buffer/JitterBuffer.h"

// static function forward declarations

static int startRoutine(HANDLE* thread, HANDLE stopEvent,
    LPTHREAD_START_ROUTINE routine, void* params);
static int stopRoutine(HANDLE* thread, HANDLE stopEvent);

// receive thread implementation

ReceiveThread::ReceiveThread(UDPSocket* udpSocket, JitterBuffer* musicJitterBuffer)
{
    this->udpSocket         = udpSocket;
    this->musicJitterBuffer = musicJitterBuffer;
    this->thread            = INVALID_HANDLE_VALUE;
    this->threadStopEv      = CreateEvent(NULL,TRUE,FALSE,NULL);
}

ReceiveThread::~ReceiveThread()
{
    stop();
}

void ReceiveThread::start()
{
    startRoutine(&thread,threadStopEv,threadRoutine,this);
}

void ReceiveThread::stop()
{
    stopRoutine(&thread,threadStopEv);
}

DWORD WINAPI ReceiveThread::threadRoutine(void* params)
{
    #ifdef DEBUG
    OutputDebugStream("Thread started...\n");
    #endif

    // parse thread parameters
    ReceiveThread* dis = (ReceiveThread*) params;

    // perform the thread routine
    int breakLoop = FALSE;
    while(!breakLoop)
    {
        HANDLE handles[] = {
            dis->threadStopEv,
            dis->udpSocket->getMessageQueue()->hasMessage
        };
        switch(WaitForMultipleObjects(3,handles,FALSE,INFINITE))
        {
        case WAIT_OBJECT_0+0:   // stop event triggered
            breakLoop = TRUE;
            break;
        case WAIT_OBJECT_0+1:   // message queue has message
            ReceiveThread::handleMsgqMsg(dis);
            break;
        default:
            OutputDebugString(L"ReceiveThread::_threadRoutine WaitForMultipleObjects");
            break;
        }
    }

    // return...
    #ifdef DEBUG
    OutputDebugStream("Thread stopped...\n");
    #endif
    return 0;
}

void ReceiveThread::handleMsgqMsg(ReceiveThread* dis)
{
    TCHAR s[256];

    // allocate memory to hold message queue message
    int msgType;
    char* element = (char*) malloc(dis->udpSocket->getMessageQueue()->elementSize);

    // get the message queue message
    dis->udpSocket->getMessageQueue()->dequeue(&msgType,element);

    // process the message queue message according to its type
    switch(msgType)
    {
    case MUSICSTREAM:
    {
        DataPacket* packet = (DataPacket*) &element;
        dis->musicJitterBuffer->put(packet->index,&packet->data);
        break;
    }
    case MICSTREAM:
    {
        // DataPacket packet;
        // memcpy(packet.string,element.string,STR_LEN);
        break;
    }
    default:
        fprintf(stderr,"WARNING: received unknown message type: %d\n",msgType);
        break;
    }
}

// static function implementations

int startRoutine(HANDLE* thread, HANDLE stopEvent,
    LPTHREAD_START_ROUTINE routine, void* params)
{
    // return immediately if the routine is already running
    if(*thread != INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    // reset the stop event
    ResetEvent(stopEvent);

    // start the thread & return
    DWORD useless;
    *thread = CreateThread(0,0,routine,params,0,&useless);
    return (*thread == INVALID_HANDLE_VALUE);
}

int stopRoutine(HANDLE* thread, HANDLE stopEvent)
{
    // return immediately if the routine is already stopped
    if(*thread == INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    // set the stop event to stop the thread
    SetEvent(stopEvent);
    WaitForSingleObject(*thread,INFINITE);

    // invalidate thread handle, so we know it's terminated
    *thread = INVALID_HANDLE_VALUE;
    return 0;
}
