//
//  libfwkeyfetch.hpp
//  libfwkeyfetch
//
//  Created by tihmstar on 21.05.25.
//

#ifndef libfwkeyfetch_hpp
#define libfwkeyfetch_hpp

#include <iostream>

#include <stdint.h>

#ifndef LIBFWKEYFETCH_API
#   define LIBFWKEYFETCH_API
#endif

namespace tihmstar {
    namespace libfwkeyfetch {
        struct fw_key{
            char iv[32 + 1];
            char key[64 + 1];
            std::string pathname;
        };
        struct pwnBundle{
            std::string firmwareUrl;
            fw_key iBSSKey;
            fw_key iBECKey;
        };
    
        LIBFWKEYFETCH_API fw_key getFirmwareKeyForKBAG(std::string kbag);
        LIBFWKEYFETCH_API fw_key getFirmwareKeyForComponent(std::string device, std::string buildnum, std::string component, uint32_t cpid = 0, std::string zipURL = "");
        LIBFWKEYFETCH_API fw_key getFirmwareKeyForPath(std::string device, std::string buildnum, std::string path, uint32_t cpid = 0, std::string zipURL = "");
        LIBFWKEYFETCH_API pwnBundle getPwnBundleForDevice(std::string device, std::string buildnum = "", uint32_t cpid = 0, std::string zipURL = "");
        LIBFWKEYFETCH_API const char *version();
    }
}

#endif /* libfwkeyfetch_hpp */
