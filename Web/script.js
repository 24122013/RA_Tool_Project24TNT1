let currentWindow = 0;
let currentAction = '';
let isScrolling = false;
let scrollTimeout;

// Mock data for process and app ID mapping
const processData = {
    '1234': 'chrome.exe',
    '5678': 'notepad.exe',
    '9012': 'explorer.exe'
};

const appData = {
    '2345': 'Google Chrome',
    '6789': 'Visual Studio Code',
    '3456': 'File Explorer'
};

// Notification system
let notificationTimeout;
function showNotification(message, type = 'info') {
    const notification = document.getElementById('notification');
    
    // Icons for different types
    const icons = {
        success: '✅',
        error: '❌',
        warning: '⚠️',
        info: 'ℹ️'
    };
    
    notification.className = `notification ${type}`;
    notification.innerHTML = `${icons[type]} ${message}`;
    
    // Clear existing timeout
    clearTimeout(notificationTimeout);
    
    // Show notification
    setTimeout(() => notification.classList.add('show'), 10);
    
    // Auto hide after 1 second
    notificationTimeout = setTimeout(() => {
        notification.classList.remove('show');
    }, 1000);
}

// Confirm with custom callback (still uses native for critical actions)
function showConfirm(message, callback) {
    if (confirm(message)) {
        callback();
    }
}

// Wheel/Scroll event handler for navigation
function handleWheel(e) {
    if (isScrolling) return;
    
    // Check if main container is visible
    if (!document.getElementById('mainContainer').classList.contains('active')) {
        return;
    }

    e.preventDefault();
    
    const delta = e.deltaY || e.detail || e.wheelDelta;
    
    if (delta > 0) {
        // Scroll down - next window
        let nextWindow = currentWindow + 1;
        if (nextWindow === 2) nextWindow = 3; // Skip Registry
        if (nextWindow <= 6) {
            navigateTo(nextWindow);
        }
    } else {
        // Scroll up - previous window
        let prevWindow = currentWindow - 1;
        if (prevWindow === 2) prevWindow = 1; // Skip Registry
        if (prevWindow >= 0) {
            navigateTo(prevWindow);
        }
    }
    
    // Prevent rapid scrolling
    isScrolling = true;
    clearTimeout(scrollTimeout);
    scrollTimeout = setTimeout(() => {
        isScrolling = false;
    }, 800);
}

// Add wheel event listener
window.addEventListener('wheel', handleWheel, { passive: false });
window.addEventListener('DOMMouseScroll', handleWheel, { passive: false });

// Connect to server
function connect() {
    const ip = document.getElementById('ipInput').value.trim();
    
    // Validate IP address format
    const ipRegex = /^(\d{1,3}\.){3}\d{1,3}$/;
    
    if (!ip) {
        showNotification('Please enter an IP address!', 'warning');
        return;
    }
    
    if (!ipRegex.test(ip)) {
        showNotification('Invalid IP Address', 'error');
        return;
    }
    
    // Validate each octet is 0-255
    const octets = ip.split('.');
    const valid = octets.every(octet => {
        const num = parseInt(octet, 10);
        return num >= 0 && num <= 255;
    });
    
    if (!valid) {
        showNotification('Invalid IP Address', 'error');
        return;
    }
    
    // Success
    showNotification(`${ip} Successfully Connected`, 'success');
    
    setTimeout(() => {
        document.getElementById('homePage').classList.add('hidden');
        setTimeout(() => {
            document.getElementById('mainContainer').classList.add('active');
            updateStatus(true);
            updateIndicator(0);
        }, 500);
    }, 1000);
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
    } else {
        box.className = 'status-box disconnected';
        text.textContent = 'Disconnected';
    }
}

// Navigate between windows
function navigateTo(index) {
    // Skip Registry window (index 2)
    if (index === 2) return;
    
    currentWindow = index;
    const wrapper = document.getElementById('windowsWrapper');
    wrapper.style.transform = `translateX(-${index * 100}vw)`;
    
    // Update indicator based on visible button index
    const buttonMap = [0, 1, 3, 4, 5, 6]; // Maps window index to button index
    const buttonIndex = buttonMap.indexOf(index);
    if (buttonIndex !== -1) {
        updateIndicatorByButton(buttonIndex);
    }
}

// Update navigation indicator by button index
function updateIndicatorByButton(buttonIndex) {
    const indicator = document.getElementById('navIndicator');
    const buttons = document.querySelectorAll('.nav-btn');
    const btn = buttons[buttonIndex];
    indicator.style.width = btn.offsetWidth + 'px';
    indicator.style.left = btn.offsetLeft + 'px';
}

// Update navigation indicator (kept for compatibility)
function updateIndicator(index) {
    const buttonMap = [0, 1, 3, 4, 5, 6];
    const buttonIndex = buttonMap.indexOf(index);
    if (buttonIndex !== -1) {
        updateIndicatorByButton(buttonIndex);
    }
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
    const value = document.getElementById('modalInput').value.trim();
    if (!value) {
        showNotification('Please enter a value!', 'warning');
        return;
    }
    
    console.log(`Action: ${currentAction}, Value: ${value}`);
    
    closeModal();
    
    // Show appropriate notification based on action
    switch(currentAction) {
        case 'kill':
            const processName = processData[value] || value;
            showNotification(`Process "${processName}" (ID: ${value}) has been killed`, 'error');
            break;
        case 'start':
            showNotification(`Process "${value}" has been started`, 'success');
            break;
        case 'killApp':
            const appName = appData[value] || value;
            showNotification(`Application "${appName}" (ID: ${value}) has been killed`, 'error');
            break;
        case 'startApp':
            showNotification(`Application "${value}" has been started`, 'success');
            break;
    }
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
    showNotification('Start Hooking', 'info');
}

function unhookKeylog() {
    console.log('Keylogger unhooked');
    showNotification('Start Unhooking', 'info');
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
    display.style.display = 'block';
    display.innerHTML = `
        <img id="screenshotImage" src="https://via.placeholder.com/600x400/f5f5f0/666?text=Screenshot+Preview" alt="Screenshot" style="display: block; margin-bottom: 15px;">
        <div style="display: flex; gap: 10px; justify-content: center;">
            <button class="action-btn" onclick="saveScreenshot()" style="flex: 0 1 auto; min-width: 120px;">Save</button>
            <button class="action-btn danger" onclick="deleteScreenshot()" style="flex: 0 1 auto; min-width: 120px;">Delete</button>
        </div>
    `;
    setTimeout(() => display.classList.add('show'), 10);
}

function saveScreenshot() {
    console.log('Screenshot saved');
    // Add save logic here - send to backend or download
    showNotification('Screenshot saved successfully!', 'success');
}

function deleteScreenshot() {
    showConfirm('Are you sure you want to delete this screenshot?', () => {
        console.log('Screenshot deleted');
        const display = document.getElementById('screenshotDisplay');
        display.classList.remove('show');
        setTimeout(() => {
            display.style.display = 'none';
            display.innerHTML = '';
        }, 400);
        showNotification('Screenshot deleted!', 'info');
    });
}

// Webcam
function startWebcam() {
    console.log('Webcam recording started');
    const display = document.getElementById('webcamDisplay');
    display.style.display = 'block';
    display.innerHTML = '<video id="webcamVideo" width="600" autoplay style="background: #000; border-radius: 10px;"><source src="" type="video/mp4">Webcam stream will appear here...</video>';
    setTimeout(() => display.classList.add('show'), 10);
}

function endWebcam() {
    console.log('Webcam recording ended');
    const display = document.getElementById('webcamDisplay');
    display.classList.remove('show');
    setTimeout(() => {
        display.style.display = 'none';
        display.innerHTML = '';
    }, 400);
}

// Shutdown
function shutdownServer() {
    showConfirm('Are you sure you want to shutdown the server?', () => {
        console.log('Shutting down server...');
        showNotification('Shutting down server...', 'warning');
        // Send shutdown command to backend here
        setTimeout(() => disconnect(), 1000);
    });
}

// Restart
function restartServer() {
    showConfirm('Are you sure you want to restart the server?', () => {
        console.log('Restarting server...');
        showNotification('Restarting server...', 'warning');
        // Send restart command to backend here
    });
}

// Initialize
window.onload = () => {
    updateStatus(false);
};
