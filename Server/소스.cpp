#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <stdio.h>

// ��� ��ũ��

#define SERVER_PORT 9000
#define BUFSIZE 4096
#define MSGSIZE 512
#define USERSIZE 100

#define MAX_FILENAME 50
#define MAX_FILESIZE 20000

// ���ڿ� ��ũ��

#define LOGIN_MENU_MSG "[ �޴� ]\n\n1. ȸ������\n2. �α���\n3. ����\n\n�Է� : "
#define FILE_MENU_MSG "[ �޴� ]\n\n1. ���� ����\n2. ȸ��Ż��\n3. �α׾ƿ�\n\n�Է� : "

#define IDE_OVERRAP_MSG "�̹� �����ϴ� ID �Դϴ�.\n"
#define ID_ERROR_MSG "���� ID �Դϴ�.\n"
#define PW_ERROR_MSG "PW �� ��ġ���� �ʽ��ϴ�.\n"
#define LOGIN_OVERRAP_MSG "�ٸ� ��ǻ�Ϳ��� �α��� ���� �����Դϴ�.\n"
#define NEW_MSG "ȸ�����Կ� �����߽��ϴ�.\n"
#define LOGIN_MSG "�α��ο� �����߽��ϴ�.\n"

#define FILE_ERROR_MSG "���� ������ �����߽��ϴ�.\n"

#define FILE_NAMEOVER_MSG "���� �̸��� �ʹ� ��ϴ�.\n"
#define FILE_SIZEOVER_MSG "���� ���� ũ�⸦ �ʰ��߽��ϴ�.\n"
#define FILE_WAIT_MSG "�ٸ� ��ǻ�Ϳ��� ������ ������ �������Դϴ�. ��ø� ��ٷ��ּ���.\n"
#define FILE_OVERRAP_MSG "���� �� ������ �̹� ������ �����մϴ�.\n"

// ���� ������

enum STATE { INIT = 1, INTRO, S_NEW, S_LOGIN_INFO, S_LOGIN_RESULT, S_LOGIN_EXIT, 
	S_FILE_INTRO, S_FILE_INFO, S_FILE_TRANS_DENY, S_FILE_TRANS_START_POINT, S_FILE_TRANS_WAIT,
	S_FILE_TRANS_DATA, S_FILE_TRANS_CONTINUE };

// �α���,���� ����� ���� ������

enum RESULT { ID_OVERRAP = 1, ID_ERROR, PW_ERROR, LOGIN_OVERRAP, SUCCESS, LOGIN };
enum FILE_RESULT { OVER_NAMESIZE = 1, OVER_FILESIZE, OVERRAP, COMPLETE};

// �α���, ���� �������� ������

enum LOGIN_PROTOCOL { LOGIN_INTRO = 1, NEW, LOGIN_INFO, LOGIN_RESULT, LOGIN_EXIT };
enum FILE_PROTOCOL {
	FILE_INTRO = 1, FILE_INFO, FILE_TRANS_DENY, FILE_TRANS_START_POINT, FILE_TRANS_WAIT,
	FILE_TRANS_DATA, FILE_TRANS_STOP, REMOVE, LOGOUT
};

// ���� ���� ����ü

struct _FileInfo{
	char name[50];	// ���� �̸�
	int totalsize;			// ���� ��ü ũ��
	int nowsize;			// ���� ���ε� �� ���� ũ��

	bool overrap;		// ������ ������ �������̼� ���ε� �ϴ� ���� �����ϱ� ���� ����
};

// ���� ���� ����ü

struct _UserInfo {
	char id[50];			// ���� ID
	char pw[50];			// ���� PW
	bool state;			// ���� ���� ����
};

// Ŭ���̾�Ʈ ���� ����ü

struct _ClientInfo {
	SOCKET sock;				// Ŭ���̾�Ʈ ����
	SOCKADDR_IN addr;		// Ŭ���̾�Ʈ ��巹��
	char buf[BUFSIZE];			// Ŭ���̾�Ʈ ����
	
	STATE state;					// ����
	HANDLE hEvent;				// �̺�Ʈ

	_UserInfo user;				// ���� ����
	_FileInfo file;					// ���� ����
};

// ���� ����

_UserInfo userlist[USERSIZE];					// ���� ����Ʈ
int user_count = 0;										// ���� ��

_ClientInfo* clientlist[USERSIZE];				// Ŭ���̾�Ʈ ����Ʈ
int client_count = 0;										// Ŭ���̾�Ʈ ��

CRITICAL_SECTION cs;								// ũ��Ƽ�� ����

// ���� �Լ�

bool SearchFile(const char *filename);																// ���� ã�� �Լ�

void err_quit(const char *msg);																			// ���� �Լ� ( ���� )
void err_display(const char *msg);																		// ���� �Լ� ( ��� )

SOCKET socket_init();																						// ���� �ʱ�ȭ

_ClientInfo* AddClientInfo(SOCKET _sock, SOCKADDR_IN _addr);					// Ŭ���̾�Ʈ �߰� �Լ�
void RemoveClientInfo(_ClientInfo* _ptr);															// Ŭ���̾�Ʈ ���� �Լ�

int recvn(SOCKET s, char *buf, int len, int flags);												// ����� ���� Recv �Լ�
bool PacketRecv(SOCKET _sock, char* _buf);													// Paking �� ���� Recv �Լ�
int GetProtocol(const char* _ptr);																		// �������� �б� �Լ�

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

DWORD WINAPI Client_Process(LPVOID);		// Ŭ���̾�Ʈ ������

void FileSave(_UserInfo* user, int count);			// ���� ���� ���� �Լ�
bool FileLoad(_UserInfo* user, int& count);			// ���� ���� �ҷ����� �Լ�

// main

int main() {
	int retval;

	// WSA �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// Listen Socket �ʱ�ȭ
	SOCKET listen_sock = socket_init();

	// ũ��Ƽ�� ���� �ʱ�ȭ
	InitializeCriticalSection(&cs);

	// ���� ����Ǿ� �ִ� ���� ���� �ҷ�����
	if (!FileLoad(userlist, user_count)) {
		printf("[ User Data Load Error ]\n");
	}

	// Ŭ���̾�Ʈ�� ������ ���� ����
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

		// ������ Ŭ���̾�Ʈ �߰�
		_ClientInfo* ptr = AddClientInfo(client_sock, client_addr);

		// �߰��� Ŭ���̾�Ʈ�� ���� ������ ����
		HANDLE hThread = CreateThread(nullptr, 0, Client_Process, ptr, 0, nullptr);

		// �����尡 nullptr �� ���, ������ ������ �����߱� ������ Ŭ���̾�Ʈ ���� ����
		// �����尡 ���������� ���� �Ǿ��ٸ� �������� �ڵ��� ����
		if (hThread == nullptr)
			RemoveClientInfo(ptr);
		else
			CloseHandle(hThread);
	}

	// ������ ���� �۾�
	closesocket(listen_sock);
	DeleteCriticalSection(&cs);

	WSACleanup();
	return 0;
}

// Ŭ���̾�Ʈ ������

DWORD WINAPI Client_Process(LPVOID arg) {
	// ������ Ŭ���̾�Ʈ ������ ����
	_ClientInfo* ptr = (_ClientInfo*)arg;

	// ���� ����
	int size;
	int retval;
	int recived;

	bool flag = false;

	LOGIN_PROTOCOL login_protocol;
	FILE_PROTOCOL file_protocol;

	RESULT login_result;
	FILE_RESULT file_result;

	while (1) {
		// Ŭ���̾�Ʈ�� ���¿� ���� �۾�
		switch (ptr->state) {
			// �ʱ�ȭ �۾� ����
		case INIT:
			// ���� ����
			ptr->state = INTRO;
			break;

			// �α��� �޴� ����
		case INTRO:
			// �α��� �޴� Packing �� �۽�
			size = PackPacket(ptr->buf, (int)LOGIN_INTRO, LOGIN_MENU_MSG);
			retval = send(ptr->sock, ptr->buf, size, 0);
			if (retval == SOCKET_ERROR) {
				err_display("intro Send");
				flag = true;
			}

			// ���� ��� ( �޴� ���� )
			if (!PacketRecv(ptr->sock, ptr->buf)) {
				err_display("Intro Recv");
				flag = true;
			}

			// ���� ���� �ʱ�ȭ
			ZeroMemory(&ptr->user, sizeof(_UserInfo));

			// �������� ����
			login_protocol = (LOGIN_PROTOCOL)GetProtocol(ptr->buf);

			// �������ݿ� ���� �۾�
			switch (login_protocol) {
			case NEW:	// ȸ������
				ptr->state = S_NEW;	// ���� ����
				UnPackPacket(ptr->buf, ptr->user.id, ptr->user.pw);	// ���� ���� ������ UnPakcing( ID, PW )
				break;

			case LOGIN_INFO:	// �α���
				ptr->state = S_LOGIN_INFO;	// ���� ����
				UnPackPacket(ptr->buf, ptr->user.id, ptr->user.pw);	// ���� ���� ������ UnPakcing( ID, PW )
				break;

			case LOGIN_EXIT:	// ���α׷� ����
				ptr->state = S_LOGIN_EXIT;	// ���� ����
				break;
			}
			break;

			// ȸ�� ���� ó�� ����
		case S_NEW:
			EnterCriticalSection(&cs);
			ptr->user.state = true;	// ������ �����ߴٴ� ���� �Ͽ� ����

			// ���Ե� ���� �� �ߺ��� ID �� �ִ� �� �˻�
			for (int i = 0; i < user_count; i++) {
				if (!strcmp(userlist[i].id, ptr->user.id)) {
					// ID �� �ߺ� �Ǿ��ٴ� ������ Packing �۾�
					size = PackPacket(ptr->buf, (LOGIN_PROTOCOL)LOGIN_RESULT, (RESULT)ID_OVERRAP, IDE_OVERRAP_MSG);
					login_result = ID_OVERRAP;	// �α��� ����� ����
					ptr->user.state = false;	// ID �ߺ��̱⿡ ���� ����
					break;
				}
			}

			// ������ ��� ���ӵ� ����, �� ID �ߺ��� �ƴ϶��
			if (ptr->user.state) {
				// ���������� ȸ�������� �Ǿ��ٴ� ������ Packing
				size = PackPacket(ptr->buf, (LOGIN_PROTOCOL)LOGIN_RESULT, (RESULT)SUCCESS, NEW_MSG);
				login_result = SUCCESS;	// �α��� ����� ����

				ptr->user.state = false;		// ȸ������ �� �ٷ� �����ϴ� ���� �ƴϱ� ������ ���� ����

				// ���� ����Ʈ�� ���� ���� �Ϸ�� ������ ������ ����
				memcpy(&userlist[user_count++], &ptr->user, sizeof(_UserInfo));

				// ���� ���� ����Ʈ ���� ����
				FileSave(userlist, user_count);
			}

			LeaveCriticalSection(&cs);

			ptr->state = S_LOGIN_RESULT;		// ���� ����
			break;


			// �α��� ó�� ����
		case S_LOGIN_INFO:
			EnterCriticalSection(&cs);
			// Ŭ���̾�Ʈ�� �Է��� ID �� PW �� ���� ����Ʈ�� �ִ� �� �˻�
			for (int i = 0; i < user_count; i++) {
				if (!strcmp(userlist[i].id, ptr->user.id)) {
					ptr->user.state = true;	// ID �� ��ġ�Ѵٸ� �ϴ� ���� ���·� ����

					// PW �˻�
					if (!strcmp(userlist[i].pw, ptr->user.pw)) {
						// ���� �����Ϸ��� ������ �ٸ� ����� ����ϰ� �ִ� �� �˻�
						if (userlist[i].state) {
							// ���� �ٸ� ����� ����ϰ� �ִٴ� ������ Packing
							size = PackPacket(ptr->buf, (LOGIN_PROTOCOL)LOGIN_RESULT, (RESULT)LOGIN_OVERRAP, LOGIN_OVERRAP_MSG);
							login_result = LOGIN_OVERRAP;		// �α��� ��� ����
						}
						else {
							printf("ID : %s , PW : %s Login\n", userlist[i].id, userlist[i].pw);

							// ���������� �α��� �Ǿ��ٴ� ������ Packing
							size = PackPacket(ptr->buf, (LOGIN_PROTOCOL)LOGIN_RESULT, (RESULT)LOGIN, LOGIN_MSG);
							login_result = LOGIN;				// �α��� ��� ����

							userlist[i].state = true; // ���� ���·� ���� ( ���� ���� 2�� �̻� ���� ���� )
						}
						break;
					}
					else {
						// PW �� ��ġ���� �ʴ´ٴ� ������ Packing
						size = PackPacket(ptr->buf, (LOGIN_PROTOCOL)LOGIN_RESULT, (RESULT)PW_ERROR, PW_ERROR_MSG);
						login_result = PW_ERROR;			// �α��� ��� ����
					}
				}
			}

			// ��ġ�ϴ� ID �� ��ã���� ���
			if (!ptr->user.state) {
				// ��ġ�ϴ� ID �� ���� ��, ������ ���ٴ� ������ Packing
				size = PackPacket(ptr->buf, (LOGIN_PROTOCOL)LOGIN_RESULT, (RESULT)ID_ERROR, ID_ERROR_MSG);
				login_result = ID_ERROR;					// �α��� ��� ����
			}

			ptr->user.state = false;		// �̹� ���� ����Ʈ���� ���� ���·� �����߱� ������ �Ŀ� ������ ���� ����
			LeaveCriticalSection(&cs);
			ptr->state = S_LOGIN_RESULT;		// ���� ����
			break;

			// �α��� ��� ó�� ����
		case S_LOGIN_RESULT:
			// �ռ� Packing �� ������ �۽�
			retval = send(ptr->sock, ptr->buf, size, 0);
			if (retval == SOCKET_ERROR) {
				err_display("Login Result Send");
				flag = true;
			}

			// �α��� ����� ���� �۾�
			switch (login_result) {
				// �α��� �̿��� ����� �ٽ� �α��� �޴��� ���� ����
			case ID_OVERRAP:
			case ID_ERROR:
			case PW_ERROR:
			case LOGIN_OVERRAP:
			case SUCCESS:
				ptr->state = INTRO;
				break;

				// �α����̶�� ���� �޴��� ���� ����
			case LOGIN:
				ptr->state = S_FILE_INTRO;
				break;
			}
			break;

			// ���α׷� ���� ����
		case S_LOGIN_EXIT:
			flag = true;	// while Ż��
			break;

			// ���� �޴� ����
		case S_FILE_INTRO:
			// ���� �޴� ���� Packing �� �۽�
			size = PackPacket(ptr->buf, (LOGIN_PROTOCOL)FILE_INTRO, FILE_MENU_MSG);
			retval = send(ptr->sock, ptr->buf, size, 0);
			if (retval == SOCKET_ERROR) {
				err_display("File Intro Send");
				flag = true;
			}

			// ���� ��� ( �޴� ���� )
			if (!PacketRecv(ptr->sock, ptr->buf)) {
				err_display("File Intro Recv");
				flag = true;
			}

			// �������� ����
			file_protocol = (FILE_PROTOCOL)GetProtocol(ptr->buf);

			// ���� �������� ������ ���� �۾�
			switch (file_protocol) {
				// ���� ���� ó��
			case FILE_INFO:
				ptr->state = S_FILE_INFO;	// ���� ����
				break;

				// ȸ�� Ż�� ó��
			case REMOVE:
				EnterCriticalSection(&cs);
				// ���� ������ ���� ����Ʈ���� �����ϴ� �۾�
				for (int i = 0; i < user_count; i++) {
					if (!strcmp(userlist[i].id, ptr->user.id)) {
						ZeroMemory(&userlist[i], sizeof(_UserInfo));
						userlist[i].state = false;			// ���� ���� ���� ����

						// ���� �� ���� ä���
						for (int j = i; j < user_count - 1; j++) {
							userlist[j] = userlist[j + 1];
						}
						break;
					}
				}
				user_count--;		// ���� �� ����

				// ���� ���� ���� ����
				FileSave(userlist, user_count);
				LeaveCriticalSection(&cs);

				ptr->state = INTRO;		// ���� ���� ( �ٽ� �α��� �޴��� )
				break;

				// �α׾ƿ� ó��
			case LOGOUT:
				EnterCriticalSection(&cs);
				for (int i = 0; i < user_count; i++) {
					if (!strcmp(userlist[i].id, ptr->user.id)) {
						userlist[i].state = false;		// ���� ���� ���� ����
						break;
					}
				}
				LeaveCriticalSection(&cs);

				ptr->state = INTRO;		// ���� ���� ( �ٽ� �α��� �޴� )
				break;
			}
			break;

			// ���� ���� ó��
		case S_FILE_INFO:
			// Ŭ���̾�Ʈ ���� ���� �ʱ�ȭ �� ���� ����
			ZeroMemory(&ptr->file, sizeof(_FileInfo));
			char filename[MSGSIZE];
			int filesize;

			// ���ε� ���� ���� �̸�, ���� ũ�⸦ UnPacking
			size = UnPackPacket(ptr->buf, filename, filesize);
			filename[size] = '\0';

			// ���� �̸� OVER SIZE
			if (size > MAX_FILENAME) {
				// ���� �̸� ����� ���� ����� �ʰ� �ߴٴ� ������ Packing
				size = PackPacket(ptr->buf, (FILE_PROTOCOL)FILE_TRANS_DENY, (FILE_RESULT)OVER_NAMESIZE, FILE_NAMEOVER_MSG);
				ptr->state = S_FILE_INTRO;		// ���� ����
			}
			// ���� ũ�� OVER SIZE
			else if (filesize > MAX_FILESIZE) {
				// ���� ũ�Ⱑ ���� ũ�⸦ �ʰ� �ߴٴ� ������ Packing
				size = PackPacket(ptr->buf, (FILE_PROTOCOL)FILE_TRANS_DENY, (FILE_RESULT)OVER_FILESIZE, FILE_SIZEOVER_MSG);
				ptr->state = S_FILE_INTRO;		// ���� ����
			}

			// ���� �̸��� ���� ũ���� ����� ���� ����� ���� �ʾҴٸ�
			else {
				while (1) {
					// ���� ���� ���ε� �� ������ �̹� �����Ѵٸ�
					if (SearchFile(filename)) {
						FILE* fp;
						// ���� �б� ���� ���� ���� ���� ���� �ִ� ������ ���κб��� ����
						fp = fopen(filename, "rb");
						fseek(fp, 0, SEEK_END);

						// ������ �� ��, ���� ������ ���� ������ ũ�⸦ ����
						int fsize;
						fsize = ftell(fp);
						fclose(fp);

						// ������ ������ �ִ� ������ ũ��� ���ε� �� ������ ũ�Ⱑ ���ٸ�
						if (filesize == fsize) {
							// �� �̻� ���� �ʿ䰡 ���� ������ ������ �̹� ���� �Ѵٴ� ������ Packing
							size = PackPacket(ptr->buf, (FILE_PROTOCOL)FILE_TRANS_DENY, (FILE_RESULT)OVERRAP, FILE_OVERRAP_MSG);
							ptr->state = S_FILE_INTRO;			// ���� ����

							EnterCriticalSection(&cs);
							// �ڽ��� ������ ���� Ŭ���̾�Ʈ �� �ڽŰ� ������ ������ ���ε� �Ϸ��� ��� ���� Ŭ���̾�Ʈ�� ã�´�. 
							for (int i = 0; i < client_count; i++) {
								// �ڽ��� �ƴϴ� && Ŭ���̾�Ʈ�� �ڽŰ� ���� ������ ���ε� �Ϸ��� �Ѵ� && Ŭ���̾�Ʈ�� ��� ���̴� 
								if (clientlist[i] != ptr && !strcmp(clientlist[i]->file.name, ptr->file.name) && clientlist[i]->file.overrap) {			
									// Ŭ���̾�Ʈ �̺�Ʈ Ȱ��ȭ
									SetEvent(clientlist[i]->hEvent);
									break;
								}
							}

							// �ڽ��� ���� ������ Ŭ���̾�Ʈ ����Ʈ���� �ʱ�ȭ ��, �ڽ��� ������ �ʱ�ȭ
							// ������ ���ε忡�� ��� ����(�����)�� ���ϱ� ���� �۾�
							for (int i = 0; i < client_count; i++) {
								if (clientlist[i] == ptr) {
									ZeroMemory(&clientlist[i]->file, sizeof(_FileInfo));
									ZeroMemory(&ptr->file, sizeof(_FileInfo));
									break;
								}
							}
							LeaveCriticalSection(&cs);
						}
						// ���� ������ ���� ������ ���ε� ���� ���� ũ�⺸�� �۴ٸ�
						else {
							// ���� �̸��� ���� ��ü ũ�� ����
							strcpy(ptr->file.name, filename);
							ptr->file.totalsize = filesize;
							// ���� ������ ���� ������ ũ�� ���� 
							ptr->file.nowsize = fsize;
						}
					}

					// ���� ���ε� ���� ������ ���� ���� ���ٸ�
					else {
						// ���� �̸�, ���� ��ü ũ�⸦ �����ϰ�, ���� ������ ���� ������ ũ��� 0 ����
						strcpy(ptr->file.name, filename);
						ptr->file.totalsize = filesize;
						ptr->file.nowsize = 0;
					}

					EnterCriticalSection(&cs);
					// ���� Ŭ���̾�Ʈ�� ����ϰ� ���� �ʴٴ� ���� �Ͽ�
					ptr->file.overrap = false;

					// ���� Ŭ���̾�Ʈ�� ��� ���ѿ��� �ɸ��� �ʾҴٸ�
					if (ptr->state != S_FILE_INTRO) {
						for (int i = 0; i < client_count; i++) {
							// �ڽ��� ���� && ������ Ŭ���̾�Ʈ �� �ڽŰ� ���� ������ ���ε� �� && �� Ŭ���̾�Ʈ�� ��� ���°� �ƴ�
							if (clientlist[i] != ptr && !strcmp(clientlist[i]->file.name, ptr->file.name) && !clientlist[i]->file.overrap) {
								// ���� Ŭ���̾�Ʈ �̺�Ʈ ��Ȱ��ȭ
								ResetEvent(ptr->hEvent);
								// ���� Ŭ���̾�Ʈ ��� ����
								ptr->file.overrap = true;
								break;
							}
						}
					}
					LeaveCriticalSection(&cs);

					// ���� Ŭ���̾�Ʈ�� ��� ���°� �Ǿ��ٸ�
					if (ptr->file.overrap) {
						printf("[%s / %d] Wait\n", inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));

						// ��� ���°� �Ǿ��ٴ� ������ Packing �� �۽�
						size = PackPacket(ptr->buf, (FILE_PROTOCOL)FILE_TRANS_WAIT, FILE_WAIT_MSG);
						retval = send(ptr->sock, ptr->buf, size, 0);
						if (retval == SOCKET_ERROR) {
							err_display("File Info in Wait Send");
							flag = true;
						}

						// ��� ����
						recived = WaitForSingleObject(ptr->hEvent, INFINITE);
						// Ÿ�̹��� ���߱� ���� ���� ĳ����
						Sleep(0);
						continue;
					}

					// ���� Ŭ���̾�Ʈ�� ��� ���°� �ƴϰ�, ��� ���ѿ��� �ɸ� ���� ���ٸ�
					else if (!ptr->file.overrap && ptr->state != S_FILE_INTRO) {
						printf("[%s / %d] ���� ���� �̸� : %s\n", inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port), filename);
						printf("[%s / %d] ���� ���� ũ�� : %d\n", inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port), filesize);

						// ���� �������� �ִ� ������ ũ��, �� ���ε带 ������ �������� ������ Packing
						size = PackPacket(ptr->buf, (FILE_PROTOCOL)FILE_TRANS_START_POINT, ptr->file.nowsize);
						ptr->state = S_FILE_TRANS_DATA;	// ���� ����
					}
					break;
				}
			}

			// Packing ������ �۽�
			retval = send(ptr->sock, ptr->buf, size, 0);
			if (retval == SOCKET_ERROR) {
				err_display("File Info Send");
				flag = true;
			}
			break;

			// ���� �۽� ó��
		case S_FILE_TRANS_DATA:
			FILE* fp;
			// ���� ���� ���� ���� ( ������ �ִ� ���Ͽ� �߰������� ���� ���� )
			fp = fopen(ptr->file.name, "ab");

			// ���� ������ �Ϸ�ǰų�, ���� ������ �ߴ��� ������ �ݺ�
			while (1) {
				// ���� ���� ����
				if (!PacketRecv(ptr->sock, ptr->buf)) {
					err_display("File Trans Data Recv");
					flag = true;
					break;
				}

				// �������� �б�
				file_protocol = (FILE_PROTOCOL)GetProtocol(ptr->buf);
				// ���� ���� �ߴ� ���������� ��� �ߴ�
				if (file_protocol == FILE_TRANS_STOP) {
					break;
				}

				// ���� ������ �� ���ۿ� �Ѱ� ���� ���� ���� ũ��
				char buf[BUFSIZE];
				int trans_size;

				// ���� ũ��� ���� ������ UnPacking
				UnPackPacket(ptr->buf, trans_size, buf);
				// UnPacking �� ������ ���Ͽ� ����
				fwrite(buf, 1, trans_size, fp);
				if (ferror(fp)) {
					perror("���� ����� ����");
					break;
				}

				// ���� ���ε� �� ũ�⸦ ������ ���� ������ ���� ũ�⿡ �߰�
				ptr->file.nowsize += trans_size;

				// ���ε� ���� ������ ��ü ũ��� ������ ���� ������ ���� ũ�Ⱑ ���ٸ� ���� �Ϸ��̱⿡ �ߴ�
				if (ptr->file.totalsize == ptr->file.nowsize) {
					printf("���� ���� �Ϸ�\n");
					break;
				}
			}
			// ���� �ݱ�
			fclose(fp);

			// ���� ���ε尡 ���� �ߴ� �Ǿ���, ������ �Ϸ� �Ǿ��� �� �۾��� ��ȿ�ϰ� �̷����
			EnterCriticalSection(&cs);
			for (int i = 0; i < client_count; i++) {
				// �ڽ��� �ƴϴ� && ���ӵ� Ŭ���̾�Ʈ �� �ڽŰ� ���� ������ ���ε� �Ϸ��� �Ѵ�
				if (clientlist[i] != ptr && !strcmp(clientlist[i]->file.name, ptr->file.name)) {
					// �ڽ��� �̹� ���ε� �۾��� ������ ������ ��� ���� Ŭ���̾�Ʈ�� �̺�Ʈ�� Ȱ��ȭ ��Ų��.
					SetEvent(clientlist[i]->hEvent);
					break;
				}
			}

			// �ڽ��� ������ �ʱ�ȭ
			for (int i = 0; i < client_count; i++) {
				if (clientlist[i] == ptr) {
					ZeroMemory(&clientlist[i]->file, sizeof(_FileInfo));
					break;
				}
			}
			LeaveCriticalSection(&cs);

			ptr->state = S_FILE_INTRO;		// ���� ����
			break;
		}	// switch end
		
		// while Ż��
		if (flag) { 
			// Ŭ���̾�Ʈ���� ������ �۾��� �ѱ�� ���� ����
			if (!PacketRecv(ptr->sock, ptr->buf)) {
				break;
			}
		}
	}

	// ������ ���� ���� Ȯ���ϰ� ���� Ŭ���̾�Ʈ�� ���� ���¸� ���� ��Ŵ
	EnterCriticalSection(&cs);
	for (int i = 0; i < user_count; i++) {
		if (!strcmp(userlist[i].id, ptr->user.id)) {
			userlist[i].state = false;
		}
	}
	LeaveCriticalSection(&cs);

	// Ŭ���̾�Ʈ ���� ����
	RemoveClientInfo(ptr);
	return 0;
}

// ���� ã�� �Լ�

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

// ���� �Լ� ( ���� )

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

// ���� �Լ� ( ��� )

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

// ���� �ʱ�ȭ �Լ�

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

// ����� ���� recv �Լ�

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

// Packing �� ���� recv �Լ�

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

// �������� �б� �Լ�

int GetProtocol(const char* _ptr)
{
	int protocol;
	memcpy(&protocol, _ptr, sizeof(int));

	return protocol;
}

// Ŭ���̾�Ʈ �߰� �Լ�

_ClientInfo* AddClientInfo(SOCKET _sock, SOCKADDR_IN _addr)
{
	EnterCriticalSection(&cs);

	_ClientInfo* ptr = new _ClientInfo;			// ��ü ����
	ZeroMemory(ptr, sizeof(_ClientInfo));		// �ʱ�ȭ
	ptr->sock = _sock;									// ���� ����
	memcpy(&ptr->addr, &_addr, sizeof(SOCKADDR_IN));	// ��巹�� ����

	// ����,���� ���� �ʱ�ȭ
	ZeroMemory(&ptr->user, sizeof(_UserInfo));
	ZeroMemory(&ptr->file, sizeof(_FileInfo));

	ptr->state = INIT;	// ���� �ʱ�ȭ
	ptr->hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);	// �̺�Ʈ �ʱ�ȭ ( ���� )

	clientlist[client_count++] = ptr;	// Ŭ���̾�Ʈ ����Ʈ ����

	LeaveCriticalSection(&cs);

	printf("\n[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
		inet_ntoa(ptr->addr.sin_addr), ntohs(ptr->addr.sin_port));

	return ptr;
}

// Ŭ���̾�Ʈ ���� �Լ�

void RemoveClientInfo(_ClientInfo* _ptr)
{
	printf("\n[TCP ����] Ŭ���̾�Ʈ ����: IP �ּ�=%s, ��Ʈ ��ȣ=%d\n",
		inet_ntoa(_ptr->addr.sin_addr), ntohs(_ptr->addr.sin_port));

	EnterCriticalSection(&cs);

	for (int i = 0; i < client_count; i++)
	{
		if (clientlist[i] == _ptr)
		{
			closesocket(clientlist[i]->sock);			// ���� �ݱ�
			CloseHandle(clientlist[i]->hEvent);		// �̺�Ʈ �ݱ�

			delete clientlist[i];									// Ŭ���̾�Ʈ ����Ʈ���� ����

			// Ŭ���̾�Ʈ ����Ʈ���� ���� �� ���� ä��� 
			for (int j = i; j < client_count - 1; j++)
			{
				clientlist[j] = clientlist[j + 1];
			}
			break;
		}
	}

	client_count--;		// Ŭ���̾�Ʈ �� ����
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

// ���� ����,�б� �Լ�

void FileSave(_UserInfo* user, int count) {
	FILE* fp = nullptr;

	// ���� ���� ���� ����
	fp = fopen("UserData.txt", "w");
	if (fp == nullptr) return;

	if (fp) {
		// ���� ������ ������ �ٷ� ����
		if (count == 0) {
			fclose(fp);
			return;
		}

		// ���� ���� ���� �۾�
		fprintf(fp, "%d\n", count);
		for (int i = 0; i < count; i++) {
			fprintf(fp, "%s %s\n", user[i].id, user[i].pw);
		}
	}
	fclose(fp);
}

bool FileLoad(_UserInfo* user, int& count) {
	FILE* fp = nullptr;

	// ���� �б� ���� ����
	fp = fopen("UserData.txt", "r");
	if (fp == nullptr) return false;


	// ������ �� �������� �о� ���� ������ �ҷ�����
	if (!feof(fp)) {
		fscanf_s(fp, "%d", &count);

		for (int i = 0; i < count; i++) {
			fscanf_s(fp, "%s %s", user[i].id, sizeof(user[i].id), user[i].pw, sizeof(user[i].pw));
		}
	}

	fclose(fp);
	return true;
}
