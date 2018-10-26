#pragma warning(disable: 4996)
#include "libssh2_config.h"
#include "libssh2.h"
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")



static int waitsocket(int socket_fd, LIBSSH2_SESSION *session)
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

	/* now make sure we wait in the correct direction */
	dir = libssh2_session_block_directions(session);

	if (dir & LIBSSH2_SESSION_BLOCK_INBOUND)
		readfd = &fd;

	if (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
		writefd = &fd;

	rc = select(socket_fd + 1, readfd, writefd, NULL, &timeout);

	return rc;
}

int main()
{
	const char *hostname = "192.168.43.115";
	const char *username = "root";
	const char *loclfile = "test.tar.gz";
	const char *scppath = "/root/test.tar.gz";
	const char *commandline = "tar xvf test.tar.gz;chmod 0777 test.sh ;./test.sh";
	char *exitsignal = (char *)"none";
	int bytecount = 0;


	LIBSSH2_SESSION *session = NULL;
	LIBSSH2_CHANNEL *channel;
	unsigned long hostaddr;
	struct sockaddr_in sin;
	struct stat fileinfo;
	
	size_t nread;
	int exitcode;
	int sock;
	int rc;
	
	char mem[1024];
	char *ptr;
	FILE *local;

#ifdef WIN32
	WSADATA wsadata;
	int err;

	err = WSAStartup(MAKEWORD(2, 0), &wsadata);
	if (err != 0) {
		fprintf(stderr, "WSAStartup failed with error: %d\n", err);
		return 1;
	}
#endif


	hostaddr = inet_addr(hostname);


	rc = libssh2_init(0);
	if (rc != 0) {
		fprintf(stderr, "libssh2 initialization failed (%d)\n", rc);
		return 1;
	}

	local = fopen(loclfile, "rb");
	if (!local) {
		fprintf(stderr, "Can't open local file %s\n", loclfile);
		return -1;
	}
	//

	stat(loclfile, &fileinfo);


	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == sock) {
		fprintf(stderr, "failed to create socket!\n");
		return -1;
	}


	unsigned long ul = 1;
	rc = ioctlsocket(sock, FIONBIO, (unsigned long *)&ul);//设置成非阻塞模式。  
	if (rc == SOCKET_ERROR)//设置失败。  

	{
		printf("ublock  set err\n");
		exit(1);
	}
	fd_set rfd;      //描述符集 这个将测试连接是否可用
	struct timeval timeout;  //时间结构体
	FD_ZERO(&rfd);//先清空一个描述符集
	timeout.tv_sec = 10;//秒
	timeout.tv_usec = 0;


	sin.sin_family = AF_INET;
	sin.sin_port = htons(22);
	sin.sin_addr.s_addr = hostaddr;

	rc = connect(sock, (struct sockaddr*)(&sin),
		sizeof(struct sockaddr_in));
	FD_SET(sock, &rfd);
	rc = select(0, 0, &rfd, 0, &timeout);
	if (rc <= 0)
	{

		printf("connect time out..\n");
		return  -1;
	}


	


	/*if (connect(sock, (struct sockaddr*)(&sin),
		sizeof(struct sockaddr_in)) != 0) {
		fprintf(stderr, "failed to connect!\n");
		return -1;
	}*/
//创建 session

	session = libssh2_session_init();
	if (!session)
		return -1;

	//建立联系
	rc = libssh2_session_handshake(session, sock);
	if (rc) {
		fprintf(stderr, "Failure establishing SSH session: %d\n", rc);
		return -1;
	}

	//键入密码 连接
	/*if (libssh2_userauth_keyboard_interactive(session, username, &kbd_callback)) {
		fprintf(stderr, "Authentication by password failed.\n");
		goto shutdown;
	}*/

	//使用public key 连接
	if (libssh2_userauth_publickey_fromfile(session, "root", "id_rsa.pub", "id_rsa", "linux")) {
		printf("tAuthentication by public key failed\n");
		goto shutdown;
	}
	//在远程创建文件
	channel = libssh2_scp_send(session, scppath, fileinfo.st_mode & 0777,
		(unsigned long)fileinfo.st_size);

	if (!channel) {
		char *errmsg;
		int errlen;
		int err = libssh2_session_last_error(session, &errmsg, &errlen, 0);
		fprintf(stderr, "Unable to open a session: (%d) %s\n", err, errmsg);
		goto shutdown;
	}

	//写入文件
	do {
		nread = fread(mem, 1, sizeof(mem), local);
		if (nread <= 0) {
			/* end of file */
			break;
		}
		ptr = mem;

		do {
			/* write the same data over and over, until error or completion */
			rc = libssh2_channel_write(channel, ptr, nread);
			if (rc < 0) {
				fprintf(stderr, "ERROR %d\n", rc);
				break;
			}
			else {
				/* rc indicates how many bytes were written this time */
				ptr += rc;
				nread -= rc;
			}
		} while (nread);

	} while (1);

	//释放句柄
	libssh2_channel_send_eof(channel);
	libssh2_channel_wait_eof(channel);
	libssh2_channel_wait_closed(channel);
	libssh2_channel_free(channel);



	//	执行命令

	//创建新的句柄
	while ((channel = libssh2_channel_open_session(session)) == NULL &&
		libssh2_session_last_error(session, NULL, NULL, 0) ==
		LIBSSH2_ERROR_EAGAIN)
	{
		waitsocket(sock, session);
	}
	if (channel == NULL)
	{
		fprintf(stderr, "Error\n");
		exit(1);
	}
	//执行命令
	while ((rc = libssh2_channel_exec(channel, commandline)) ==
		LIBSSH2_ERROR_EAGAIN)
	{
		waitsocket(sock, session);
	}
	if (rc != 0)
	{
		fprintf(stderr, "Error\n");
		exit(1);
	}
	
	
	
	
	exitcode = 127;
	while ((rc = libssh2_channel_close(channel)) == LIBSSH2_ERROR_EAGAIN)
		waitsocket(sock, session);

	if (rc == 0)
	{
		exitcode = libssh2_channel_get_exit_status(channel);
		libssh2_channel_get_exit_signal(channel, &exitsignal,
			NULL, NULL, NULL, NULL, NULL);
	}

	libssh2_channel_free(channel);
	channel = NULL;

shutdown:
	//释放资源
	if (session) {
		libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing");
		libssh2_session_free(session);
	}
#ifdef WIN32
	closesocket(sock);
#else
	close(sock);
#endif
	if (local)
		fclose(local);
	fprintf(stderr, "all done\n");
	libssh2_exit();

	return 0;
}











