#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <stdio.h>

// 상수 매크로

#define SERVER_PORT 9000
#define BUFSIZE 4096
#define MSGSIZE 512
#define USERSIZE 100

#define MAX_FILENAME 50
#define MAX_FILESIZE 20000

// 문자열 매크로

#define LOGIN_MENU_MSG "[ 메뉴 ]\n\n1. 회원가입\n2. 로그인\n3. 종료\n\n입력 : "
#define FILE_MENU_MSG "[ 메뉴 ]\n\n1. 파일 전송\n2. 회원탈퇴\n3. 로그아웃\n\n입력 : "

#define IDE_OVERRAP_MSG "이미 존재하는 ID 입니다.\n"
#define ID_ERROR_MSG "없는 ID 입니다.\n"
#define PW_ERROR_MSG "PW 가 일치하지 않습니다.\n"
#define LOGIN_OVERRAP_MSG "다른 컴퓨터에서 로그인 중인 계정입니다.\n"
#define NEW_MSG "회원가입에 성공했습니다.\n"
#define LOGIN_MSG "로그인에 성공했습니다.\n"

#define FILE_ERROR_MSG "파일 전송을 실패했습니다.\n"

#define FILE_NAMEOVER_MSG "파일 이름이 너무 깁니다.\n"
#define FILE_SIZEOVER_MSG "허용된 파일 크기를 초과했습니다.\n"
#define FILE_WAIT_MSG "다른 컴퓨터에서 동일한 파일을 전송중입니다. 잠시만 기다려주세요.\n"
#define FILE_OVERRAP_MSG "전송 할 파일이 이미 서버에 존재합니다.\n"

// 상태 열거형

enum STATE { INIT = 1, INTRO, S_NEW, S_LOGIN_INFO, S_LOGIN_RESULT, S_LOGIN_EXIT, 
	S_FILE_INTRO, S_FILE_INFO, S_FILE_TRANS_DENY, S_FILE_TRANS_START_POINT, S_FILE_TRANS_WAIT,
	S_FILE_TRANS_DATA, S_FILE_TRANS_CONTINUE };

// 로그인,파일 결과에 대한 열거형

enum RESULT { ID_OVERRAP = 1, ID_ERROR, PW_ERROR, LOGIN_OVERRAP, SUCCESS, LOGIN };
enum FILE_RESULT { OVER_NAMESIZE = 1, OVER_FILESIZE, OVERRAP, COMPLETE};

// 로그인, 파일 프로토콜 열거형

enum LOGIN_PROTOCOL { LOGIN_INTRO = 1, NEW, LOGIN_INFO, LOGIN_RESULT, LOGIN_EXIT };
enum FILE_PROTOCOL {
	FILE_INTRO = 1, FILE_INFO, FILE_TRANS_DENY, FILE_TRANS_START_POINT, FILE_TRANS_WAIT,
	FILE_TRANS_DATA, FILE_TRANS_STOP, REMOVE, LOGOUT
};

// 파일 정보 구조체

struct _FileInfo{
	char name[50];	// 파일 이름
	int totalsize;			// 파일 전체 크기
	int nowsize;			// 현재 업로드 된 파일 크기

	bool overrap;		// 동일한 파일을 여러명이서 업로드 하는 것을 방지하기 위한 변수
};

// 유저 정보 구조체

struct _UserInfo {
	char id[50];			// 유저 ID
	char pw[50];			// 유저 PW
	bool state;			// 유저 접속 상태
};

// 클라이언트 정보 구조체

struct _ClientInfo {
	SOCKET sock;				// 클라이언트 소켓
	SOCKADDR_IN addr;		// 클라이언트 어드레스
	char buf[BUFSIZE];			// 클라이언트 버퍼
	
	STATE state;					// 상태
	HANDLE hEvent;				// 이벤트

	_UserInfo user;				// 유저 정보
	_FileInfo file;					// 파일 정보
};

// 전역 변수

_UserInfo userlist[USERSIZE];					// 유저 리스트
int user_count = 0;										// 유저 수

_ClientInfo* clientlist[USERSIZE];				// 클라이언트 리스트
int client_count = 0;										// 클라이언트 수

CRITICAL_SECTION cs;								// 크리티컬 섹션

// 전역 함수

bool SearchFile(const char *filename);																// 파일 찾기 함수

void err_quit(const char *msg);																			// 에러 함수 ( 종료 )
void err_display(const char *msg);																		// 에러 함수 ( 출력 )

SOCKET socket_init();																						// 소켓 초기화

_ClientInfo* AddClientInfo(SOCKET _sock, SOCKADDR_IN _addr);					// 클라이언트 추가 함수
void RemoveClientInfo(_ClientInfo* _ptr);															// 클라이언트 삭제 함수

int recvn(SOCKET s, char *buf, int len, int flags);												// 사용자 지정 Recv 함수
bool PacketRecv(SOCKET _sock, char* _buf);													// Paking 을 위한 Recv 함수
int GetProtocol(const char* _ptr);																		// 프로토콜 읽기 함수

// Packing

int PackPacket(char* _buf, int _protocol);
int PackPacket(char* _buf, int _protocol, int data);
int PackPacket(char* _buf, int _protocol, const char* _str);
int PackPacket(char* _buf, int _protocol, int _result, const char* _str);

// UnPacking

int UnPackPacket(const char* _buf, char* _data);
int UnPackPacket(const char* _buf, char* _data, int& size);
int UnPackPacket(const char* _buf, int& size, char* _data);
int UnPackPacket(const char* _buf, char* _data, char* _data2);

DWORD WINAPI Client_Process(LPVOID);		// 클라이언트 스레드

void FileSave(_UserInfo* user, int count);			// 유저 정보 저장 함수
bool FileLoad(_UserInfo* user, int& count);			// 유저 정보 불러오기 함수

// main

int main() {
	int retval;

	// WSA 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// Listen Socket 초기화
	SOCKET listen_sock = socket_init();

	// 크리티컬 섹션 초기화
	InitializeCriticalSection(&cs);

	// 현재 저장되어 있는 유저 정보 불러오기
	if (!FileLoad(userlist, user_count)) {
		printf("[ User Data Load Error ]\n");
	}

	// 클라이언트와 연결을 위한 변수
	SOCKET client_sock;
	SOCKADDR_IN client_addr;
	int addrlen;

	while (1) {
		// Accept
		addrlen = sizeof(SOCKADDR_IN);
		client_sock = accept(listen_sock, (SOCKADDR*)&client_addr, &addrlen);
		if (client_sock == INVALID_SOCKET) {
			err_display("Accept");
			continue;
		}

		// 접속한 클라이언트 추가
		_ClientInfo* ptr = AddClientInfo(client_sock, client_addr);

		// 추가된 클라이언트를 위한 스레드 생성
		HANDLE hThread = CreateThread(nullptr, 0, Client_Process, ptr, 0, nullptr);

		// 스레드가 nullptr 일 경우, 스레드 생성에 실패했기 때문에 클라이언트 정보 삭제
		// 스레드가 성공적으로 생성 되었다면 스레드의 핸들을 닫음
		if (hThread == nullptr)
			RemoveClientInfo(ptr);
		else
			CloseHandle(hThread);
	}

	// 마지막 정리 작업
	closesocket(listen_sock);
	DeleteCriticalSection(&cs);

	WSACleanup();
	return 0;
}

// 클라이언트 스레드

DWORD WINAPI Client_Process(LPVOID arg) {
	// 접속한 클라이언트 정보를 저장
	_ClientInfo* ptr = (_ClientInfo*)arg;

	// 각종 변수
	int size;
	int retval;
	int recived;

	bool flag = false;

	LOGIN_PROTOCOL login_protocol;
	FILE_PROTOCOL file_protocol;

	RESULT login_result;
	FILE_RESULT file_result;

	while (1) {
		// 클라이언트의 상태에 따라 작업
		switch (ptr->state) {
			// 초기화 작업 상태
		case INIT:
			// 상태 갱신
			ptr->state = INTRO;
			break;

			// 로그인 메뉴 상태
		case INTRO:
			// 로그인 메뉴 Packing 후 송신
			size = PackPacket(ptr->buf, (int)LOGIN_INTRO, LOGIN_MENU_MSG);
			retval = send(ptr->sock, ptr->buf, size, 0);
			if (retval == SOCKET_ERROR) {
				err_display("intro Send");
				flag = true;
			}

			// 수신 대기 ( 메뉴 선택 )
			if (!PacketRecv(ptr->sock, ptr->buf)) {
				err_display("Intro Recv");
				flag = true;
			}

			// 유저 정보 초기화
			ZeroMemory(&ptr->user, sizeof(_UserInfo));

			// 프로토콜 갱신
			login_protocol = (LOGIN_PROTOCOL)GetProtocol(ptr->buf);

			// 프로토콜에 따라 작업
			switch (login_protocol) {
			case NEW:	// 회원가입
				ptr->state = S_NEW;	// 상태 갱신
				UnPackPacket(ptr->buf, ptr->user.id, ptr->user.pw);	// 수신 받은 데이터 UnPakcing( ID, PW )
				break;

			case LOGIN_INFO:	// 로그인
				ptr->state = S_LOGIN_INFO;	// 상태 갱신
				UnPackPacket(ptr->buf, ptr->user.id, ptr->user.pw);	// 수신 받은 데이터 UnPakcing( ID, PW )
				break;

			case LOGIN_EXIT:	// 프로그램 종료
				ptr->state = S_LOGIN_EXIT;	// 상태 갱신
				break;
			}
			break;

			// 회원 가입 처리 상태
		case S_NEW:
			EnterCriticalSection(&cs);
			ptr->user.state = true;	// 유저가 접속했다는 가정 하에 시작

			// 가입된 유저 중 중복된 ID 가 있는 지 검사
			for (int i = 0; i < user_count; i++) {
				if (!strcmp(userlist[i].id, ptr->user.id)) {
					// ID 가 중복 되었다는 정보를 Packing 작업
					size = PackPacket(ptr->buf, (LOGIN_PROTOCOL)LOGIN_RESULT, (RESULT)ID_OVERRAP, IDE_OVERRAP_MSG);
					login_result = ID_OVERRAP;	// 로그인 결과값 갱신
					ptr->user.state = false;	// ID 중복이기에 접속 해제
					break;
				}
			}

			// 유저가 계속 접속된 상태, 즉 ID 중복이 아니라면
			if (ptr->user.state) {
				// 정상적으로 회원가입이 되었다는 정보를 Packing
				size = PackPacket(ptr->buf, (LOGIN_PROTOCOL)LOGIN_RESULT, (RESULT)SUCCESS, NEW_MSG);
				login_result = SUCCESS;	// 로그인 결과값 갱신

				ptr->user.state = false;		// 회원가입 후 바로 접속하는 것은 아니기 때문에 접속 해제

				// 유저 리스트에 현재 가입 완료된 유저의 정보를 갱신
				memcpy(&userlist[user_count++], &ptr->user, sizeof(_UserInfo));

				// 현재 유저 리스트 정보 저장
				FileSave(userlist, user_count);
			}

			LeaveCriticalSection(&cs);

			ptr->state = S_LOGIN_RESULT;		// 상태 갱신
			break;


			// 로그인 처리 상태
		case S_LOGIN_INFO:
			EnterCriticalSection(&cs);
			// 클라이언트가 입력한 ID 와 PW 가 유저 리스트에 있는 지 검사
			for (int i = 0; i < user_count; i++) {
				if (!strcmp(userlist[i].id, ptr->user.id)) {
					ptr->user.state = true;	// ID 가 일치한다면 일단 접속 상태로 갱신

					// PW 검사
					if (!strcmp(userlist[i].pw, ptr->user.pw)) {
						// 현재 접속하려는 계정이 다른 사람이 사용하고 있는 지 검사
						if (userlist[i].state) {
							// 현재 다른 사람이 사용하고 있다는 정보를 Packing
							size = PackPacket(ptr->buf, (LOGIN_PROTOCOL)LOGIN_RESULT, (RESULT)LOGIN_OVERRAP, LOGIN_OVERRAP_MSG);
							login_result = LOGIN_OVERRAP;		// 로그인 결과 갱신
						}
						else {
							printf("ID : %s , PW : %s Login\n", userlist[i].id, userlist[i].pw);

							// 정상적으로 로그인 되었다는 정보를 Packing
							size = PackPacket(ptr->buf, (LOGIN_PROTOCOL)LOGIN_RESULT, (RESULT)LOGIN, LOGIN_MSG);
							login_result = LOGIN;				// 로그인 결과 갱신

							userlist[i].state = true; // 접속 상태로 고정 ( 같은 계정 2인 이상 접속 방지 )
						}
						break;
					}
					else {
						// PW 가 일치하지 않는다는 정보를 Packing
						size = PackPacket(ptr->buf, (LOGIN_PROTOCOL)LOGIN_RESULT, (RESULT)PW_ERROR, PW_ERROR_MSG);
						login_result = PW_ERROR;			// 로그인 결과 갱신
					}
				}
			}

			// 일치하는 ID 를 못찾았을 경우
			if (!ptr->user.state) {
				// 일치하는 ID 가 없다 즉, 계정이 없다는 정보를 Packing
				size = PackPacket(ptr->buf, (LOGIN_PROTOCOL)LOGIN_RESULT, (RESULT)ID_ERROR, ID_ERROR_MSG);
				login_result = ID_ERROR;					// 로그인 결과 갱신
			}

			ptr->user.state = false;		// 이미 유저 리스트에서 접속 상태로 고정했기 때문에 후에 재사용을 위해 갱신
			LeaveCriticalSection(&cs);
			ptr->state = S_LOGIN_RESULT;		// 상태 갱신
			break;

			// 로그인 결과 처리 상태
		case S_LOGIN_RESULT:
			// 앞서 Packing 한 정보를 송신
			retval = send(ptr->sock, ptr->buf, size, 0);
			if (retval == SOCKET_ERROR) {
				err_display("Login Result Send");
				flag = true;
			}

			// 로그인 결과에 따른 작업
			switch (login_result) {
				// 로그인 이외의 결과는 다시 로그인 메뉴로 상태 갱신
			case ID_OVERRAP:
			case ID_ERROR:
			case PW_ERROR:
			case LOGIN_OVERRAP:
			case SUCCESS:
				ptr->state = INTRO;
				break;

				// 로그인이라면 파일 메뉴로 상태 갱신
			case LOGIN:
				ptr->state = S_FILE_INTRO;
				break;
			}
			break;

			// 프로그램 종료 상태
		case S_LOGIN_EXIT:
			flag = true;	// while 탈출
			break;

			// 파일 메뉴 상태
		case S_FILE_INTRO:
			// 파일 메뉴 정보 Packing 후 송신
			size = PackPacket(ptr->buf, (LOGIN_PROTOCOL)FILE_INTRO, FILE_MENU_MSG);
			retval = send(ptr->sock, ptr->buf, size, 0);
			if (retval == SOCKET_ERROR) {
				err_display("File Intro Send");
				flag = true;
			}

			// 수신 대기 ( 메뉴 선택 )
			if (!PacketRecv(ptr->sock, ptr->buf)) {
				err_display("File Intro Recv");
				flag = true;
			}

			// 프로토콜 갱신
			file_protocol = (FILE_PROTOCOL)GetProtocol(ptr->buf);

			// 파일 프로토콜 종류에 따라 작업
			switch (file_protocol) {
				// 파일 정보 처리
			case FILE_INFO:
				ptr->state = S_FILE_INFO;	// 상태 갱신
				break;

				// 회원 탈퇴 처리
			case REMOVE:
				EnterCriticalSection(&cs);
				// 유저 정보를 유저 리스트에서 삭제하는 작업
				for (int i = 0; i < user_count; i++) {
					if (!strcmp(userlist[i].id, ptr->user.id)) {
						ZeroMemory(&userlist[i], sizeof(_UserInfo));
						userlist[i].state = false;			// 유저 접속 상태 해제

						// 삭제 된 공간 채우기
						for (int j = i; j < user_count - 1; j++) {
							userlist[j] = userlist[j + 1];
						}
						break;
					}
				}
				user_count--;		// 유저 수 감소

				// 현재 유저 정보 저장
				FileSave(userlist, user_count);
				LeaveCriticalSection(&cs);

				ptr->state = INTRO;		// 상태 갱신 ( 다시 로그인 메뉴로 )
				break;

				// 로그아웃 처리
			case LOGOUT:
				EnterCriticalSection(&cs);
				for (int i = 0; i < user_count; i++) {
					if (!strcmp(userlist[i].id, ptr->user.id)) {
						userlist[i].state = false;		// 유저 접속 상태 해제
						break;
					}
				}
				LeaveCriticalSection(&cs);

				ptr->state = INTRO;		// 상태 갱신 ( 다시 로그인 메뉴 )
				break;
			}
			break;

			// 파일 정보 처리
		case S_FILE_INFO:
			// 클라이언트 파일 정보 초기화 및 지역 변수
			ZeroMemory(&ptr->file, sizeof(_FileInfo));
			char filename[MSGSIZE];
			int filesize;

			// 업로드 받을 파일 이름, 파일 크기를 UnPacking
			size = UnPackPacket(ptr->buf, filename, filesize);
			filename[size] = '\0';

			// 파일 이름 OVER SIZE
			if (size > MAX_FILENAME) {
				// 파일 이름 사이즈가 제한 사이즈를 초과 했다는 정보를 Packing
				size = PackPacket(ptr->buf, (FILE_PROTOCOL)FILE_TRANS_DENY, (FILE_RESULT)OVER_NAMESIZE, FILE_NAMEOVER_MSG);
				ptr->state = S_FILE_INTRO;		// 상태 갱신
			}
			// 파일 크기 OVER SIZE
			else if (filesize > MAX_FILESIZE) {
				// 파일 크기가 제한 크기를 초과 했다는 정보를 Packing
				size = PackPacket(ptr->buf, (FILE_PROTOCOL)FILE_TRANS_DENY, (FILE_RESULT)OVER_FILESIZE, FILE_SIZEOVER_MSG);
				ptr->state = S_FILE_INTRO;		// 상태 갱신
			}

			// 파일 이름과 파일 크기의 사이즈가 제한 사이즈를 넘지 않았다면
			else {
				while (1) {
					// 서버 내에 업로드 할 파일이 이미 존재한다면
					if (SearchFile(filename)) {
						FILE* fp;
						// 파일 읽기 모드로 열고 현재 서버 내에 있는 파일의 끝부분까지 읽음
						fp = fopen(filename, "rb");
						fseek(fp, 0, SEEK_END);

						// 파일의 끝 즉, 현재 서버가 가진 파일의 크기를 저장
						int fsize;
						fsize = ftell(fp);
						fclose(fp);

						// 서버가 가지고 있는 파일의 크기와 업로드 할 파일의 크기가 같다면
						if (filesize == fsize) {
							// 더 이상 받을 필요가 없기 때문에 파일이 이미 존재 한다는 정보를 Packing
							size = PackPacket(ptr->buf, (FILE_PROTOCOL)FILE_TRANS_DENY, (FILE_RESULT)OVERRAP, FILE_OVERRAP_MSG);
							ptr->state = S_FILE_INTRO;			// 상태 갱신

							EnterCriticalSection(&cs);
							// 자신을 제외한 현재 클라이언트 중 자신과 동일한 파일을 업로드 하려고 대기 중인 클라이언트를 찾는다. 
							for (int i = 0; i < client_count; i++) {
								// 자신이 아니다 && 클라이언트가 자신과 같은 파일을 업로드 하려고 한다 && 클라이언트는 대기 중이다 
								if (clientlist[i] != ptr && !strcmp(clientlist[i]->file.name, ptr->file.name) && clientlist[i]->file.overrap) {			
									// 클라이언트 이벤트 활성화
									SetEvent(clientlist[i]->hEvent);
									break;
								}
							}

							// 자신의 파일 정보를 클라이언트 리스트에서 초기화 후, 자신의 정보도 초기화
							// 이후의 업로드에서 대기 상태(데드락)를 피하기 위한 작업
							for (int i = 0; i < client_count; i++) {
								if (clientlist[i] == ptr) {
									ZeroMemory(&clientlist[i]->file, sizeof(_FileInfo));
									ZeroMemory(&ptr->file, sizeof(_FileInfo));
									break;
								}
							}
							LeaveCriticalSection(&cs);
						}
						// 현재 서버가 가진 파일이 업로드 받을 파일 크기보다 작다면
						else {
							// 파일 이름과 파일 전체 크기 저장
							strcpy(ptr->file.name, filename);
							ptr->file.totalsize = filesize;
							// 현재 서버가 가진 파일의 크기 저장 
							ptr->file.nowsize = fsize;
						}
					}

					// 만일 업로드 받을 파일이 서버 내에 없다면
					else {
						// 파일 이름, 파일 전체 크기를 저장하고, 현재 서버가 가진 파일의 크기는 0 지정
						strcpy(ptr->file.name, filename);
						ptr->file.totalsize = filesize;
						ptr->file.nowsize = 0;
					}

					EnterCriticalSection(&cs);
					// 현재 클라이언트는 대기하고 있지 않다는 가정 하에
					ptr->file.overrap = false;

					// 현재 클라이언트는 어떠한 제한에도 걸리지 않았다면
					if (ptr->state != S_FILE_INTRO) {
						for (int i = 0; i < client_count; i++) {
							// 자신은 제외 && 접속한 클라이언트 중 자신과 같은 파일을 업로드 중 && 그 클라이언트는 대기 상태가 아님
							if (clientlist[i] != ptr && !strcmp(clientlist[i]->file.name, ptr->file.name) && !clientlist[i]->file.overrap) {
								// 현재 클라이언트 이벤트 비활성화
								ResetEvent(ptr->hEvent);
								// 현재 클라이언트 대기 상태
								ptr->file.overrap = true;
								break;
							}
						}
					}
					LeaveCriticalSection(&cs);

					// 현재 클라이언트가 대기 상태가 되었다면
					if (ptr->file.overrap) {
						printf("[%s / %d] Wait\n", inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));

						// 대기 상태가 되었다는 정보를 Packing 후 송신
						size = PackPacket(ptr->buf, (FILE_PROTOCOL)FILE_TRANS_WAIT, FILE_WAIT_MSG);
						retval = send(ptr->sock, ptr->buf, size, 0);
						if (retval == SOCKET_ERROR) {
							err_display("File Info in Wait Send");
							flag = true;
						}

						// 대기 상태
						recived = WaitForSingleObject(ptr->hEvent, INFINITE);
						// 타이밍을 맞추기 위한 강제 캐스팅
						Sleep(0);
						continue;
					}

					// 현재 클라이언트가 대기 상태가 아니고, 어떠한 제한에도 걸린 것이 없다면
					else if (!ptr->file.overrap && ptr->state != S_FILE_INTRO) {
						printf("[%s / %d] 받을 파일 이름 : %s\n", inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port), filename);
						printf("[%s / %d] 받을 파일 크기 : %d\n", inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port), filesize);

						// 현재 서버내에 있는 파일의 크기, 즉 업로드를 시작할 시작점의 정보를 Packing
						size = PackPacket(ptr->buf, (FILE_PROTOCOL)FILE_TRANS_START_POINT, ptr->file.nowsize);
						ptr->state = S_FILE_TRANS_DATA;	// 상태 갱신
					}
					break;
				}
			}

			// Packing 데이터 송신
			retval = send(ptr->sock, ptr->buf, size, 0);
			if (retval == SOCKET_ERROR) {
				err_display("File Info Send");
				flag = true;
			}
			break;

			// 파일 송신 처리
		case S_FILE_TRANS_DATA:
			FILE* fp;
			// 파일 쓰기 모드로 열기 ( 기존에 있던 파일에 추가적으로 쓰기 가능 )
			fp = fopen(ptr->file.name, "ab");

			// 파일 수신이 완료되거나, 파일 수신을 중단할 때까지 반복
			while (1) {
				// 파일 정보 수신
				if (!PacketRecv(ptr->sock, ptr->buf)) {
					err_display("File Trans Data Recv");
					flag = true;
					break;
				}

				// 프로토콜 읽기
				file_protocol = (FILE_PROTOCOL)GetProtocol(ptr->buf);
				// 만약 수신 중단 프로토콜일 경우 중단
				if (file_protocol == FILE_TRANS_STOP) {
					break;
				}

				// 파일 정보를 쓸 버퍼와 넘겨 받은 파일 현재 크기
				char buf[BUFSIZE];
				int trans_size;

				// 파일 크기와 파일 정보를 UnPacking
				UnPackPacket(ptr->buf, trans_size, buf);
				// UnPacking 한 정보를 파일에 쓰기
				fwrite(buf, 1, trans_size, fp);
				if (ferror(fp)) {
					perror("파일 입출력 오류");
					break;
				}

				// 현재 업로드 한 크기를 서버가 가진 파일의 현재 크기에 추가
				ptr->file.nowsize += trans_size;

				// 업로드 받을 파일의 전체 크기와 서버가 가진 파일의 현재 크기가 같다면 수신 완료이기에 중단
				if (ptr->file.totalsize == ptr->file.nowsize) {
					printf("파일 수신 완료\n");
					break;
				}
			}
			// 파일 닫기
			fclose(fp);

			// 파일 업로드가 도중 중단 되었든, 수신이 완료 되었든 이 작업은 유효하게 이루어짐
			EnterCriticalSection(&cs);
			for (int i = 0; i < client_count; i++) {
				// 자신은 아니다 && 접속된 클라이언트 중 자신과 같은 파일을 업로드 하려고 한다
				if (clientlist[i] != ptr && !strcmp(clientlist[i]->file.name, ptr->file.name)) {
					// 자신은 이미 업로드 작업을 끝났기 때문에 대기 중인 클라이언트의 이벤트를 활성화 시킨다.
					SetEvent(clientlist[i]->hEvent);
					break;
				}
			}

			// 자신의 정보를 초기화
			for (int i = 0; i < client_count; i++) {
				if (clientlist[i] == ptr) {
					ZeroMemory(&clientlist[i]->file, sizeof(_FileInfo));
					break;
				}
			}
			LeaveCriticalSection(&cs);

			ptr->state = S_FILE_INTRO;		// 상태 갱신
			break;
		}	// switch end
		
		// while 탈출
		if (flag) { 
			// 클라이언트에게 마무리 작업을 넘기기 위한 수신
			if (!PacketRecv(ptr->sock, ptr->buf)) {
				break;
			}
		}
	}

	// 스레드 종료 전에 확실하게 현재 클라이언트의 접속 상태를 해제 시킴
	EnterCriticalSection(&cs);
	for (int i = 0; i < user_count; i++) {
		if (!strcmp(userlist[i].id, ptr->user.id)) {
			userlist[i].state = false;
		}
	}
	LeaveCriticalSection(&cs);

	// 클라이언트 정보 삭제
	RemoveClientInfo(ptr);
	return 0;
}

// 파일 찾기 함수

bool SearchFile(const char *filename)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFindFile = FindFirstFile(filename, &FindFileData);
	if (hFindFile == INVALID_HANDLE_VALUE)
		return false;
	else {
		FindClose(hFindFile);
		return true;
	}
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

// 소켓 초기화 함수

SOCKET socket_init() {
	int retval;

	// socket()
	SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVER_PORT);
	retval = bind(listen_sock, (SOCKADDR *)&serveraddr,
		sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(listen_sock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");

	return listen_sock;
}

// 사용자 정의 recv 함수

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

// Packing 을 위한 recv 함수

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

// 프로토콜 읽기 함수

int GetProtocol(const char* _ptr)
{
	int protocol;
	memcpy(&protocol, _ptr, sizeof(int));

	return protocol;
}

// 클라이언트 추가 함수

_ClientInfo* AddClientInfo(SOCKET _sock, SOCKADDR_IN _addr)
{
	EnterCriticalSection(&cs);

	_ClientInfo* ptr = new _ClientInfo;			// 객체 생성
	ZeroMemory(ptr, sizeof(_ClientInfo));		// 초기화
	ptr->sock = _sock;									// 소켓 설정
	memcpy(&ptr->addr, &_addr, sizeof(SOCKADDR_IN));	// 어드레스 설정

	// 유저,파일 정보 초기화
	ZeroMemory(&ptr->user, sizeof(_UserInfo));
	ZeroMemory(&ptr->file, sizeof(_FileInfo));

	ptr->state = INIT;	// 상태 초기화
	ptr->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);	// 이벤트 초기화 ( 수동 )

	clientlist[client_count++] = ptr;	// 클라이언트 리스트 갱신

	LeaveCriticalSection(&cs);

	printf("\n[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
		inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));

	return ptr;
}

// 클라이언트 삭제 함수

void RemoveClientInfo(_ClientInfo* _ptr)
{
	printf("\n[TCP 서버] 클라이언트 종료: IP 주소=%s, 포트 번호=%d\n",
		inet_ntoa(_ptr->addr.sin_addr), ntohs(_ptr->addr.sin_port));

	EnterCriticalSection(&cs);

	for (int i = 0; i < client_count; i++)
	{
		if (clientlist[i] == _ptr)
		{
			closesocket(clientlist[i]->sock);			// 소켓 닫기
			CloseHandle(clientlist[i]->hEvent);		// 이벤트 닫기

			delete clientlist[i];									// 클라이언트 리스트에서 삭제

			// 클라이언트 리스트에서 삭제 된 공간 채우기 
			for (int j = i; j < client_count - 1; j++)
			{
				clientlist[j] = clientlist[j + 1];
			}
			break;
		}
	}

	client_count--;		// 클라이언트 수 감소
	LeaveCriticalSection(&cs);
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

int PackPacket(char* _buf, int _protocol, int data) {
	int size = 0;
	char* ptr = _buf;

	ptr = ptr + sizeof(int);

	memcpy(ptr, &_protocol, sizeof(int));
	ptr = ptr + sizeof(int);
	size = size + sizeof(int);

	memcpy(ptr, &data, sizeof(int));
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

int PackPacket(char* _buf, int _protocol, int _result, const char* _str) {
	int size = 0;
	char* ptr = _buf;
	int len = strlen(_str);

	ptr = ptr + sizeof(int);

	memcpy(ptr, &_protocol, sizeof(int));
	ptr = ptr + sizeof(int);
	size = size + sizeof(int);

	memcpy(ptr, &_result, sizeof(int));
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

int UnPackPacket(const char* _buf, int& size, char* _data) {
	const char* ptr = _buf + sizeof(int);

	memcpy(&size, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(_data, ptr, size);
	ptr = ptr + size;

	return size;
}

int UnPackPacket(const char* _buf, char* _data, int& size) {
	const char* ptr = _buf + sizeof(int);
	int len = 0;

	memcpy(&len, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(_data, ptr, len);
	ptr = ptr + len;

	memcpy(&size, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	return len;
}

int UnPackPacket(const char* _buf, char* _data, char* _data2) {
	const char* ptr = _buf + sizeof(int);
	int len = 0;
	int len2 = 0;

	memcpy(&len, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(_data, ptr, len);
	ptr = ptr + len;

	memcpy(&len2, ptr, sizeof(int));
	ptr = ptr + sizeof(int);

	memcpy(_data2, ptr, len2);
	ptr = ptr + len2;

	return 0;
}

// 파일 저장,읽기 함수

void FileSave(_UserInfo* user, int count) {
	FILE* fp = nullptr;

	// 파일 쓰기 모드로 열기
	fp = fopen("UserData.txt", "w");
	if (fp == nullptr) return;

	if (fp) {
		// 유저 정보가 없으면 바로 종료
		if (count == 0) {
			fclose(fp);
			return;
		}

		// 유저 정보 쓰기 작업
		fprintf(fp, "%d\n", count);
		for (int i = 0; i < count; i++) {
			fprintf(fp, "%s %s\n", user[i].id, user[i].pw);
		}
	}
	fclose(fp);
}

bool FileLoad(_UserInfo* user, int& count) {
	FILE* fp = nullptr;

	// 파일 읽기 모드로 열기
	fp = fopen("UserData.txt", "r");
	if (fp == nullptr) return false;


	// 파일의 끝 지점까지 읽어 유저 정보를 불러오기
	if (!feof(fp)) {
		fscanf_s(fp, "%d", &count);

		for (int i = 0; i < count; i++) {
			fscanf_s(fp, "%s %s", user[i].id, sizeof(user[i].id), user[i].pw, sizeof(user[i].pw));
		}
	}

	fclose(fp);
	return true;
}
