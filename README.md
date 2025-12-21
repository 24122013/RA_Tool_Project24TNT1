# RA Tool – Remote Administration Tool

> **Mục đích học tập – nghiên cứu**
> Dự án mô phỏng hệ thống quản trị & điều khiển máy tính từ xa (Remote Administration Tool – RAT) thông qua giao diện Web theo mô hình **Server – Client**, phục vụ đồ án môn học **Mạng Máy Tính**

## ✨ Tác giả

* ***Nguyễn Đức Tâm: 24122013***
* ***Nguyễn Tuấn Lâm: 24122006***
* ***Nguyễn Nguyễn Trâm Anh: 24122027***


## 📌 Giới thiệu

**RAT** là một hệ thống cho phép:

* Máy **Server** lắng nghe kết nối
* Máy **Client** kết nối đến Server để gửi/nhận lệnh
* Thực hiện các chức năng quản trị từ xa (theo phạm vi code của dự án)

Dự án được viết chủ yếu bằng **C++**, chạy trên **Windows**, sử dụng **socket TCP/UDP**.

---

## 📋 Mục lục

1. [Yêu cầu hệ thống](#-yêu-cầu-hệ-thống)
2. [Cấu trúc thư mục](#-cấu-trúc-thư-mục)
3. [Hướng dẫn build](#-hướng-dẫn-build)
4. [Chạy Server](#-chạy-server)
5. [Chạy Client](#-chạy-client)
6. [Chạy Web](#-chạy-web)
7. [Mở port & cấu hình Firewall](#-mở-port--cấu-hình-firewall)
8. [Hướng dẫn sử dụng & chức năng](#-hướng-dẫn-sử-dụng--chức-năng)
9. [Lưu ý bảo mật & pháp lý](#-lưu-ý-bảo-mật--pháp-lý)

---

## 🧾 Yêu cầu hệ thống

### Phần mềm

* Windows 10/11 (khuyến nghị)
* **MinGW / MSYS2 / Visual Studio (MSVC)**
* Git
* (Tuỳ chọn) Visual Studio Code

### Kiến thức nền

* C++ cơ bản
* Socket TCP/IP
* Giao thức TCP/UDP
* Command line
* Web Socket
* UI/UX cơ bản

---

## 📁 Cấu trúc thư mục

```
RA_Tool_Project24TNT1/
│
├── Server/          # Mã nguồn Server
├── Client/          # Mã nguồn Client
├── Web/             # Giao diện Web
├── README.md
└── .gitignore
```

---

## 🛠️ Hướng dẫn build

### 1️⃣ Clone project

```bash
git clone https://github.com/24122013/RA_Tool_Project24TNT1.git
cd RA_Tool_Project24TNT1
```

---

### 2️⃣ Build bằng **MinGW (g++)**

#### Build Server

```bash
cd Server
g++ server.cpp -o .\server.exe -lws2_32 -lgdiplus -lvfw32 -lpsapi -lshell32 -ladvapi32 -lole32 -lgdi32 -lcrypt32 -luser32
```

Hoặc nếu muốn server không phải tự build thì Client có thể build theo lệnh này và gửi file .exe cho server tự chạy (Lúc này đã tích hợp tính năng giấu cửa sổ console, nếu không muốn thì hãy bỏ tag ***-mwindows*** trong lệnh). 

[Lưu ý: máy server có thể có nhiều card mạng và tính năng broadcast có thể sẽ ưu tiên card mạng khác và "hét" vào sai đường mạng, khi đó bên web client sẽ không tự động phát hiện IP server, nhưng vẫn có thể kết nối thủ công nếu biết IP LAN của máy server.]

```bash
cd Server
g++ server.cpp -o .\server.exe -static -static-libgcc -static-libstdc++ -mwindows -lws2_32 -lgdiplus -lvfw32 -lpsapi -lshell32 -ladvapi32 -lole32 -lgdi32 -lcrypt32 -luser32
```

#### Build Client

```bash
cd Client
g++ .\client.cpp -o .\client.exe -lws2_32 -lcrypt32

```

### 3️⃣ Build bằng **Visual Studio** (tuỳ chọn)

1. Mở `Server.cpp` hoặc `Client.cpp`
2. Tạo project Console Application
3. Thêm file `.cpp` vào project
4. Build → Run

---

## ▶️ Chạy Server

Trên máy Server:

```bash
cd Server
.\server.exe
```

Server sẽ:

* Mở port lắng nghe (5656)
* Chờ client kết nối


## ▶️ Chạy Client

Trên máy Client:

```bash
cd Client
.\client.exe 
```

Client sẽ:

* Mở TCP Socket port 5656 để giao tiếp với những server xung quanh
* Mở Web Socket (localhost: port 8080) để kết nối với giao diện Web Browser


## ▶️ Mở Web

```bash
cd Web
.\index.html
```

1 giao diện Web sẽ hiện lên và yêu cầu chọn địa chỉ IP của Server, hoặc nhập thủ công để kết nối


## 🔓 Mở port & cấu hình Firewall

### 🔹 Trên Windows (Server)

1. Mở **Windows Defender Firewall**
2. Chọn **Advanced settings**
3. **Inbound Rules → New Rule**
4. Chọn **Port** → TCP
5. Nhập port (5656)
6. Chọn **Allow the connection**
7. Apply cho Domain / Private / Public

---

### 🔹 Tắt Firewall (chỉ để test – KHÔNG khuyến nghị)

```bash
netsh advfirewall set allprofiles state off
```

Bật lại:

```bash
netsh advfirewall set allprofiles state on
```

---

## ⚙️ Hướng dẫn sử dụng & chức năng

Ví dụ:

* Client kết nối đến Server
* Web HTML kết nối với Client thông qua Web Socket
* Web thông qua Client ra lệnh cho Server
* Server nhận lệnh
* Thực thi lệnh
* Gửi kết quả về Web thông qua Client

| Thành phần| Vai trò                    |
|-----------|----------------------------|
| Web       | Gửi lệnh, hiển thị kết quả |
| Client    | Trung gian WebSocket ↔ TCP |
| Server    | Thực thi lệnh              |


| Chức năng        | Mô tả                                                           |
|------------------|-----------------------------------------------------------------|
| Kết nối Server   | Client thiết lập kết nối TCP tới Server                         |
| Kết nối Web      | Web HTML kết nối tới Client thông qua WebSocket                 |
| Gửi lệnh         | Web gửi lệnh điều khiển đến Client                              |
| Chuyển tiếp lệnh | Client chuyển tiếp lệnh từ Web đến Server                       |
| Thực thi         | Server nhận và thực thi lệnh                                    |
| Phản hồi         | Server gửi kết quả về Client, Client chuyển tiếp kết quả về Web |

---

## ⚠️ Lưu ý bảo mật & pháp lý

⚠️ **Dự án chỉ dùng cho mục đích học tập**

* Không sử dụng trái phép trên máy người khác
* Không triển khai trên môi trường thật
* Không chịu trách nhiệm cho hành vi lạm dụng
