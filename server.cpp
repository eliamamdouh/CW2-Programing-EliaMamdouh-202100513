#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <fstream> // Added for file operations

using namespace std;

#define MAX_LEN 200
#define NUM_COLORS 6

struct terminal {
    int id;
    string name;
    int socket;
    thread th;
};

vector<terminal> clients;
string def_col = "\033[0m";
string colors[] = {"\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m"};
int seed = 0;
mutex cout_mtx, clients_mtx;

string color(int code);
void set_name(int id, const char name[]);
void shared_print(const string& str, bool endLine = true);
int broadcast_message(const string& message, int sender_id);
int broadcast_message(int num, int sender_id);
void end_connection(int id);
void handle_client(int client_socket, int id);
bool authenticate_user(const string& username, const string& password); // Function to authenticate user
bool signup_user(const string& username, const string& password); // Function to sign up new user

int main() {
    int server_socket;
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(10000);
    server.sin_addr.s_addr = INADDR_ANY;
    memset(server.sin_zero, '\0', sizeof server.sin_zero);

    if (bind(server_socket, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 8) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    cout << colors[NUM_COLORS - 1] << "\n\t  ====== Welcome to the chat-room ======   " << endl << def_col;

    while (true) {
        struct sockaddr_in client;
        socklen_t len = sizeof(sockaddr_in);
        int client_socket = accept(server_socket, (struct sockaddr *)&client, &len);
        if (client_socket == -1) {
            perror("accept");
            continue;
        }
        seed++;
        thread t(handle_client, client_socket, seed);
        {
            lock_guard<mutex> guard(clients_mtx);
            clients.push_back({seed, "Anonymous", client_socket, move(t)});
        }
    }

    for (int i = 0; i < clients.size(); i++) {
        if (clients[i].th.joinable())
            clients[i].th.join();
    }

    close(server_socket);
    return 0;
}

string color(int code) {
    return colors[code % NUM_COLORS];
}

void set_name(int id, const char name[]) {
    lock_guard<mutex> guard(clients_mtx);
    for (auto& client : clients) {
        if (client.id == id) {
            client.name = name;
            break;
        }
    }
}

void shared_print(const string& str, bool endLine) {
    lock_guard<mutex> guard(cout_mtx);
    cout << str;
    if (endLine)
        cout << endl;
}

int broadcast_message(const string& message, int sender_id) {
    char temp[MAX_LEN];
    strcpy(temp, message.c_str());
    for (const auto& client : clients) {
        if (client.id != sender_id) {
            send(client.socket, temp, sizeof(temp), 0);
        }
    }
    return 0; // Placeholder return value
}

int broadcast_message(int num, int sender_id) {
    for (const auto& client : clients) {
        if (client.id != sender_id) {
            send(client.socket, &num, sizeof(num), 0);
        }
    }
    return 0; // Placeholder return value
}

void end_connection(int id) {
    lock_guard<mutex> guard(clients_mtx);
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (it->id == id) {
            it->th.detach();
            close(it->socket);
            clients.erase(it);
            break;
        }
    }
}

void handle_client(int client_socket, int id) {
    char name[MAX_LEN], str[MAX_LEN];
    recv(client_socket, name, sizeof(name), 0);
    recv(client_socket, str, sizeof(str), 0); // Receive password
    int choice;
    recv(client_socket, &choice, sizeof(choice), 0); // Receive choice (1 for sign-up, 2 for login)

    if (choice == 1) { // Sign-up
        if (!signup_user(name, str)) {
            // Inform the client that sign-up failed
            send(client_socket, "0", 2, 0); // Sign-up failed
            // Close the connection and exit the function
            close(client_socket);
            return;
        }
        // Inform the client that sign-up was successful
        send(client_socket, "1", 2, 0); // Sign-up success
    } else if (choice == 2) { // Login
        if (!authenticate_user(name, str)) {
            // Inform the client that login failed
            send(client_socket, "0", 2, 0); // Authentication failed
            // Close the connection and exit the function
            close(client_socket);
            return;
        }
        // Inform the client that login was successful
        send(client_socket, "1", 2, 0); // Authentication success
    } else {
        // Invalid choice
        close(client_socket);
        return;
    }

    // Display welcome message
    string welcome_message = string(name) + " has joined";
    broadcast_message("#NULL", id);
    broadcast_message(id, id);
    broadcast_message(welcome_message, id);
    shared_print(color(id) + welcome_message + def_col);

    while (true) {
        int bytes_received = recv(client_socket, str, sizeof(str), 0);
        if (bytes_received <= 0)
            return;
        if (strcmp(str, "#exit") == 0) {
            // Display leaving message
            string message = string(name) + " has left";
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
    }
}

bool authenticate_user(const string& username, const string& password) {
    ifstream file("user_credentials.txt");
    if (!file.is_open()) {
        cerr << "Error: Unable to open file." << endl;
        return false;
    }

    bool authenticated = false;
    string line;
    while (getline(file, line)) {
        istringstream iss(line);
        string user, pass;
        if (!(iss >> user >> pass)) {
            cerr << "Error: Invalid data format in file." << endl;
            file.close();
            return false;
        }
        if (user == username && pass == password) {
            authenticated = true;
            break;
        }
    }

    file.close();
    if (authenticated) {
        cout << "Authentication successful." << endl;
        return true;
    } else {
        cerr << "User doesn't have access to the chatroom" << endl;
        return false;
    }
}

bool signup_user(const string& username, const string& password) {
    // Open the file to append new user credentials
    ofstream file("user_credentials.txt", ios::app);
    if (!file.is_open()) {
        cerr << "Error: Unable to open file for writing." << endl;
        return false;
    }

    // Save new user credentials to file
    file << username << " " << password << endl;
    file.close();
    return true;
}
