/*
 * P2P Micropayment System - Client Program
 * Course: Computer Networks (Fall 2025)
 * 
 * This client program implements a peer-to-peer payment system where:
 * - Server handles user registration, authentication, and account management
 * - Money transfers happen directly between clients (P2P), not through server
 * - Multi-threaded design allows simultaneous operations
 */

#include <iostream>
#include <string>
#include <cstring>
#include <algorithm>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <chrono>
#include <sstream>
#include <sys/socket.h>
 #include <netinet/in.h>
 #include <arpa/inet.h>
 #include <unistd.h>
 #include <errno.h>
 
 using namespace std;
 
 // Constants
 #define BUFFER_SIZE 4096  // Maximum size for network messages
 #define CRLF "\r\n"       // Carriage Return + Line Feed (protocol requirement)
 
 // Global variables for network connections
 int server_socket = -1;   // Socket for persistent connection to server
 int listen_socket = -1;   // Socket for listening to P2P connections from other clients
 string username = "";     // Current logged-in username
 string server_ip = "";    // Server's IP address
 int server_port = 0;      // Server's port number
 int my_port = 0;          // Our listening port for P2P connections
 
 // Global variables for account state
 int account_balance = 10000;        // Current account balance (initialized with default)
 string server_public_key = "";      // Server's public key (for Phase 2)
 bool is_logged_in = false;          // Login status flag
 bool is_running = true;             // Main loop control flag
 
 // Mutex locks for thread synchronization
 mutex cout_mutex;    // Protects console output from multiple threads
 mutex socket_mutex;  // Protects server_socket from concurrent access
 
 // Structure to hold online user information
 struct OnlineUser {
     string username;  // User's account name
     string ip;        // User's IP address
     int port;         // User's listening port for P2P connections
 };
 
 // Map to store all currently online users (username -> OnlineUser)
 map<string, OnlineUser> online_users;
 mutex users_mutex;  // Protects online_users map from concurrent access
 
 // Function prototypes
 void print_menu();
 void handle_register();
 void handle_login();
 void handle_list();
 void handle_transfer();
 void handle_exit();
 int connect_to_server(const string& ip, int port);
 bool send_message(int sock, const string& message);
 string receive_message(int sock);
 void parse_online_list(const string& response);
 void listener_thread();
 void handle_client_connection(int client_sock);
 void safe_print(const string& message);
 
 /*
  * Main Function
  * Sets up initial configuration, starts P2P listener thread,
  * and runs the main menu loop for user interaction.
  */
 int main() {
     cout << "========================================" << endl;
     cout << "   P2P Micropayment System - Client    " << endl;
     cout << "========================================" << endl;
     cout << endl;
 
     // Get server connection information from user
     cout << "Enter Server IP address: ";
     getline(cin, server_ip);
     
     cout << "Enter Server Port: ";
     string port_str;
     getline(cin, port_str);
     server_port = stoi(port_str);
 
   // Get our listening port for accepting P2P connections
   cout << "Enter your listening port for P2P connections: ";
   getline(cin, port_str);
   my_port = stoi(port_str);

   // ============================================================================
   // IMPORTANT: Establish persistent connection to server at startup
   // ============================================================================
   // The TA's server expects a persistent connection model:
   // 1. Connect to server when program starts
   // 2. Use this SAME connection for ALL operations (Register, Login, List, etc.)
   // 3. Keep connection open until Exit
   //
   // If we create temporary connections, the server may terminate.
   //
   // 重要：在程式啟動時建立持久連線
   // 助教的 server 期望持久連線模式：
   // 1. 程式啟動時連接到 server
   // 2. 所有操作（註冊、登入、查詢等）都使用同一個連線
   // 3. 保持連線直到離線
   //
   // 如果建立臨時連線，server 可能會終止。
   // ============================================================================
   cout << "\nConnecting to server..." << endl;
   server_socket = connect_to_server(server_ip, server_port);
   if (server_socket == -1) {
       cout << "Failed to connect to server. Exiting." << endl;
       return 1;
   }
   cout << "Connected to server successfully!" << endl;
   cout << "You can now Register (if new user) or Login (if existing user)." << endl;

    // Start listener thread for P2P connections in background
    // This thread will accept incoming transfer requests from other clients
    thread listener(listener_thread);
    listener.detach();  // Detach so it runs independently

    // Give listener thread time to initialize and start listening
    this_thread::sleep_for(chrono::milliseconds(500));

    // Main menu loop - continues until user chooses to exit
     while (is_running) {
         print_menu();
         
         string choice;
         cout << "\nEnter your choice: ";
         getline(cin, choice);
 
         // Dispatch to appropriate handler based on user choice
         if (choice == "1") {
             handle_register();
         } else if (choice == "2") {
             handle_login();
         } else if (choice == "3") {
             handle_list();
         } else if (choice == "4") {
             handle_transfer();
         } else if (choice == "5") {
             handle_exit();
         } else {
             cout << "Invalid choice. Please try again." << endl;
         }
     }
 
     // Cleanup: close all sockets before exiting
     if (server_socket != -1) {
         close(server_socket);
     }
     if (listen_socket != -1) {
         close(listen_socket);
     }
 
     return 0;
 }
 
 /*
  * Print Main Menu
  * Displays available options to the user
  */
 void print_menu() {
     cout << "\n========================================" << endl;
     cout << "              Main Menu                 " << endl;
     cout << "========================================" << endl;
     cout << "1. Register" << endl;
     cout << "2. Login" << endl;
     cout << "3. List (Get account balance and online users)" << endl;
     cout << "4. Transfer money to another user" << endl;
     cout << "5. Exit" << endl;
     cout << "========================================" << endl;
 }
 
/*
 * Handle Registration
 * ==================================================================================
 * IMPORTANT: Uses persistent connection established at startup
 * ==================================================================================
 * Registers a new user with the server using the persistent connection.
 * The connection was established when the program started and will be reused
 * for all operations (Login, List, Transfer, Exit).
 *
 * Protocol: REGISTER#<username>#<amount>\r\n
 * Response: 100 OK\r\n (success) or 210 FAIL\r\n (failure)
 *
 * Note: According to the assignment specification, registration requires both
 * username and initial deposit amount.
 *
 * 重要：使用程式啟動時建立的持久連線
 * 使用程式啟動時建立的持久連線來註冊新使用者。此連線會被重複使用於
 * 所有操作（登入、查詢、轉帳、離線）。
 *
 * 注意：根據作業規格，註冊需要使用者名稱和初始存款金額。
 */
void handle_register() {
    cout << "\n--- Register ---" << endl;
    
    // Check if server connection is available
    if (server_socket == -1) {
        cout << "Error: Not connected to server." << endl;
        return;
    }
    
    // Check if already logged in (can't register if already logged in)
    if (is_logged_in) {
        cout << "You are already logged in. Please logout first if you want to register a new account." << endl;
        return;
    }
    
    cout << "Enter username: ";
    string user;
    getline(cin, user);
    
    cout << "Enter initial deposit amount: ";
    string amount_str;
    getline(cin, amount_str);
    int amount = stoi(amount_str);

    // Send registration message using persistent connection: REGISTER#username#amount\r\n
    // 使用持久連線發送註冊訊息
    string message = "REGISTER#" + user + "#" + to_string(amount) + CRLF;
    
    if (!send_message(server_socket, message)) {
        cout << "Failed to send registration request." << endl;
        return;
    }

    // Receive response from server
    string response = receive_message(server_socket);

    if (response.empty()) {
        cout << "No response from server." << endl;
        return;
    }

    // Parse response and inform user of result
    if (response.find("100 OK") != string::npos) {
        cout << "\nRegistration successful!" << endl;
        cout << "You can now login using option 2." << endl;
    } else if (response.find("210 FAIL") != string::npos) {
        cout << "\nRegistration failed. Username may already exist." << endl;
    } else {
        cout << "\nUnexpected response: " << response << endl;
    }
}
 
 /*
  * Handle Login
  * Logs in to the server using the existing persistent connection.
  * Uses the same connection established at program startup.
  * Protocol: <username>#<port>\r\n
  * Response: Multiple lines containing balance, public key, and online user list
  */
void handle_login() {
    cout << "\n--- Login ---" << endl;
    
    // Check if server connection is available
    if (server_socket == -1) {
        cout << "Error: Not connected to server." << endl;
        return;
    }
    
    // Check if already logged in
    if (is_logged_in) {
        cout << "You are already logged in. Please logout first (option 5) before logging in again." << endl;
        return;
    }
    
    cout << "Enter username: ";
    getline(cin, username);

    // Send login message using persistent connection: username#port\r\n
    // Port is where we're listening for P2P connections
    string message = username + "#" + to_string(my_port) + CRLF;
    if (!send_message(server_socket, message)) {
        cout << "Failed to send login request." << endl;
        username = "";
        return;
    }

    // Receive response from server
    string response = receive_message(server_socket);

    if (response.empty()) {
        cout << "No response from server." << endl;
        username = "";
        return;
    }

    // Check for authentication failure
    if (response.find("220 AUTH_FAIL") != string::npos) {
        cout << "\nLogin failed. Please register first." << endl;
        username = "";
        return;
    }

    // Parse response to extract balance and online users
    parse_online_list(response);
    is_logged_in = true;
    cout << "\nLogin successful!" << endl;
}
 
 /*
  * Handle List Request
  * Requests updated account balance and online user list from server.
  * Protocol: List\r\n
  * Response: Same format as login response
  */
 void handle_list() {
     // Check if user is logged in
     if (!is_logged_in) {
         cout << "Please login first." << endl;
         return;
     }
 
     cout << "\n--- Requesting updated list ---" << endl;
 
     // Send list request to server
     string message = "List" + string(CRLF);
     if (!send_message(server_socket, message)) {
         cout << "Failed to send list request." << endl;
         return;
     }
 
    // Receive response from server
    string response = receive_message(server_socket);

    if (response.empty()) {
        cout << "No response from server." << endl;
        return;
    }

    // DEBUG: Show what we received
    cout << "\n[DEBUG] Received response:" << endl;
    cout << "'" << response << "'" << endl;
    cout << "[DEBUG] Response length: " << response.length() << " bytes" << endl;

    // Parse and display updated information
    parse_online_list(response);
}
 
 /*
  * Handle Money Transfer (P2P)
  * Transfers money directly to another client without going through server.
  * This is the core P2P functionality - client connects directly to recipient.
  * Protocol: <sender>#<amount>#<recipient>\r\n
  */
 void handle_transfer() {
     // Check if user is logged in
     if (!is_logged_in) {
         cout << "Please login first." << endl;
         return;
     }
 
     cout << "\n--- Transfer Money ---" << endl;
     
     // Show list of online users (excluding ourselves)
     users_mutex.lock();
     if (online_users.empty()) {
         cout << "No other users online." << endl;
         users_mutex.unlock();
         return;
     }
 
     cout << "Online users:" << endl;
     int index = 1;
     vector<string> user_list;
     for (const auto& pair : online_users) {
         // Don't show ourselves in the list
         if (pair.first != username) {
             cout << index++ << ". " << pair.first << endl;
             user_list.push_back(pair.first);
         }
     }
     users_mutex.unlock();
 
     if (user_list.empty()) {
         cout << "No other users online." << endl;
         return;
     }
 
     // Get recipient username from user
     cout << "Enter recipient username: ";
     string recipient;
     getline(cin, recipient);
 
     // Check if recipient exists and is online
     users_mutex.lock();
     auto it = online_users.find(recipient);
     if (it == online_users.end()) {
         cout << "User not found or not online." << endl;
         users_mutex.unlock();
         return;
     }
     OnlineUser target_user = it->second;  // Copy recipient's connection info
     users_mutex.unlock();
 
     // Get transfer amount from user
     cout << "Enter amount to transfer: ";
     string amount_str;
     getline(cin, amount_str);
     int amount = stoi(amount_str);
 
     // Validate amount
     if (amount <= 0) {
         cout << "Invalid amount." << endl;
         return;
     }
 
     // Check if we have sufficient balance
     if (amount > account_balance) {
         cout << "Insufficient balance." << endl;
         return;
     }
 
     // P2P Connection: Connect directly to recipient's client
     cout << "Connecting to " << recipient << " at " << target_user.ip << ":" << target_user.port << "..." << endl;
     int peer_sock = connect_to_server(target_user.ip, target_user.port);
     if (peer_sock == -1) {
         cout << "Failed to connect to recipient." << endl;
         return;
     }
 
     // Send transfer message directly to recipient: sender#amount#recipient\r\n
     string transfer_msg = username + "#" + to_string(amount) + "#" + recipient + CRLF;
     if (!send_message(peer_sock, transfer_msg)) {
         cout << "Failed to send transfer request." << endl;
         close(peer_sock);
         return;
     }
 
     cout << "Transfer request sent to " << recipient << endl;
     close(peer_sock);  // Close P2P connection after sending
 
     // Wait for recipient to report transaction to server
     this_thread::sleep_for(chrono::milliseconds(500));
     
     // Request updated balance from server to reflect the transfer
     cout << "\nRequesting updated balance from server..." << endl;
     string list_msg = "List" + string(CRLF);
     if (send_message(server_socket, list_msg)) {
         string response = receive_message(server_socket);
         if (!response.empty()) {
             parse_online_list(response);
             cout << "Balance updated after transfer." << endl;
         } else {
             cout << "Warning: Could not retrieve updated balance from server." << endl;
         }
     } else {
         cout << "Warning: Failed to send list request to server." << endl;
     }
 }
 
 /*
  * Handle Exit
  * Logs out from server and exits the program gracefully.
  * Protocol: Exit\r\n
  * Response: Bye\r\n
  */
 void handle_exit() {
    cout << "\n--- Exiting ---" << endl;
    
    // If logged in, send proper logout notification to server
    if (is_logged_in && server_socket != -1) {
        cout << "Logging out..." << endl;
        
        // Send exit message to server
        string message = "Exit" + string(CRLF);
        if (send_message(server_socket, message)) {
            // Wait for server's goodbye response
            string response = receive_message(server_socket);
            if (response.find("Bye") != string::npos) {
                cout << "Logged out successfully." << endl;
            }
        }
    }
    
    // Close persistent server connection
    if (server_socket != -1) {
        close(server_socket);
        server_socket = -1;
    }
    
    // Mark logged out and stop the program
    is_logged_in = false;
    is_running = false;
    cout << "Goodbye!" << endl;
}
 
 /*
  * Connect to Server
  * Creates a TCP socket and connects to the specified IP and port.
  * Returns: socket file descriptor on success, -1 on failure
  */
 int connect_to_server(const string& ip, int port) {
     // Create TCP socket
     int sock = socket(AF_INET, SOCK_STREAM, 0);
     if (sock == -1) {
         perror("socket");
         return -1;
     }
 
     // Set up server address structure
     struct sockaddr_in server_addr;
     memset(&server_addr, 0, sizeof(server_addr));
     server_addr.sin_family = AF_INET;
     server_addr.sin_port = htons(port);  // Convert port to network byte order
     
     // Convert IP address from string to binary form
     if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
         perror("inet_pton");
         close(sock);
         return -1;
     }
 
     // Connect to server
     if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
         perror("connect");
         close(sock);
         return -1;
     }
 
     return sock;
 }
 
 /*
  * Send Message
  * Sends a message through the specified socket.
  * Returns: true on success, false on failure
  */
bool send_message(int sock, const string& message) {
    ssize_t sent = send(sock, message.c_str(), message.length(), 0);
    if (sent == -1) {
        perror("send");
        return false;
    }
    // cout << "[DEBUG] Actually sent " << sent << " bytes" << endl;
    return true;
}
 
 /*
  * Receive Message
  * Receives a message from the specified socket.
  * Returns: received message as string, empty string on error
  */
string receive_message(int sock) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    
    // cout << "[DEBUG] Calling recv() on socket " << sock << "..." << endl;
    ssize_t received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    // cout << "[DEBUG] recv() returned: " << received << endl;
    
    if (received == -1) {
        perror("recv");
        return "";
    } else if (received == 0) {
        // cout << "[DEBUG] Connection closed by peer" << endl;
        return "";
    }
    
    // cout << "[DEBUG] Received " << received << " bytes" << endl;
    return string(buffer, received);
}
 
 /*
  * Parse Online List
  * Parses server response containing balance and online user list.
  * Response format:
  *   Line 1: Account balance
  *   Line 2: Server public key
  *   Line 3: Number of online users
  *   Line 4+: username#ip#port for each online user
  */
 void parse_online_list(const string& response) {
     istringstream iss(response);
     string line;
     
    // First line: account balance
    if (getline(iss, line)) {
        // Remove any CR/LF characters
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
        
        cout << "[DEBUG] Line 1 (balance): '" << line << "'" << endl;
        
        if (!line.empty()) {
            account_balance = stoi(line);
            cout << "Account Balance: $" << account_balance << endl;
        } else {
            cout << "[ERROR] Empty balance line!" << endl;
        }
     }
     
    // Second line: server public key
    if (getline(iss, line)) {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
        
        cout << "[DEBUG] Line 2 (public key): '" << line << "'" << endl;
        
         server_public_key = line;
         // Don't print the full key, just indicate we received it
         if (!server_public_key.empty()) {
             cout << "Server Public Key: [Received]" << endl;
         }
     }
     
    // Third line: number of online users
    int num_users = 0;
    if (getline(iss, line)) {
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
        
        cout << "[DEBUG] Line 3 (num_users): '" << line << "'" << endl;
        
        if (!line.empty()) {
            num_users = stoi(line);
            cout << "Number of online users: " << num_users << endl;
        } else {
            cout << "[ERROR] Empty num_users line!" << endl;
        }
     }
     
     // Clear current online users map before updating
     users_mutex.lock();
     online_users.clear();
     
     // Parse each online user's information
     cout << "\nOnline Users:" << endl;
     cout << "----------------------------------------" << endl;
    for (int i = 0; i < num_users; i++) {
        if (getline(iss, line)) {
            line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
            line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
            
            cout << "[DEBUG] User line " << (i+1) << ": '" << line << "'" << endl;
             
             // Parse: username#ip#port
             size_t pos1 = line.find('#');
             size_t pos2 = line.find('#', pos1 + 1);
             
             if (pos1 != string::npos && pos2 != string::npos) {
                 OnlineUser user;
                 user.username = line.substr(0, pos1);
                 user.ip = line.substr(pos1 + 1, pos2 - pos1 - 1);
                 user.port = stoi(line.substr(pos2 + 1));
                 
                 cout << "[DEBUG] Parsed: username='" << user.username << "', ip='" << user.ip << "', port=" << user.port << endl;
                 
                 // Add user to online users map
                 online_users[user.username] = user;
                 cout << user.username << " @ " << user.ip << ":" << user.port << endl;
             } else {
                 cout << "[ERROR] Failed to parse user line (pos1=" << pos1 << ", pos2=" << pos2 << ")" << endl;
             }
         } else {
             cout << "[ERROR] Could not read user line " << (i+1) << endl;
         }
     }
     cout << "----------------------------------------" << endl;
     users_mutex.unlock();
 }
 
 /*
  * Listener Thread
  * Background thread that listens for incoming P2P connections from other clients.
  * When another client wants to transfer money to us, they connect to this listener.
  * For each incoming connection, spawns a new handler thread.
  */
 void listener_thread() {
     // Create listening socket
     listen_socket = socket(AF_INET, SOCK_STREAM, 0);
     if (listen_socket == -1) {
         perror("listener socket");
         return;
     }
 
     // Set socket option to reuse address (useful for quick restart)
     int opt = 1;
     if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
         perror("setsockopt");
         close(listen_socket);
         return;
     }
 
     // Bind socket to our listening port
     struct sockaddr_in listen_addr;
     memset(&listen_addr, 0, sizeof(listen_addr));
     listen_addr.sin_family = AF_INET;
     listen_addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all network interfaces
     listen_addr.sin_port = htons(my_port);     // Convert port to network byte order
 
     if (::bind(listen_socket, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) == -1) {
         perror("bind");
         close(listen_socket);
         return;
     }
 
     // Start listening for connections (queue up to 5 pending connections)
     if (listen(listen_socket, 5) == -1) {
         perror("listen");
         close(listen_socket);
         return;
     }
 
     safe_print("P2P listener started on port " + to_string(my_port));
 
     // Accept incoming connections in a loop
     while (is_running) {
         struct sockaddr_in client_addr;
         socklen_t client_len = sizeof(client_addr);
         
         // Accept incoming connection (blocking call)
         int client_sock = accept(listen_socket, (struct sockaddr*)&client_addr, &client_len);
         if (client_sock == -1) {
             if (is_running) {
                 perror("accept");
             }
             continue;
         }
 
         // Handle this client connection in a new thread
         thread handler(handle_client_connection, client_sock);
         handler.detach();  // Detach so handler runs independently
     }
 }
 
 /*
  * Handle Client Connection
  * Handles an incoming P2P transfer from another client.
  * Receives transfer message, updates local balance, and reports to server.
  * Protocol: <sender>#<amount>#<recipient>\r\n
  */
 void handle_client_connection(int client_sock) {
     // Receive transfer message from peer
     string message = receive_message(client_sock);
     
     if (message.empty()) {
         close(client_sock);
         return;
     }
 
     // Parse transfer message: sender#amount#recipient
     size_t pos1 = message.find('#');
     size_t pos2 = message.find('#', pos1 + 1);
     
     if (pos1 != string::npos && pos2 != string::npos) {
         string sender = message.substr(0, pos1);
         string amount_str = message.substr(pos1 + 1, pos2 - pos1 - 1);
        string recipient = message.substr(pos2 + 1);
        
        // Remove CRLF from recipient field
        recipient.erase(std::remove(recipient.begin(), recipient.end(), '\r'), recipient.end());
        recipient.erase(std::remove(recipient.begin(), recipient.end(), '\n'), recipient.end());
         
         int amount = stoi(amount_str);
         
         // Display transfer notification to user
         safe_print("\n*** Incoming Transfer ***");
         safe_print("From: " + sender);
         safe_print("Amount: $" + to_string(amount));
         safe_print("To: " + recipient);
         safe_print("************************\n");
         
         // Update local balance (optimistic update)
         account_balance += amount;
         safe_print("Transfer received. New balance: $" + to_string(account_balance));
         
         // Report transaction to server so it can update both accounts
         if (is_logged_in && server_socket != -1) {
             socket_mutex.lock();  // Lock because main thread may also use server_socket
             
             // Send transaction report: TRANSACTION#sender#recipient#amount\r\n
             string transaction_msg = "TRANSACTION#" + sender + "#" + recipient + "#" + amount_str + CRLF;
             if (send_message(server_socket, transaction_msg)) {
                 string response = receive_message(server_socket);
                 if (!response.empty()) {
                     safe_print("Server response: " + response);
                 } else {
                     safe_print("Warning: No response from server for transaction report");
                 }
             } else {
                 safe_print("Warning: Failed to report transaction to server");
             }
             
             socket_mutex.unlock();
         } else {
             safe_print("Warning: Not logged in, transaction not reported to server");
         }
     }
     
     // Close P2P connection
     close(client_sock);
 }
 
 /*
  * Safe Print
  * Thread-safe printing function that uses mutex to prevent output mixing.
  * Multiple threads may want to print simultaneously (main thread, listener thread, handler threads).
  */
 void safe_print(const string& message) {
     lock_guard<mutex> lock(cout_mutex);
     cout << message << endl;
 }
 
 
 