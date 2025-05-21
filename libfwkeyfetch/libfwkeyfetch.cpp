//
//  libfwkeyfetch.cpp
//  libfwkeyfetch
//
//  Created by tihmstar on 21.05.25.
//

#ifndef LIBFWKEYFETCH_API
#   ifdef WIN32
#       define LIBFWKEYFETCH_API __declspec(dllimport)
#   elif __GNUC__ >=4
#       define LIBFWKEYFETCH_API __attribute__((visibility("default")))
#   else
#       define LIBFWKEYFETCH_API
#   endif
#endif

#include "../include/libfwkeyfetch/libfwkeyfetch.hpp"

#include <curl/curl.h>

#include <libgeneral/macros.h>

#ifdef HAVE_LIBFRAGMENTZIP
#include <libfragmentzip/libfragmentzip.h>
#endif //HAVE_LIBFRAGMENTZIP

extern "C" {
#include "jssy.h"
}

#ifdef WITH_REMOTE_KEYS
#define FIRMWARE_JSON_URL_START "https://raw.githubusercontent.com/tihmstar/fwkeydb/master/keys/firmware/"
#define FWKEYDB_LATEST_RELESES "https://api.github.com/repos/tihmstar/fwkeydb/releases/latest"
#define FWKEYDB_KBAGMAP_TEMPLATE_STR "https://github.com/tihmstar/fwkeydb/releases/download/%s/KBAGMap.zip"
#define FWKEYDB_KBAGMAP_FILE "KBAGMap.json"
#endif

using namespace tihmstar;
using fw_key = tihmstar::libfwkeyfetch::fw_key;
using pwnBundle = tihmstar::libfwkeyfetch::pwnBundle;

#pragma mark private
static size_t downloadFunction(void* buf, size_t size, size_t nmemb, std::string &data) {
    data.append((char*)buf, size*nmemb);
    return nmemb*size;
}

static inline void hexToInts(const char* hex, unsigned int** buffer, size_t* bytes) {
    *bytes = strlen(hex) / 2;
    *buffer = (unsigned int*) malloc((*bytes) * sizeof(int));
    size_t i;
    for(i = 0; i < *bytes; i++) {
        sscanf(hex, "%2x", &((*buffer)[i]));
        hex += 2;
    }
}

static std::string getRemoteFile(std::string url){
    CURL *mc = NULL;
    cleanup([&]{
        safeFreeCustom(mc, curl_easy_cleanup);
    });
    std::string buf;
    assure(mc = curl_easy_init());
    
    curl_easy_setopt(mc, CURLOPT_URL, url.c_str());
    curl_easy_setopt(mc, CURLOPT_USERAGENT, "libfwkeyfetch/" VERSION_COMMIT_COUNT " APIKEY=" VERSION_COMMIT_SHA);
    curl_easy_setopt(mc, CURLOPT_CONNECTTIMEOUT, 30);
    curl_easy_setopt(mc, CURLOPT_FOLLOWLOCATION, 1);
    
    curl_easy_setopt(mc, CURLOPT_WRITEFUNCTION, &downloadFunction);
    curl_easy_setopt(mc, CURLOPT_WRITEDATA, &buf);
    
    assure(curl_easy_perform(mc) == CURLE_OK);
    {
        long http_code = 0;
        curl_easy_getinfo (mc, CURLINFO_RESPONSE_CODE, &http_code);
        assure(http_code == 200);
    }
    return buf;
}

static std::string getFirmwareJson(std::string device, std::string buildnum, uint32_t cpid){
    std::string cpid_str;
    if (cpid) {
        char buf[0x100] = {};
        snprintf(buf, sizeof(buf), "0x%x/",cpid);
        cpid_str = buf;
    }
    
    {
        //try localhost
        std::string url("http://localhost:8888/firmware/");
        url += device + "/";
        if (cpid_str.size()) {
            try {return getRemoteFile(url + cpid_str + buildnum);} catch (...) {}
        }
        try {return getRemoteFile(url + buildnum);} catch (...) {}
    }

#ifdef FIRMWARE_JSON_URL_START
    {
        //retrying with api server
        std::string url(FIRMWARE_JSON_URL_START);
        url += device + "/";
                
        if (cpid_str.size()) {
            try {return getRemoteFile(url + cpid_str + buildnum);} catch (...) {}
        }
        try {return getRemoteFile(url + buildnum);} catch (...) {}
    }
#endif
    
    reterror("failed to get FirmwareJson from Server");
}

static std::string getDeviceJson(std::string device){
    try {
        std::string url("localhost:8888/device/");
        url += device;
        return getRemoteFile(url);
    } catch (...) {
        //retrying with local server
    }
    
#ifdef DEVICE_JSON_URL_START
    try {
        std::string url(DEVICE_JSON_URL_START);
        url += device;
        return getRemoteFile(url);
    } catch (...) {
        //fall though and fail
    }
#endif
    
    reterror("failed to get DeviceJson from Server");
}

#ifdef HAVE_LIBFRAGMENTZIP
static std::string getFirmwareJsonFromZip(std::string device, std::string buildnum, std::string zipURL, uint32_t cpid){
    fragmentzip_t *fz = NULL;
    char *outBuf = NULL;
    cleanup([&]{
        safeFree(outBuf);
        safeFreeCustom(fz, fragmentzip_close);
    });
    size_t outBufSize = 0;
    
    std::string cpid_str;
    if (cpid) {
        char buf[0x100] = {};
        snprintf(buf, sizeof(buf), "0x%x/",cpid);
        cpid_str = buf;
    }

    retassure(fz = fragmentzip_open(zipURL.c_str()), "Failed to open zipURL '%s'",zipURL.c_str());
        
    {
        int err = 0;
        std::string filePath = "firmware/" + device + "/";
        if ((err = fragmentzip_download_to_memory(fz, (filePath + cpid_str + buildnum).c_str(), &outBuf, &outBufSize, NULL))) {
            err = fragmentzip_download_to_memory(fz, (filePath + buildnum).c_str(), &outBuf, &outBufSize, NULL);
        }
        retassure(!err, "Failed to get firmware json from zip with err=%d",err);
    }
    
    return {outBuf,outBuf+outBufSize};
}

static std::string getDeviceJsonFromZip(std::string device, std::string zipURL){
    fragmentzip_t *fz = NULL;
    char *outBuf = NULL;
    cleanup([&]{
        safeFree(outBuf);
        safeFreeCustom(fz, fragmentzip_close);
    });
    size_t outBufSize = 0;
    
    retassure(fz = fragmentzip_open(zipURL.c_str()), "Failed to open zipURL '%s'",zipURL.c_str());
    
    {
        int err = 0;
        std::string filePath = "firmware/" + device;
        retassure(!(err = fragmentzip_download_to_memory(fz, filePath.c_str(), &outBuf, &outBufSize, NULL)), "Failed to get firmware json from zip with err=%d",err);
    }

    return {outBuf,outBuf+outBufSize};
}

#endif //HAVE_LIBFRAGMENTZIP

static fw_key getFirmwareKeyForComparator(std::string device, std::string buildnum, std::function<bool(const jssytok_t *e, std::string apiversion)> comparator, uint32_t cpid, std::string zipURL){
    jssytok_t* tokens = NULL;
    unsigned int * tkey = NULL;
    unsigned int * tiv = NULL;
    cleanup([&]{
        safeFree(tiv);
        safeFree(tkey);
        safeFree(tokens);
    });
    fw_key rt = {0};
    long tokensCnt = 0;
    std::string apiversion;

#ifdef HAVE_LIBFRAGMENTZIP
    std::string json = (zipURL.size()) ? getFirmwareJsonFromZip(device, buildnum, zipURL, cpid) : getFirmwareJson(device, buildnum, cpid);
#else
    std::string json = getFirmwareJson(device, buildnum, cpid);
#endif //HAVE_LIBFRAGMENTZIP
    
    retassure((tokensCnt = jssy_parse(json.c_str(), json.size(), NULL, 0)) > 0, "failed to parse json");
    assure(tokens = (jssytok_t*)malloc(sizeof(jssytok_t)*tokensCnt));
    assure(jssy_parse(json.c_str(), json.size(), tokens, tokensCnt * sizeof(jssytok_t)) == tokensCnt);
    
    jssytok_t *keys = jssy_dictGetValueForKey(tokens, "keys");
    assure(keys);
    
    {
        jssytok_t *apivers = jssy_dictGetValueForKey(tokens, "version");
        if (apivers){
            retassure(apivers->type == JSSY_STRING, "Got version, but not string!");
            apiversion = {apivers->value,apivers->value+apivers->size};
        }
    }
    
    jssytok_t *iv = NULL;
    jssytok_t *key = NULL;
    jssytok_t *path = NULL;

    jssytok_t *tmp = keys->subval;
    for (size_t i=0; i<keys->size; tmp=tmp->next, i++) {
        jssytok_t *tmpV = (tmp->type == JSSY_DICT_KEY) ? tmp->subval : tmp;
        if (comparator(tmpV, apiversion)){
            iv = jssy_dictGetValueForKey(tmpV, "iv");
            key = jssy_dictGetValueForKey(tmpV, "key");
            path = jssy_dictGetValueForKey(tmpV, "filename");
            break;
        }
    }
    assure(iv && key);
    
    assure(iv->size <= sizeof(rt.iv));
    assure(key->size <= sizeof(rt.key));
    memcpy(rt.iv, iv->value, iv->size);
    memcpy(rt.key, key->value, key->size);
    rt.iv[sizeof(rt.iv)-1] = 0;
    rt.key[sizeof(rt.key)-1] = 0;

    if (path) rt.pathname = {path->value,path->value+path->size};
    
    {
        size_t bytes = 0;
        hexToInts(rt.iv, &tiv, &bytes);
        retassure(bytes == 16 || bytes == 0, "IV has bad length. Expected=16 actual=%lld. Got IV=%s",bytes,rt.iv);
        if (!bytes) memset(rt.iv, 0, sizeof(rt.iv)); //indicate no key required
        hexToInts(rt.key, &tkey, &bytes);
        retassure(bytes == 32  || bytes == 16 || bytes == 0, "KEY has bad length. Expected 32 or 16 actual=%lld. Got KEY=%s",bytes,rt.key);
        if (!bytes) memset(rt.key, 0, sizeof(rt.key)); //indicate no key required
    }
    return rt;
}

static std::string getLatestfwkeydbRelease(){
    static std::string ret;
    if (!ret.size()){
#ifndef FWKEYDB_LATEST_RELESES
    reterror("Compiled without getLatestfwkeydbRelease support");
#endif
        jssytok_t* tokens = NULL;
        cleanup([&]{
            safeFree(tokens);
        });
        long tokensCnt = 0;

        auto json = getRemoteFile(FWKEYDB_LATEST_RELESES);

        retassure((tokensCnt = jssy_parse(json.c_str(), json.size(), NULL, 0)) > 0, "failed to parse json");
        assure(tokens = (jssytok_t*)malloc(sizeof(jssytok_t)*tokensCnt));
        assure(jssy_parse(json.c_str(), json.size(), tokens, tokensCnt * sizeof(jssytok_t)) == tokensCnt);
        
        jssytok_t *tagname = jssy_dictGetValueForKey(tokens, "tag_name");
        assure(tagname);
        assure(tagname->type == JSSY_STRING && tagname->size > 0);
        ret = {tagname->value, tagname->value+tagname->size};
    }
        
    return ret;
}

#pragma mark public
LIBFWKEYFETCH_API fw_key libfwkeyfetch::getFirmwareKeyForKBAG(std::string kbag){
#ifndef HAVE_LIBFRAGMENTZIP
    reterror("compiled without libfragmentzip");
#else
    int err = 0;
    fragmentzip_t *fz = NULL;
    char *outBuf = NULL;
    jssytok_t* tokens = NULL;
    cleanup([&]{
        safeFree(tokens);
        safeFree(outBuf);
        safeFreeCustom(fz, fragmentzip_close);
    });
    size_t outBufSize = 0;
    long tokensCnt = 0;
    jssytok_t *tgt = NULL;
    char kbagmapurl[sizeof(FWKEYDB_KBAGMAP_TEMPLATE_STR) + 20] = {};
    
    auto lastFWKeyDBRelease = getLatestfwkeydbRelease();
    
    retassure(snprintf(kbagmapurl, sizeof(kbagmapurl), FWKEYDB_KBAGMAP_TEMPLATE_STR,lastFWKeyDBRelease.c_str()) < sizeof(kbagmapurl), "kbagmapurl buf too small");
    
    retassure(fz = fragmentzip_open(kbagmapurl), "Failed to open kbagmapurl '%s'",kbagmapurl);
    retassure(!(err = fragmentzip_download_to_memory(fz, FWKEYDB_KBAGMAP_FILE, &outBuf, &outBufSize, NULL)), "Failed to get %s from zip with err=%d",FWKEYDB_KBAGMAP_FILE,err);

    retassure((tokensCnt = jssy_parse(outBuf, outBufSize, NULL, 0)) > 0, "failed to parse json");
    assure(tokens = (jssytok_t*)malloc(sizeof(jssytok_t)*tokensCnt));
    assure(jssy_parse(outBuf, outBufSize, tokens, tokensCnt * sizeof(jssytok_t)) == tokensCnt);

    retassure(tgt = jssy_dictGetValueForKey(tokens, kbag.c_str()), "Failed to find IV/Key for KBAG '%s'",kbag.c_str());
    
    {
        fw_key rt = {};
        jssytok_t *iv = NULL;
        jssytok_t *key = NULL;

        iv = jssy_dictGetValueForKey(tgt, "iv");
        key = jssy_dictGetValueForKey(tgt, "key");

        assure(iv && key);
        
        assure(iv->size <= sizeof(rt.iv));
        assure(key->size <= sizeof(rt.key));
        memcpy(rt.iv, iv->value, iv->size);
        memcpy(rt.key, key->value, key->size);
        rt.iv[sizeof(rt.iv)-1] = 0;
        rt.key[sizeof(rt.key)-1] = 0;
        
        return rt;
    }
#endif
}

LIBFWKEYFETCH_API fw_key libfwkeyfetch::getFirmwareKeyForComponent(std::string device, std::string buildnum, std::string component, uint32_t cpid, std::string zipURL){
    if (component == "RestoreLogo")
        component = "AppleLogo";
    else if (component == "RestoreRamDisk")
        component = "RestoreRamdisk";
    else if (component == "RestoreDeviceTree")
        component = "DeviceTree";
    else if (component == "RestoreKernelCache")
        component = "Kernelcache";
    
    return getFirmwareKeyForComparator(device, buildnum, [&component](const jssytok_t *e, std::string apiversion)->bool{
        if (apiversion == "1.0") {
            jssytok_t *names = jssy_dictGetValueForKey(e, "names");
            jssytok_t *name = names->subval;
            for (size_t i=0; i<names->size; name=name->next, i++) {
                if (strncmp(component.c_str(), name->value, name->size) == 0) {
                    return true;
                }
            }
            return false;
        }else{
            jssytok_t *image = jssy_dictGetValueForKey(e, "image");
            assure(image);
            return strncmp(component.c_str(), image->value, image->size) == 0;
        }
    }, cpid, zipURL);
}

LIBFWKEYFETCH_API fw_key libfwkeyfetch::getFirmwareKeyForPath(std::string device, std::string buildnum, std::string path, uint32_t cpid, std::string zipURL){
    return getFirmwareKeyForComparator(device, buildnum, [&path](const jssytok_t *e, std::string apiversion){
        jssytok_t *filename = jssy_dictGetValueForKey(e, "filename");
        assure(filename);
        return strncmp(path.c_str(), filename->value, filename->size) == 0;
    }, cpid, zipURL);
}

LIBFWKEYFETCH_API pwnBundle libfwkeyfetch::getPwnBundleForDevice(std::string device, std::string buildnum, uint32_t cpid, std::string zipURL){
    jssytok_t* tokens = NULL;
    cleanup([&]{
        safeFree(tokens);
    });
    long tokensCnt = 0;

    auto getKeys = [cpid,zipURL](std::string device, std::string curbuildnum)->pwnBundle{
        pwnBundle rt;
        rt.iBSSKey = getFirmwareKeyForComponent(device, curbuildnum, "iBSS", cpid, zipURL);
        rt.iBECKey = getFirmwareKeyForComponent(device, curbuildnum, "iBEC", cpid, zipURL);
        return rt;
    };
    
    if (buildnum.size()) { //if buildnum is given, try to get keys once. error is fatal.
        return getKeys(device,buildnum);
    }
    
#ifdef HAVE_LIBFRAGMENTZIP
    std::string json = (zipURL.size()) ? getDeviceJsonFromZip(device, zipURL) : getDeviceJson(device);
#else
    std::string json = getDeviceJson(device);
#endif //HAVE_LIBFRAGMENTZIP

    assure((tokensCnt = jssy_parse(json.c_str(), json.size(), NULL, 0)) > 0);
    assure(tokens = (jssytok_t*)malloc(sizeof(jssytok_t)*tokensCnt));
    assure(jssy_parse(json.c_str(), json.size(), tokens, tokensCnt * sizeof(jssytok_t)) == tokensCnt);
    
    assure(tokens->type == JSSY_ARRAY);

    jssytok_t *tmp = tokens->subval;
    for (size_t i=0; i<tokens->size; tmp=tmp->next, i++) {
        jssytok_t *deviceName = jssy_dictGetValueForKey(tmp, "identifier");
        assure(deviceName && deviceName->type == JSSY_STRING);
        if (strncmp(deviceName->value, device.c_str(), deviceName->size))
            continue;
        jssytok_t *buildID = jssy_dictGetValueForKey(tmp, "buildid");
        
        std::string curbuildnum = std::string(buildID->value,buildID->size);
        
        //if we are interested in *any* bundle, ignore errors until we run out of builds.
        try {
            return getKeys(device,curbuildnum);
        } catch (...) {
            continue;
        }
    }
    
    reterror("Failed to create pwnBundle for device=%s buildnum=%s",device.c_str(),buildnum.size() ? buildnum.c_str() : "any");
    return {};
}


LIBFWKEYFETCH_API const char *libfwkeyfetch::version(){
    return VERSION_STRING;
}
