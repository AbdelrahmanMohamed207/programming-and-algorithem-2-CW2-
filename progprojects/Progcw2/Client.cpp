#include <iostream>       // Standard library for input and output operations
#include <boost/asio.hpp> // Boost library for network and low-level I/O operations
#include <thread>         // Standard library for using threads
#include <cctype>         // For character handling functions like isalpha and islower
#include <atomic>         // For atomic variables which are useful in concurrent programming

using namespace boost::asio; // Simplify usage of boost asio functions
using namespace std;
using ip::tcp;               // Using TCP/IP protocol

// Function to encrypt a string using the Caesar cipher method
string caesar_encrypt(const string& text, int shift) {
    string result;
    for (char c : text) {
        if (isalpha(c)) {  // Check if the character is an alphabet
            char base = islower(c) ? 'a' : 'A';  // Determine the starting alphabet (lowercase or uppercase)
            c = static_cast<char>(((c - base + shift) % 26) + base);  // Shift character within the alphabet range
        }
        result.push_back(c);  // Append the encrypted character to result
    }
    return result;
}

// Function to decrypt a string using the Caesar cipher method
string caesar_decrypt(const string& text, int shift) {
    return caesar_encrypt(text, 26 - shift);  // Decrypt by using the inverse shift
}

// Atomic flag to control the message reading and writing loop
std::atomic<bool> running(true);

// Function to read messages from the server
void read_messages(tcp::socket& socket, int shift) {
    try {
        while (running) {  // Continue as long as running is true
            boost::asio::streambuf buf;
            read_until(socket, buf, "\n");  // Read from socket until a newline is found
            istream response_stream(&buf);
            string response;
            getline(response_stream, response);  // Extract the string from stream
            if (!response.empty()) {
                cout << "\n> Received: " << caesar_decrypt(response, shift) << endl;  // Decrypt and display the message
                cout << "> Enter message (Type 'logout' to exit): ";  // Prompt user for next message
                cout.flush();  // Ensure prompt is visible immediately
            }
        }
    } catch (const std::exception& e) {
        cerr << "Read error: " << e.what() << endl;  // Display error if reading fails
        cout << "> Enter message (Type 'logout' to exit): ";
        cout.flush();
    }
}

// Function to send messages to the server
void write_messages(tcp::socket& socket, const string& username, int shift) {
    try {
        while (running) {  // Continue as long as running is true
            string msg;
            cout << "> Enter message (Type 'logout' to exit): ";  // Prompt user for message
            getline(cin, msg);  // Read user input
            if (msg == "logout") {
                running = false;  // Set running to false to terminate both read and write loops
                cout << "Logging out and exiting..." << endl;
                string encrypted_logout = caesar_encrypt("logout", shift);  // Encrypt the logout message
                write(socket, buffer(encrypted_logout + "\n"));  // Send logout message to server
                break;
            }
            string message_with_name = username + ": " + msg;  // Format message with username
            string encrypted_msg = caesar_encrypt(message_with_name, shift);  // Encrypt the message
            write(socket, buffer(encrypted_msg + "\n"));  // Send encrypted message to server
        }
    } catch (const std::exception& e) {
        cerr << "Write error: " << e.what() << endl;  // Display error if writing fails
    }
}

// Main function to setup network connection and handle user interactions
int main() {
    int shift = 3;  // Caesar cipher shift amount
    io_context io;
    tcp::socket socket(io);
    tcp::resolver resolver(io);

    try {
        auto endpoints = resolver.resolve("127.0.0.1", "1234");  // Resolve server address and port
        connect(socket, endpoints);  // Establish TCP connection
        socket.set_option(tcp::no_delay(true));  // Disable Nagle's algorithm

        cout << "Type 'register' to register or 'login' to login: ";  // Prompt for initial action
        string action;
        getline(cin, action);

        cout << "Enter username: ";  // Prompt for username
        string username;
        getline(cin, username);

        cout << "Enter password: ";  // Prompt for password
        string password;
        getline(cin, password);

        // Send initial action, username, and password without encryption as it's needed for the server to handle auth
        write(socket, buffer(action + "\n" + username + "\n" + password + "\n"));

        boost::asio::streambuf buf;
        read_until(socket, buf, "\n");  // Read server response
        istream response_stream(&buf);
        string response;
        getline(response_stream, response);  // Extract response
        cout << "Server response: " << response << endl;  // Display server response

        // Create separate threads for reading and writing messages
        thread read_thread(read_messages, ref(socket), shift);
        thread write_thread(bind(write_messages, ref(socket), username, shift));

        write_thread.join();  // Wait for write thread to finish
        read_thread.join();  // Wait for read thread to finish
    } catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;  // Display any exceptions that occur
    }

    socket.close();  // Ensure the socket is closed on all code paths
    return 0;
}
