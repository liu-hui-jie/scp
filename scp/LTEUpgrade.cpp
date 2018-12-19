#include "libssh2_config.h"
#include "libssh2.h"
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <windows.h>
#include <wlanapi.h>
#include <errno.h>
#include <stdio.h>
#include <atlconv.h>
#include <string>

#pragma warning(disable:4996)
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "wlanapi.lib")

//IP和文件常量
const char *g_HostNameARM = "192.168.43.116";
const char *g_HostNameX86Wlan = "192.168.43.115";
const char *g_HostNameX86Eth = "192.168.100.71";
const char *g_UpgradeFileNameARM = "LTEUpdate_ARM.zip";
const char *g_UpgradeFileNameX86 = "LTEUpdate_x86.zip";
const char *g_WorkDirARM = "/home/root/wupeng/LTELocation";
const char *g_WorkDirX86 = "/root/LTELocation";
using namespace std;

void FireWallIO(bool off)
{

    if(off) {
		printf("正在关闭防火墙...\n");
        system("netsh advfirewall set currentprofile state off");
    } else {
		printf("正在打开防火墙...\n");
        system("netsh advfirewall set currentprofile state on");
    }
}

bool ConnectOK(const char*ip)
{
	char Line[1024] = {0};
    sprintf(Line, "ping -w 2 -l 1 %s > temp.txt", ip);
    system(Line);
    bool ret = false;
    FILE *fr;
    fr = fopen("temp.txt", "r");
    while(fgets(Line, 1024, fr) != NULL) {
        if(strstr(Line, "ms")) {
            ret = true;
            break;
        }
    }
    fclose(fr);
    system("del temp.txt");
    return ret;
}

//检测WiFi连接并获取名称；
//int  GetWifiName(char *SSID)
//{
//    int id = 0;
//    HANDLE ClientHandle;
//    DWORD nv, i, c;
//    PWLAN_INTERFACE_INFO_LIST ilist;
//    PWLAN_AVAILABLE_NETWORK_LIST nlist;
//
//    if(WlanOpenHandle(1, 0, &nv, &ClientHandle) == 0) {
//        if(WlanEnumInterfaces(ClientHandle, 0, &ilist) == 0) {
//            for(i = 0; i < ilist->dwNumberOfItems; i++) {
//                if(WlanGetAvailableNetworkList(ClientHandle, &ilist->InterfaceInfo[i].InterfaceGuid, 0, 0, &nlist) == 0) {
//                    for(c = 0; c < nlist->dwNumberOfItems; c++) {
//                        if(nlist->Network[c].dwFlags  & WLAN_AVAILABLE_NETWORK_CONNECTED) {
//                            memcpy(SSID, nlist->Network[c].dot11Ssid.ucSSID, nlist->Network[c].dot11Ssid.uSSIDLength);
//                            SSID[nlist->Network[c].dot11Ssid.uSSIDLength] = 0;
//                        }
//                    }
//                    WlanFreeMemory(nlist);
//                }
//            }
//            WlanFreeMemory(ilist);
//        }
//        WlanCloseHandle(ClientHandle, 0);
//    }
//    return 0;
//}

int waitsocket(int socket_fd, LIBSSH2_SESSION *session)
{
    struct timeval timeout;
    int rc;
    fd_set fd;
    fd_set *writefd = NULL;
    fd_set *readfd = NULL;
    int dir;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    FD_ZERO(&fd);
    FD_SET(socket_fd, &fd);
    dir = libssh2_session_block_directions(session);
    if(dir & LIBSSH2_SESSION_BLOCK_INBOUND) {
        readfd = &fd;
    }
    if(dir & LIBSSH2_SESSION_BLOCK_OUTBOUND) {
        writefd = &fd;
    }
    rc = select(socket_fd + 1, readfd, writefd, NULL, &timeout);
    return rc;
}

void ShowError(LIBSSH2_SESSION *session)
{
    char *errMsg;
    int errLen;
    int err = libssh2_session_last_error(session, &errMsg, &errLen, 0);
    printf("Error(%d): %s\n", err, errMsg);
}

int GetParm(char *hostname, char *filename, char *workdir)
{
	printf("正在检查网络链路...\n");
    if(ConnectOK(g_HostNameARM)) {
        strcpy(hostname, g_HostNameARM);
        strcpy(filename, g_UpgradeFileNameARM);
        strcpy(workdir, g_WorkDirARM);
        return 1;
    }
    int ret = 0;
    if(ConnectOK(g_HostNameX86Eth)) {
        ret = 3;
        strcpy(hostname, g_HostNameX86Eth);
    } else if(ConnectOK(g_HostNameX86Wlan)) {
        ret = 2;
        strcpy(hostname, g_HostNameX86Wlan);
    }
    if(ret) {
        strcpy(filename, g_UpgradeFileNameX86);
        strcpy(workdir, g_WorkDirX86);
    }
    return ret;
}

int main()
{
    int bytecount = 0;
    char hostname[64] = {0};
    char filename[64] = {0};
    char workdir[64] = {0};
    char commandline[2048] = {0};
    const char *username = "root";
    const char *password = "linux";
    int opt = 1;
    FireWallIO(true);
    int iprev = GetParm(hostname, filename, workdir);
    if(iprev == 0) {
		printf("网络链路异常,按任意键退出...\n");
		getchar();
        return 1;
    }
    if(iprev != 3) {
    }
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel;
    WSADATA wsadata;
    int sock, status;
    int rc = WSAStartup(MAKEWORD(2, 0), &wsadata);
    if(rc) {
        printf("WSAStartup failed with error(%d)\n", rc);
        return 1;
    }
    FILE *local = fopen(filename, "rb");
    if(!local) {
        printf("%s 不在当前目录或损坏\n", filename);
        goto quit;
    }
    libssh2_init(0);
    sock = socket(AF_INET, SOCK_STREAM, 0);

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(22);
    sin.sin_addr.s_addr = inet_addr(hostname);
    if(connect(sock, (struct sockaddr *)(&sin), sizeof(sin))) {
        printf("无法连接设备，请保证设备与电脑连接在同一局域网下，且设备已连接\n");
        goto quit;
    }

    printf("Upload file Start...\n");
    sprintf(commandline, "echo n | pscp.exe -q -P 22 -pw %s authorized_keys %s root@%s:/ 2> report", password, filename, hostname);
    status = system(commandline);
    if(status != 3 && status) {
        printf("\n上传文件失败，请反馈report文件\n\n");
        goto quit;
    }

    system("del report");
    printf("\nUpload file %s OK\n",filename);
    sprintf(commandline, "mv /%s %s/ && unzip -o -q %s/%s -d %s && cd %s && cp -r LTEUpdate*/LTEv*/* . && rm -rf LTEUpdate*  && chmod +x update.sh && ./update.sh 2>/dev/null", filename, workdir, workdir, filename, workdir, workdir);
    //printf("commandline: %s\n", commandline);
    session = libssh2_session_init();
    libssh2_session_set_blocking(session, 0);
    while(libssh2_session_handshake(session, sock) == LIBSSH2_ERROR_EAGAIN);
    libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA1);
    while((rc = libssh2_userauth_publickey_fromfile(session, username, "id_rsa.pub", "id_rsa", password)) == LIBSSH2_ERROR_EAGAIN);
    if(rc) {
        ShowError(session);
        goto shutdown;
    }
    while((channel = libssh2_channel_open_session(session)) == NULL && libssh2_session_last_error(session, NULL, NULL, 0) == LIBSSH2_ERROR_EAGAIN) {
        waitsocket(sock, session);
    }
    while((rc = libssh2_channel_exec(channel, commandline)) == LIBSSH2_ERROR_EAGAIN) {
        waitsocket(sock, session);
    }
    while(1) {
        do {
            char buffer[0x4000];
            rc = libssh2_channel_read(channel, buffer, sizeof(buffer));
            if(rc > 0) {
                bytecount += rc;
            }
        } while(rc > 0);
        if(rc == LIBSSH2_ERROR_EAGAIN) {
            waitsocket(sock, session);
        } else {
            break;
        }
    }
    while((rc = libssh2_channel_close(channel)) == LIBSSH2_ERROR_EAGAIN) {
        waitsocket(sock, session);
    }
    libssh2_channel_free(channel);
    printf("Upgrade OK\n");
shutdown:
    libssh2_session_disconnect(session, "Normal Shutdown");
    libssh2_session_free(session);
quit:
    if(sock) {
        closesocket(sock);
    }
    if(local) {
        fclose(local);
    }
    libssh2_exit();

    printf("按任意键退出...\n");
    getchar();
    return 0;
}
