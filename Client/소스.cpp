// argv[1] = tera.jpg 
// argv[2] = mabinogi.jpg
// argv[3] = 파일이름이최대크기를넘어섰을경우를체크하기위한파일입니다생각보다50바이트의크기가상당하군요.txt

#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <stdio.h>

#include <conio.h>

// 상수 매크로

#define SERVER_PORT 9000
#define SERVER_IP "127.0.0.1"
#define BUFSIZE 4096
#define MAX_FILE_READ_SIZE 1024
#define FILE_NAME_SIZE 128
#define MSGSIZE 512

// 로그인,파일 결과에 대한 열거형

enum RESULT { NODATA = -1, ID_OVERRAP = 1, ID_ERROR, PW_ERROR, LOGIN_OVERRAP, SUCCESS, LOGIN };
enum FILE_RESULT { OVER_NAMESIZE = 1, OVER_FILESIZE, OVERRAP, COMPLETE };

// 로그인, 파일 프로토콜 열거형

enum LOGIN_PROTOCOL { LOGIN_INTRO = 1, NEW, LOGIN_INFO, LOGIN_RESULT, LOGIN_EXIT };
enum FILE_PROTOCOL {
	FILE_INTRO = 1, FILE_INFO, FILE_TRANS_DENY, FILE_TRANS_START_POINT, FILE_TRANS_WAIT,
	FILE_TRANS_DATA, FILE_TRANS_STOP, REMOVE, LOGOUT
};

// 파일 정보 구조체

struct _FileInfo {
	char name[50];		// 파일 이름
	int totalsize;				// 파일 전체 크기
	int nowsize;				// 현재 업로드 된 파일 크기
}FileInfo;

// 유저 정보 구조체

struct _UserInfo {
	char id[50];				// 유저 ID
	char pw[50];				// 유저 PW
	bool state;				// 유저 접속 상태 ( 여기서는 쓰이지 않음 )
}user;

// 전역 함수

void err_quit(const char *msg);																						// 에러 함수 ( 종료 )
void err_display(const char *msg);																					// 에러 함수 ( 출력 )

SOCKET socket_init();																									// 소켓 초기화

int recvn(SOCKET s, char *buf, int len, int flags);															// 사용자 정의 Recv
bool PacketRecv(SOCKET _sock, char* _buf);																// Packing 전용 Recv
int GetProtocol(const char* _ptr);																					// 프로토콜 읽기

// Packing

int PackPacket(char* _buf, int _protocol);
int PackPacket(char* _buf, int _protocol, const char* _str);
int PackPacket(char* _buf, int _protocol, const char* _str, int _size);
int PackPacket(char* _buf, int _protocol, int _size, const char* _str);
int PackPacket(char* _buf, int _protocol, const char* _str, const char* _str2);

// UnPacking

int UnPackPacket(const char* _buf, char* _data);
void UnPackPacket(const char* _buf, int& _data);
int UnPackPacket(const char* _buf, int& _result, char* _data);

// main

int main(int argc, char* argv[]) {
	int retval;

	// WSA 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;

	// Socket 초기화
	SOCKET sock = socket_init();

	// 파일 정보 초기화
	ZeroMemory(&FileInfo, sizeof(FileInfo));

	// 변수
	char buf[BUFSIZE];
	FILE* fp = nullptr;

	LOGIN_PROTOCOL login_protocol;
	FILE_PROTOCOL file_protocol;

	RESULT login_result = NODATA;
	FILE_RESULT file_result;

	bool flag = false;

	int size;
	char msg[MSGSIZE];

	while (1) {
		// 데이터 수신 대기
		// 수신 받는 데이터를 기준으로 설계
		if (!PacketRecv(sock, buf)) {
			err_display("Recv");
			break;
		}

		// 현재 클라이언트가 로그인 상태가 아니라면
		if (login_result != LOGIN) {
			// 프로토콜 읽기
			login_protocol = (LOGIN_PROTOCOL)GetProtocol(buf);

			// 프로토콜에 따라 작업
			switch (login_protocol) {
				// 로그인 메뉴 선택
			case LOGIN_INTRO:
				// 유저 정보 초기화
				ZeroMemory(&user, sizeof(_UserInfo));
				char num[MSGSIZE];

				// 메뉴 정보 UnPacking
				size = UnPackPacket(buf, msg);
				msg[size] = '\0';

				// 메뉴 예외 처리를 위한 반복문
				while (1) {
					printf("%s", msg);
					scanf("%s", num);

					num[1] = '\0';

					switch (atoi(num)) {
						// 회원 가입
					case 1:
						printf("\n\nID : ");
						scanf("%s", user.id);

						printf("\nPW : ");
						scanf("%s", user.pw);

						// 회원가입 정보와 입력된 ID,PW 정보를 Packing
						size = PackPacket(buf, (LOGIN_PROTOCOL)NEW, user.id, user.pw);
						break;

						// 로그인
					case 2:
						printf("\n\nID : ");
						scanf("%s", user.id);

						printf("\nPW : ");
						scanf("%s", user.pw);

						// 로그인 정보와 입력된 ID,PW 정보를 Packing
						size = PackPacket(buf, (LOGIN_PROTOCOL)LOGIN_INFO, user.id, user.pw);
						break;

						// 프로그램 종료
					case 3:
						// 프로그램 종료 프로토콜 Packing
						size = PackPacket(buf, (LOGIN_PROTOCOL)LOGIN_EXIT);
						flag = true;	// 반복문 탈출
						break;

						// 메뉴 이외의 입력에 대한 예외 처리
					default:
						system("cls");
						continue;
					}
					break;
				}

				// Packing 된 데이터를 송신
				retval = send(sock, buf, size, 0);
				if (retval == SOCKET_ERROR) {
					err_display("send");
					flag = true;
				}
				break;

				// 로그인 처리 결과
			case LOGIN_RESULT:
				// 로그인 결과와 그에 따른 정보를 UnPacking
				size = UnPackPacket(buf, (int&)login_result, msg);
				msg[size] = '\0';

				printf("%s", msg);

				if (getch())
					system("cls");

				break;

			} // switch end
		}

		// 현재 클라이언트가 로그인 상태라면
		else {
			// 프로토콜 읽기
			file_protocol = (FILE_PROTOCOL)GetProtocol(buf);

			// 프로토콜에 따라 작업
			switch (file_protocol) {
				// 파일 메뉴 선택
			case FILE_INTRO:
				char num[MSGSIZE];

				// 메뉴 정보 UnPacking
				size = UnPackPacket(buf, msg);
				msg[size] = '\0';

				// 예외 처리를 위한 반복문
				while (1) {
					printf("%s", msg);
					scanf("%s", num);

					num[1] = '\0';

					switch (atoi(num)) {
						// 파일 송신
					case 1:
						int index;
						index = 1;

						while (1) {
							system("cls");
							printf("[ 파일 목록 ]\n");
							printf("보낼 수 있는 파일의 최대 크기 : 20 kb\n\n");

							char temp[10][MSGSIZE];

							if (argv[1] == nullptr) {
								strcpy(temp[1], "tera.jpg");
								strcpy(temp[2], "mabinogi.jpg");
								strcpy(temp[3], "파일이름이최대크기를넘어섰을경우를체크하기위한파일입니다생각보다50바이트의크기가상당하군요.txt");

								while (index != 4) {
									printf("%d. %s\n", index, temp[index]);
									index++;
								}
							}

							else {
								// 클라이언트가 가지고 있는 파일 목록 출력
								while (argv[index] != nullptr) {
									printf("%d. %s\n", index, argv[index]);
									index++;
								}
							}

							printf("\n선택 : ");
							scanf("%s", msg);

							// 예외 처리
							if (atoi(msg) <= 0 || atoi(msg) > index) {
								continue;
							}

							// 선택한 파일 이름 저장
							if (argv[1] == nullptr)
								strcpy(FileInfo.name, temp[atoi(msg)]);
							else
								strcpy(FileInfo.name, argv[atoi(msg)]);

							// 파일 읽기 모드로 열기
							fp = fopen(FileInfo.name, "rb");
							if (!fp) {
								err_quit("File Open Error");						
							}

							// 파일의 끝, 파일 전체 크기를 읽어 저장한다
							fseek(fp, 0, FILE_END);
							FileInfo.totalsize = ftell(fp);
							fclose(fp);

							// 읽어 온 파일의 정보를 Packing
							size = PackPacket(buf, (FILE_PROTOCOL)FILE_INFO, FileInfo.name, FileInfo.totalsize);
							break;
						}
						break;

						// 회원탈퇴
					case 2:
						// 회원탈퇴 프로토콜 Packing
						size = PackPacket(buf, (FILE_PROTOCOL)REMOVE);
						login_result = NODATA;	// 로그인 결과 초기화
						system("cls");
						break;

						// 로그아웃
					case 3:
						// 로그아웃 프로토콜 Packing
						size = PackPacket(buf, (FILE_PROTOCOL)LOGOUT);
						login_result = NODATA;	// 로그인 결과 초기화
						system("cls");
						break;

						// 메뉴 이외의 입력에 대한 예외 처리
					default:
						system("cls");
						continue;
					} // while end
					break;
				} // switch end

				// Packing 데이터 송신
				retval = send(sock, buf, size, 0);
				if (retval == SOCKET_ERROR) {
					err_display("send");
					flag = true;
				}
				break;

				// 파일 송신 실패 ( 파일 이름, 파일 크기 등 제한에 걸린 경우 )
			case FILE_TRANS_DENY:
				// 실패 이유에 대한 정보 UnPacking
				size = UnPackPacket(buf, (int&)file_result, msg);
				msg[size] = '\0';

				printf("\n%s", msg);

				if (getch())
					system("cls");
				break;

				// 파일 송신 대기 ( 서버의 클라이언트 중 자신과 같은 파일을 업로드하고 있는 경우 )
			case FILE_TRANS_WAIT:
				// 대기한다는 정보 UnPacking
				size = UnPackPacket(buf, msg);
				msg[size] = '\0';

				printf("\n%s", msg);
				break;

				// 파일 송신 처리
			case FILE_TRANS_START_POINT:
				// 파일 송신 시작점 UnPacking
				UnPackPacket(buf, FileInfo.nowsize);

				printf("\n\n[ 파일 전송 진행 중 ]\n");
				printf("아무 키나 누르면 전송이 취소됩니다.\n\n");

				// 파일 읽기 모드로 열기
				fp = fopen(FileInfo.name, "rb");
				// 파일 송신 시작점부터 파일의 끝까지 읽기
				fseek(fp, FileInfo.nowsize, SEEK_SET);

				while (1) {
					// 작업 흐름에 관계없이 아무 키나 누를 경우
					if (kbhit() == 1) {
						// 파일 송신 중단 프로토콜 Packing 후 송신
						size = PackPacket(buf, (FILE_PROTOCOL)FILE_TRANS_STOP);
						retval = send(sock, buf, size, 0);
						if (retval == SOCKET_ERROR) {
							err_display("send");
							break;
						}
						// 반복문 탈출
						break;
					}

					// 파일 버퍼, 파일 크기 저장할 변수
					char f_buf[BUFSIZE];
					int trans_size;

					// MAX_FILE_READ_SIZE 만큼의 파일의 크기 읽어 파일 버퍼에 저장하고, 크기를 변수에 저장
					trans_size = fread(f_buf, 1, MAX_FILE_READ_SIZE, fp);

					// 아직 파일을 다 보내지 않았을 경우
					if (trans_size > 0) {
						// 현재 읽은 파일 크기와 파일 버퍼를 Packing 후 송신
						size = PackPacket(buf, (FILE_PROTOCOL)FILE_TRANS_DATA, trans_size, f_buf);
						retval = send(sock, buf, size, 0);
						if (retval == SOCKET_ERROR) {
							err_display("send");
							break;
						}

						// 파일 정보 갱신
						FileInfo.nowsize += trans_size;
						printf("■");
						// 여러 상황을 위한 검사하기 위한 Sleep
						Sleep(1000);
					}
					
					// 송신할 파일 크기가 0 이고, 현재 보낸 파일의 크기와 파일 전체 크기가 같을 경우
					else if (trans_size == 0 && FileInfo.nowsize == FileInfo.totalsize) {
						printf("\n\n파일 전송 완료 : %d byte\n", FileInfo.totalsize);
						break;	// 반복문 탈출
					}
					// 예외 처리
					else {
						perror("파일 입출력 오류");
						break;	// 반복문 탈출
					}
				}

				// 파일 닫기
				fclose(fp);

				if (getch())
					system("cls");
				break;

			} // switch end
		}

		// 반복문 탈출 ( 프로그램 종료 )
		if (flag) break;
	} // while end

	// 마무리 작업
	if (fp != nullptr) fclose(fp);

	closesocket(sock);
	WSACleanup();
	return 0;
}

// 에러 함수 ( 종료 )

void err_quit(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// 에러 함수 ( 출력 )

void err_display(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char *)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

// 소켓 초기화

SOCKET socket_init() {
	int retval;

	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");

	// connect()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(SERVER_PORT);
	serveraddr.sin_addr.s_addr = inet_addr(SERVER_IP);
	retval = connect(sock, (SOCKADDR *)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("connect()");

	return sock;
}

// 사용자 정의 Recv

int recvn(SOCKET s, char *buf, int len, int flags)
{
	int received;
	char *ptr = buf;
	int left = len;

	while (left > 0) {
		received = recv(s, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
}

// Packing 전용 Recv

bool PacketRecv(SOCKET _sock, char* _buf)
{
	int size;

	int retval = recvn(_sock, (char*)&size, sizeof(size), 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("recv error()");
		return false;
	}
	else if (retval == 0)
	{
		return false;
	}

	retval = recvn(_sock, _buf, size, 0);
	if (retval == SOCKET_ERROR)
	{
		err_display("recv error()");
		return false;

	}
	else if (retval == 0)
	{
		return false;
	}

	return true;
}

// 프로토콜 읽기

int GetProtocol(const char* _ptr)
{
	int protocol;
	memcpy(&protocol, _ptr, sizeof(int));

	return protocol;
}

// Packing

int PackPacket(char* _buf, int _protocol) {
	int size = 0;
	char* ptr = _buf;

	ptr = ptr + sizeof(int);

	memcpy(ptr, &_protocol, sizeof(int));
	ptr = ptr + sizeof(int);
	size = size + sizeof(int);

	ptr = _buf;
	memcpy(ptr, &size, sizeof(int));

	return size + sizeof(int);
}

int PackPacket(char* _buf, int _protocol, const char* _str) {
	int size = 0;
	char* ptr = _buf;
	int len = strlen(_str);

	ptr = ptr + sizeof(int);

	memcpy(ptr, &_protocol, sizeof(int));
	ptr = ptr + sizeof(int);
	size = size + sizeof(int);

	memcpy(ptr, &len, sizeof(int));
	ptr = ptr + sizeof(int);
	size = size + sizeof(int);

	memcpy(ptr, _str, len);
	ptr = ptr + len;
	size = size + len;

	ptr = _buf;
	memcpy(ptr, &size, sizeof(int));

	return size + sizeof(int);
}

int PackPacket(char* _buf, int _protocol, const char* _str, int _size) {
	int size = 0;
	char* ptr = _buf;
	int len = strlen(_str);

	ptr = ptr + sizeof(int);

	memcpy(ptr, &_protocol, sizeof(int));
	ptr = ptr + sizeof(int);
	size = size + sizeof(int);

	memcpy(ptr, &len, sizeof(int));
	ptr = ptr + sizeof(int);
	size = size + sizeof(int);

	memcpy(ptr, _str, len);
	ptr = ptr + len;
	size = size + len;

	memcpy(ptr, &_size, sizeof(int));
	ptr = ptr + sizeof(int);
	size = size + sizeof(int);

	ptr = _buf;
	memcpy(ptr, &size, sizeof(int));

	return size + sizeof(int);
}

int PackPacket(char* _buf, int _protocol, int _size, const char* _str) {
	int size = 0;
	char* ptr = _buf;

	ptr = ptr + sizeof(int);

	memcpy(ptr, &_protocol, sizeof(int));
	ptr = ptr + sizeof(int);
	size = size + sizeof(int);

	memcpy(ptr, &_size, sizeof(int));
	ptr = ptr + sizeof(int);
	size = size + sizeof(int);

	memcpy(ptr, _str, _size);
	ptr = ptr + _size;
	size = size + _size;

	ptr = _buf;
	memcpy(ptr, &size, sizeof(int));

	return size + sizeof(int);
}

int PackPacket(char* _buf, int _protocol, const char* _str, const char* _str2) {
	int size = 0;
	char* ptr = _buf;
	int len = strlen(_str);
	int len2 = strlen(_str2);

	ptr = ptr + sizeof(int);

	memcpy(ptr, &_protocol, sizeof(int));
	ptr = ptr + sizeof(int);
	size = size + sizeof(int);

	memcpy(ptr, &len, sizeof(int));
	ptr = ptr + sizeof(int);
	size = size + sizeof(int);

	memcpy(ptr, _str, len);
	ptr = ptr + len;
	size = size + len;

	memcpy(ptr, &len2, sizeof(int));
	ptr = ptr + sizeof(int);
	size = size + sizeof(int);

	memcpy(ptr, _str2, len2);
	ptr = ptr + len2;
	size = size + len2;

	ptr = _buf;
	memcpy(ptr, &size, sizeof(int));

	return size + sizeof(int);
}

// UnPacking

int UnPackPacket(const char* _buf, char* _data) {
	const char* ptr = _buf + sizeof(int);
	int len = 0;

	memcpy(&len, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(_data, ptr, len);
	ptr = ptr + len;

	return len;
}

void UnPackPacket(const char* _buf, int& _data) {
	const char* ptr = _buf + sizeof(int);

	memcpy(&_data, ptr, sizeof(int));
	ptr = ptr + sizeof(int);
}

int UnPackPacket(const char* _buf, int& result, char* _data) {
	const char* ptr = _buf + sizeof(int);
	int len = 0;

	memcpy(&result, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(&len, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(_data, ptr, len);
	ptr = ptr + len;

	return len;
}
