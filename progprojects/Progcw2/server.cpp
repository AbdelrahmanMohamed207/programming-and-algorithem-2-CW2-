#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <set>
#include <thread>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>

bool addUser(const string& username, const string& hashedPassword) {
        if (findUser(username)) {
            return false; // User already exists
        }
        Node* newNode = new Node;
        newNode->username = username;
        newNode->password = hashedPassword;
        newNode->next = head;
        head = newNode;
        return true;
    }

    Node* findUser(const string& username) {
        Node* current = head;
        while (current) {
            if (current->username == username) {
                return current;
            }
            current = current->next;
        }
        return nullptr;
    }

    bool authenticateUser(const string& username, const string& password) {
        hash<string> hasher;
        string hashedInputPassword = to_string(hasher(password));
        Node* user = findUser(username);
        if (user) {
            return user->password == hashedInputPassword;
        }
        return false;
    }

    void addMessage(const string& username, const string& message) {
        Node* user = findUser(username);
        if (user) {
            MessageNode* newMessage = new MessageNode{message, user->messageHead};
            user->messageHead = newMessage;
            saveUser(username);  // Save messages to file
        }
    }

    void saveUser(const string& username) {
    ofstream file("users/" + username + ".txt");
    if (!file) {
        cerr << "Unable to open file for saving user data: " << username << endl;
        return;
    }
    Node* user = findUser(username);
    if (!user) {
        cerr << "User not found when trying to save: " << username << endl;
        return;
    }
    file << user->username << '\n';
    file << user->password << '\n';
    MessageNode* message = user->messageHead;
    while (message) {
        file << message->message << '\n';
        message = message->next;
    }
    file.close();
}


    void loadUser(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "Failed to open " << filename << endl;
            return;
        }
        string username;
        getline(file, username);
        string hashedPassword;
        getline(file, hashedPassword);
        addUser(username, hashedPassword);
        string message;
        while (getline(file, message)) {
            addMessage(username, message);
        }
        file.close();
    }
};

void handle_client(client_ptr client) {
    try {
        while (true) {
            boost::asio::streambuf buf;
            istream is(&buf);

            read_until(*client, buf, "\n");
            string action;
            getline(is, action);

            read_until(*client, buf, "\n");
            string username;
            getline(is, username);

            read_until(*client, buf, "\n");
            string password;
            getline(is, password);

            cout << "Received action: " << action << ", Username: " << username << ", Password: " << password << endl;

            if (action == "login") {
                if (!authenticate_user(username, password)) {
                    cerr << "Authentication failed for user: " << username << endl;
                    write(*client, buffer("Authentication failed.\n"));
                    return;
                }
                write(*client, buffer("Welcome " + username + "!\n"));
            } else if (action == "register") {
                if (!register_user(username, password)) {
                    write(*client, buffer("Registration failed, user already exists.\n"));
                    return;
                }
                write(*client, buffer("Registration successful.\n"));
            } else {
                cerr << "Invalid action: " << action << endl;
                write(*client, buffer("Invalid action.\n"));
                return;
            }

            clients.insert(client);

            while (true) {
                read_until(*client, buf, "\n");
                string encrypted_message;
                if (getline(is, encrypted_message)) {
                    if (encrypted_message.empty()) break;

                    // Forwarding the encrypted message
                    broadcast_message(encrypted_message, client);

                    // Decrypt and store the message for the user
                    string decrypted_message = caesar_decrypt(encrypted_message, 3);
                    userList.addMessage(username, encrypted_message);
                }
            }
        }
    } catch (const exception& e) {
        cerr << "Client disconnected: " << e.what() << "\n";
        lock_guard<mutex> lock(clients_mutex);
        clients.erase(client);
    }
}
