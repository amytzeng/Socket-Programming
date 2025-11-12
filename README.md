# P2P 小額支付系統 - Client 端程式

## 課程資訊

- **課程名稱**: 計算機網路
- **學期**: 2025 年秋季班
- **授課教師**: Yeali S. Sun 教授
- **作業階段**: 第一階段 - Client 端實作

---

## 目錄

1. [專案概述](#專案概述)
2. [開發環境](#開發環境)
3. [編譯與執行](#編譯與執行)
4. [程式功能](#程式功能)
5. [程式架構](#程式架構)
6. [協定實作](#協定實作)
7. [測試指南](#測試指南)
8. [問題排除](#問題排除)

---

## 專案概述

### 作業目標

本專案實作一個 P2P（Peer-to-Peer）小額支付系統的 Client 端程式。在這個系統中，Server 主要負責使用者管理和資訊查詢，而實際的轉帳操作則是在 Client 之間直接進行，不經過 Server 轉送。

### 核心功能需求

根據作業規格，Client 端程式必須實作以下功能：

**註冊功能** - 使用者可以向 Server 註冊新帳號，輸入使用者名稱後，Server 會為該帳號建立初始餘額（預設為 10000 元）。註冊過程使用短暫連線，註冊完成後即關閉該連線。

**登入功能** - 使用者以註冊的名稱登入 Server，同時告知 Server 自己用來接收 P2P 轉帳的 port number。登入成功後會建立持續性的連線，並接收 Server 回傳的帳戶餘額、Server 的 public key、以及目前的線上使用者清單（包含每個使用者的 IP 位址和 port number）。

**查詢清單功能** - 已登入的使用者可以隨時向 Server 請求最新的帳戶餘額和線上使用者清單。這個功能讓使用者能夠確認自己的餘額，以及查看目前有哪些使用者在線上可以進行轉帳。

**P2P 轉帳功能** - 這是本作業的核心功能。當使用者要轉帳給另一個線上使用者時，Client 程式會從先前取得的線上清單中找到對方的 IP 位址和 port number，然後直接建立 TCP 連線到對方的 Client 程式，發送轉帳訊息。整個轉帳過程不經過 Server，這是典型的 P2P 通訊架構。收款方的 Client 收到轉帳訊息後，會向 Server 報告這筆交易，讓 Server 更新雙方的帳戶餘額記錄。

**離線通知功能** - 使用者結束程式前必須主動通知 Server 即將離線。Client 發送離線訊息給 Server，等待 Server 回應確認後，才關閉所有連線並結束程式。這確保 Server 能及時更新線上清單，移除已離線的使用者。

### 技術特色

**多執行緒架構** - 程式採用多執行緒設計，主執行緒負責使用者介面和與 Server 的通訊，另有一個專門的 listener 執行緒持續監聽其他 Client 的連線請求。當有其他 Client 連線進來進行轉帳時，會動態建立新的 handler 執行緒來處理該轉帳請求。這種設計確保程式可以同時處理多個任務而不會互相阻塞。

**標準 Socket API** - 使用 POSIX 標準的 socket API 進行網路通訊，包括 TCP socket 的建立、連線、監聽、接受連線、發送和接收資料等操作。程式同時扮演 client 和 server 的雙重角色：作為 client 連接到 Server 和其他 Client，作為 server 接受其他 Client 的轉帳連線。

**完整的錯誤處理** - 所有的 socket 操作都包含錯誤檢查和適當的錯誤處理機制，確保在網路異常時程式能夠優雅地處理而不會當機。錯誤訊息會清楚地告知使用者發生了什麼問題。

---

## 開發環境

### 作業系統

本程式在以下環境中開發和測試：

- **macOS** (Darwin 24.5.0) - 開發環境
- **Linux** (Ubuntu 或其他發行版) - 相容且推薦用於最終測試

程式使用標準的 POSIX API，理論上可在任何支援 POSIX 的類 Unix 系統上執行。

### 編譯器

- **g++** (GNU C++ Compiler)
- 支援 C++11 標準（使用 `-std=c++11` 編譯選項）
- 需要支援 pthread 執行緒函式庫

### 相依函式庫

程式使用的都是標準函式庫，不需要額外安裝第三方套件：

- **標準 C++ 函式庫**: iostream, string, vector, map, sstream 等
- **POSIX Socket API**: sys/socket.h, netinet/in.h, arpa/inet.h
- **POSIX Thread**: pthread (透過 -pthread 編譯選項連結)
- **Unix 系統呼叫**: unistd.h (用於 close, read, write 等函式)

---

## 編譯與執行

### 編譯程式

本專案提供 Makefile 自動化編譯流程。在終端機中切換到專案目錄後，執行以下指令：

```bash
make
```

編譯成功後會產生 `client` 可執行檔。如果編譯過程出現錯誤，請確認你的系統已安裝 g++ 編譯器，並且支援 C++11 標準。

### 清除編譯產物

如果需要重新編譯或清理編譯產生的檔案，可以執行：

```bash
make clean
```

這會刪除 `client` 可執行檔和 `client.o` 目標檔。

### 重新編譯

如果想要先清理再重新編譯，可以使用：

```bash
make rebuild
```

這相當於依序執行 `make clean` 和 `make`。

### 執行程式

編譯完成後，執行 Client 程式：

```bash
./client
```

程式啟動後會要求你輸入三項連線資訊：

1. **Server IP address** - Server 的 IP 位址（本機測試時輸入 `127.0.0.1`）
2. **Server Port** - Server 監聽的 port number（例如 `12345`，需配合 Server 啟動時指定的 port）
3. **Your listening port for P2P connections** - 你的 Client 用來接收其他 Client 轉帳的 port（例如 `9000`，必須與 Server port 不同）

輸入完成後會進入主選單，你可以選擇要執行的功能。

---

## 程式功能

### 主選單

程式啟動並完成初始設定後，會顯示以下選單：

```
========================================
              Main Menu                 
========================================
1. Register
2. Login
3. List (Get account balance and online users)
4. Transfer money to another user
5. Exit
========================================
```

以下詳細說明每個功能的使用方式。

### 1. 註冊 (Register)

**使用時機**: 第一次使用系統時需要先註冊帳號。

**操作步驟**:
1. 在主選單輸入 `1`
2. 輸入你想要的使用者名稱（建議使用英數字元）
3. 等待 Server 回應

**成功情況**: 系統顯示 "Registration successful!"，表示帳號已建立，初始餘額為 10000 元。

**失敗情況**: 系統顯示 "Registration failed"，可能的原因包括：使用者名稱已被註冊、使用者名稱格式不符、或 Server 發生錯誤。

**重要提醒**: 註冊是一次性操作，使用短暫連線。註冊完成後該連線會立即關閉，這是正常行為。註冊後需要使用登入功能才能使用其他功能。

### 2. 登入 (Login)

**使用時機**: 註冊完成後，使用登入功能建立與 Server 的持續性連線。

**操作步驟**:
1. 在主選單輸入 `2`
2. 輸入你已註冊的使用者名稱
3. 等待 Server 回應並接收資料

**成功情況**: 系統會顯示：
- 你的帳戶餘額
- Server 的 public key（第一階段顯示 [Received] 即可）
- 目前線上的使用者數量
- 每個線上使用者的詳細資訊（名稱、IP 位址、port number）

**失敗情況**: 系統顯示 "Login failed. Please register first."，表示該使用者名稱尚未註冊。

**重要提醒**: 登入成功後會保持與 Server 的連線，直到你選擇離線為止。只有登入後才能使用查詢清單、轉帳等功能。

### 3. 查詢清單 (List)

**使用時機**: 登入後想要確認最新的帳戶餘額或查看目前有哪些使用者在線上時使用。

**操作步驟**:
1. 確認已經登入
2. 在主選單輸入 `3`
3. 等待 Server 回應

**顯示資訊**:
- 你目前的帳戶餘額（會反映最新的轉帳記錄）
- Server 的 public key
- 目前線上的使用者清單（包含你自己）

**使用情境**: 當你完成轉帳後想確認餘額是否正確更新、或者想要查看是否有新的使用者上線可以進行轉帳時，都可以使用這個功能。

### 4. 轉帳 (Transfer)

**使用時機**: 登入後想要轉帳給其他線上使用者。

**操作步驟**:
1. 確認已經登入
2. 在主選單輸入 `4`
3. 系統會顯示目前線上的其他使用者清單（不包含你自己）
4. 輸入收款人的使用者名稱
5. 輸入轉帳金額（必須是正整數）
6. 系統會檢查餘額是否足夠
7. 如果餘額足夠，系統會直接連線到收款人的 Client 並發送轉帳訊息

**成功情況**: 系統顯示 "Transfer request sent to [收款人]"，並在短暫等待後自動向 Server 查詢更新後的餘額。

**失敗情況**: 可能的錯誤包括：
- 收款人不在線上或不存在
- 轉帳金額無效（小於等於 0）
- 餘額不足
- 無法連線到收款人的 Client（對方網路問題或防火牆阻擋）

**技術細節**: 這個功能展現了 P2P 架構的核心概念。轉帳訊息是直接從你的 Client 傳送到對方的 Client，不經過 Server。Server 只在事後收到收款方的交易報告，用來更新帳戶餘額記錄。

**重要提醒**: 
- 收款方必須在線上才能接收轉帳
- 轉帳前建議先使用查詢清單功能確認對方在線上
- 轉帳金額不能超過你目前的餘額

### 5. 離線 (Exit)

**使用時機**: 結束程式前必須使用此功能正常離線。

**操作步驟**:
1. 在主選單輸入 `5`
2. 如果你已登入，系統會先通知 Server 你即將離線
3. 等待 Server 回應確認
4. 關閉所有連線
5. 程式結束

**重要性**: 正確的離線流程可以確保：
- Server 及時更新線上清單，移除你的資訊
- 其他使用者不會誤以為你還在線上而嘗試轉帳給你
- 所有連線被正確關閉，避免資源洩漏

**注意事項**: 不要直接強制關閉程式（例如 Ctrl+C），請務必使用選單的離線功能正常結束程式。

---

## 程式架構

### 整體設計概念

本程式採用典型的 Client-Server 混合 P2P 架構。程式同時扮演兩個角色：作為 Client 連接到 Server 進行使用者管理相關操作；作為 Server 接受其他 Client 的 P2P 轉帳連線。這種雙重角色的設計需要使用多執行緒技術來同時處理不同的任務。

### 執行緒架構

**主執行緒 (Main Thread)** - 負責使用者介面的呈現和互動，包括顯示選單、接收使用者輸入、執行對應的功能（註冊、登入、查詢、轉帳、離線）、以及與 Server 的通訊。這是程式的控制中心，所有使用者發起的操作都在這個執行緒中執行。

**監聽執行緒 (Listener Thread)** - 在程式啟動時就會建立並在背景持續執行。這個執行緒負責監聽指定的 port，等待其他 Client 的連線請求。當有其他 Client 要轉帳給你時，他們會連接到這個 port。監聽執行緒使用 blocking accept() 呼叫，當有新連線時會立即接受該連線。

**處理執行緒 (Handler Threads)** - 當監聽執行緒接受一個新的轉帳連線時，會動態建立一個新的處理執行緒來處理該筆轉帳。這個執行緒會接收轉帳訊息、解析內容、更新本地餘額、向 Server 報告交易、然後結束。採用這種設計的好處是可以同時處理多筆轉帳，不會因為處理一筆轉帳而阻塞其他轉帳請求。

### Socket 管理

**Server Socket** - 用來連接到 Server 的 socket，在登入時建立，離線時關閉。這是一個持續性的連線，用於發送查詢清單、離線通知等訊息，以及接收 Server 的回應。主執行緒和處理執行緒都可能使用這個 socket（例如報告交易時），因此需要使用 mutex 來保護，避免同時發送造成資料混亂。

**Listening Socket** - 用來監聽其他 Client 連線的 socket，在程式啟動時建立並綁定到使用者指定的 port。這個 socket 只由監聽執行緒使用，一直保持在 listening 狀態直到程式結束。

**Peer Socket** - 當主執行緒要發起轉帳時，會建立一個新的 socket 連接到目標 Client，發送轉帳訊息後即關閉。這是短暫連線，用完就釋放。同樣地，當監聽執行緒接受一個連線時，也會得到一個 peer socket 用來接收對方的轉帳訊息，處理完畢後關閉。

### 資料結構

**OnlineUser 結構** - 儲存線上使用者的資訊，包含 username（使用者名稱）、ip（IP 位址字串）、port（port number 整數）。這些資訊來自 Server 的線上清單回應，用於轉帳時查找目標使用者的連線位址。

**online_users Map** - 使用 C++ STL 的 map 容器，以使用者名稱為 key，OnlineUser 結構為 value。這樣可以快速查找特定使用者的連線資訊。因為主執行緒和監聽執行緒都可能讀取這個 map（主執行緒在轉帳時查詢，Server 回應時更新），需要使用 mutex 保護以確保執行緒安全。

### 同步機制

**cout_mutex** - 保護標準輸出的 mutex。因為多個執行緒都可能輸出訊息到終端機（主執行緒顯示選單和回應，處理執行緒顯示收到轉帳的通知），為了避免輸出混亂，使用 mutex 確保一次只有一個執行緒可以輸出。

**users_mutex** - 保護 online_users map 的 mutex。當主執行緒查詢或更新線上清單時會鎖定這個 mutex，確保資料的一致性。

**socket_mutex** - 保護 server_socket 的 mutex。當處理執行緒需要向 Server 報告交易時，會鎖定這個 mutex，避免與主執行緒同時使用 server_socket 造成資料混亂。

---

## 協定實作

本系統使用明確定義的文字協定進行通訊。所有訊息都以 ASCII 文字編碼，使用 `#` 符號分隔欄位，並以 `\r\n`（CRLF）作為訊息結束標記。

### 訊息格式規則

**CRLF 行結束符號** - 所有訊息必須以 Carriage Return (CR, `\r`, ASCII 13) 加上 Line Feed (LF, `\n`, ASCII 10) 結束。這是網路協定的標準慣例，程式中定義為 `#define CRLF "\r\n"`。

**欄位分隔符號** - 使用 `#` 字元來分隔訊息中的不同欄位。例如 `username#port` 或 `sender#amount#recipient`。

**字元編碼** - 所有訊息使用 ASCII 文字編碼，不包含二進位資料（第一階段不涉及加密）。使用者名稱區分大小寫，建議使用英數字元。

### Client-Server 協定

#### 1. 註冊

**請求格式**:
```
REGISTER#<username>\r\n
```

**成功回應**:
```
100 OK\r\n
```

**失敗回應**:
```
210 FAIL\r\n
```

**範例**:
```
Client -> Server: REGISTER#alice\r\n
Server -> Client: 100 OK\r\n
```

**說明**: 註冊是短暫連線，Server 回應後會立即關閉連線。使用者名稱不可重複，Server 會為新使用者建立帳戶並設定初始餘額 10000 元。

#### 2. 登入

**請求格式**:
```
<username>#<portNum>\r\n
```

**成功回應**:
```
<accountBalance>\r\n
<serverPublicKey>\r\n
<numberOfAccountsOnline>\r\n
<userAccount1>#<ipAddr1>#<port1>\r\n
<userAccount2>#<ipAddr2>#<port2>\r\n
...
```

**失敗回應**:
```
220 AUTH_FAIL\r\n
```

**範例**:
```
Client -> Server: alice#9000\r\n
Server -> Client: 10000\r\n
                 SERVER_PUBLIC_KEY_ABC123\r\n
                 2\r\n
                 alice#127.0.0.1#9000\r\n
                 bob#127.0.0.1#9001\r\n
```

**說明**: 登入建立持續性連線。第一行是帳戶餘額，第二行是 Server 的 public key（第一階段接收但不使用），第三行是線上人數，後續每行是一個線上使用者的資訊（名稱、IP、port）。

#### 3. 查詢清單

**請求格式**:
```
List\r\n
```

**回應格式**: 與登入成功回應相同。

**範例**:
```
Client -> Server: List\r\n
Server -> Client: 9500\r\n
                 SERVER_PUBLIC_KEY_ABC123\r\n
                 3\r\n
                 alice#127.0.0.1#9000\r\n
                 bob#127.0.0.1#9001\r\n
                 charlie#127.0.0.1#9002\r\n
```

**說明**: 查詢清單使用持續性的 server socket，可以隨時呼叫。回應的餘額會反映最新的轉帳記錄。

#### 4. 交易報告

**請求格式**:
```
TRANSACTION#<sender>#<recipient>#<amount>\r\n
```

**說明**: 這是收款方在收到 P2P 轉帳後向 Server 報告交易。Server 收到後會更新付款方和收款方的帳戶餘額。這個訊息由程式自動發送，使用者不需要手動操作。

#### 5. 離線

**請求格式**:
```
Exit\r\n
```

**回應**:
```
Bye\r\n
```

**範例**:
```
Client -> Server: Exit\r\n
Server -> Client: Bye\r\n
```

**說明**: 離線通知讓 Server 將該使用者從線上清單中移除。收到 Bye 回應後才關閉連線並結束程式。

### Client-Client (P2P) 協定

#### 轉帳訊息

**格式**:
```
<senderUsername>#<amount>#<recipientUsername>\r\n
```

**範例**:
```
Client A -> Client B: alice#500#bob\r\n
```

**說明**: P2P 轉帳是單向訊息，付款方直接連線到收款方並發送此訊息。第一階段不需要收款方回應確認。收款方收到後會自動向 Server 報告交易（使用 TRANSACTION 訊息），然後 Server 會更新雙方餘額。

**重要提醒**: 這個訊息是在兩個 Client 之間直接傳遞的，不經過 Server。這正是 P2P 架構的核心特色。

---

## 測試指南

### 準備測試環境

在測試前需要準備以下項目：

**編譯 Client 程式** - 確認你的 Client 程式已成功編譯，執行 `make` 確保沒有編譯錯誤。

**準備 Server 程式** - 你需要一個 Server 程式來測試。可以使用助教提供的 Server Binary（`macos_server_arm` 適用於 Apple Silicon Mac），或是自己編譯的 test_server（執行 `make test_server`）。

**開啟多個終端機視窗** - 測試時需要同時執行 Server 和一個或多個 Client，建議開啟 2-3 個終端機視窗。

### 基本功能測試（單一 Client）

這個測試只需要一個 Server 和一個 Client，可以驗證註冊、登入、查詢、離線等基本功能。

**步驟 1: 啟動 Server**

在第一個終端機視窗執行：
```bash
./macos_server_arm 12345
```

這會讓 Server 監聽 port 12345。注意不要使用 8888 等常見 port，因為可能已被其他程式佔用。保持這個視窗開啟，觀察 Server 的日誌輸出。

**步驟 2: 啟動 Client**

在第二個終端機視窗執行：
```bash
./client
```

當程式詢問連線資訊時輸入：
- Server IP: `127.0.0.1`
- Server Port: `12345`
- Your listening port: `9000`

**步驟 3: 測試註冊**

選擇選單的 `1`，輸入使用者名稱（例如 `alice`）。應該看到 "Registration successful!" 訊息。在 Server 的視窗應該能看到註冊的日誌。

**步驟 4: 測試登入**

選擇選單的 `2`，輸入剛才註冊的使用者名稱 `alice`。登入成功後應該看到：
- 帳戶餘額: $10000
- Server Public Key: [Received]
- 線上使用者: 1（只有你自己）

**步驟 5: 測試查詢**

選擇選單的 `3`，應該再次看到相同的資訊（餘額、線上清單）。

**步驟 6: 測試離線**

選擇選單的 `5`，應該看到 "Logged out successfully" 和 "Goodbye!" 訊息。程式正常結束。

### P2P 轉帳測試（多個 Client）

這個測試需要兩個 Client 同時運行，可以驗證最重要的 P2P 轉帳功能。

**步驟 1: Server 保持運行**

使用前一個測試中已啟動的 Server，或重新啟動 Server。

**步驟 2: 啟動第一個 Client (alice)**

在第二個終端機視窗執行 `./client`，輸入連線資訊後：
- 選擇 `1` 註冊使用者 `alice`（如果之前已註冊可跳過）
- 選擇 `2` 登入使用者 `alice`

**步驟 3: 啟動第二個 Client (bob)**

在第三個終端機視窗執行 `./client`，輸入連線資訊時注意 listening port 要不同（例如 `9001`），然後：
- 選擇 `1` 註冊使用者 `bob`
- 選擇 `2` 登入使用者 `bob`

**步驟 4: 確認雙方都在線上**

在 alice 的終端機視窗選擇 `3` 查詢清單，應該看到線上使用者有 2 人：alice 和 bob。在 bob 的視窗做同樣操作確認。

**步驟 5: alice 轉帳給 bob**

在 alice 的終端機視窗：
- 選擇 `4` 進行轉帳
- 輸入收款人: `bob`
- 輸入金額: `500`

**步驟 6: 觀察結果**

在 bob 的終端機視窗應該立即看到收到轉帳的通知訊息，顯示來自 alice 的 500 元。在 alice 的視窗應該看到 "Transfer request sent to bob" 訊息。

**步驟 7: 確認餘額更新**

在兩個 Client 視窗分別選擇 `3` 查詢餘額：
- alice 的餘額應該是 9500（10000 - 500）
- bob 的餘額應該是 10500（10000 + 500）

**步驟 8: 測試反向轉帳**

可以讓 bob 轉帳給 alice，驗證雙向轉帳都能正常運作。

**步驟 9: 清理**

在兩個 Client 視窗分別選擇 `5` 離線，正常結束程式。

### 錯誤情況測試

為了確保程式的健壯性，建議也測試一些錯誤情況：

**重複註冊** - 嘗試註冊已存在的使用者名稱，應該看到 "Registration failed" 訊息。

**未註冊登入** - 嘗試登入一個不存在的使用者名稱，應該看到 "Login failed. Please register first." 訊息。

**餘額不足** - 嘗試轉帳超過你餘額的金額，應該看到 "Insufficient balance" 訊息。

**收款人不存在** - 嘗試轉帳給一個不存在或離線的使用者，應該看到錯誤訊息。

**連線失敗** - 如果 Server 沒有運行就執行 Client，應該看到 "Failed to connect to server" 訊息。

### 常見測試問題

**Port 已被佔用** - 如果 Server 無法啟動，可能是 port 被其他程式佔用。可以用 `lsof -i :<port>` 檢查，或換一個不同的 port number。

**Client 無法連接** - 確認 Server 正在運行，且你輸入的 IP 和 port 正確。本機測試時 IP 應該是 `127.0.0.1`。

**轉帳失敗** - 確認收款人已經登入且在線上。可以先用查詢清單功能確認對方的狀態。同時確認雙方的 listening port 沒有被防火牆阻擋。

---

## 問題排除

### 啟用 Debug 模式

程式中預設已將所有 debug 訊息註解掉，以保持輸出的簡潔。如果在開發或除錯時需要查看詳細的網路通訊過程，可以手動啟用 debug 訊息。

**啟用方法**：編輯 `client.cpp` 檔案，搜尋所有被註解的 `// cout << "[DEBUG]"` 行，移除開頭的 `//` 即可啟用。

**Debug 訊息位置**：

程式中共有 12 處 debug 訊息被註解，分布在以下位置：

**註冊功能 (handle_register 函式)**：
- 第 172 行：連線嘗試訊息
- 第 181 行：連線成功訊息
- 第 185-186 行：發送訊息內容和長度
- 第 194 行：等待回應訊息
- 第 199-200 行：收到回應的內容和長度

**網路通訊函式**：
- 第 495 行 (send_message 函式)：實際發送的位元組數
- 第 508 行 (receive_message 函式)：呼叫 recv() 訊息
- 第 510 行 (receive_message 函式)：recv() 回傳值
- 第 516 行 (receive_message 函式)：連線關閉訊息
- 第 520 行 (receive_message 函式)：接收的位元組數

**啟用範例**：
```cpp
// 原本（已註解）：
// cout << "[DEBUG] Attempting to connect to " << server_ip << ":" << server_port << endl;

// 啟用後（移除註解）：
cout << "[DEBUG] Attempting to connect to " << server_ip << ":" << server_port << endl;
```

**啟用後需重新編譯**：
```bash
make rebuild
```

啟用 debug 模式後，程式會顯示詳細的網路通訊資訊，包括：
- Socket 連線建立過程
- 發送的訊息內容和長度
- 接收的回應內容和長度
- 實際傳輸的位元組數

這些資訊對於診斷網路連線問題、協定格式錯誤等狀況非常有幫助。

### 編譯相關問題

**找不到 pthread 函式庫**

錯誤訊息可能是 "undefined reference to pthread_create" 或類似訊息。這表示沒有正確連結 pthread 函式庫。

解決方法：確認 Makefile 中的 CXXFLAGS 包含 `-pthread` 選項。目前的 Makefile 已經包含此選項，如果仍有問題，可能是編譯器版本過舊。

**C++11 標準不支援**

錯誤訊息可能是關於 C++11 語法的錯誤（例如 thread, mutex 等）。

解決方法：確認你的 g++ 版本支援 C++11（通常 g++ 4.8 以上都支援）。可以執行 `g++ --version` 檢查版本。Makefile 已包含 `-std=c++11` 選項。

**Socket 相關標頭檔找不到**

錯誤訊息是找不到 sys/socket.h 或相關標頭檔。

解決方法：這些是 POSIX 標準標頭檔，應該在所有類 Unix 系統都有。如果在 Windows 上編譯，需要使用 WSL (Windows Subsystem for Linux) 或安裝 MinGW 等工具。

### 執行相關問題

**連線被拒絕 (Connection refused)**

這表示無法連接到指定的 IP 和 port。

可能原因和解決方法：
- Server 沒有在運行：確認 Server 程式已啟動並監聽指定的 port
- IP 或 port 輸入錯誤：檢查你輸入的連線資訊是否正確
- 防火牆阻擋：檢查防火牆設定，本機測試時應該不會有這個問題

**Port already in use (Address already in use)**

這表示你指定的 port 已經被其他程式佔用。

解決方法：
- 檢查是否有其他 Client 或 Server 實例在運行
- 使用 `lsof -i :<port>` 查看哪個程式佔用該 port
- 換一個不同的 port number（建議使用 10000 以上的數字）

**Segmentation fault 或程式當機**

這通常表示記憶體存取錯誤或多執行緒同步問題。

除錯方法：
- 重新編譯時加上 `-g` 選項以包含除錯資訊
- 使用 gdb 或 lldb 除錯器執行程式找出當機位置
- 檢查是否有 race condition（多個執行緒同時存取共享資料）
- 確認所有 socket 操作都有檢查回傳值

**收不到轉帳訊息**

P2P 轉帳時付款方顯示成功但收款方沒有收到通知。

可能原因和解決方法：
- 收款方的 listening port 被防火牆阻擋：檢查防火牆設定
- 收款方輸入的 listening port 與登入時告知 Server 的不一致：確認兩者相同
- 網路延遲：稍等片刻，P2P 連線可能需要一些時間建立
- 收款方的 listener 執行緒沒有正常啟動：檢查程式啟動時是否顯示 "P2P listener started" 訊息

### 協定相關問題

**Server 回應格式解析錯誤**

程式顯示 "No response from server" 或解析出來的資料不正確。

可能原因和解決方法：
- CRLF 格式問題：確認所有發送的訊息都以 `\r\n` 結束
- 接收 buffer 太小：目前程式使用 4096 bytes 的 buffer，一般情況下應該足夠
- Server 版本不符：確認使用的是助教提供的正確版本 Server
- 網路問題導致訊息不完整：可以增加接收逾時或重試機制

**訊息格式不符規範**

Server 回應錯誤碼（如 230 Input format error）。

解決方法：
- 檢查你發送的訊息格式是否完全符合協定規範
- 確認欄位分隔符號是 `#` 而不是其他字元
- 確認訊息以 `\r\n` 結束，不是只有 `\n`
- 使用 Wireshark 或 tcpdump 抓取網路封包檢查實際發送的內容

### 其他常見問題

**macOS Server 無法執行**

雙擊或執行 macos_server_arm 時顯示權限錯誤或 "文件已損壞"。

解決方法：
- 確認已設定執行權限：`chmod +x macos_server_arm`
- macOS 安全性設定可能阻擋：在「系統偏好設定 > 安全性與隱私權」中允許執行
- Apple Silicon Mac 需要 ARM 版本，Intel Mac 需要 x86 版本

**多個 Client 測試時選單輸入混亂**

這是因為背景執行緒的輸出與主執行緒的選單混在一起。

這是正常現象：程式使用 mutex 保護輸出，但無法阻止背景輸出插入到你正在輸入的過程中。如果影響操作，可以在收到轉帳通知後重新選擇功能即可。

改進建議：可以考慮實作一個更複雜的 UI 系統（例如使用 ncurses 函式庫），但這超出第一階段作業的範圍。

---

## 參考資料

本程式開發過程中參考的資料來源：

### Socket Programming

- **Beej's Guide to Network Programming** - 經典的 socket programming 教學文件
- **POSIX Socket API 手冊** - 系統標準文件（man pages）
- Stevens, W. Richard. "Unix Network Programming" - 網路程式設計權威書籍

### 多執行緒程式設計

- **POSIX Threads (pthread) 官方文件**
- C++11 標準執行緒函式庫參考文件
- "C++ Concurrency in Action" by Anthony Williams

### 協定設計

- RFC 文件關於網路協定設計的指引
- 課程講義關於 P2P 架構的說明

---

## 作者資訊

此程式為計算機網路課程 2025 年秋季班第一階段作業。如有任何問題或建議，請洽詢授課教師或助教。

---

**最後更新日期**: 2025 年 11 月
