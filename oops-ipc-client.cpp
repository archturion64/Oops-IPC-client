#include <memory>
#include <atomic>
#include <unordered_map>
#include <thread>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <unistd.h>
#include <cstring>


#include "oops-ipc-client.h"

constexpr static const int INVALID_SOCKET_FD {-1};
constexpr static const int POLL_TIMEOUT_MS {1000};
constexpr static const std::size_t SOCKET_PAYLOAD_SIZE{4096};

static bool callCallBack(CallBackFunctionT& cb, const DataReceivedT& data, const std::size_t& length);
static void runSocketReceive (const std::string socketPath);

struct SocketData
{
    int socketFd {INVALID_SOCKET_FD};
    std::atomic<bool> shouldStop {false};
    DataReceivedT data;
    CallBackFunctionT callBack;
};

static SocketData* SocketFactory (const std::string& socketPath, const CallBackFunctionT& callback)
{
    SocketData* retVal {nullptr};
    try {
        retVal = new SocketData();
        retVal->callBack = callback;
        retVal->data = DataReceivedT(SOCKET_PAYLOAD_SIZE, 0);

        sockaddr_un socketInfo{};
        socketInfo.sun_family = AF_UNIX;
        std::memcpy(socketInfo.sun_path,
                    socketPath.c_str(),
                    std::min(socketPath.size(), sizeof (socketInfo.sun_path)));

        const socklen_t socketInfoSize = static_cast<socklen_t>(strlen(socketInfo.sun_path)
                                                                + sizeof(socketInfo.sun_family));
        if ((INVALID_SOCKET_FD == (retVal->socketFd = socket(AF_UNIX, SOCK_STREAM, 0))) ||
                (INVALID_SOCKET_FD == connect(retVal->socketFd,
                                              reinterpret_cast<struct sockaddr*>(&socketInfo),
                                              socketInfoSize)))
        {
            std::cerr << "Failed to establish socket connection!" << "\n";
            delete retVal;
            retVal = nullptr;
        }
    }  catch (const std::bad_alloc& ex) {
        std::cerr << ex.what() << "Failed to allocate memmory for the SocketData struct!" << "\n";
    }
    return retVal;
}

struct SocketJunkYard {
    void operator()(SocketData* sdata)
    {
        if (sdata != nullptr)
        {
            sdata->shouldStop.store(true);
            sdata->callBack = nullptr;
            if (INVALID_SOCKET_FD != sdata->socketFd)
            {
                close(sdata->socketFd);
            }
            sdata->socketFd = INVALID_SOCKET_FD;
            delete sdata;
        }
    }
};

using SocketPointerT = std::shared_ptr<SocketData>;

static std::unordered_map< std::string, std::shared_ptr<SocketData> > activeSockets;

bool callCallBack(CallBackFunctionT& cb, const DataReceivedT& data, const std::size_t& length)
{
    bool retVal{false};
    if (cb)
    {
        cb(data.data(), length);
    }
    return retVal;
}

void runSocketReceive (const std::string socketPath)
{
    try {
        auto & sock =  *activeSockets.at(socketPath).get();
        pollfd pfd{};
        pfd.fd = sock.socketFd;
        pfd.events = POLLIN | POLLPRI;

        while(sock.shouldStop.load() == false)
        {
            if ((poll(&pfd, 1, POLL_TIMEOUT_MS) > 0) && (pfd.revents & (POLLIN | POLLPRI)))
            {
                const auto receivedBytes = recv(sock.socketFd,
                                                &sock.data,
                                                sock.data.size(),
                                                0);
                if (receivedBytes > 0)
                {
                    if (callCallBack(sock.callBack,
                                     sock.data,
                                     static_cast<std::size_t>(receivedBytes)) == false)
                    {
                        std::cerr << "No receiver registered for "  <<  socketPath << "\n";
                        break;
                    }

                } else {
                    if (receivedBytes < 0){
                        std::cerr << "IPC error during transmission on "  <<  socketPath << "\n";
                    }else{
                        std::cerr << "Server terminated the communication for " << socketPath << "\n";
                    }
                    break;
                }
            }
        }
    }  catch (const std::out_of_range& oor) {
        std::cerr << oor.what() << " Failed while trying to access " << socketPath << "\n";
    }
}

bool registerSocket(const std::string& socketPath, const CallBackFunctionT& callback)
{
    bool retVal{false};
    SocketPointerT ptr = SocketPointerT(SocketFactory(socketPath, callback), SocketJunkYard());
    if (ptr)
    {
        activeSockets[socketPath] = std::move(ptr);
        std::thread thread (runSocketReceive, socketPath);
        thread.detach();
        retVal = true;
    }
    return retVal;
}

bool registerCallback (const std::string& socketPath, const CallBackFunctionT callback)
{
    return registerSocket(socketPath, callback);
}

void deregisterCallback (const std::string& socketPath)
{
    activeSockets.erase(socketPath);
}

#ifndef __cplusplus
bool registerCallback (const char* socketPath, const LegacyCallBackT callback)
{
    return registerSocket(socketPath, CallBackFunctionT(callback));
}

void deregisterCallback (const char* socketPath)
{
    activeSockets.erase(socketPath);
}

#endif

