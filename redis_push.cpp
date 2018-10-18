#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include "hiredis.h"
#include "xLock.h"
#include "xRedisClient.h"
#include "xRedisPool.h"
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <cmath>
#include <ctime>

using namespace std;

#define _REDIS_KEY_INFO "jerry:redis:test"

// AP Hash Function
unsigned int APHash(const char *str) {
    unsigned int hash = 0;
    int i;
    for (i=0; *str; i++) {
        if ((i&  1) == 0) {
            hash ^= ((hash << 7) ^ (*str++) ^ (hash >> 3));
        } else {
            hash ^= (~((hash << 11) ^ (*str++) ^ (hash >> 5)));
        }
    }
    return (hash&  0x7FFFFFFF);
}

enum {
 CACHE_TYPE_1, 
 CACHE_TYPE_2,
 CACHE_TYPE_MAX,
};

RedisNode RedisList1[1]=
{
    {0,"127.0.0.1", 6379, "ulucu888", 1, 5, 0}
};

int getLocalAddress(char* pout)
{
    struct ifaddrs * ifAddrStruct=NULL;
    void * tmpAddrPtr=NULL;

    getifaddrs(&ifAddrStruct);

    while (ifAddrStruct!=NULL) {
        if (ifAddrStruct->ifa_addr->sa_family==AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            fprintf(stderr, "%s IP Address %s\n", ifAddrStruct->ifa_name, addressBuffer);
            if(0 == strcmp(ifAddrStruct->ifa_name, "eth0")) {
                strcpy(pout, addressBuffer);
                break;
            } 
            //centos 7+ use enxxxxx
            else if(ifAddrStruct->ifa_name[0]==0x65&&ifAddrStruct->ifa_name[1]==0x6e){
                    strcpy(pout, addressBuffer);
                break;
            }

            
        } else if (ifAddrStruct->ifa_addr->sa_family==AF_INET6) { // check it is IP6
        } 
        ifAddrStruct=ifAddrStruct->ifa_next;
    }
    return 0;
}

int redisClientInit(xRedisClient & xRedis)
{
	int ret = -1;

	if (!xRedis.Init(CACHE_TYPE_MAX)) {
        fprintf(stderr, "redisClientInit redis init fail\n");
        return ret;
    }

	if(!xRedis.ConnectRedisCache(RedisList1, 1, CACHE_TYPE_1)) {
        fprintf(stderr, "redisClientInit redis connect fail\n");
        return ret;
    }

	return 0;
}

void writeInfoToRedis(xRedisClient & xRedis, char* value)
{
    if(NULL == value)
        return ;

    std::vector<std::string> redis_list_json;	
    char ipAddress[INET_ADDRSTRLEN] = {0};
    std::string key(_REDIS_KEY_INFO);
    getLocalAddress(ipAddress);
    key.append(ipAddress);
    
    RedisDBIdx dbi(&xRedis);
    
    if(!dbi.CreateDBIndex(key.c_str(), APHash, CACHE_TYPE_1)) {
        fprintf(stderr, "writeInfoToRedis redis createDBIndex fail");
        return ;
    }

    fprintf(stderr, "face filter create db index\n");    

    redis_list_json.push_back(value);
    int64_t len = 0;
    bool bRet = xRedis.rpush(dbi, key, redis_list_json, len);
    if(bRet){
        fprintf(stderr, "writeInfoToRedis redis set json success length = %ld\n", len);
    } else {
        fprintf(stderr, "writeInfoToRedis redis set json fail [%s]", dbi.GetErrInfo());
    }
	
    redis_list_json.clear();
}

int main(int argc, char **argv) {
    if (argc < 2) {
        return 0;
    }
	
   	xRedisClient xRedis;
	
	if(-1 == redisClientInit(xRedis))
	{
		fprintf(stderr, "redisClientInit failed\n");
		return 0;
	}
    writeInfoToRedis(xRedis, argv[1]);

    return 0;
}




