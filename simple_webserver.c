//2015041050 허준수 임베디드 시스템 과제

#include<stdio.h> //입출력을 위한 라이브러리 선언
#include<stdlib.h>      //atoi, exit을 위한 라이브러리 선언
#include<unistd.h>      //서버 소켓을 닫기 위한 라이브러리 선언
#include<string.h>      //문자열 처리를 위한 라이브러리 선언
#include<arpa/inet.h>   //ip주소 처리를 위한 라이브러리 선언
#include<sys/socket.h>  //소켓 처리를 위한 라이브러리 선언
#include<pthread.h>     //쓰레드 처리를 위한 라이브러리 선언
#include<fcntl.h>       //O_RDONLY를 사용하기 위한 헤더

#define BUFSIZE 1024     //메세지 최대 길이 1024byte로 제한

typedef struct Client{          //클라이언트의 정보를 저장해놓을 구조체
        int sock;               //소켓 번호
        char ip[16];            //ip 주소
}Client;

void *thread_func(void *arg);   //클라이언트별 생성한 쓰레드를 서버에서 처리할 함수 정의
void request_handling(char buf[BUFSIZE], Client *client_info);        //클라이언트의 요청을 
int calc_cgi(char *buf);        //cgi 계산후 리턴해주는 함수 정의
void make_log(char *addr, char *file_name, int file_size);      //로그 생성 함수 정의
void error_handling(char *message);  //에러 처러 함수 정의

pthread_mutex_t m_lock;
char path[256] = "";            //프로그램 실행시 지정한 경로를 저장해놓는 변수

int main(int argc, char **argv)  //메인함수 시작
{
        int nSockOpt = 1;       //소켓 옵션을 변경하기 위한 변수
        int server_sock;  //서버소켓과 클라이언트 소켓 변수
        struct sockaddr_in server_addr, client_addr;  //서버와 클라이언트 주소 변수
        int client_addr_size;  //클라이언트의 주소 크기 변수
        pthread_t client_id;  //쓰레드로 처리할 클라이언트 변수
        int thread_return;  //쓰레드 종료후 리턴값을 받기 위한 변수
        char *client_ip;        //클라이언트 ip주소를 담기위한 변수
        int pid;   //스레드 id를 저장하기 위한 변수
        char buf[BUFSIZE];      //요청을 읽기위한 변수
        Client *clinet_info;    //클라이언트가 생성될때마다 저장하기 위한 구조체 포인터
        FILE *fp;               //log.txt를 열기 위한 변수

        if (argc != 3)  //입력을 잘못했을때 사용방법을 알려주고 프로그램을 종료한다.
        {
                printf(" Usage : %s <path> <port>\n", argv[0]);        //실행방법 출력
                exit(1);        //프로그램 종료
        }

        fp = fopen("log.txt", "w");     //log.txt를 w모드로 연다.
        fclose(fp);                     //파일을 바로 닫아서 안에있는 내용을 전부 지운다

        pthread_mutex_init(&m_lock, NULL);      

        strcpy(path, argv[1]);          //프로그램 실행시 지정한 경로를 path에 저장한다
        if(path[strlen(path) - 1] == '/'){      //파일 경로 맨 뒷글자가 /로 끝나면
                path[strlen(path) - 1] = 0;     //없애준다.
        }

        if((server_sock=socket(PF_INET, SOCK_STREAM, 0)) == -1)  //서버 소켓을 생성한 뒤, 생성이 되었는지 판별
                error_handling("socket() error");       //소켓이 생성되지 않는다면 에러를 출력

        memset(&server_addr, 0, sizeof(server_addr));  //서버 주소 변수 모두 0으로 초기화
        server_addr.sin_family=AF_INET;  //IPv4주소체계 사용
        server_addr.sin_addr.s_addr=inet_addr("0.0.0.0");  //IP를 모든 NIC에서 바인딩할 수 있도록 설정
        server_addr.sin_port=htons(atoi(argv[2]));  //포트번호 설정


        setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &nSockOpt, sizeof(nSockOpt));         //서버 소켓을 재사용할수있게 바꿔준다.

        //소켓에 주소를 할당하고 오류가 발생하면 에러를 출력
        if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr))==-1)
                error_handling("bind() error");         //에러 출력 부분
        //서버 소켓에서 클라이언트의 접속 요청을 기다리도록 설정
        if (listen(server_sock, 100)==-1)
                error_handling("listen() error");       //에러 출력 부분

        while(1)  //클라이언트가 접속했을때 처리해주는 반복문
        {
                //클라이언트가 생성될때마다 새로 할당해준다.
                clinet_info = (Client *)malloc(sizeof(Client));

                //클라이언트 주소의 크기를 저장시켜놓는다.
                client_addr_size=sizeof(client_addr);

                //accept함수를 통해 클라이언트의 접속 요청을 받는다.
                clinet_info->sock=accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_size);
                
                //클라이언트의 ip주소를 저장해놓는다
                client_ip = inet_ntoa(client_addr.sin_addr);
                //저장된 클라이언트 ip주소를 구조체 문자형 배열에 옮겨넣는다.
                strncpy(clinet_info->ip, client_ip, strlen(client_ip) + 1);

                //접속에 성공한다면
                if(clinet_info->sock != -1){
                        //서버가 클라이언트를 제어할 쓰레드를 생성한다.
                        pid = pthread_create(&client_id, NULL, thread_func, (void*)clinet_info);

                        if(pid < 0){    //스레드가 생성되지 못했다면
                                close(clinet_info->sock);       //클라이언트 소켓을 닫아주고
                                error_handling("Thread create() error\n");      //스레드 생성 오류라고 에러 출력
                        }
                        pthread_detach(client_id);
                }
                else{           //접속에 실패한다면
                        close(clinet_info->sock);       //클라이언트 소켓을 닫아준다.
                        return 0;
                }
        }

        //프로그램이 종료되면 서버소켓을 닫는다.
        close(server_sock);

        //0값을 반환하며 프로그램 종료
        return 0;
}

void request_handling(char buf[BUFSIZE], Client *client_info){          //요청을 처리하는 함수
        char file_name[256] = "";               //요청한 파일의 이름을 저장하는 변수 (ex. /images/05_01.gif)
        char file_path[256] = "";               //요청한 파일의 경로를 저장하는 변수 (ex. ./html/images/05_01.gif)
        char cgi_len[15];                       //total.cgi의 정수 리턴값을 문자열로 받기위한 변수
        char file_extension[5];                 //파일의 확장자를 저장하기 위한 변수 (ex. .jpg, .gif, .htm)
        
        int filed, size;        //filed : 파일을 열기위한 변수, size : 파일 크기 계산을 위한 변수
        unsigned long long int n = 0;   //total.cgi를 처리하고 받아올 변수
        int i = 0;      //반복문을 위한 변수
        unsigned long int sum = 0;      //총 파일 크기를 계산하기 위한 변수
        
        //buf : GET 파일경로 HTTP/1.1
        buf[1024] = 0;
        for(i = 4; i < 1024; i++){      //버퍼의 4번째 인덱스부터 탐색
                if(buf[i] == ' '){      //공백이 있다면
                      buf[i] = 0;       //NULL값으로 만든다
                      break;            //NULL값으로 만들고 종료하면 buf : GET 파일경로 이렇게 바뀜
                }
        }

        strncpy(file_name, &buf[4], 256);     //파일의 이름을 file_name에 받아놓는다.
        strcpy(file_path, path);        //파일 경로를 file_path에 복사해놓는다. 

        if(strncmp(buf, "GET /", 6) == 0){        //index.html을 처리해야할때
                sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");     //요청에 대한 응답을 보낼 문자열 저장
                write(client_info->sock, buf, strlen(buf));     //응답을 클라이언트 소켓에 보낸다.

                strcat(file_path, "/index.html");               //file_path에 /index.html을 붙여서 파일경로/index.html로 만든다.
                filed = open(file_path, O_RDONLY);              //file_path에 있는 파일을 열어서 파일 디스크립터인 filed에 저장한다.

                while((size = read(filed, buf, BUFSIZE)) > 0){  //filed에 있는 내용을 계속 읽어온다. (size : 읽어온 크기)
                        write(client_info->sock, buf, size);    //읽어오면서 클라이언트 소켓에 읽어온 내용을 전달한다.
                        sum += size;            //파일의 크기를 계속 추가시킨다.
                }

                make_log(client_info->ip, "/index.html", sum);   //로그 작성
                close(filed);   //filed를 닫아준다.
        }


        strcat(file_path, file_name);   //file_path에 file_name을 붙여 파일경로/파일이름 으로 만든다.
        filed = open(file_path, O_RDONLY);      //file_path에 있는 파일을 열어서 파일 디스크립터인 filed에 저장한다.

        strcpy(file_extension, &file_name[strlen(file_name) - 4]);      //파일 이름 뒤의 4개 문자를 추출하여 파일 확장자 이름을 file_extension에 저장한다.

        if(strcmp(file_extension, ".jpg") == 0){          //jpg file일때
                sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: image/jpg\r\n\r\n");     //요청에 대한 응답을 보낼 문자열 저장
                write(client_info->sock, buf, strlen(buf));             //응답을 클라이언트 소켓에 보낸다.
                sum = 0;        //파일의 크기를 0으로 초기화한다.

                while((size = read(filed, buf, BUFSIZE)) > 0){  //filed에 있는 내용을 계속 읽어온다. (size : 읽어온 크기)
                        write(client_info->sock, buf, size);    //읽어오면서 클라이언트 소켓에 읽어온 내용을 전달한다.
                        sum += size;    //파일의 크기를 계속 추가시킨다.
                }
                make_log(client_info->ip, file_name, sum);      //로그 작성
        }
        else if(strcmp(file_extension, ".gif") == 0){     //gif file일때
                sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: image/gif\r\n\r\n");     //요청에 대한 응답을 보낼 문자열 저장   
                write(client_info->sock, buf, strlen(buf));             //응답을 클라이언트 소켓에 보낸다.
                sum = 0;        //파일의 크기를 0으로 초기화한다.

                while((size = read(filed, buf, BUFSIZE)) > 0){  //filed에 있는 내용을 계속 읽어온다. (size : 읽어온 크기)
                        write(client_info->sock, buf, size);    //읽어오면서 클라이언트 소켓에 읽어온 내용을 전달한다.
                        sum += size;    //파일의 크기를 계속 추가시킨다.
                }
                make_log(client_info->ip, file_name, sum);      //로그 작성
        }
        else if(strstr(file_name, "?from=") != NULL){   //cgi request일때
                n = calc_cgi(file_name);        //n에다가 total.cgi 계산한 값을 넣어준다.

                //요청에 대한 응답을 보낼 문자열 저장과 동시에 n의 값을 전달해준다.
                sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<HTML><BODY>%lld</BODY></HTML>\r\n", n);        
                write(client_info->sock, buf, strlen(buf));     //응답을 클라이언트 소켓에 보낸다.

                sprintf(cgi_len, "%lld", n);    //cgi_len에 n의값을 문자열로 저장한다.
                make_log(client_info->ip, file_name, strlen(cgi_len));  //로그 작성
        }
        else if(strcmp(file_extension, ".htm") == 0){     //htm file일때
                sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");     //요청에 대한 응답을 보낼 문자열 저장
                write(client_info->sock, buf, strlen(buf));     //응답을 클라이언트 소켓에 보낸다.
                sum = 0;        //파일의 크기를 0으로 초기화한다.

                while((size = read(filed, buf, BUFSIZE)) > 0){  //filed에 있는 내용을 계속 읽어온다. (size : 읽어온 크기)
                        write(client_info->sock, buf, size);    //읽어오면서 클라이언트 소켓에 읽어온 내용을 전달한다.
                        sum += size;    //파일의 크기를 계속 추가시킨다.
                }
                make_log(client_info->ip, file_name, sum);      //로그 작성
        }
        else{           //Not Found일때
                sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<HTML><BODY>NOT FOUND</BODY></HTML>\r\n");      //요청에 대한 응답을 보낼 문자열 저장
                write(client_info->sock, buf, strlen(buf));     //응답을 클라이언트 소켓에 보낸다.
                make_log(client_info->ip, file_name, 9);        //로그 작성
        }
        close(filed);   //파일 디스크립터를 종료시킨다.
}

//클라이언트별 생성한 쓰레드를 서버에서 처리할 함수
void *thread_func(void *arg)
{
        Client *client_info=(Client*)arg;  //접속한 클라이언트의 소켓 정보를 저장하는 변수
        int str_len=0;  //클라이언트에게 수신된 문자의 길이를 저장하는 변수 (-1이면 수신 실패)
        char buf[BUFSIZE];  //클라이언트의 이름과 메세지를 저장하는 변수
        
        str_len = read(client_info->sock, buf, BUFSIZE);

        if(str_len != -1){  //수신에 성공했다면
                request_handling(buf, client_info); //요청을 처리해주는 함수 실행
        }

        else{  //수신에 실패한다면
                error_handling("recieve() error");      //에러 처리
        }      
        close(client_info->sock);       //스레드가 끝나면 클라이언트 소켓을 닫아준다.
}

int calc_cgi(char *buf){        //total.cgi기능을 처리하기 위한 함수
        char *tok;      //문자열을 토큰 단위로 분리하기 위해 사용하는 변수
        int n,m;        //NNN : n  , MMM : m
        int num;        //NNN부터 MMM까지의 갯수를 저장하기 위한 변수

        unsigned long long int sum = 0;         //총 합을 저장하는 변수

        //토큰 분리하기 위한 변수를 우선 받아온 버퍼만큼 크기를 할당한다.
        tok = (char *)malloc(sizeof(char) * strlen(buf));
        strcpy(tok, buf);       //buf의 내용을 tok에 저장 (/total.cgi?from=NNN&to=MMM)

        tok = strtok(tok, "=");         //tok에 /total.cgi?from 분리
        tok = strtok(NULL, "&");        //tok에 NNN 분리
        n = atoi(tok);  //NNN을 숫자로 변환

        tok = strtok(NULL, "=");        //tok에 &to 분리
        tok = strtok(NULL, "\0");       //tok에 MMM 분리
        m = atoi(tok);                 //MMM을 숫자로 변환

        num = m - n + 1;        //n부터 m까지의 갯수를 저장

        if(num % 2 == 0){       //갯수가 짝수이면
                sum = (n + m) * (num / 2);      //n + m 한 다음 갯수의 절반을 곱하여 합을 구한다.
        }
        else{   //갯수가 홀수이면
                sum = (n + m) * (num / 2) + (n + (num / 2));    //n+m 한 다음 갯수의 절반을 곱하고 가장 가운데있는 수를 한번 더 더해준다.
        }

        return sum;     //합을 리턴한다.
}

void make_log(char *addr, char *file_name, int file_size){      //로그작성을 위한 함수
        FILE *log;      //로그 파일을 열기 위한 파일포인터
        char buf[BUFSIZE] = "";      //로그 파일에 저장을 하기 위한 문자열 변수

        if((log = fopen("log.txt", "a")) != NULL){      //로그 파일을 열고 파일이 열린다면
                sprintf(buf, "%s %s %d\n", addr, file_name, file_size); //버퍼에 로그에 작성할 문자열을 저장한다 (IP 파일이름 파일크기)
                fprintf(log, "%s", buf);        //로그에 작성을 한다.
                fclose(log);    //파일포인터를 닫는다.
        }
        else{
                fclose(log);
                error_handling("log open error");        //파일이 열리지 않는다면 에러처리
        }
        
        
}

//에러 처리를 위한 함수(에러문을 출력한 뒤 프로그램을 종료한다.)
void error_handling(char *message)
{
        fputs(message, stderr);  //에러 메세지를 출력을 해준다.
        fputc('\n', stderr);  //깔끔하게 보이게 하기 위한 줄바꿈
        exit(1);  //프로그램 종료
}