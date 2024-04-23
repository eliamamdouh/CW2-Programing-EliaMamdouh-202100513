#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <fstream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <sstream>

using namespace std;

#define MAX_LEN 200
#define NUM_COLORS 6
#define MAX_CLIENTS 100 // Maximum number of clients

struct Terminal {
    int id;
    string name;
    int socket;
    thread th;
};

Terminal clients[MAX_CLIENTS]; // Array to store client information
int num_clients = 0;
string def_col = "\033[0m";
string colors[] = {"\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m"};
int seed = 0;
mutex cout_mtx, clients_mtx;

// Function declarations
string color(int code);
void set_name(int id, const char name[]);
void shared_print(const string& str, bool endLine = true);
int broadcast_message(const string& message, int sender_id);
int broadcast_message(int num, int sender_id);
void end_connection(int id);
void handle_client(int client_socket, int id);
bool authenticate_user(const string& username, const string& password);
bool signup_user(const string& username, const string& password);
string decryptMessage(const string& encryptedMessage, int shift);

// Caesar cipher decryption function
string decryptMessage(const string& encryptedMessage, int shift) {
    string decryptedMessage = encryptedMessage;
    for (char& c : decryptedMessage) {
        if (isalnum(c)) { // Check if character is alphanumeric
            char base;
            if (isdigit(c)) {
                base = '0';
            }
            else if (isupper(c)) {
                base = 'A';
            }
            else {
                base = 'a';
            }
            c = ((c - base + 26 - shift) % 26) + base; // Reversing the shift for decryption
        }
    }
    return decryptedMessage;
}

// Function to set terminal color based on client id
string color(int code) {
    return colors[code % NUM_COLORS];
}

// Function to set name of client
void set_name(int id, const char name[]) {
    lock_guard<mutex> guard(clients_mtx);
    for (auto& client : clients) {
        if (client.id == id) {
            client.name = name;
            break;
        }
    }
}

// Function to print message to console shared by multiple threads
void shared_print(const string& str, bool endLine) {
    lock_guard<mutex> guard(cout_mtx);
    cout << str;
    if (endLine)
        cout << endl;
}

// Function to broadcast message to all clients
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

// Function to broadcast numeric message to all clients
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
    for (int i = 0; i < num_clients; ++i) {
        if (clients[i].id == id) {
            clients[i].th.detach();
            close(clients[i].socket);
            // Shift elements to fill the gap
            for (int j = i; j < num_clients - 1; ++j) {
                clients[j].id = clients[j + 1].id;
                clients[j].name = clients[j + 1].name;
                clients[j].socket = clients[j + 1].socket;
                clients[j].th = move(clients[j + 1].th);
            }
            num_clients--;
            break;
        }
    }
}

// Function to handle client connections
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
        string decrypted_message = decryptMessage(string(str), 3); // Decrypt with shift of 3
        broadcast_message(string(name), id);
        broadcast_message(id, id);
        broadcast_message(decrypted_message, id);
        shared_print(color(id) + name + " : " + def_col + decrypted_message);
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
        if (!(iss >> user) || !(iss >> pass)) {
            cerr << "Error: Invalid data format in file." << endl;
            file.close();
            return false;
        }
        // Check if the username and password match
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


// Function to sign up new user
bool signup_user(const string& username, const string& password) {
    ifstream file("user_credentials.txt");
    string line;
    while (getline(file, line)) {
        size_t pos = line.find(" ");
        if (pos != string::npos) {
            string existing_username = line.substr(0, pos);
            string existing_password = line.substr(pos + 1);
            if (existing_username == username) {
                file.close();
                cerr << "Error: Username already exists." << endl;
                return false;
            }
            if (existing_password == password) {
                file.close();
                cerr << "Error: Password already in use." << endl;
                return false;
            }
        }
    }
    file.close();

    // Open the file to append new user credentials
    ofstream output_file("user_credentials.txt", ios::app);
    if (!output_file.is_open()) {
        cerr << "Error: Unable to open file for writing." << endl;
        return false;
    }

    // Save new user credentials to file
    output_file << username << " " << password << endl;
    output_file.close();
    return true;
}

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
            clients[num_clients++] = {seed, "Anonymous", client_socket, move(t)};
        }
    }

    for (int i = 0; i < num_clients; i++) {
        if (clients[i].th.joinable())
            clients[i].th.join();
    }

    close(server_socket);
    return 0;
}
