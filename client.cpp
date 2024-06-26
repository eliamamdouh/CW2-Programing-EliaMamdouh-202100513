#include <iostream>
#include <string>
#include <thread>
#include <fstream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <signal.h>

using namespace std;

bool exit_flag = false;
thread t_send, t_recv;
int client_socket;
string def_col = "\033[0m";
string colors[] = {"\033[31m", "\033[32m", "\033[33m", "\033[34m", "\033[35m", "\033[36m"};

// Function declarations
string encryptMessage(const string& message, int shift);
string decryptMessage(const string& encryptedMessage, int shift);
void catch_ctrl_c(int signal);
string color(int code);
void send_message(int client_socket);
void recv_message(int client_socket);
bool signup_user(int client_socket);
bool login_user(int client_socket);

// Caesar cipher encryption function
string encryptMessage(const string& message, int shift) {
    string encryptedMessage = message;
    for (char& c : encryptedMessage) {
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
            c = ((c - base + shift) % 26) + base; 
        }
    }
    return encryptedMessage;
}

// Caesar cipher decryption function
string decryptMessage(const string& encryptedMessage, int shift) {
    return encryptMessage(encryptedMessage, 26 - shift); // Reversing the shift for decryption
}

// Handler for "Ctrl + C"
void catch_ctrl_c(int signal) {
    char str[200] = "#exit";
    send(client_socket, str, sizeof(str), 0);
    exit_flag = true;
    t_send.detach();
    t_recv.detach();
    close(client_socket);
    exit(signal);
}

// Function to set terminal color based on client id
string color(int code) {
    return colors[code % 6];
}

// Function to send message to server
void send_message(int client_socket) {
    while (true) {
        cout << colors[1] << "You: " << def_col;
        char str[200];
        cin.getline(str, sizeof(str));
        string encrypted_message = encryptMessage(string(str), 3); // Encrypt with shift of 3
        cout << "Encrypted Message: " << encrypted_message << endl; // Display encrypted message
        send(client_socket, encrypted_message.c_str(), sizeof(str), 0);
        if (strcmp(str, "#exit") == 0) {
            exit_flag = true;
            t_recv.detach();
            close(client_socket);
            return;
        }
    }
}

// Function to receive message from server
void recv_message(int client_socket) {
    while (true) {
        if (exit_flag)
            return;
        char name[200], str[200];
        int color_code;
        int bytes_received = recv(client_socket, name, sizeof(name), 0);
        if (bytes_received <= 0)
            continue;
        recv(client_socket, &color_code, sizeof(color_code), 0);
        recv(client_socket, str, sizeof(str), 0);
        cout << "\033[2K\r"; // Erase the current line
        if (strcmp(name, "#NULL") != 0)
            cout << color(color_code) << name << " : " << def_col << str << endl;
        else
            cout << color(color_code) << str << endl;
        cout << colors[1] << "You: " << def_col << flush;
    }
}

// Function to handle user sign-up process
bool signup_user(int client_socket) {
    char username_signup[200], password_signup[200];
    cout << "Enter a username: ";
    cin.getline(username_signup, sizeof(username_signup));
    cout << "Enter a password: ";
    cin.getline(password_signup, sizeof(password_signup));

    // Send choice (1 for sign-up)
    int choice = 1;
    send(client_socket, username_signup, sizeof(username_signup), 0);
    send(client_socket, password_signup, sizeof(password_signup), 0);
    send(client_socket, &choice, sizeof(choice), 0);

    char response[2];
    recv(client_socket, response, sizeof(response), 0);
    if (response[0] == '1') {
        cout << "Sign-up successful." << endl;
        return true;
    } else {
        return false;
    }
}

// Function to handle user login process
bool login_user(int client_socket) {
    char username[200], password[200];
    cout << "Enter your username: ";
    cin.getline(username, sizeof(username));
    cout << "Enter your password: ";
    cin.getline(password, sizeof(password));

    // Send choice (2 for login)
    int choice = 2;
    send(client_socket, username, sizeof(username), 0);
    send(client_socket, password, sizeof(password), 0);
    send(client_socket, &choice, sizeof(choice), 0);

    char response[2];
    recv(client_socket, response, sizeof(response), 0);
    if (response[0] == '1') {
        cout << "Log-in successful." << endl;
        return true;
    } else {
        return false;
    }
}

int main() {
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in client;
    client.sin_family = AF_INET;
    client.sin_port = htons(10000); // Port no. of server
    client.sin_addr.s_addr = INADDR_ANY;
    //client.sin_addr.s_addr=inet_addr("127.0.0.1"); // Provide IP address of server
    bzero(&client.sin_zero, sizeof(client.sin_zero));

    if (connect(client_socket, (struct sockaddr *)&client, sizeof(struct sockaddr_in)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    signal(SIGINT, catch_ctrl_c);

    cout << colors[5] << "\n\t  ====== Welcome to the chat-room ======   " << endl << def_col;

    int choice;
    cout << "Choose an option:" << endl;
    cout << "1. Sign up" << endl;
    cout << "2. Log in" << endl;
    cout << "Your choice: ";
    cin >> choice;
    cin.ignore(); // Clear input buffer

    bool login_success = false;
    if (choice == 1) {
        if (!signup_user(client_socket)) {
            cout << "Sign-up failed. Exiting..." << endl;
            close(client_socket);
            exit(EXIT_FAILURE);
        }
    } else if (choice == 2) {
        login_success = login_user(client_socket);
        if (!login_success) {
            cout << "Log-in failed. Exiting..." << endl;
            close(client_socket);
            exit(EXIT_FAILURE);
        }
    } else {
        cout << "Invalid choice. Exiting..." << endl;
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    // Start sending and receiving messages
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
