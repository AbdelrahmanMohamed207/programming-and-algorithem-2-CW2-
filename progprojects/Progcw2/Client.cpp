#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include <cctype> // For isalpha and islower

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
