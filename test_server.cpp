/*
 * Test Server for P2P Micropayment System
 * Course: Computer Networks (Fall 2025)
 * 
 * This is a simplified test server for Phase 1 development.
 * It implements the basic server functionality needed to test client programs:
 * - User registration and authentication
 * - Online user list management
 * - Transaction reporting and balance updates
 * 
 * Note: The actual TA's server may have different implementation details.
 * This test server is provided for development and testing purposes only.
 */

#include <iostream>
#include <string>
#include <cstring>
#include <map>
#include <thread>
#include <mutex>
#include <vector>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;

#define BUFFER_SIZE 4096  // Maximum size for network messages
#define CRLF "\r\n"       // Carriage Return + Line Feed (protocol requirement)

/*
 * User Structure
 * Stores information about each registered user
 */
struct User {
    string username;  // User's account name
    int balance;      // Current account balance
    string ip;        // User's IP address (when online)
    int port;         // User's listening port for P2P connections (when online)
    bool online;      // Online status flag
};

// Global data structures
map<string, User> users;  // Map of all registered users (username -> User)
mutex users_mutex;        // Protects users map from concurrent access
string server_public_key = "SERVER_PUBLIC_KEY_PLACEHOLDER";  // Server's public key (for Phase 2)

/*
 * Handle Client Connection
 * Processes all messages from a single client connection.
 * Supports multiple message types:
 * - REGISTER: Register new user (short-lived connection)
 * - username#port: Login (persistent connection)
 * - List: Get balance and online users
 * - TRANSACTION: Report P2P transaction
 * - Exit: Logout and disconnect
 */
void handle_client(int client_sock, const string& client_ip) {
    char buffer[BUFFER_SIZE];
    string current_user = "";  // Track which user is logged in on this connection
    
    // Main message handling loop
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        
        // Receive message from client
        ssize_t received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        if (received <= 0) {
            // Connection closed or error
            if (received == 0) {
                cout << "[" << client_ip << "] Connection closed by client" << endl;
            } else {
                cout << "[" << client_ip << "] Error receiving data" << endl;
            }
            break;
        }
        
        string message(buffer, received);
        cout << "[" << client_ip << "] Received: " << message;
        
        // Parse and handle different message types
        if (message.find("REGISTER#") == 0) {
            /*
             * REGISTRATION HANDLER
             * Protocol: REGISTER#<username>\r\n
             * Response: 100 OK\r\n (success) or 210 FAIL\r\n (failure)
             * Note: Registration uses short-lived connection (closes after response)
             */
            size_t pos = message.find('#');
            string username = message.substr(pos + 1);
            username = username.substr(0, username.find("\r\n"));
            
            users_mutex.lock();
            if (users.find(username) != users.end()) {
                // Username already exists
                string response = "210 FAIL" + string(CRLF);
                send(client_sock, response.c_str(), response.length(), 0);
                cout << "[Server] Registration failed: " << username << " already exists" << endl;
            } else {
                // Create new user with initial balance of 10000
                User new_user;
                new_user.username = username;
                new_user.balance = 10000;
                new_user.online = false;
                users[username] = new_user;
                
                string response = "100 OK" + string(CRLF);
                send(client_sock, response.c_str(), response.length(), 0);
                cout << "[Server] Registered new user: " << username << endl;
            }
            users_mutex.unlock();
            
            // Registration is a one-time operation, close connection
            break;
            
        } else if (message.find("List") == 0) {
            /*
             * LIST REQUEST HANDLER
             * Protocol: List\r\n
             * Response: <balance>\r\n<public_key>\r\n<num_users>\r\n<user1>#<ip1>#<port1>\r\n...
             * Note: Requires active login session
             */
            if (current_user.empty()) {
                cout << "[Server] List request without login, closing connection" << endl;
                break;
            }
            
            users_mutex.lock();
            
            // Get current user's balance
            int balance = users[current_user].balance;
            
            // Build response message
            ostringstream oss;
            oss << balance << CRLF;
            oss << server_public_key << CRLF;
            
            // Count online users
            int online_count = 0;
            for (const auto& pair : users) {
                if (pair.second.online) {
                    online_count++;
                }
            }
            oss << online_count << CRLF;
            
            // List all online users with their connection info
            for (const auto& pair : users) {
                if (pair.second.online) {
                    oss << pair.second.username << "#" 
                        << pair.second.ip << "#" 
                        << pair.second.port << CRLF;
                }
            }
            
            string response = oss.str();
            send(client_sock, response.c_str(), response.length(), 0);
            cout << "[Server] Sent list to client: " << current_user << endl;
            
            users_mutex.unlock();
            
            // Keep connection alive for more requests
            continue;
            
        } else if (message.find("TRANSACTION#") == 0) {
            /*
             * TRANSACTION REPORT HANDLER
             * Protocol: TRANSACTION#<sender>#<receiver>#<amount>\r\n
             * Response: Success or failure message
             * 
             * This handles P2P transaction reports. The receiver's client sends this
             * to inform the server about a completed P2P transfer so both accounts
             * can be updated.
             */
            if (current_user.empty()) {
                cout << "[Server] Transaction report without login, closing connection" << endl;
                break;
            }
            
            // Parse transaction message: TRANSACTION#sender#receiver#amount
            size_t pos1 = message.find('#');
            size_t pos2 = message.find('#', pos1 + 1);
            size_t pos3 = message.find('#', pos2 + 1);
            
            if (pos1 != string::npos && pos2 != string::npos && pos3 != string::npos) {
                string sender = message.substr(pos1 + 1, pos2 - pos1 - 1);
                string receiver = message.substr(pos2 + 1, pos3 - pos2 - 1);
                string amount_str = message.substr(pos3 + 1);
                amount_str = amount_str.substr(0, amount_str.find("\r\n"));
                int amount = stoi(amount_str);
                
                users_mutex.lock();
                
                // Verify both users exist
                if (users.find(sender) == users.end() || users.find(receiver) == users.end()) {
                    string response = "Transaction failed: Invalid users" + string(CRLF);
                    send(client_sock, response.c_str(), response.length(), 0);
                    cout << "[Server] Transaction failed: Invalid users" << endl;
                    users_mutex.unlock();
                    continue;
                }
                
                // Verify sender has sufficient balance
                if (users[sender].balance < amount) {
                    string response = "Transaction failed: Insufficient balance" + string(CRLF);
                    send(client_sock, response.c_str(), response.length(), 0);
                    cout << "[Server] Transaction failed: Sender " << sender << " has insufficient balance" << endl;
                    users_mutex.unlock();
                    continue;
                }
                
                // Update balances: deduct from sender, add to receiver
                users[sender].balance -= amount;
                users[receiver].balance += amount;
                
                string response = "Transaction successful" + string(CRLF);
                send(client_sock, response.c_str(), response.length(), 0);
                cout << "[Server] Transaction processed: " << sender << " -> " << receiver << " $" << amount << endl;
                cout << "[Server] New balances: " << sender << "=$" << users[sender].balance 
                     << ", " << receiver << "=$" << users[receiver].balance << endl;
                
                users_mutex.unlock();
            } else {
                // Invalid message format
                string response = "Transaction failed: Invalid format" + string(CRLF);
                send(client_sock, response.c_str(), response.length(), 0);
                cout << "[Server] Transaction failed: Invalid message format" << endl;
            }
            
            // Keep connection alive for more requests
            continue;
            
        } else if (message.find("Exit") == 0) {
            /*
             * EXIT/LOGOUT HANDLER
             * Protocol: Exit\r\n
             * Response: Bye\r\n
             * Note: Marks user as offline and closes connection
             */
            if (!current_user.empty()) {
                users_mutex.lock();
                if (users.find(current_user) != users.end()) {
                    users[current_user].online = false;
                }
                users_mutex.unlock();
                cout << "[Server] User logged out: " << current_user << endl;
            }
            
            string response = "Bye" + string(CRLF);
            send(client_sock, response.c_str(), response.length(), 0);
            cout << "[Server] Client disconnected gracefully" << endl;
            
            // Exit command closes the connection
            break;
            
        } else if (message.find('#') != string::npos && message.find("REGISTER") == string::npos) {
            /*
             * LOGIN HANDLER
             * Protocol: <username>#<port>\r\n
             * Response: <balance>\r\n<public_key>\r\n<num_users>\r\n<user1>#<ip1>#<port1>\r\n...
             * Note: Creates persistent connection that stays open until Exit or disconnect
             */
            size_t pos = message.find('#');
            string username = message.substr(0, pos);
            string port_str = message.substr(pos + 1);
            port_str = port_str.substr(0, port_str.find("\r\n"));
            int port = stoi(port_str);
            
            users_mutex.lock();
            if (users.find(username) == users.end()) {
                // User not registered
                string response = "220 AUTH_FAIL" + string(CRLF);
                send(client_sock, response.c_str(), response.length(), 0);
                cout << "[Server] Login failed: " << username << " not registered" << endl;
                users_mutex.unlock();
                
                // Failed login, close connection
                break;
            } else {
                // Update user status to online
                users[username].online = true;
                users[username].ip = client_ip;
                users[username].port = port;
                current_user = username;
                
                // Build login response
                ostringstream oss;
                oss << users[username].balance << CRLF;
                oss << server_public_key << CRLF;
                
                // Count online users
                int online_count = 0;
                for (const auto& pair : users) {
                    if (pair.second.online) {
                        online_count++;
                    }
                }
                oss << online_count << CRLF;
                
                // List all online users
                for (const auto& pair : users) {
                    if (pair.second.online) {
                        oss << pair.second.username << "#" 
                            << pair.second.ip << "#" 
                            << pair.second.port << CRLF;
                    }
                }
                
                string response = oss.str();
                send(client_sock, response.c_str(), response.length(), 0);
                cout << "[Server] User logged in: " << username << endl;
                users_mutex.unlock();
                
                // Keep connection alive for subsequent requests
                continue;
            }
        } else {
            // Unknown message format
            cout << "[Server] Unknown message format, closing connection" << endl;
            break;
        }
    }
    
    // Clean up: mark user as offline if they were logged in
    if (!current_user.empty()) {
        users_mutex.lock();
        if (users.find(current_user) != users.end()) {
            users[current_user].online = false;
            cout << "[Server] Marked user offline: " << current_user << endl;
        }
        users_mutex.unlock();
    }
    
    // Close client socket
    close(client_sock);
}

/*
 * Main Function
 * Sets up the server socket, binds to specified port, and accepts client connections.
 * Each client connection is handled in a separate thread.
 */
int main(int argc, char* argv[]) {
    // Check command line arguments
    if (argc < 2) {
        cout << "Usage: " << argv[0] << " <port> [options]" << endl;
        cout << "Options: -d (debug), -s (show list), -a (show all)" << endl;
        return 1;
    }
    
    int port = stoi(argv[1]);
    
    cout << "========================================" << endl;
    cout << "    Test Server for Phase 1 Testing    " << endl;
    cout << "========================================" << endl;
    cout << "Listening on port: " << port << endl;
    cout << "Note: This is a simplified test server" << endl;
    cout << "========================================" << endl;
    
    // Create server socket
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("socket");
        return 1;
    }
    
    // Set socket options to reuse address (allows quick restart)
    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(server_sock);
        return 1;
    }
    
    // Bind socket to specified port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all network interfaces
    server_addr.sin_port = htons(port);        // Convert port to network byte order
    
    if (::bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_sock);
        return 1;
    }
    
    // Start listening for connections (queue up to 10 pending connections)
    if (listen(server_sock, 10) == -1) {
        perror("listen");
        close(server_sock);
        return 1;
    }
    
    cout << "Server started successfully. Waiting for connections..." << endl;
    
    // Accept client connections in an infinite loop
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // Accept incoming connection (blocking call)
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock == -1) {
            perror("accept");
            continue;
        }
        
        // Get client's IP address
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        
        cout << "\n[Server] New connection from " << client_ip << endl;
        
        // Handle this client in a new thread
        thread handler(handle_client, client_sock, string(client_ip));
        handler.detach();  // Detach so handler runs independently
    }
    
    // Cleanup (never reached in this infinite loop)
    close(server_sock);
    return 0;
}
