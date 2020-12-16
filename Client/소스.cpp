// argv[1] = tera.jpg 
// argv[2] = mabinogi.jpg
// argv[3] = �����̸����ִ�ũ�⸦�Ѿ����츦üũ�ϱ����������Դϴٻ�������50����Ʈ��ũ�Ⱑ����ϱ���.txt

#pragma comment(lib, "ws2_32.lib")
#include <winsock2.h>
#include <stdio.h>

#include <conio.h>

// ��� ��ũ��

#define SERVER_PORT 9000
#define SERVER_IP "127.0.0.1"
#define BUFSIZE 4096
#define MAX_FILE_READ_SIZE 1024
#define FILE_NAME_SIZE 128
#define MSGSIZE 512

// �α���,���� ����� ���� ������

enum RESULT { NODATA = -1, ID_OVERRAP = 1, ID_ERROR, PW_ERROR, LOGIN_OVERRAP, SUCCESS, LOGIN };
enum FILE_RESULT { OVER_NAMESIZE = 1, OVER_FILESIZE, OVERRAP, COMPLETE };

// �α���, ���� �������� ������

enum LOGIN_PROTOCOL { LOGIN_INTRO = 1, NEW, LOGIN_INFO, LOGIN_RESULT, LOGIN_EXIT };
enum FILE_PROTOCOL {
	FILE_INTRO = 1, FILE_INFO, FILE_TRANS_DENY, FILE_TRANS_START_POINT, FILE_TRANS_WAIT,
	FILE_TRANS_DATA, FILE_TRANS_STOP, REMOVE, LOGOUT
};

// ���� ���� ����ü

struct _FileInfo {
	char name[50];		// ���� �̸�
	int totalsize;				// ���� ��ü ũ��
	int nowsize;				// ���� ���ε� �� ���� ũ��
}FileInfo;

// ���� ���� ����ü

struct _UserInfo {
	char id[50];				// ���� ID
	char pw[50];				// ���� PW
	bool state;				// ���� ���� ���� ( ���⼭�� ������ ���� )
}user;

// ���� �Լ�

void err_quit(const char *msg);																						// ���� �Լ� ( ���� )
void err_display(const char *msg);																					// ���� �Լ� ( ��� )

SOCKET socket_init();																									// ���� �ʱ�ȭ

int recvn(SOCKET s, char *buf, int len, int flags);															// ����� ���� Recv
bool PacketRecv(SOCKET _sock, char* _buf);																// Packing ���� Recv
int GetProtocol(const char* _ptr);																					// �������� �б�

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

	// WSA �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;

	// Socket �ʱ�ȭ
	SOCKET sock = socket_init();

	// ���� ���� �ʱ�ȭ
	ZeroMemory(&FileInfo, sizeof(FileInfo));

	// ����
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
		// ������ ���� ���
		// ���� �޴� �����͸� �������� ����
		if (!PacketRecv(sock, buf)) {
			err_display("Recv");
			break;
		}

		// ���� Ŭ���̾�Ʈ�� �α��� ���°� �ƴ϶��
		if (login_result != LOGIN) {
			// �������� �б�
			login_protocol = (LOGIN_PROTOCOL)GetProtocol(buf);

			// �������ݿ� ���� �۾�
			switch (login_protocol) {
				// �α��� �޴� ����
			case LOGIN_INTRO:
				// ���� ���� �ʱ�ȭ
				ZeroMemory(&user, sizeof(_UserInfo));
				char num[MSGSIZE];

				// �޴� ���� UnPacking
				size = UnPackPacket(buf, msg);
				msg[size] = '\0';

				// �޴� ���� ó���� ���� �ݺ���
				while (1) {
					printf("%s", msg);
					scanf("%s", num);

					num[1] = '\0';

					switch (atoi(num)) {
						// ȸ�� ����
					case 1:
						printf("\n\nID : ");
						scanf("%s", user.id);

						printf("\nPW : ");
						scanf("%s", user.pw);

						// ȸ������ ������ �Էµ� ID,PW ������ Packing
						size = PackPacket(buf, (LOGIN_PROTOCOL)NEW, user.id, user.pw);
						break;

						// �α���
					case 2:
						printf("\n\nID : ");
						scanf("%s", user.id);

						printf("\nPW : ");
						scanf("%s", user.pw);

						// �α��� ������ �Էµ� ID,PW ������ Packing
						size = PackPacket(buf, (LOGIN_PROTOCOL)LOGIN_INFO, user.id, user.pw);
						break;

						// ���α׷� ����
					case 3:
						// ���α׷� ���� �������� Packing
						size = PackPacket(buf, (LOGIN_PROTOCOL)LOGIN_EXIT);
						flag = true;	// �ݺ��� Ż��
						break;

						// �޴� �̿��� �Է¿� ���� ���� ó��
					default:
						system("cls");
						continue;
					}
					break;
				}

				// Packing �� �����͸� �۽�
				retval = send(sock, buf, size, 0);
				if (retval == SOCKET_ERROR) {
					err_display("send");
					flag = true;
				}
				break;

				// �α��� ó�� ���
			case LOGIN_RESULT:
				// �α��� ����� �׿� ���� ������ UnPacking
				size = UnPackPacket(buf, (int&)login_result, msg);
				msg[size] = '\0';

				printf("%s", msg);

				if (getch())
					system("cls");

				break;

			} // switch end
		}

		// ���� Ŭ���̾�Ʈ�� �α��� ���¶��
		else {
			// �������� �б�
			file_protocol = (FILE_PROTOCOL)GetProtocol(buf);

			// �������ݿ� ���� �۾�
			switch (file_protocol) {
				// ���� �޴� ����
			case FILE_INTRO:
				char num[MSGSIZE];

				// �޴� ���� UnPacking
				size = UnPackPacket(buf, msg);
				msg[size] = '\0';

				// ���� ó���� ���� �ݺ���
				while (1) {
					printf("%s", msg);
					scanf("%s", num);

					num[1] = '\0';

					switch (atoi(num)) {
						// ���� �۽�
					case 1:
						int index;
						index = 1;

						while (1) {
							system("cls");
							printf("[ ���� ��� ]\n");
							printf("���� �� �ִ� ������ �ִ� ũ�� : 20 kb\n\n");

							char temp[10][MSGSIZE];

							if (argv[1] == nullptr) {
								strcpy(temp[1], "tera.jpg");
								strcpy(temp[2], "mabinogi.jpg");
								strcpy(temp[3], "�����̸����ִ�ũ�⸦�Ѿ����츦üũ�ϱ����������Դϴٻ�������50����Ʈ��ũ�Ⱑ����ϱ���.txt");

								while (index != 4) {
									printf("%d. %s\n", index, temp[index]);
									index++;
								}
							}

							else {
								// Ŭ���̾�Ʈ�� ������ �ִ� ���� ��� ���
								while (argv[index] != nullptr) {
									printf("%d. %s\n", index, argv[index]);
									index++;
								}
							}

							printf("\n���� : ");
							scanf("%s", msg);

							// ���� ó��
							if (atoi(msg) <= 0 || atoi(msg) > index) {
								continue;
							}

							// ������ ���� �̸� ����
							if (argv[1] == nullptr)
								strcpy(FileInfo.name, temp[atoi(msg)]);
							else
								strcpy(FileInfo.name, argv[atoi(msg)]);

							// ���� �б� ���� ����
							fp = fopen(FileInfo.name, "rb");
							if (!fp) {
								err_quit("File Open Error");						
							}

							// ������ ��, ���� ��ü ũ�⸦ �о� �����Ѵ�
							fseek(fp, 0, FILE_END);
							FileInfo.totalsize = ftell(fp);
							fclose(fp);

							// �о� �� ������ ������ Packing
							size = PackPacket(buf, (FILE_PROTOCOL)FILE_INFO, FileInfo.name, FileInfo.totalsize);
							break;
						}
						break;

						// ȸ��Ż��
					case 2:
						// ȸ��Ż�� �������� Packing
						size = PackPacket(buf, (FILE_PROTOCOL)REMOVE);
						login_result = NODATA;	// �α��� ��� �ʱ�ȭ
						system("cls");
						break;

						// �α׾ƿ�
					case 3:
						// �α׾ƿ� �������� Packing
						size = PackPacket(buf, (FILE_PROTOCOL)LOGOUT);
						login_result = NODATA;	// �α��� ��� �ʱ�ȭ
						system("cls");
						break;

						// �޴� �̿��� �Է¿� ���� ���� ó��
					default:
						system("cls");
						continue;
					} // while end
					break;
				} // switch end

				// Packing ������ �۽�
				retval = send(sock, buf, size, 0);
				if (retval == SOCKET_ERROR) {
					err_display("send");
					flag = true;
				}
				break;

				// ���� �۽� ���� ( ���� �̸�, ���� ũ�� �� ���ѿ� �ɸ� ��� )
			case FILE_TRANS_DENY:
				// ���� ������ ���� ���� UnPacking
				size = UnPackPacket(buf, (int&)file_result, msg);
				msg[size] = '\0';

				printf("\n%s", msg);

				if (getch())
					system("cls");
				break;

				// ���� �۽� ��� ( ������ Ŭ���̾�Ʈ �� �ڽŰ� ���� ������ ���ε��ϰ� �ִ� ��� )
			case FILE_TRANS_WAIT:
				// ����Ѵٴ� ���� UnPacking
				size = UnPackPacket(buf, msg);
				msg[size] = '\0';

				printf("\n%s", msg);
				break;

				// ���� �۽� ó��
			case FILE_TRANS_START_POINT:
				// ���� �۽� ������ UnPacking
				UnPackPacket(buf, FileInfo.nowsize);

				printf("\n\n[ ���� ���� ���� �� ]\n");
				printf("�ƹ� Ű�� ������ ������ ��ҵ˴ϴ�.\n\n");

				// ���� �б� ���� ����
				fp = fopen(FileInfo.name, "rb");
				// ���� �۽� ���������� ������ ������ �б�
				fseek(fp, FileInfo.nowsize, SEEK_SET);

				while (1) {
					// �۾� �帧�� ������� �ƹ� Ű�� ���� ���
					if (kbhit() == 1) {
						// ���� �۽� �ߴ� �������� Packing �� �۽�
						size = PackPacket(buf, (FILE_PROTOCOL)FILE_TRANS_STOP);
						retval = send(sock, buf, size, 0);
						if (retval == SOCKET_ERROR) {
							err_display("send");
							break;
						}
						// �ݺ��� Ż��
						break;
					}

					// ���� ����, ���� ũ�� ������ ����
					char f_buf[BUFSIZE];
					int trans_size;

					// MAX_FILE_READ_SIZE ��ŭ�� ������ ũ�� �о� ���� ���ۿ� �����ϰ�, ũ�⸦ ������ ����
					trans_size = fread(f_buf, 1, MAX_FILE_READ_SIZE, fp);

					// ���� ������ �� ������ �ʾ��� ���
					if (trans_size > 0) {
						// ���� ���� ���� ũ��� ���� ���۸� Packing �� �۽�
						size = PackPacket(buf, (FILE_PROTOCOL)FILE_TRANS_DATA, trans_size, f_buf);
						retval = send(sock, buf, size, 0);
						if (retval == SOCKET_ERROR) {
							err_display("send");
							break;
						}

						// ���� ���� ����
						FileInfo.nowsize += trans_size;
						printf("��");
						// ���� ��Ȳ�� ���� �˻��ϱ� ���� Sleep
						Sleep(1000);
					}
					
					// �۽��� ���� ũ�Ⱑ 0 �̰�, ���� ���� ������ ũ��� ���� ��ü ũ�Ⱑ ���� ���
					else if (trans_size == 0 && FileInfo.nowsize == FileInfo.totalsize) {
						printf("\n\n���� ���� �Ϸ� : %d byte\n", FileInfo.totalsize);
						break;	// �ݺ��� Ż��
					}
					// ���� ó��
					else {
						perror("���� ����� ����");
						break;	// �ݺ��� Ż��
					}
				}

				// ���� �ݱ�
				fclose(fp);

				if (getch())
					system("cls");
				break;

			} // switch end
		}

		// �ݺ��� Ż�� ( ���α׷� ���� )
		if (flag) break;
	} // while end

	// ������ �۾�
	if (fp != nullptr) fclose(fp);

	closesocket(sock);
	WSACleanup();
	return 0;
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

// ���� �ʱ�ȭ

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

// ����� ���� Recv

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

// Packing ���� Recv

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

// �������� �б�

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
