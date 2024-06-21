#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <fcntl.h>	  // For open function and file control options
#include <sys/stat.h> // For file permission constants
#include <dirent.h>
using namespace std;
#define MAX_LEN 200
#define NUM_COLORS 6
#define MAXBUF 1024

using namespace std;

struct terminal
{
	int id;
	string name;
	int socket;
	thread th;
};

vector<terminal> clients;
string def_col = "\033[0m";
string colors[] = {"\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m"};
int seed = 0;
mutex cout_mtx, clients_mtx, file_mtx; // creating mutex so that only one thread can access them at a time

string color(int code);
void set_name(int id, char name[]);
void shared_print(string str, bool endLine);
int broadcast_message(string message, int sender_id);
int broadcast_message(int num, int sender_id);
void end_connection(int id);
void handle_client(int client_socket, int id);
void file_rec(int client_sockfd, int id);
void file_send(int client_sockfd, const string &fileName);
void sendFileList(int clientSocket);

bool containsAny(char buf[], char end[])
{
	for (int i = 0; end[i] != '\0'; i++)
	{
		for (int j = 0; buf[j] != '\0'; j++)
		{
			if (end[i] == buf[j])
			{
				return true;
			}
		}
	}
	return false; // No matching character found
}

int main()
{
	int server_socket;
	if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("socket: ");
		exit(-1);
	}

	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(9000);
	server.sin_addr.s_addr = INADDR_ANY;
	bzero(&server.sin_zero, 0);

	if ((bind(server_socket, (struct sockaddr *)&server, sizeof(struct sockaddr_in))) == -1)
	{
		perror("bind error: ");
		exit(-1);
	}

	if ((listen(server_socket, 8)) == -1) // max 8 connections can be made
	{
		perror("listen error: ");
		exit(-1);
	}

	struct sockaddr_in client;
	int client_socket;
	unsigned int len = sizeof(sockaddr_in);

	cout << colors[NUM_COLORS - 1] << "\n\t  ====== Welcome to the chat-room ======   " << endl
		 << def_col;

	while (1)
	{
		if ((client_socket = accept(server_socket, (struct sockaddr *)&client, &len)) == -1)
		{
			perror("accept error: ");
			exit(-1);
		}
		seed++;
		thread t(handle_client, client_socket, seed);
		lock_guard<mutex> guard(clients_mtx);
		clients.push_back({seed, string("Anonymous"), client_socket, (move(t))});
	}

	for (int i = 0; i < clients.size(); i++)
	{
		if (clients[i].th.joinable())
			clients[i].th.join();
	}

	close(server_socket);
	return 0;
}

string color(int code)
{
	return colors[code % NUM_COLORS];
}

// Set name of client
void set_name(int id, char name[])
{
	for (int i = 0; i < clients.size(); i++)
	{
		if (clients[i].id == id)
		{
			clients[i].name = string(name);
		}
	}
}

// For synchronisation of cout statements
void shared_print(string str, bool endLine = true)
{
	lock_guard<mutex> guard(cout_mtx);
	cout << str;
	if (endLine)
		cout << endl;
}

// Broadcast message to all clients except the sender
int broadcast_message(string message, int sender_id)
{
	char temp[MAX_LEN];
	strcpy(temp, message.c_str()); // c_str converts string to char array , strcpy to copy string
	for (int i = 0; i < clients.size(); i++)
	{
		if (clients[i].id != sender_id)
		{
			send(clients[i].socket, temp, sizeof(temp), 0);
		}
	}
	return 0;
}

// Broadcast a number to all clients except the sender
int broadcast_message(int num, int sender_id)
{
	for (int i = 0; i < clients.size(); i++)
	{
		if (clients[i].id != sender_id)
		{
			send(clients[i].socket, &num, sizeof(num), 0);
		}
	}
	return 0;
}

void end_connection(int id)
{
	for (int i = 0; i < clients.size(); i++)
	{
		if (clients[i].id == id)
		{
			lock_guard<mutex> guard(clients_mtx);
			clients[i].th.detach();
			clients.erase(clients.begin() + i);
			close(clients[i].socket);
			break;
		}
	}
}

void handle_client(int client_socket, int id)
{
	char name[MAX_LEN], str[MAX_LEN];
	recv(client_socket, name, sizeof(name), 0);
	set_name(id, name);

	// Display welcome message
	string welcome_message = string(name) + string(" has joined");
	broadcast_message("#NULL", id);
	broadcast_message(id, id);
	broadcast_message(welcome_message, id);
	shared_print(color(id) + welcome_message + def_col);

	while (1)
	{
		int bytes_received = recv(client_socket, str, sizeof(str), 0);
		if (bytes_received <= 0)
			return;

		if (strcmp(str, "$exit") == 0)
		{
			// Display leaving message
			string message = string(name) + string(" has left");
			broadcast_message("#NULL", id);
			broadcast_message(id, id);
			broadcast_message(message, id);
			shared_print(color(id) + message + def_col);
			end_connection(id);
			return;
		}

		broadcast_message(string(name), id);
		broadcast_message(id, id);
		broadcast_message(string(str), id);
		shared_print(color(id) + name + " : " + def_col + str);

		if (strcmp(str, "$ft") == 0)
		{
			file_rec(client_socket, id);
			fflush(stdout);
		}
		else if (strcmp(str, "$rt") == 0)
		{
			sendFileList(client_socket);
			char buffer[MAXBUF];
			recv(client_socket, buffer, sizeof(buffer), 0);
			string requestedFile = buffer;
			memset(buffer, 0, MAXBUF);
			file_send(client_socket, requestedFile);
			fflush(stdout);
		}
	}
}

vector<string> getFilesInDirectory(const char *path)
{
	vector<string> files;
	DIR *dir;
	struct dirent *entry;

	if ((dir = opendir(path)) != nullptr)
	{
		while ((entry = readdir(dir)) != nullptr)
		{
			if (entry->d_type == DT_REG)
			{
				files.push_back(entry->d_name);
			}
		}
		closedir(dir);
	}

	return files;
}

void sendFileList(int clientSocket)
{
	string directoryPath = "/home/uzair/Desktop/Computer Network/CN project sol/Final Step/ser/"; // You can change this to the desired directory
	vector<string> files = getFilesInDirectory(directoryPath.c_str());

	stringstream fileList;
	for (const auto &file : files)
	{
		fileList << file << "\n";
	}

	string fileListStr = fileList.str();
	send(clientSocket, fileListStr.c_str(), fileListStr.size(), 0);
}

void file_send(int client_socket, const string &filename)
{
	ifstream file(filename, ios::binary);

	if (!file.is_open())
	{
		cerr << "Error: Unable to open file " << filename << endl;
		return;
	}

	file.seekg(0, ios::end);
	int fileSize = file.tellg();
	file.seekg(0, ios::beg);

	send(client_socket, reinterpret_cast<char *>(&fileSize), sizeof(fileSize), 0);

	char buffer[MAXBUF];
	while (!file.eof())
	{
		file.read(buffer, sizeof(buffer));
		send(client_socket, buffer, file.gcount(), 0);
		memset(buffer, 0, MAXBUF);
	}

	file.close();
}

void file_rec(int client_socket, int id)
{

	cout << "In func now " << endl;
	char buf[MAXBUF];
	int file_name_len, dest_fd, read_len;

	// Receive notification that a file is being sent
	recv(client_socket, buf, sizeof(buf), 0);
	memset(buf, 0, MAXBUF);

	// Receive the file name from the client
	recv(client_socket, buf, sizeof(buf), 0);
	// cout << endl;

	cout << "Receiving file: " << buf << endl;

	// Open the file for writing
	dest_fd = open(buf, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	cout << "File open for writing" << endl;
	if (dest_fd < 0)
	{
		perror("Error opening file for writing");
		return;
	}
	// char end[4] = "eof";
	//  Receive and write the file content
	std::regex nullRegex("eof");
	while (true)
	{
		memset(buf, 0, MAXBUF);

		// Receive file content from the client
		read_len = recv(client_socket, buf, sizeof(buf), 0);

		if (read_len < 0)
		{
			perror("Error receiving file content");
			break;
		}

		// Write to the destination file
		if (std::regex_search(buf, nullRegex))
		{
			printf("File received successfully\n");
			break;
		}
		write(dest_fd, buf, read_len);

		// End of file reached
		if (read_len == 0)
		{
			printf("File received successfully.\n");
			break;
		}
	}

	// Close the destination file
	close(dest_fd);
}
