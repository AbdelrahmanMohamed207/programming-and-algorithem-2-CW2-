#include <iostream>             // Includes the standard input/output stream library
#include <fstream>              // Provides facilities for file-based input/output
#include <sstream>              // Provides string stream classes
#include <mutex>                // For using mutexes in multithreading to protect shared data
#include <set>                  // Provides the set container from the Standard Template Library
#include <thread>               // Provides the thread class and related functions for threading
#include <boost/asio.hpp>       // Boost ASIO includes for asynchronous input/output operations
#include <boost/filesystem.hpp> // Provides facilities to manipulate files and directories

using namespace std;                        // Uses the standard namespace to avoid prefixing std::
using namespace boost::asio;                // Uses the boost::asio namespace to simplify usage of the library
using ip::tcp;                              // Simplifies usage of the TCP protocol type under the ip namespace
using client_ptr = shared_ptr<tcp::socket>; // Defines client_ptr as an alias for shared_ptr managing tcp::socket objects
using client_set = set<client_ptr>;         // Defines client_set as a set of shared_ptr to tcp::socket for managing multiple clients


// MessageNode struct for storing linked list of messages per user
struct MessageNode {
    string message;
    MessageNode* next;
};

// Node struct for storing user information and their messages
struct Node {
    string username;
    string password;
    MessageNode* messageHead;
    Node* next;

    Node() : messageHead(nullptr), next(nullptr) {}
};

// UserList class manages a linked list of users
class UserList {
private:
    Node* head;

public:
    UserList() : head(nullptr) {}
    ~UserList() {
        clear();  // Destructor clears all nodes when UserList is destroyed
    }

    // Deletes all nodes from the list
    void clear() {
        while (head != nullptr) {
            Node* temp = head;
            head = head->next;
            clearMessages(temp); // Clear messages before deleting the user node
            delete temp;
        }
    }

    // Deletes all message nodes for a given user
    void clearMessages(Node* user) {
        while (user->messageHead != nullptr) {
            MessageNode* temp = user->messageHead;
            user->messageHead = user->messageHead->next;
            delete temp;
        }
    }

    // Adds a new user if not already present
    bool addUser(const string& username, const string& hashedPassword) {
        if (findUser(username)) {
            return false; // Return false if user already exists
        }
        Node* newNode = new Node;
        newNode->username = username;
        newNode->password = hashedPassword;
        newNode->next = head;
        head = newNode;
        return true;
    }

    // Finds a user by username
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

    // Authenticates a user by comparing hashed passwords
    bool authenticateUser(const string& username, const string& password) {
        hash<string> hasher;
        string hashedInputPassword = to_string(hasher(password));
        Node* user = findUser(username);
        if (user) {
            return user->password == hashedInputPassword;
        }
        return false;
    }

    // Adds a message to the user's message list
    void addMessage(const string& username, const string& message) {
        Node* user = findUser(username);
        if (user) {
            MessageNode* newMessage = new MessageNode{message, user->messageHead};
            user->messageHead = newMessage;
            saveUser(username);  // Save messages to file
        }
    }

    // Saves a user's data to file
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

    // Loads a user's data from file
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

UserList userList;  // Creates an instance of the UserList class to manage user data throughout the application.

io_context io;  // Defines an io_context object from Boost ASIO, which is used to manage asynchronous I/O operations.

mutex clients_mutex;  // Defines a mutex used to synchronize access to shared resources (e.g., the client_set) across different threads.

client_set clients;  // Declares a set (container) that will store client_ptr objects (shared pointers to tcp::socket). 
                     // This is used to manage and track all active client socket connections.

// Function to encrypt a string using Caesar cipher
string caesar_encrypt(const string& text, int shift) {
    string result;
    for (char c : text) {
        if (isalpha(c)) {
            char base = islower(c) ? 'a' : 'A';
            c = static_cast<char>(((c - base + shift) % 26) + base);
        }
        result.push_back(c);
    }
    return result;
}

// Function to decrypt a string using Caesar cipher
string caesar_decrypt(const string& text, int shift) {
    return caesar_encrypt(text, 26 - shift);
}

// Registers a new user and saves their data
bool register_user(const string& username, const string& password) {
    hash<string> hasher;
    string hashedPassword = to_string(hasher(password));
    if (!userList.addUser(username, hashedPassword)) {
        return false;
    }
    userList.saveUser(username); // Save user data to file
    return true;
}

// Authenticates a user
bool authenticate_user(const string& username, const string& password) {
    return userList.authenticateUser(username, password);
}

// Broadcasts a message to all clients except the sender
void broadcast_message(const string& message, const client_ptr& sender) {
    lock_guard<mutex> lock(clients_mutex);
    cout << "Forwarding encrypted message: " << message << endl;
    for (auto& client : clients) {
        if (client != sender) {
            async_write(*client, buffer(message + "\n"), [](const boost::system::error_code& ec, std::size_t length) {
                if (ec) {
                    cerr << "Error sending message: " << ec.message() << endl;
                }
            });
        }
    }
}

// Handles a single client's requests and interactions
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

            cout << "Received action: " << username << " has " << action << endl;

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

                    // Decrypt the message
                    string decrypted_message = caesar_decrypt(encrypted_message, 3);
                    
                    // Check if the decrypted message contains "logout"
                    if (decrypted_message.find("logout") != string::npos) {
                        cout << "Received action: "  << username << " has logged out" << endl;
                        return; // Exit the handle_client function
                    }

                    // Forwarding the encrypted message
                    broadcast_message(encrypted_message, client);

                    // Decrypt and store the message for the user
                    userList.addMessage(username, decrypted_message);
                }
            }
        }
    } catch (const exception& e) {
        cerr << "Client disconnected: " << e.what() << "\n";
        lock_guard<mutex> lock(clients_mutex);
        clients.erase(client);
    }
}

// Main function to initialize server, load users, and accept client connections
int main() {
    try {
        boost::filesystem::path userDir("users");
        boost::filesystem::create_directories(userDir);
        if (boost::filesystem::exists(userDir) && boost::filesystem::is_directory(userDir)) {
            for (auto& entry : boost::filesystem::directory_iterator(userDir)) {
                userList.loadUser(entry.path().string());
            }
        }

        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 1234));
        while (true) {
            client_ptr new_client = make_shared<tcp::socket>(io);
            acceptor.accept(*new_client);

            new_client->set_option(tcp::no_delay(true));

            thread([new_client]() {
                handle_client(new_client);
            }).detach();
        }
    } catch (const exception& e) {
        cerr << "Server exception: " << e.what() << "\n";
    }
    return 0;
}
