/*
 * G5 - TCP Transfer && LB Dispenser
 * Author      : calvin
 * Email       : calvinwillliams.c@gmail.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#ifndef _H_G5_
#define _H_G5_

#define VERSION		"1.2.4"

#define SERVICE_NAME	"G5"
#define SERVICE_DESC	"TCP Transfer && Load-Balance Dispenser"

/*
config file format :
rule_id	mode	( rule-properties ) client_addr ( client-properties ) -> forward_addr ( forward-properties ) -> server_addr ( server-properties ) ;
	mode		G  : manage port
			MS : master/slave mode
			RR : round & robin mode
			LC : least connection mode
			RT : response Time mode
			RD : random mode
			HS : hash mode
	rule-properties timeout n , ...
	client_addr	format : ip1.ip2.ip3.ip4:port
			ip1~4,port allow use '*' or '?' for match
			one or more address seprating by blank
	client-properties client_connection_count n , ...
	forward_addr	format : ip1.ip2.ip3.ip4:port
			one or more address seprating by blank
	server_addr	format : ip1.ip2.ip3.ip4:port
			one or more address seprating by blank
	( all seprated by blank charset )
demo :
admin G ( timeout 300 ) 192.168.1.79:* - 192.168.1.54:8060 ;
webdog MS ( timeout 120 ) 192.168.1.54:* 192.168.1.79:* 192.168.1.79:* - 192.168.1.54:8079 > 192.168.1.79:8089 192.168.1.79:8090 ;
hsbl LB 192.168.1.*:* ( client_connection_count 2 ) - 192.168.1.54:8080 > 192.168.1.79:8089 192.168.1.79:8090 192.168.1.79:8091 ;

manage port command :
	ver
	list rules
	add rule ...
	modify rule ...
	remove rule ...
	dump rule
	list forwards
	quit
demo :
	add rule webdog2 MS 1.2.3.4:1234 - 192.168.1.54:1234 > 4.3.2.1:4321 ;
	modify rule webdog2 MS 4.3.2.1:4321 - 192.168.1.54:1234 > 1.2.3.4:1234 ;
	remove rule webdog2 ;
*/

#if ( defined __linux )
#define USE_EPOLL
#elif ( defined _WIN32 )
#define USE_SELECT
#elif ( defined __unix )
#define USE_SELECT
#endif

#if ( defined __linux ) || ( defined __unix )
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <stdarg.h>
#include <limits.h>
#define _VSNPRINTF		vsnprintf
#define _SNPRINTF		snprintf
#define _CLOSESOCKET		close
#define _ERRNO			errno
#define _EWOULDBLOCK		EWOULDBLOCK
#define _ECONNABORTED		ECONNABORTED
#define _EINPROGRESS		EINPROGRESS
#define _ECONNRESET		ECONNRESET
#define _SOCKLEN_T		socklen_t
#define _GETTIMEOFDAY(_tv_)	gettimeofday(&(_tv_),NULL)
#define _LOCALTIME(_tt_,_stime_) \
	localtime_r(&(_tt_),&(_stime_));
#elif ( defined _WIN32 )
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <io.h>
#include <windows.h>
#define _VSNPRINTF		_vsnprintf
#define _SNPRINTF		_snprintf
#define _CLOSESOCKET		closesocket
#define _ERRNO			GetLastError()
#define _EWOULDBLOCK		WSAEWOULDBLOCK
#define _ECONNABORTED		WSAECONNABORTED
#define _EINPROGRESS		WSAEINPROGRESS
#define _ECONNRESET		WSAECONNRESET
#define _SOCKLEN_T		int
#define _GETTIMEOFDAY(_tv_) \
	{ \
		SYSTEMTIME stNow ; \
		GetLocalTime( & stNow ); \
		(_tv_).tv_usec = stNow.wMilliseconds * 1000 ; \
		time( & ((_tv_).tv_sec) ); \
	}
#define _SYSTEMTIME2TIMEVAL_USEC(_syst_,_tv_) \
		(_tv_).tv_usec = (_syst_).wMilliseconds * 1000 ;
#define _SYSTEMTIME2TM(_syst_,_stime_) \
		(_stime_).tm_year = (_syst_).wYear - 1900 ; \
		(_stime_).tm_mon = (_syst_).wMonth - 1 ; \
		(_stime_).tm_mday = (_syst_).wDay ; \
		(_stime_).tm_hour = (_syst_).wHour ; \
		(_stime_).tm_min = (_syst_).wMinute ; \
		(_stime_).tm_sec = (_syst_).wSecond ;
#define _LOCALTIME(_tt_,_stime_) \
	{ \
		SYSTEMTIME	stNow ; \
		GetLocalTime( & stNow ); \
		_SYSTEMTIME2TM( stNow , (_stime_) ); \
	}
#endif

#ifndef ULONG_MAX
#define ULONG_MAX 0xffffffffUL
#endif

#define FOUND				9	/* �ҵ� */ /* found */
#define NOT_FOUND			4	/* �Ҳ��� */ /* not found */

#define MATCH				1	/* ƥ�� */ /* match */
#define NOT_MATCH			-1	/* ��ƥ�� */ /* not match */

#define RULE_ID_MAXLEN			64	/* �ת������������ */ /* The longest length of forwarding rules */
#define RULE_MODE_MAXLEN		2	/* �ת������ģʽ���� */ /* The longest length of forwarding rules mode */

#define FORWARD_RULE_MODE_G		"G"	/* ����˿� */ /* manager port */
#define FORWARD_RULE_MODE_MS		"MS"	/* ����ģʽ */ /* master-standby mode */
#define FORWARD_RULE_MODE_RR		"RR"	/* ��ѯģʽ */ /* polling mode */
#define FORWARD_RULE_MODE_LC		"LC"	/* ��������ģʽ */ /* minimum number of connections mode */
#define FORWARD_RULE_MODE_RT		"RT"	/* ��С��Ӧʱ��ģʽ */ /* minimum response time mode */
#define FORWARD_RULE_MODE_RD		"RD"	/* ���ģʽ */ /* random mode */
#define FORWARD_RULE_MODE_HS		"HS"	/* HASHģʽ */ /* HASH mode */

#define RULE_CLIENT_MAXCOUNT		10	/* �������������ͻ����������� */ /* maximum clients in rule */
#define RULE_FORWARD_MAXCOUNT		3	/* �������������ת������������ */ /* maximum forwards in rule */
#define RULE_SERVER_MAXCOUNT		100	/* ������������������������� */ /* maximum servers in rule */

#define DEFAULT_FORWARD_RULE_MAXCOUNT	100	/* ȱʡ���ת���������� */ /* maximum forward rules for default */
#define DEFAULT_CONNECTION_MAXCOUNT	1024	/* ȱʡ����������� */ /* ���ת���Ự���� = ����������� * 3 */ /* maximum connections for default */
#define DEFAULT_TRANSFER_BUFSIZE	4096	/* ȱʡͨѶת����������С */ /* communication I/O buffer for default */

/* �����ַ��Ϣ�ṹ */ /* network address information structure */
struct NetAddress
{
	char			ip[ 64 + 1 ] ; /* ip��ַ */ /* ip address */
	char			port[ 10 + 1 ] ; /* �˿� */ /* port */
	struct sockaddr_in	sockaddr ; /* sock��ַ�ṹ */ /* sock structure */
} ;

/* �ͻ�����Ϣ�ṹ */ /* client information structure */
struct ClientNetAddress
{
	struct NetAddress	netaddr ; /* �����ַ�ṹ */ /* network address structure */
	int			sock ; /* sock������ */ /* sock fd */
	
	unsigned long		client_connection_count ; /* �ͻ����������� */ /* client number of connections */
	unsigned long		maxclients ; /* ���ͻ������� */ /* amount of clients */
} ;

/* ת������Ϣ�ṹ */ /* forward information structure */
struct ForwardNetAddress
{
	struct NetAddress	netaddr ; /* �����ַ�ṹ */ /* network address structure */
	int			sock ; /* sock������ */ /* sock fd */
} ;

/* �������Ϣ�ṹ */ /* server information structure */
struct ServerNetAddress
{
	struct NetAddress	netaddr ; /* �����ַ�ṹ */ /* network address structure */
	int			sock ; /* sock������ */ /* sock fd */
	
	unsigned long		server_connection_count ; /* ������������� */ /* server connection number */
} ;

#define SERVER_UNABLE_IGNORE_COUNT	100 /* ����˲�����ʱ����ݽ����� */

/* ͳ�ƶ���Ϣ�ṹ */ /* stat information structure */
struct StatNetAddress
{
	struct NetAddress	netaddr ; /* �����ַ�ṹ */ /* network address structure */
	
	unsigned long		connection_count ; /* �������� */ /* number of connections */
} ;

/* ת������ṹ */ /* forwarding rules structure */
struct ForwardRule
{
	char				rule_id[ RULE_ID_MAXLEN + 1 ] ; /* ����ID���ַ����� */ /* rule id */
	char				rule_mode[ RULE_MODE_MAXLEN + 1 ] ; /* �������� */ /* rule mode */
	
	long				timeout ; /* ��ʱʱ�䣨�룩 */
	
	struct ClientNetAddress		client_addr[ RULE_CLIENT_MAXCOUNT ] ; /* �ͻ��˵�ַ�ṹ */ /* client address structure */
	unsigned long			client_count ; /* �ͻ��˹����������� */
	
	struct ForwardNetAddress	forward_addr[ RULE_FORWARD_MAXCOUNT ] ; /* ת���˵�ַ�ṹ */ /* forward information structure */
	unsigned long			forward_count ; /* ת���˹����������� */
	
	struct ServerNetAddress		server_addr[ RULE_SERVER_MAXCOUNT ] ; /* ����˵�ַ�ṹ */ /* server information structure */
	unsigned long			server_count ; /* ����˹����������� */
	unsigned long			select_index ; /* ��ǰ��������� */ /* current server index */
	
	union
	{
		struct
		{
			unsigned long	server_unable ; /* ���񲻿����ݽ����� */
		} RR[ RULE_SERVER_MAXCOUNT ] ;
		struct
		{
			unsigned long	server_unable ; /* ���񲻿����ݽ����� */
		} LC[ RULE_SERVER_MAXCOUNT ] ;
		struct
		{
			unsigned long	server_unable ; /* ���񲻿����ݽ����� */
			struct timeval	tv1 ; /* �����ʱ��� */
			struct timeval	tv2 ; /* ���дʱ��� */
			struct timeval	dtv ; /* �����дʱ����� */
		} RT[ RULE_SERVER_MAXCOUNT ] ;
	} status ;
} ;

#define FORWARD_SESSION_TYPE_UNUSED	0	/* ת���Ựδ�õ�Ԫ */
#define FORWARD_SESSION_TYPE_MANAGE	1	/* �������ӻỰ */
#define FORWARD_SESSION_TYPE_LISTEN	2	/* ��������Ự */
#define FORWARD_SESSION_TYPE_CLIENT	3	/* �ͻ��˻Ự */
#define FORWARD_SESSION_TYPE_SERVER	4	/* ����˻Ự */

#define CONNECT_STATUS_CONNECTING	0	/* �첽���ӷ������ */
#define CONNECT_STATUS_RECEIVING	1	/* �ȴ������� */
#define CONNECT_STATUS_SENDING		2	/* �ȴ������� */
#define CONNECT_STATUS_SUSPENDING	3	/* �ݽ��� */

#define IO_BUFSIZE			4096	/* ͨѶ������������� */

#define TRY_CONNECT_MAXCOUNT		5	/* �첽�������ӷ���������� */

/* �����Ự�ṹ */ /* listen to the session structure  */
struct ListenNetAddress
{
	struct NetAddress	netaddr ; /* �����ַ�ṹ */ /* network address structure */
	int			sock ; /* sock������ */ /* sock fd */
	
	char			rule_mode[ 2 + 1 ] ; /* �������� */ /* rule mode */
} ;

/* ת���Ự�ṹ */ /* forwarding session structure */
struct ForwardSession
{
	char				forward_session_type ; /* ת���Ự���� */
	
	struct ClientNetAddress		client_addr ; /* �ͻ��˵�ַ�ṹ */
	struct ListenNetAddress		listen_addr ; /* �����˵�ַ�ṹ */
	struct ServerNetAddress		server_addr ; /* ����˵�ַ�ṹ */
	unsigned long			client_session_index ; /* �ͻ��˻Ự���� */
	unsigned long			server_session_index ; /* ����˻Ự���� */
	
	struct ForwardRule		*p_forward_rule ; /* ת������ָ�� */
	struct ForwardRule		old_forward_rule ; /* ���߱�������ת������ */
	unsigned long			client_index ; /* �ͻ������� */
	unsigned long			server_index ; /* ��������� */
	
	unsigned char			status ; /* �Ự״̬ */
	unsigned long			try_connect_count ; /* �������ӷ���˴��� */
	
	long				active_timestamp ; /* ����ʱ��� */
	
	char				io_buffer[ IO_BUFSIZE + 1 ] ; /* ������������� */
	long				io_buflen ; /* ������������������ݳ��� */
} ;

/* �����в����ṹ */ /* command line argument structure */
struct CommandParam
{
	char				*config_pathfilename ; /* -f ... */
	
	unsigned long			forward_rule_maxcount ; /* -r ... */
	unsigned long			forward_connection_maxcount ; /* -c ... */
	unsigned long			transfer_bufsize ; /* -b ... */
	
	char				debug_flag ; /* -d */
	
	char				install_service_flag ; /* --install-service */
	char				uninstall_service_flag ; /* --uninstall-service */
	char				service_flag ; /* --service */
} ;

/* �ڲ�����ṹ */ /* internal cache structure */
struct ServerCache
{
	struct timeval			tv ;
	struct tm			stime ;
	char				datetime[ 10 + 1 + 8 + 1 ] ;
} ;

/* ������������ṹ */ /* server environment structure */

#define WAIT_EVENTS_COUNT		1024	/* �ȴ��¼��������� */

struct ServerEnv
{
	struct CommandParam		cmd_para ; /* �����в����ṹ */
	
	struct ForwardRule		*forward_rule ; /* ת������ṹ���ϻ���ַ */
	unsigned long			forward_rule_count ; /* ת������ṹ���� */
	
#ifdef USE_EPOLL
	int				epoll_fds ; /* epoll������ */
	struct epoll_event		*p_event ; /* ��ǰepoll�¼��ṹָ�� */
	struct epoll_event		events[ WAIT_EVENTS_COUNT ] ; /* epoll�¼��ṹ���� */
	int				sock_count ; /* epoll sock���� */
	int				sock_index ; /* ��ǰepoll sock���� */
#endif
	struct ForwardSession		*forward_session ; /* ��ǰת���Ự */
	unsigned long			forward_session_maxcount ; /* ת���Ự������� */
	unsigned long			forward_session_count ; /* ת���Ự���� */
	unsigned long			forward_session_use_offsetpos ; /* ת���Ự�ص�ǰƫ���������ڻ�ȡ���е�Ԫ�ã� */
	
	struct ServerCache		server_cache ; /* ���������� */
	
	unsigned long			maxsessions_per_ip ; /* ÿ���ͻ���ip���Ự���� */
	struct StatNetAddress		*stat_addr ; /* ͳ�Ƶ�ַ�ṹ���ϻ���ַ�����ڿ���ÿ���ͻ���ip���Ự���� */
	unsigned long			stat_addr_maxcount ; /* ͳ�Ƶ�ַ�ṹ���� */
} ;

int G5( struct ServerEnv *pse );

#endif
