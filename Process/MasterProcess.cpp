#include "MasterProcess.h"
#include <iostream>

bool processIsAlive(pid_t)
{
    return true;
}

MasterProcess::MasterProcess(uint16_t _processNum,
                             map<int, signalHandler> &map,
                             string &programName, string &mutexName)
    : pMutex(mutexName), shmName(programName),
      eventLoop(new muduo::net::EventLoop()),
      handlers(map), processNum(_processNum)
{
    //register signal handers
    //signalProcessInit();

    //init shared memory;
    shmFd = shm_open(shmName.c_str(), O_RDWR | O_CREAT, 0);
    assert(shmFd != -1);
    int res = ftruncate(shmFd, sizeof(int) * (processNum - 1));
    assert(res != -1);
    shmAddr = (int *)mmap(nullptr, sizeof(int) * (processNum - 1), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    assert(shmAddr != nullptr);
}

MasterProcess::~MasterProcess()
{
    shm_unlink(shmName.c_str());
    close(shmFd);
    delete eventLoop;
}

/**
*  init the signal, register to poller，
*  signalfd has not implemented on wsl
*/
void MasterProcess::signalProcessInit()
{
    sigset_t mask;
    int sfd;
    sigemptyset(&mask);
    for (pair<const int, signalHandler> &entry : handlers)
    {
        sigaddset(&mask, entry.first);
    }
    //assert(sigprocmask(SIG_BLOCK, &mask, NULL) != -1);
    /*if(sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
    {
        cout<<errno<<endl;
    }*/
    sfd = signalfd(-1, &mask, SFD_NONBLOCK);
    cout << errno << endl;
    assert(sfd != -1);
    muduo::net::Channel *channel = new muduo::net::Channel(eventLoop, sfd);
    channel->setReadCallback(std::bind(&MasterProcess::signalHandlers, this));
    eventLoop->updateChannel(channel);
}

/**
*  start worker processes,
*  start loop() to process signals
*/
void MasterProcess::start()
{
    pid_t pid;
    for (uint16_t i = 0; i < processNum - 1; i++)
    {
        int fd = eventfd(0, EFD_NONBLOCK);
        *(shmAddr + i) = fd;

        if ((pid = fork()) == 0)
        {
            //construct worker object
            //need re-create the object on heap, such as pmutex;
            cout << "fork: " << *(shmAddr + i) << endl;
            break;
        }
    }
    if (pid == 0)
    {
        //start worker to process network IO
        while (true)
        {
            sleep(2);
            //for test
            shm_unlink(shmName.c_str());
            cout << "i am child, doing network io" << endl;
        }
    }
    else
    {
        eventLoop->runEvery(checkInterval, bind(&MasterProcess::checkWorkerStatus, this));
        //master should process signal
        eventLoop->loop();
    }
}

void MasterProcess::checkWorkerStatus()
{
    for (int i = 0; i < eventfds.size(); ++i)
    {
        if (!processIsAlive(eventfds[i].first))
        {
            int fd = eventfd(0, EFD_NONBLOCK);
            pid_t pid = fork();
            if (pid == 0)
            {
                //
            }
            else
            {
                eventfds[i].first = pid;
            }
        }
        else
        {
            cout << "check ok" << endl;
        }
    }
}

void MasterProcess::signalHandlers()
{
}