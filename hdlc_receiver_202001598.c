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
#define ADDRESS 'B'
// U-frame 안에 들어가 어떤 동작을 해야하는지 알려주는 control bit 정의
// 중간에 poll bit를 사용하여 주국에서 종국으로 갈 때는 1, 반대인 경우는 0으로 설정
#define SABM 0b11111100
#define DISC 0b11001010
#define UA 0b11000110

// 소켓 통신과 관련된 변수 및 구조체 변수 선언
int server_fd, new_socket, valread;
int opt = 1;
struct sockaddr_in address;
int addrlen = sizeof(address);
// frame struct를 담을 buffer 선언
unsigned char buffer[4096] = {0};
char *ack = "ACK";

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

//2진수를 출력해주는 함수 구현(control 출력 시 사용)
void printBinary(unsigned char control) {
    int i;
    for (i = 7; i >= 0; i--) {
        unsigned char mask = 1 << i;
        unsigned char bit = (control & mask) ? '1' : '0';
        printf("%c", bit);
    }
    printf(".\n");
}

//////////////////////////////////////아래는 소켓통신을 하기 위한 밑작업///////////////////////////////////////////////

int main(int argc, char const *argv[]) {
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // Bind the socket to the specified port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }    
    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }    
    printf("Waiting for incoming connection...\n");

    // Accept incoming connections
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }

    printf("Connection accepted\n");

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    while (1) {
        // buffer의 값을 0으로 설정
        memset(buffer, 0, sizeof(buffer));

        // sender로부터 frame이 담겨있는 buffer를 받음 
        valread = read(new_socket, buffer, sizeof(buffer));

        // 받은 buffer에 들어있는 frame을 넣어줄 received_frame 선언 후, 
        // 받은 buffer를 received_frame에 복사
        Frame received_frame;
        memcpy(&received_frame,buffer, sizeof(Frame));

        // 받은 frame의 종류 출력
        printf("\nThe frame is an %c-frame.\n", received_frame.type);
        // 받은 frame이 U-frame이면 (control에 따라 연결 및 연결 해제)
        if(received_frame.type == 'U') {    
            // 받은 flag와 cflag 값 출력
            printf("The values of the received frame's flag and cflag are %X and %X.\n",received_frame.flag,received_frame.closingFlag);
            if ((received_frame.flag == FLAG) && (received_frame.closingFlag == CFLAG)) {
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
                    
                    // 받은 control 값이 정의되어 있는 SABM 값과 같다면 (연결 요청)
                    if (received_frame.control == SABM) {

                        // SABM 메세지를 받았다는 문구 출력
                        printf("The SABM message has been received.\n");

                        // buffer의 값을 0으로 설정
                        memset(buffer, 0, sizeof(buffer));

                        // U-frame 생성 후, 연결 요청에 대한 응답에 알맞는 값들을 변수에 넣어줌
                        Frame send_frame;
                        send_frame.type = 'U';
                        send_frame.flag = FLAG;
                        send_frame.address = 'A'; 
                        // control에 연결 요청에 대한 응답을 의미하는 UA 넣어줌
                        send_frame.control = UA;
                        send_frame.closingFlag = CFLAG;

                        // U-frame값 buffer에 복사
                        memcpy(buffer, &send_frame, sizeof(Frame));

                        // UA 메세지를 보낸다는 문구를 출력하고, sender에게 U-frame이 담겨있는 buffer 전달
                        printf("Sending a UA message.\n");
                        send(new_socket,buffer,sizeof(buffer),0);

                        // buffer의 값을 0으로 설정
                        memset(buffer, 0, sizeof(buffer));

                    // 그렇지 않고 받은 control 값이 정의되어 있는 DISC 값과 같다면 (연결 해제 요청)
                    } else if (received_frame.control == DISC) {

                        // DISC 메세지를 받았다는 문구 출력
                        printf("The DISC message has been received.\n");

                        // buffer의 값을 0으로 설정
                        memset(buffer, 0, sizeof(buffer));

                        // U-frame 생성 후, 연결 해제 요청에 대한 응답에 알맞는 값들을 변수에 넣어줌
                        Frame send_frame;
                        send_frame.type = 'U';
                        send_frame.flag = FLAG;
                        send_frame.address = 'A'; 
                        // control에 연결 해제 요청에 대한 응답을 의미하는 UA 넣어줌
                        send_frame.control = UA;
                        send_frame.closingFlag = CFLAG;

                        // U-frame값 buffer에 복사
                        memcpy(buffer, &send_frame, sizeof(Frame));

                        // UA 메세지를 보낸다는 문구를 출력하고, sender에게 U-frame이 담겨있는 buffer 전달
                        printf("Sending a UA message.\n");
                        send(new_socket,buffer,sizeof(buffer),0);

                        // buffer의 값을 0으로 설정
                        memset(buffer, 0, sizeof(buffer));
                    } 
                } else {
                    // 받은 address 값을 정의되어 있는 값과 비교 후, 틀리면 실패 문구 출력
                    printf("Address check failed.\n");
                }
            } else {
                // flag와 cflag값을 정의된 값과 비교 후, 틀리면 실패 문구 출력
                printf("Flag and CFlag check failed.\n");
            }
        // 받은 frame이 I-frame이면 (data 송수신)
        } else {
            // 받은 flag와 cflag 값 출력
            printf("The values of the received frame's flag and cflag are %X and %X.\n",received_frame.flag,received_frame.closingFlag);
            if ((received_frame.flag == FLAG) && (received_frame.closingFlag == CFLAG)) {
                // 받은 flag와 cflag값을 정의되어 있는 값과 비교 후, 맞으면 성공 문구 출력 
                printf("Flag and CFlag checked successfully.\n");
                // 받은 address 값 출력
                printf("The value of the received frame's address is %c.\n",received_frame.address);
                if (received_frame.address == ADDRESS) {
                    // 받은 address 값을 정의되어 있는 값과 비교 후, 맞으면 성공 문구 출력                   
                    printf("Address checked successfully.\n");
                    // 받은 control 값에서 비트 연산자를 이용해서 seq 값을 추출 후, 선언한 received_seq에 넣어줌 
                    unsigned char received_seq = ((received_frame.control ^ 0b10001000) >> 4);
                    // 그 후, 받은 seq num 출력
                    printf("Received SEQ for message %u\n", (unsigned int)received_seq);
                    // sender에서 보낸 message 출력
                    printf("Received : %s", received_frame.data);

                    // 0 또는 1을 반환해서 선언한 ran 변수에 저장
                    int ran = rand()%2;

                    // ran 변수가 0이면 (보낼 frame이 lost 되지 않는다면)
                    if (ran == 0) {    

                        // 현재 I-frame을 이용해서 data를 보내므로, 맨 앞 비트를 1로 설정
                        // 지금은 종국에서 주국으로 가는 상황이므로 poll bit를 0 으로 설정
                        // 마지막으로, 보내는 data의 ack num을 오른쪽 000에 넣어주기 위해, 비트 연산자 사용.
                        // (지금은 sender와 receiver의 기능이 나누어져 있어 receiver가 보내는 control 값의 왼쪽 000에 넣을 seq값은 항상 0)
                        unsigned char bits = 0b10000000;
                        bits |= received_seq;
                        
                        // I-frame 생성 후, data 전달에 알맞는 값들을 변수에 넣어줌
                        Frame send_frame;
                        send_frame.type = 'I';
                        send_frame.flag = FLAG;
                        send_frame.address = 'A'; 
                        // control에 위에서 ack num을 넣어준 bits를 넣어줌
                        send_frame.control = bits;
                        send_frame.closingFlag = CFLAG;

                        // buffer 값을 0으로 설정 
                        memset(buffer, 0, sizeof(buffer));

                        // I-frame값 buffer에 복사
                        memcpy(buffer, (void *)&send_frame, sizeof(Frame));
                        
                        //sender에게 I-frame이 담겨있는 buffer 전달
                        send(new_socket,buffer,sizeof(buffer),0);

                    // ran 변수가 0이 아니면 (보낼 frame이 lost 된다면)
                    } else {
                        // 5초 sleep (sender에서 time out을 유도하기 위해서)
                        sleep(5);
                        // Frame이 lost 되었다는 문구 출력 
                        // (원래 receiver 측에서는 frame이 lost 되었다는 사실을 예측할 수 없지만, 가독성을 위해서 출력)
                        printf("Frame was lost in transit.\n");
                        // Resend ACK to client (원래는 lost되었으면 sender에 안보내는 것이 정석이지만,
                        // 보내지 않으면 sender에서 read를 무한정 기다리기 때문에 제대로 동작시키기 위해 보내줌)
                        send(new_socket, ack, strlen(ack), 1);
                    }
                } else {
                    // flag와 cflag값을 정의된 값과 비교 후, 틀리면 실패 문구 출력
                    printf("Address check failed.\n");
                }
            } else {
                // flag와 cflag값을 정의된 값과 비교 후, 틀리면 실패 문구 출력
                printf("Flag and CFlag check failed.\n");
            }
        }
    }

    // Close socket
    close(new_socket);
	close(server_fd);

    return 0;
}