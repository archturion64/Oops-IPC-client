#ifndef OOPSIPCCLIENT_H
#define OOPSIPCCLIENT_H

#ifdef __cplusplus
#include <iostream>
#include <functional>

using DataReceivedT = std::string;
using CallBackFunctionT = std::function<void(const char*, const unsigned long)>;

bool registerCallback (const std::string &socketPath, const CallBackFunctionT callback);
void deregisterCallback (const std::string &socketPath, const CallBackFunctionT callback);
#else // C

typedef void (*LegacyCallBackT)(const char* buf, const unsigned long bufSize);
bool registerCallback (const char* socketPath, const LegacyCallBackT callback);
void deregisterCallback (const char* socketPath, const LegacyCallBackT callback);
#endif // __cplusplus


#endif // OOPSIPCCLIENT_H
