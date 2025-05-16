#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// 포트 번호 정의
#define PORT 8080
// window size 와 frame 안에 들어갈 flag, cflac, Address 정의
#define WINDOW_SIZE 8
#define FLAG 0x7E
#define CFLAG 0x7E
#define ADDRESS 'A'
// U-frame 안에 들어가 어떤 동작을 해야하는지 알려주는 control bit 정의
// 중간에 poll bit를 사용하여 주국에서 종국으로 갈 때는 1, 반대인 경우는 0으로 설정
#define SABM 0b11111100
#define DISC 0b11001010
#define UA 0b11000110

// 소켓 통신과 관련된 변수 및 구조체 변수 선언
int sock = 0, valread;
struct sockaddr_in serv_addr;
// frame struct를 담을 buffer 선언
unsigned char buffer[4096] = {0};

// 보내는 data의 번호인 seq_num 설정
int seq_num = (0%WINDOW_SIZE);

// frame 구조체 정의
// type으로 어떤 frame인지 구분하고, control로 어떤 동작을 수행해야 하는지 구분
// 그 외에 frame의 시작과 끝을 구별하는 flag, closingFlag, 보낼 주소를 나타내는 address, message를 담을 data 선언
typedef struct {
    char type; 
    int flag;
    char address;
    unsigned char control;
    char data[1024];
    int closingFlag;
} Frame;

//S-frame은 사용하지 않지만 따로 형식상 구현
typedef struct {
    char type;
    int flag;
    char address;
    unsigned char control;
    int closingFlag;
} S_Frame;

//unsigned char type 변수를 2진수로 출력해주는 함수 구현(control 출력할 때 사용) 
void printBinary(unsigned char control) {
    int i;
    for (i = 7; i >= 0; i--) {
        unsigned char mask = 1 << i;
        unsigned char bit = (control & mask) ? '1' : '0';
        printf("%c", bit);
    }
    printf(".\n");
}

/////////////////////////////////// 아래는 socket 통신을 하기 위한 밑작업 //////////////////////////////////////////

int main(int argc, char const *argv[]) {
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("Invalid address/ Address not supported \n");
        return -1;
    }
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed \n");
        return -1;
    }
    
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    // 연결 여부를 나타내주는 connection 선언 (0이면 연결 안되있음, 1이면 연결됨)
    int connection = 0;
    // 메뉴창을 보고 입력한 번호를 나타내는 select 선언
    int select = 0;

    while (1) {
        // 메뉴창 출력
        printf("\n----------------------------\n");
	    printf("- 1. connect               -\n");
		printf("- 2. chat                  -\n");
		printf("- 3. disconnect            -\n");
		printf("----------------------------\n");
		printf("- What do you want ? : ");
        
        // 메뉴창을 보고 원하는 동작에 맞는 번호 입력
        // 다만, integer type 값 이외에 다른 type 값을 입력한 경우, 경고 문구와 함께 다시 입력하도록 동작
        if (scanf("%d", &select) != 1) {
            printf("Invalid input. Please enter the number in the view.\n");
            // 입력 버퍼 비워줌
            while (getchar() != '\n') {}
            // continue하기 전에 select 0으로 초기화
            select = 0;
            continue;
        }

        // 1번을 입력하지 않아 연결이 되어있지 않은 상태에서 2, 3번을 입력한 경우, 
        // 경고 문구와 함께 다시 입력하도록 동작
        if(connection == 0 && (select == 2 || select == 3)) {
            printf("Not connection.\n");
            //continue하기 전에 select 0으로 초기화
            select = 0;
            continue;
        } 

        // 1번을 입력해 연결이 되어 있음에도, 다시 1번을 입력한 경우,
        // 경고 문구와 함께 다시 입력하도록 동작
        if(connection == 1 && select == 1) {
            printf("Already connection.\n");
            //continue하기 전에 select 0으로 초기화
            select = 0;
            continue;
        } 


        //1을 입력받은 경우 (연결)
        if(select == 1){
            
            // U-frame 생성 후, 연결 요청에 알맞는 값들을 멤버 변수에 넣어줌
            Frame send_frame;
            send_frame.type = 'U';
            send_frame.flag = FLAG;
            send_frame.address = 'B'; 
            // control에 연결 요청을 의미하는 SABM 넣어줌
            send_frame.control = SABM;
            send_frame.closingFlag = CFLAG;
            
            // buffer 값을 0으로 설정 
            memset(buffer, 0, sizeof(buffer));

            // U-frame값 buffer에 복사
            memcpy(buffer, (void *)&send_frame, sizeof(Frame));

            // 연결 요청 문구를 출력하고, receiver에게 U-frame이 담겨있는 buffer 전달
            printf("\nRequest a connection.\n");
            send(sock,buffer,sizeof(buffer),0);

            // buffer 값을 0으로 설정
            memset(buffer, 0, sizeof(buffer));
            
            // receiver로부터 U-frame이 담겨있는 buffer를 받음
            valread = read(sock, buffer, sizeof(buffer));

            // 받은 buffer에 들어있는 U-frame을 넣어줄 received_frame 선언 후, 
            // 받은 buffer를 received_frame에 복사
            Frame received_frame;
            memcpy(&received_frame, buffer, sizeof(Frame));

            // 받은 frame의 종류 출력
            printf("The type of frame is %c-frame.\n", received_frame.type);
            // 받은 flag와 cflag 값 출력
            printf("The values of the received frame's flag and cflag are %X and %X.\n",received_frame.flag,received_frame.closingFlag);
            if ((received_frame.flag == FLAG) && (received_frame.flag == CFLAG)) {
                // 받은 flag와 cflag값을 정의되어 있는 값과 비교 후, 맞으면 성공 문구 출력 
                printf("Flag and CFlag checked successfully.\n");
                // 받은 address 값 출력
                printf("The value of the received frame's address is %c.\n",received_frame.address);
                if (received_frame.address == ADDRESS) {  
                    // 받은 address 값을 정의되어 있는 값과 비교 후, 맞으면 성공 문구 출력                   
                    printf("Address checked successfully.\n");
                    // 받은 control 값을 위에서 구현한 함수를 이용해 이진수로 출력
                    printf("The value of the received frame's control is ");
                    printBinary(received_frame.control);

                    // 받은 control 값을 정의되어 있는 UA 값과 비교 후, 
                    switch (received_frame.control)
                    {
                    // 맞으면 UA 메세지을 받았다는 문구 출력 후, 연결 성공 문구 출력
                    case UA :
                        printf("The UA message has been received.\n");
                        printf("Connection successful.\n");
                        // 연결 여부를 나타내주는 connection의 값을 1 증가
                        connection++;
                        break;
                    // 틀리면 UA 메세지를 받지 못했다는 문구 출력 후, break
                    default:
                        printf("No UA message has been received.\n");
                        break;
                    }
                } else {
                    // 받은 address 값을 정의되어 있는 값과 비교 후, 틀리면 실패 문구 출력
                    printf("Address check failed.\n");
                }
            } else {
                // flag와 cflag값을 정의된 값과 비교 후, 틀리면 실패 문구 출력 
                printf("Flag and CFlag check failed.\n");
            }
        // 2를 입력받는 경우 (채팅)
        // (참고로 저는 stop and wait 방법으로 채팅을 주고 받도록 구현하였습니다.)
        } else if (select == 2) {
            // 타이머 값 저장할 변수 선언
            time_t start, end;
            double result;

            // 입력받은 message가 들어갈 메모리 할당 
            char* message = malloc(1024);

            // 메모리 할당 못받았을 경우, 경고 문구 출력 후 return
            if (message == NULL) {
                printf("Failed to allocate memory\n");
			    return -1;
            }
            
            // 개행 문자를 생략해준 후, (이것을 넣지 않으면 올바르게 문자열을 받지 못했음)
            // 전달하고 싶은 문자열 입력
            scanf("\n");
            fgets(message, 1024, stdin);

            // 현재 I-frame을 이용해서 data를 보내므로, 맨 앞 비트를 1로 설정
            // 지금은 주국에서 종국으로 가는 상황이므로 poll bit를 1로 설정
            // 마지막으로, 보내는 data의 seq num을 왼쪽 000에 넣어주기 위해, 비트 연산자 사용.
            // (지금은 sender와 receiver의 기능이 나누어져 있어 sender가 보내는 control 값의 오른쪽 000에 넣을 ack값은 항상 0)
            unsigned char bits = 0b10001000;
            bits |= (seq_num << 4);

            // I-frame 생성 후, data 전달에 알맞는 값들을 변수에 넣어줌
            Frame send_frame;
            send_frame.type = 'I';
            send_frame.flag = FLAG;
            send_frame.address = 'B'; 
            // control에 위에서 seq num을 넣어준 bits를 넣어줌
            send_frame.control = bits;
            strcpy(send_frame.data,message);
            send_frame.closingFlag = CFLAG;

            // buffer 값을 0으로 설정 
            memset(buffer, 0, sizeof(buffer));

            // I-frame값 buffer에 복사
            memcpy(buffer, (void *)&send_frame, sizeof(Frame));

            // 타이머를 키고,  receiver에게 I-frame이 담겨있는 buffer 전달
            start = time(NULL);
            send(sock,buffer,sizeof(buffer),0);
            
            // buffer 값을 0으로 설정
            memset(buffer, 0, sizeof(buffer));
            
            // receiver로부터 I-frame이 담겨있는 buffer를 받음
            valread = read(sock,buffer,sizeof(buffer));

            // 타이머 종료
            end = time(NULL);

            // 타이머가 종료된 시간에 시작 시간을 빼서 총 데이터를 주고 받는데 걸린 시간 계산
            result = (double)(end - start);

            // response time 출력
            printf("response time: %f sec\n",result);

            //while문의 반복에 사용될 tf 변수 선언
            int tf = 0;
            do {
                //만약 response time이 5초를 넘기지 않았다면 (receiver가 보낸 I-frame이 lost되지 않았다면)
                if (result < 5) {
                    // 받은 buffer에 들어있는 I-frame을 넣어줄 received_frame 선언 후, 
                    // 받은 buffer를 received_frame에 복사
                    Frame received_frame;
                    memcpy(&received_frame, buffer, sizeof(Frame));
                    // 받은 frame의 종류 출력
                    printf("\nThe type of frame is %c-frame.\n", received_frame.type);
                    // 받은 flag와 cflag 값 출력
                    printf("The values of the received frame's flag and cflag are %X and %X.\n",received_frame.flag,received_frame.closingFlag);
                    if ((received_frame.flag == FLAG) && (received_frame.flag == CFLAG)) {
                        // 받은 flag와 cflag값을 정의되어 있는 값과 비교 후, 맞으면 성공 문구 출력 
                        printf("Flag and CFlag checked successfully.\n");
                        // 받은 address 값 출력
                        printf("The value of the received frame's address is %c.\n",received_frame.address);
                        if (received_frame.address == ADDRESS) {             
                            // 받은 address 값을 정의되어 있는 값과 비교 후, 맞으면 성공 문구 출력
                            printf("Address checked successfully.\n");
                            // 받은 control 값에서 비트 연산자를 이용해서 ack 값을 추출 후, 선언한 received_ack에 넣어줌 
                            unsigned char received_ack = received_frame.control ^ 0b10000000;
                            // 그 후, 받은 ack num 출력
                            printf("Received ACK for message %u\n", (unsigned int)received_ack);
                            // result 변수에 0 넣어줘서 원상태로 초기화
                            result = 0;
                            // seq_num에 1 더해줌
                            seq_num++;
                            // tf를 0으로 설정해 반복 멈춤
                            tf = 0;
                        } else {
                            // 받은 address 값을 정의되어 있는 값과 비교 후, 틀리면 실패 문구 출력
                            printf("Address check failed.\n");
                            // tf를 0으로 설정해 반복 멈춤
                            tf = 0;
                        }
                    } else {
                        // flag와 cflag값을 정의된 값과 비교 후, 틀리면 실패 문구 출력
                        printf("Flag and CFlag check failed.\n");
                        // tf를 0으로 설정해 반복 멈춤
                        tf = 0;
                    }     
                //만약 response time이 5초를 넘겼다면 (receiver가 보낸 I-frame이 lost되었다면)                
                } else {
                    // frame을 받지 못하고 time out 되었다는 문구 출력
                    printf("Timed out without receiving the frame.\n");

                    // buffer 값을 0으로 설정 
                    memset(buffer, 0, sizeof(buffer));

                    // 재전송할 I-frame값 buffer에 복사
                    memcpy(buffer, (void *)&send_frame, sizeof(Frame));

                    // 타이머를 키고,  receiver에게 재전송할 I-frame이 담겨있는 buffer 전달
                    start = time(NULL);
                    send(sock,buffer,sizeof(buffer),0);

                    // buffer 값을 0으로 설정
                    memset(buffer, 0, sizeof(buffer));

                    // receiver로부터 I-frame이 담겨있는 buffer를 받음
                    valread = read(sock, buffer, sizeof(buffer));

                    //타이머 종료
                    end = time(NULL);

                    // 타이머가 종료된 시간에 시작 시간을 빼서 총 데이터를 주고 받는데 걸린 시간 계산
                    result = (double)(end - start);

                    // response time 출력
                    printf("response time: %f sec\n",result);

                    //tf를 1로 설정해 반복실행해서, 재전송해서 받은 data가 lost가 일어났는지 확인
                    tf = 1;
                }
            } while(tf);
        // 3을 입력받은 경우 (연결 해체)
        } else if (select == 3) {

            // U-frame 생성 후, 연결 해제 요청에 알맞는 값들을 변수에 넣어줌
            Frame send_frame;
            send_frame.type = 'U';
            send_frame.flag = FLAG;
            send_frame.address = 'B'; 
            // control에 연결 해제 요청을 의미하는 control인 DISC 넣어줌
            send_frame.control = DISC;
            send_frame.closingFlag = CFLAG;
            
            // buffer 값을 0으로 설정 
            memset(buffer, 0, sizeof(buffer));

            // U-frame값 buffer에 복사
            memcpy(buffer, (void *)&send_frame, sizeof(Frame));

            // 연결 해제 요청 문구를 출력하고, receiver에게 U-frame이 담겨있는 buffer 전달
            printf("\nRequest a disconnection.\n");
            send(sock,buffer,sizeof(buffer),0);

            // buffer 값을 0으로 설정
            memset(buffer, 0, sizeof(buffer));
            
            // receiver로부터 U-frame이 담겨있는 buffer를 받음
            valread = read(sock, buffer, sizeof(buffer));

            // 받은 buffer에 들어있는 U-frame을 넣어줄 received_frame 선언 후, 
            // 받은 buffer를 received_frame에 복사
            Frame received_frame;
            memcpy(&received_frame, buffer, sizeof(Frame));

            // 받은 frame의 종류 출력
            printf("The type of frame is %c-frame.\n", received_frame.type);
            // 받은 flag와 cflag 값 출력
            printf("The values of the received frame's flag and cflag are %X and %X.\n",received_frame.flag,received_frame.closingFlag);
            if ((received_frame.flag == FLAG) && (received_frame.flag == CFLAG)) {
                // 받은 flag와 cflag값을 정의되어 있는 값과 비교 후, 맞으면 성공 문구 출력 
                printf("Flag and CFlag checked successfully.\n");
                // 받은 address 값 출력
                printf("The value of the received frame's address is %c.\n",received_frame.address);
                if (received_frame.address == ADDRESS) {    
                    // 받은 address 값을 정의되어 있는 값과 비교 후, 맞으면 성공 문구 출력   
                    printf("Address checked successfully.\n");
                    // 받은 control 값을 위에서 구현한 함수를 이용해 이진수로 출력
                    printf("The value of the received frame's control is ");
                    printBinary(received_frame.control);
                    // 받은 control 값을 정의되어 있는 UA 값과 비교 후,
                    switch (received_frame.control)
                    {
                    // 맞으면 UA 메세지을 받았다는 문구 출력 후, 연결 해제 성공 문구 출력
                    case UA :
                        printf("The UA message has been received.\n");
                        printf("Disconnection successful.\n");
                        // 그 후, 연결이 되기 전 상태로 변수들 초기화
                        memset(buffer, 0, sizeof(buffer));
                        seq_num = 0;
                        connection = 0;
                        break;
                    // 틀리면 UA 메세지를 받지 못했다는 문구 출력 후, break
                    default:
                        printf("No UA message has been received.\n");
                        break;
                    }
                } else {
                    // 받은 address 값을 정의되어 있는 값과 비교 후, 틀리면 실패 문구 출력
                    printf("Address check failed.\n");
                }
            } else {
                // flag와 cflag값을 정의된 값과 비교 후, 틀리면 실패 문구 출력
                printf("Flag and CFlag check failed.\n");
            }
        // 1, 2, 3 외에 다른 integer 값을 입력 받았다면, 정의되지 않은 동작을 알리는 문구 출력
        } else {
            printf("undefined behavior.\n");
        }

        // 반복문이 끝나기 전에 select 값 0으로 설정해서 입력값 초기화 
        select = 0;    
    }

    // Close socket
    close(sock);

    return 0;
}