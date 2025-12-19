# RA Tool – Remote Administration Tool

> **Mục đích học tập – nghiên cứu**
> Dự án mô phỏng hệ thống quản trị & điều khiển máy tính từ xa (Remote Administration Tool – RAT) theo mô hình **Server – Client**, phục vụ đồ án môn học **Mạng Máy Tính**

## ✨ Tác giả

* Nguyễn Đức Tâm: 24122013
* Nguyễn Tuấn Lâm: 24122006
* Nguyễn Nguyễn Trâm Anh: 24122027


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
6. [Mở port & cấu hình Firewall](#-mở-port--cấu-hình-firewall)
7. [Hướng dẫn sử dụng & chức năng](#-hướng-dẫn-sử-dụng--chức-năng)
8. [Lưu ý bảo mật & pháp lý](#-lưu-ý-bảo-mật--pháp-lý)

---

## 🧾 Yêu cầu hệ thống

### Phần mềm

* Windows 10/11 (khuyến nghị)
* **MinGW / MSYS2 / Visual Studio (MSVC)**
* Git
* (Tuỳ chọn) Visual Studio Code

### Kiến thức nền

* C/C++ cơ bản
* Socket TCP/IP
* Command line

---

## 📁 Cấu trúc thư mục

```
RA_Tool_Project24TNT1/
│
├── Server/          # Mã nguồn Server
├── Client/          # Mã nguồn Client
├── Web/             # (Nếu có) giao diện Web
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
g++ server.cpp -o server -lws2_32
```

#### Build Client

```bash
cd Client
g++ client.cpp -o client -lws2_32
```

📌 `-lws2_32` là bắt buộc cho lập trình socket trên Windows.

---

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
server.exe
```

Server sẽ:

* Mở port lắng nghe (ví dụ: `12345`)
* Chờ client kết nối

📌 **Ghi nhớ port đang dùng để client kết nối**.

---

## ▶️ Chạy Client

Trên máy Client:

```bash
cd Client
client.exe <IP_SERVER> <PORT>
```

Ví dụ:

```bash
client.exe 192.168.1.10 12345
```

---

## 🔓 Mở port & cấu hình Firewall

### 🔹 Trên Windows (Server)

1. Mở **Windows Defender Firewall**
2. Chọn **Advanced settings**
3. **Inbound Rules → New Rule**
4. Chọn **Port** → TCP
5. Nhập port (ví dụ `12345`)
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

### 🔹 Mở port trên Router (nếu khác mạng LAN)

* Cấu hình **Port Forwarding**
* Forward port từ **Public IP → IP máy Server**

---

## ⚙️ Hướng dẫn sử dụng & chức năng

(Tuỳ theo code hiện tại của project)

Ví dụ:

* Client kết nối đến Server
* Server nhận lệnh
* Thực thi lệnh
* Gửi kết quả về Client

| Chức năng | Mô tả                         |
| --------- | ----------------------------- |
| Kết nối   | Client kết nối TCP tới Server |
| Gửi lệnh  | Client gửi chuỗi lệnh         |
| Thực thi  | Server xử lý lệnh             |
| Phản hồi  | Server gửi kết quả            |

---

## ⚠️ Lưu ý bảo mật & pháp lý

⚠️ **Dự án chỉ dùng cho mục đích học tập**

* Không sử dụng trái phép trên máy người khác
* Không triển khai trên môi trường thật
* Không chịu trách nhiệm cho hành vi lạm dụng




