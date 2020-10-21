#include "StreamElementsUtils.hpp"

#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>

#include <unistd.h>
#include <limits.h>

#include <string>
#include <algorithm>

static std::string GetUserName()
{
    char username[_POSIX_LOGIN_NAME_MAX];
    getlogin_r(username, _POSIX_LOGIN_NAME_MAX);
    username[_POSIX_LOGIN_NAME_MAX - 1] = 0;
    
    return username;
}

std::string GetComputerSystemUniqueId()
{
    // Get MacOS serial number
    // https://stackoverflow.com/questions/5868567/unique-identifier-of-a-mac
    // -framework IOKit -framework Foundation
    
    io_service_t platformExpert = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("IOPlatformExpertDevice"));

    if (platformExpert) {
        CFStringRef cfsSerial = (CFStringRef)IORegistryEntryCreateCFProperty(platformExpert, CFSTR(kIOPlatformSerialNumberKey), kCFAllocatorDefault, 0);

        IOObjectRelease(platformExpert);

        if (cfsSerial) {
            NSString* nsSerial = [NSString stringWithString:(NSString*)cfsSerial];

            CFRelease(cfsSerial);

            if (nsSerial) {
                std::string username = GetUserName();
                std::string hash = CreateSHA256Digest(username);
                std::transform(hash.begin(), hash.end(), hash.begin(), ::toupper);

                std::string result = "ALSU/"; // AppLe Serial & Username-hash
                result += [nsSerial UTF8String];
                result += "-";
                result += hash;

                return result;
            } else {
                return "ERR_NO_NS_SERIAL";
            }
        } else {
            return "ERR_NO_CFS_SERIAL";
        }
    } else {
        return "ERR_NO_PLATFORM_EXPERT";
    }
}
