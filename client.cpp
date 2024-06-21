// to include most c++ lib
#include <bits/stdc++.h>
// for system and socket-related operations
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
// for handling errors
#include <errno.h>
// for string func
#include <string.h>
// for Internet address manipulation
#include <arpa/inet.h>
// for POSIX operating system API
#include <unistd.h>
// for threads
#include <thread>
// for signals
#include <signal.h>
// for mutexs to control threads
#include <mutex>
#include <fcntl.h>	// For open function and file control options
#include <unistd.h> // For close function
#include <dirent.h>
#include <regex>
#include <string>

using namespace std;
#define MAX_LEN 200
#define NUM_COLORS 6
#define MAXBUF 1024

using namespace std;

bool exit_flag = false;
thread t_send, t_recv;
int client_socket;
string def_col = "\033[0m";
string colors[] = {"\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m"};
mutex file_mtx;
void catch_ctrl_c(int signal);
string color(int code);
int eraseText(int cnt);
void send_message(int client_socket);
void recv_message(int client_socket);
void file_send(int client_socket);
void rec_send(int client_socket);
void file_list(int client_socket);
void file_rec(int client_socket, const string &fileName);

bool isNullTerminated(const char *buffer, int length)
{
	for (int i = 0; i < length; ++i)
	{
		if (buffer[i] == '\0')
		{
			return true;
		}
	}
	return false;
}
int main()
{
	if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket: ");
		exit(-1);
	}

	struct sockaddr_in client;
	client.sin_family = AF_INET;
	client.sin_port = htons(9000); // Port no. of server
	client.sin_addr.s_addr = INADDR_ANY;
	// client.sin_addr.s_addr=inet_addr("127.0.0.1"); // Provide IP address of server
	bzero(&client.sin_zero, 0);

	if ((connect(client_socket, (struct sockaddr *)&client, sizeof(struct sockaddr_in))) == -1)
	{
		perror("connect: ");
		exit(-1);
	}

	signal(SIGINT, catch_ctrl_c); // calls catch_ctrl_c func when ctrl+c is pressed
	char name[MAX_LEN];
	cout << "Enter your name : ";
	cin.getline(name, MAX_LEN);
	send(client_socket, name, sizeof(name), 0);

	cout << colors[NUM_COLORS - 1] << "\n\t  ====== Welcome to the chat-room ======   " << endl
		 << def_col;

	thread t1(send_message, client_socket);
	thread t2(recv_message, client_socket);

	t_send = move(t1);
	t_recv = move(t2);

	if (t_send.joinable())
		t_send.join();
	if (t_recv.joinable())
		t_recv.join();

	return 0;
}

// Handler for "Ctrl + C"
void catch_ctrl_c(int signal)
{
	char str[MAX_LEN] = "#exit";
	send(client_socket, str, sizeof(str), 0);
	exit_flag = true;
	t_send.detach();
	t_recv.detach();
	close(client_socket);
	exit(signal);
}

string color(int code)
{
	return colors[code % NUM_COLORS];
}

// Erase text from terminal
int eraseText(int cnt)
{
	char back_space = 8;
	for (int i = 0; i < cnt; i++)
	{
		cout << back_space;
	}
	return 0;
}

// Send message to everyone
void send_message(int client_socket)
{
	while (1)
	{
		cout << colors[1] << "You : " << def_col;
		char str[MAX_LEN];
		cin.getline(str, MAX_LEN);
		send(client_socket, str, sizeof(str), 0);
		if (strcmp(str, "$exit") == 0) // comparing str with #exit
		{
			cout << "Leaving Chat Room..." << endl;
			exit_flag = true;
			t_recv.detach(); // ends threads
			close(client_socket);
			return;
		}
		else if (strcmp(str, "$ft") == 0)
		{
			file_send(client_socket);
		}
		else if (strcmp(str, "$rt") == 0)
		{
			char buffer[MAXBUF];
			recv(client_socket, buffer, sizeof(buffer), 0);
			cout << "File list from server:\n"
				 << buffer << endl;

			// User enters the file name to download
			string requestedFile;
			cout << "Enter the name of the file you want to download: ";
			cin >> requestedFile;

			// Example: Sending the requested file name to the server
			send(client_socket, requestedFile.c_str(), requestedFile.size(), 0);

			// Receive the file from the server
			file_rec(client_socket, requestedFile);
			memset(buffer, 0, MAXBUF);
		}
		eraseText(6);
	}
}

// Receive message
void recv_message(int client_socket)
{
	while (1)
	{
		if (exit_flag)
			return;
		char name[MAX_LEN], str[MAX_LEN];
		int color_code;
		int bytes_received = recv(client_socket, name, sizeof(name), 0);
		if (bytes_received <= 0)
			continue;
		recv(client_socket, &color_code, sizeof(color_code), 0);
		recv(client_socket, str, sizeof(str), 0);
		eraseText(6);
		if (strcmp(name, "#NULL") != 0)
			cout << color(color_code) << name << " : " << def_col << str << endl;
		else
			cout << color(color_code) << str << endl;
		cout << colors[1] << "You : " << def_col;
		fflush(stdout); // clears bufffer
	}
}

void file_list(int client_socket)
{
	char buffer[MAXBUF] = {0};
	recv(client_socket, buffer, sizeof(buffer), 0);
	cout << "Files in directory:\n"
		 << buffer << endl;
	cout << "File in directory received : " << endl;
	memset(buffer, 0, MAXBUF);
}

void file_rec(int client_socket, const string &filename)
{
	ofstream file(filename, ios::binary);

	if (!file.is_open())
	{
		cerr << "Error: Unable to create file " << filename << endl;
		return;
	}

	int fileSize;
	recv(client_socket, reinterpret_cast<char *>(&fileSize), sizeof(fileSize), 0);

	char buffer[MAXBUF];
	int totalReceived = 0;

	while (totalReceived < fileSize)
	{
		int bytesRead = recv(client_socket, buffer, sizeof(buffer), 0);
		totalReceived += bytesRead;
		file.write(buffer, bytesRead);
		memset(buffer, 0, MAXBUF);
	}

	file.close();
	cout << "File Recieved." << endl;
}

void file_send(int client_socket)
{

	char buf[MAXBUF];
	int file_name_len, source_fd, read_len;
	int filefd;

	// Notify the server that a file is being sent
	strcpy(buf, "FILE_TRANSFER_REQUEST");
	send(client_socket, buf, strlen(buf), 0);

	memset(buf, 0, MAXBUF);

	// Get the file name from the client to send
	cout << "> Write file name to send: ";
	cin.getline(buf, MAXBUF);

	send(client_socket, buf, strlen(buf), 0);

	// Open the file for reading
	filefd = open(buf, O_RDONLY);
	if (filefd < 0)
	{
		perror("Error opening file");
		return;
	}
	memset(buf, 0, MAXBUF);
	// Send the entire file
	while (true)
	{
		memset(buf, 0, MAXBUF);

		// Read file into the buffer
		read_len = read(filefd, buf, MAXBUF);

		// Send file to the server
		send(client_socket, buf, read_len, 0);

		if (read_len == 0)
		{
			cout << "> File was sent\n";
			memset(buf, 0, MAXBUF);
			// After sending the file content
			send(client_socket, "eof", strlen("eof"), 0);
			memset(buf, 0, MAXBUF);
			break;
		}
	}

	close(filefd);

	memset(buf, 0, MAXBUF);
}
