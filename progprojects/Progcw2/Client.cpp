#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include <cctype> // For isalpha and islower

using namespace boost::asio;
using namespace std;
using ip::tcp;

// Caesar Cipher Encryption
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

// Caesar Cipher Decryption
string caesar_decrypt(const string& text, int shift) {
    return caesar_encrypt(text, 26 - shift); // Decrypt by shifting the opposite way
}

void read_messages(tcp::socket& socket, int shift) {
    try {
        while (true) {
            boost::asio::streambuf buf;
            read_until(socket, buf, "\n");
            istream response_stream(&buf);
            string response;
            getline(response_stream, response);
            if (!response.empty()) {
                cout << "Received: " << caesar_decrypt(response, shift) << endl;
            }
        }
    } catch (const std::exception& e) {
        cerr << "Read error: " << e.what() << endl;
    }
}

void write_messages(tcp::socket& socket, const string& username, int shift) {
    try {
        while (running) {
            string msg;
            cout << "Enter message (type 'logout' to exit): ";
            getline(cin, msg);
            if (msg == "logout") {
                running = false;
                cout << "Logging out and exiting..." << endl;
                // Send a logout command to the server
                string encrypted_logout = caesar_encrypt("logout", shift); // Encrypt the word "logout"
                write(socket, buffer(encrypted_logout + "\n"));
                break; // Break the loop and exit
            }
            // Prepend the username before encrypting
            string message_with_name = username + ": " + msg;
            string encrypted_msg = caesar_encrypt(message_with_name, shift);
            write(socket, buffer(encrypted_msg + "\n"));
        }
    } catch (const std::exception& e) {
        cerr << "Write error: " << e.what() << endl;
    }
}

int main() {
    int shift = 3; // Caesar shift amount
    io_context io;
    tcp::socket socket(io);
    tcp::resolver resolver(io);

    try {
        auto endpoints = resolver.resolve("127.0.0.1", "1234");
        connect(socket, endpoints);
        socket.set_option(tcp::no_delay(true));

        cout << "Type 'register' to register or 'login' to login: ";
        string action;
        getline(cin, action);

        cout << "Enter username: ";
        string username;
        getline(cin, username);

        cout << "Enter password: ";
        string password;
        getline(cin, password);

        // Send initial action, username, and password without encryption as it's needed for the server to handle auth
        write(socket, buffer(action + "\n" + username + "\n" + password + "\n"));

        boost::asio::streambuf buf;
        read_until(socket, buf, "\n");
        istream response_stream(&buf);
        string response;
        getline(response_stream, response);
        cout << "Server response: " << response << endl;

        // Separate threads for reading and writing
        thread read_thread(read_messages, ref(socket), shift);
        thread write_thread(bind(write_messages, ref(socket), username, shift));

        read_thread.join();
        write_thread.join();
    } catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;
        socket.close();
    }

    return 0;
}
