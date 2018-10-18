#include <unistd.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <iostream>
#include <vector>
#include <string.h>
#include <dirent.h>
#include "hiredis.h"
#include "xLock.h"
#include "xRedisClient.h"
#include "xRedisPool.h"
#include <ifaddrs.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <cmath>

static bool bflag = true;

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

void init_daemon(void)
{
    int pid;
    int i;
    if(pid=fork())
        exit(0);//是父进程，结束父进程
    else if(pid< 0)
        exit(1);//fork失败，退???
    //是第一子进程，后台继续执行
    setsid();//第一子进程成为新的会话组长和进程组长
    //并与控制终端分离
    if(pid=fork())
        exit(0);//是第一子进程，结束第一子进???
    else if(pid< 0)
        exit(1);//fork失败，退???
    //是第二子进程，继???
    //第二子进程不再是会话组长
    for(i=0;i< NOFILE;++i)//关闭打开的文件描述符
    {
        if(2 == i) {
            continue;
        }
        close(i);
    }
    chdir("/home");//改变工作目录???tmp
    umask(0);//重设文件创建掩模
    return;
}

void signal_reload(int signal)
{
    /*当收到SIGUSR1信号时程序重新导入所有参???/
    /* FILE *fp;
    time_t t;
    if((fp=fopen("face_daemon.log","a")) >=0)
    {
        t=time(0);
        fprintf(fp,"I received signal(%d), reload all parameters at %s\n", signal, asctime(localtime(&t)) );
        fclose(fp);
    }*/
    /*重新导入参数*/
}

void signal_handle(int signal)
{
    /*当收到SIGUSR2信号时程序退???/
    /* FILE *fp;
    time_t t;
    if((fp=fopen("face_daemon.log","a")) >=0)
    {
        t=time(0);
        fprintf(fp,"I received signal(%d), exit at %s\n", signal, asctime(localtime(&t)) );
        fclose(fp);
    }
    exit(0); */
    fprintf(stderr, "face_daemon received signal(%d)\n", signal);
    bflag = false;
}

int readInfoFromRedis(xRedisClient & xRedis, char* ipAddress, char* value)
{
    int ret = -1;
	std::string key(_REDIS_KEY_INFO);
    key.append(ipAddress);
	
    RedisDBIdx dbi(&xRedis);
    
    if(!dbi.CreateDBIndex(key.c_str(), APHash, CACHE_TYPE_1)) {
        fprintf(stderr, "readInfoFromRedis redis createDBIndex fail\n");
        return ret;
    }
    
    std::string content;
    
    bool bRet = xRedis.lpop(dbi, key, content);
    if(bRet){
        fprintf(stderr, "readInfoFromRedis redis read json success\n");
    } else {
        if(NULL != dbi.GetErrInfo()) {
            fprintf(stderr, "readInfoFromRedis redis read json fail [%s]\n", dbi.GetErrInfo());
        }
        return ret;
    }
    
    if (0 == content.size()) {
        return ret;
    }
	
    strcpy(value, content.c_str());
    
    fprintf(stderr, "readInfoFromRedis value : %s\n", value);

    return 1;
}

int main(int argc, char ** argv)
{
    FILE *fp;
    time_t t;
    init_daemon();//初始化为Daemon
    signal(SIGCHLD, SIG_IGN);
    signal(SIGUSR1, signal_reload);
    signal(SIGUSR2, signal_handle);
	
	char ipAddress[INET_ADDRSTRLEN] = {0};
	getLocalAddress(ipAddress);

	xRedisClient xRedis;
	
	if(-1 == redisClientInit(xRedis))
	{
		fprintf(stderr, "redisClientInit failed\n");
		return 0;
	}

    while(bflag)
    {
        char value[256] = {0};
		
        if(readInfoFromRedis(xRedis, ipAddress, value))
	{
	    //fprintf(stderr, "readInfoFromRedis value : %s\n", value);
	}
        
        usleep(100 * 1000);
    }
    
    return 0;
}




