#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUF_SIZE 100
#define MAX_CLNT 256

void * handle_clnt(void * arg);
void send_msg(char * msg, int len);
void error_handling(char * msg);

int clnt_cnt=0; // 서버에 접속한 클라이언트의 수
int clnt_socks[MAX_CLNT]; // 클라이언트와의 송수인을 위해 생성한 소켓의 파일 디스크립터를 저장한 배열
pthread_mutex_t mutx; // 뮤텍스를 통한 쓰레드 동기화를 위한 변수

int main(int argc, char *argv[])
{
	int serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;
	int clnt_adr_sz;
	pthread_t t_id;

	if(argc!=2) { // 실행파일 경로/Port 번호를 입력으로 받음 
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	pthread_mutex_init(&mutx, NULL); // 뮤텍스 생성
	serv_sock=socket(PF_INET, SOCK_STREAM, 0); // TCP 소켓을 생성

	// 서버 주소 정보를 초기화
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family=AF_INET;
	serv_adr.sin_addr.s_addr=htonl(INADDR_ANY);
	serv_adr.sin_port=htons(atoi(argv[1]));

	if(bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr))==-1)
		error_handling("bind() error");
	if(listen(serv_sock, 5)==-1)
		error_handling("listen() error");
	
	// 쓰레드 함수 호출이 완료되면 자동으로 쓰레드가 종료될 수 있도록 pthread_detach 함수를 호출.
	while(1)
	{
		clnt_adr_sz+sizeof(clnt_adr);
		// 클라이언트 연결을 수락하고 클라이언트와 송수인을 위한 새로운 소캣을 생성
		clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);

		pthread_mutex_lock(&mutx); // 뮤택스 Lock
		clnt_socks[clnt_cnt++]=clnt_sock; // 클라이언트 수와 파일 디스크립터 등록
		pthread_mutex_unlock(&mutx); // 뮤택스 UnLock

		pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock); // 쓰레드 생성 및 실행
		pthread_detach(t_id); // 쓰레드가 종료되면 소멸시킴
		printf("Connected client IP : %s\n", inet_ntoa(clnt_adr.sin_addr)); // 클라이언트 IP정보를 문자열로 변환하여 출력
	}
	close(serv_sock);
	return 0;
}

void * handle_clnt(void * arg)
{
	int clnt_sock=*((int*)arg); // 클라이언트와의 연결을 위해 생성된 소켓의 파일 디스크립터
	int str_len=0, i;
	char msg[BUF_SIZE];

	while((str_len=read(clnt_sock, msg, sizeof(msg)))!=0) // 클라이언트로부터 EOF를 수신할때까지 읽음
		send_msg(msg, str_len); // send_msg 함수 호출

	pthread_mutex_lock(&mutx); // 뮤텍스 Lock
	for(i=0; i<clnt_cnt; i++) // remove disconnected client
	{
		if(clnt_sock==clnt_socks[i])
		{
			while(i++<clnt_cnt-1)
				clnt_socks[i]=clnt_socks[i+1]; // 현재 해당하는 파일 디스크립터를 찾으면 클라이언트가 연결요청을 했으므로 해당정보를 덮어씌우고 삭제함.
			break;
		}
	}
	clnt_cnt--; // 클라이언트 수 감소
	pthread_mutex_unlock(&mutx); // 뮤택스UnLock
	close(clnt_sock); // 클라이언트와의 송수신을 위한 생성 소켓을 종료함.
	return NULL;
}

void send_msg(char * msg, int len) // send to all
{
	int i; 
	pthread_mutex_lock(&mutx); // 뮤텍스 Lock
	for(i=0; i<clnt_cnt; i++) // 현재 연결된 모든 클라이언트에게 메세지를 전송함.
		write(clnt_socks[i], msg, len);
	pthread_mutex_unlock(&mutx); // 뮤텍스 UnLock
}

void error_handling(char * msg) 
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
