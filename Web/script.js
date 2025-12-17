// --- BIẾN TOÀN CỤC ---
let currentWindow = 0;
let ws = null;
let currentAction = ''; 
let isInSubMenu = false; // Biến này dùng để check xem ta đang ở Main hay Sub

// Biến Parse Dữ liệu
let parsingMode = null; 
let expectedItems = 0;
let currentItemsReceived = 0;
let tempRowData = []; 

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

    parsingMode = null; // Reset chế độ parse

    // A. XỬ LÝ KHI RỜI TAB KEYLOG (Index 2)
    if (currentWindow === 2) {
        ws.send("CMD|UNHOOK");
        setTimeout(() => {
            ws.send("CMD|QUIT");
            isInSubMenu = false;
            performUITransition(index);
        }, 200);
        return;
    }

    // B. XỬ LÝ KHI RỜI TAB WEBCAM (Index 4)
    // Webcam cần gửi STOP trước khi QUIT để giải phóng Camera
    else if (currentWindow === 4) {
        ws.send("CMD|STOP"); // Tắt camera
        document.getElementById('liveIndicator').classList.remove('active');
        setTimeout(() => {
            ws.send("CMD|QUIT"); // Thoát menu Webcam
            isInSubMenu = false;
            performUITransition(index);
        }, 200);
        return;
    }

    // C. XỬ LÝ KHI RỜI TAB SCREENSHOT (Index 3)
    // Screenshot cũng nằm trong vòng lặp while(true)
    else if (currentWindow === 3) {
        ws.send("CMD|QUIT");
        isInSubMenu = false;
    }

    // D. CÁC TAB KHÁC (Process/App)
    else if (isInSubMenu) {
        ws.send("CMD|QUIT");
        isInSubMenu = false;
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
    // Logic an toàn cho Keylog
    if(isInSubMenu) { ws.send("CMD|QUIT"); isInSubMenu=false; }
    
    setTimeout(() => {
        ws.send("CMD|KEYLOG");
        isInSubMenu = true;
        setTimeout(() => ws.send("CMD|HOOK"), 100);
    }, 200);
    showToast('Keylog Hooked', 'success');
}

function unhookKeylog() {
    if(isInSubMenu) { ws.send("CMD|QUIT"); isInSubMenu=false; }
    
    setTimeout(() => {
        ws.send("CMD|KEYLOG");
        isInSubMenu = true;
        setTimeout(() => ws.send("CMD|UNHOOK"), 100);
    }, 200);
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

    setTimeout(() => {
        ws.send("CMD|KEYLOG");
        isInSubMenu = true;
        setTimeout(() => ws.send("CMD|PRINT"), 100);
    }, 200);
}

function deleteLogs() {
    if(!confirm("Clear Keylog history on screen?")) return;
    document.getElementById('keylogOutput').innerHTML = '';
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
    // Thay đổi UI để hiển thị khung hình
    const container = document.getElementById('webcamVideo').parentNode;
    
    // Thay thế thẻ <video> bằng <img> để hiển thị MJPEG stream (Base64)
    // Vì WebSockets gửi từng frame ảnh, dùng img src update sẽ mượt hơn
    if (!document.getElementById('webcamTarget')) {
        const img = document.createElement('img');
        img.id = 'webcamTarget';
        img.style.width = '100%';
        img.style.borderRadius = '10px';
        img.style.boxShadow = '0 5px 20px rgba(0,0,0,0.1)';
        document.getElementById('webcamVideo').style.display = 'none'; // Ẩn video gốc
        container.appendChild(img);
    }

    // Gửi lệnh
    ws.send("CMD|WEBCAM"); // Vào menu Webcam
    isInSubMenu = true;
    
    setTimeout(() => {
        ws.send("CMD|START"); // Bắt đầu stream
        document.getElementById('liveIndicator').classList.add('active');
        showToast("Webcam started", "success");
    }, 300);
}

function endWebcam() {
    ws.send("CMD|STOP"); // Dừng stream
    document.getElementById('liveIndicator').classList.remove('active');
    
    // Xóa ảnh đang hiện
    const target = document.getElementById('webcamTarget');
    if(target) target.src = "";
    
    showToast("Webcam stopped", "warning");
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