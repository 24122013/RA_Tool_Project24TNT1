let currentWindow = 0;
let currentAction = '';

// Connect to server
function connect() {
    const ip = document.getElementById('ipInput').value;
    if (ip) {
        document.getElementById('homePage').classList.add('hidden');
        setTimeout(() => {
            document.getElementById('mainContainer').classList.add('active');
            updateStatus(true);
            updateIndicator(0);
            showSuccessNotification();
        }, 500);
    }
}

// Show success notification
function showSuccessNotification() {
    const notification = document.getElementById('successNotification');
    notification.classList.add('show');
    setTimeout(() => {
        notification.classList.remove('show');
    }, 3000);
}

// Disconnect
function disconnect() {
    updateStatus(false);
    document.getElementById('mainContainer').classList.remove('active');
    document.getElementById('homePage').classList.remove('hidden');
    currentWindow = 0;
    navigateTo(0);
}

// Update connection status
function updateStatus(connected) {
    const box = document.getElementById('statusBox');
    const text = document.getElementById('statusText');
    if (connected) {
        box.className = 'status-box connected';
        text.textContent = 'Connected';
        text.classList.add('success');
    } else {
        box.className = 'status-box disconnected';
        text.textContent = 'Disconnected';
        text.classList.remove('success');
    }
}

// Navigate between windows
function navigateTo(index) {
    if (index === 5) {
        // System Control doesn't need special handling
    }
    currentWindow = index;
    const wrapper = document.getElementById('windowsWrapper');
    wrapper.style.transform = `translateX(-${index * 100}vw)`;
    updateIndicator(index);
}

// Update navigation indicator
function updateIndicator(index) {
    const indicator = document.getElementById('navIndicator');
    const buttons = document.querySelectorAll('.nav-btn');
    const btn = buttons[index];
    indicator.style.width = btn.offsetWidth + 'px';
    indicator.style.left = btn.offsetLeft + 'px';
}

// Show input modal
function showInput(action) {
    currentAction = action;
    const modal = document.getElementById('inputModal');
    const title = document.getElementById('modalTitle');
    
    switch(action) {
        case 'kill':
            title.textContent = 'Enter Process ID';
            break;
        case 'start':
            title.textContent = 'Enter Application Name';
            break;
        case 'killApp':
            title.textContent = 'Enter Application ID';
            break;
        case 'startApp':
            title.textContent = 'Enter Application Name';
            break;
    }
    
    modal.classList.add('active');
    document.getElementById('modalInput').focus();
}

// Close modal
function closeModal() {
    document.getElementById('inputModal').classList.remove('active');
    document.getElementById('modalInput').value = '';
}

// Submit input
function submitInput() {
    const value = document.getElementById('modalInput').value;
    console.log(`Action: ${currentAction}, Value: ${value}`);
    // Here you would send the command to backend
    closeModal();
}

// Show process list
function showProcessList() {
    const list = document.getElementById('processList');
    list.style.display = 'block';
    list.innerHTML = `
        <div class="process-item">Process 1 - ID: 1234</div>
        <div class="process-item">Process 2 - ID: 5678</div>
        <div class="process-item">Process 3 - ID: 9012</div>
    `;
    setTimeout(() => list.classList.add('show'), 10);
}

// Show app list
function showAppList() {
    const list = document.getElementById('appList');
    list.style.display = 'block';
    list.innerHTML = `
        <div class="process-item">Chrome - ID: 2345</div>
        <div class="process-item">VSCode - ID: 6789</div>
        <div class="process-item">Explorer - ID: 3456</div>
    `;
    setTimeout(() => list.classList.add('show'), 10);
}

// Keylog functions
function hookKeylog() {
    console.log('Keylogger hooked');
}

function unhookKeylog() {
    console.log('Keylogger unhooked');
}

function printKeylog() {
    const output = document.getElementById('keylogOutput');
    output.style.display = 'block';
    output.innerHTML = `
        [2025-12-13 10:30:15] Key pressed: H
        [2025-12-13 10:30:16] Key pressed: e
        [2025-12-13 10:30:16] Key pressed: l
        [2025-12-13 10:30:17] Key pressed: l
        [2025-12-13 10:30:17] Key pressed: o
        [2025-12-13 10:30:18] Key pressed: [Space]
    `;
    setTimeout(() => output.classList.add('show'), 10);
}

// Screenshot
function captureScreenshot() {
    const display = document.getElementById('screenshotDisplay');
    display.innerHTML = '<img src="https://via.placeholder.com/600x400/f5f5f0/666?text=Screenshot+Preview" alt="Screenshot">';
    setTimeout(() => display.classList.add('show'), 10);
}

// Webcam
function startWebcam() {
    console.log('Webcam recording started');
}

function endWebcam() {
    const display = document.getElementById('webcamDisplay');
    display.innerHTML = '<video width="600" controls><source src="" type="video/mp4">Your browser does not support video.</video>';
    setTimeout(() => display.classList.add('show'), 10);
}

// Shutdown
function shutdownServer() {
    if (confirm('Are you sure you want to shut down the server?')) {
        console.log('Shutting down server...');
        disconnect();
    }
}

// Restart
function restartServer() {
    if (confirm('Are you sure you want to restart the server?')) {
        console.log('Restarting server...');
        disconnect();
    }
}

// Initialize
window.onload = () => {
    updateStatus(false);
};