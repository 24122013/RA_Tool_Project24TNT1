let currentWindow = 0;
let currentAction = '';
let confirmCallback = null;
let touchStartX = 0;
let touchEndX = 0;
let isDragging = false;

// Store available process/app IDs
let availableProcessIds = ['1234', '5678', '9012', '3456', '7890'];
let availableAppIds = ['2345', '6789', '3456', '4567', '8901'];

// Toast Notification System
function showToast(message, type = 'success') {
    const container = document.getElementById('toastContainer');
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    
    let icon = '✓';
    if (type === 'error') icon = '✕';
    if (type === 'warning') icon = '⚠';
    
    toast.innerHTML = `
        <div class="toast-icon">${icon}</div>
        <div class="toast-content">
            <div class="toast-message">${message}</div>
        </div>
    `;
    
    container.appendChild(toast);
    
    // Trigger animation
    setTimeout(() => toast.classList.add('show'), 10);
    
    // Auto remove after 3 seconds
    setTimeout(() => {
        toast.classList.remove('show');
        setTimeout(() => toast.remove(), 400);
    }, 3000);
}

// Custom Confirm Dialog
function showConfirm(title, message, onConfirm) {
    const dialog = document.getElementById('confirmDialog');
    document.getElementById('confirmTitle').textContent = title;
    document.getElementById('confirmMessage').textContent = message;
    
    confirmCallback = onConfirm;
    dialog.classList.add('active');
}

function closeConfirm(confirmed) {
    const dialog = document.getElementById('confirmDialog');
    dialog.classList.remove('active');
    
    if (confirmed && confirmCallback) {
        confirmCallback();
    }
    confirmCallback = null;
}

// IP Validation
function validateIP(ip) {
    const ipPattern = /^(\d{1,3}\.){3}\d{1,3}$/;
    if (!ipPattern.test(ip)) return false;
    
    const parts = ip.split('.');
    return parts.every(part => {
        const num = parseInt(part);
        return num >= 0 && num <= 255;
    });
}

// Handle Enter Key for IP input
function handleEnterKey(event) {
    if (event.key === 'Enter') {
        connect();
    }
}

// Handle Enter Key for Modal input
function handleModalEnterKey(event) {
    if (event.key === 'Enter') {
        submitInput();
    }
}

// Connect to server
function connect() {
    const ip = document.getElementById('ipInput').value.trim();
    
    if (!ip) {
        showToast('Please enter your IP Address', 'warning');
        return;
    }
    
    if (!validateIP(ip)) {
        showToast('Invalid IP Address', 'error');
        return;
    }
    
    document.getElementById('homePage').classList.add('hidden');
    setTimeout(() => {
        document.getElementById('mainContainer').classList.add('active');
        updateStatus(true);
        updateIndicator(0);
        showSuccessNotification();
    }, 500);
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
    const value = document.getElementById('modalInput').value.trim();
    
    if (!value) {
        showToast('Please enter a suitable ID', 'warning');
        return;
    }
    
    // Validate based on action type
    let isValid = false;
    let successMessage = '';
    
    switch(currentAction) {
        case 'kill':
            // Check if ID exists in process list
            if (!availableProcessIds.includes(value)) {
                showToast('Please enter a suitable ID', 'warning');
                return;
            }
            isValid = true;
            successMessage = `Process ${value} killed successfully`;
            break;
            
        case 'start':
            // For starting, accept any non-empty application name
            isValid = true;
            successMessage = `Application "${value}" started successfully`;
            break;
            
        case 'killApp':
            // Check if ID exists in app list
            if (!availableAppIds.includes(value)) {
                showToast('Please enter a suitable ID', 'warning');
                return;
            }
            isValid = true;
            successMessage = `Application ${value} killed successfully`;
            break;
            
        case 'startApp':
            // For starting, accept any non-empty application name
            isValid = true;
            successMessage = `Application "${value}" started successfully`;
            break;
    }
    
    if (isValid) {
        showToast(successMessage, 'success');
        console.log(`Action: ${currentAction}, Value: ${value}`);
        closeModal();
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
        <div class="process-item">Process 4 - ID: 3456</div>
        <div class="process-item">Process 5 - ID: 7890</div>
    `;
    setTimeout(() => list.classList.add('show'), 10);
    showToast('Process list loaded', 'success');
}

// Show app list
function showAppList() {
    const list = document.getElementById('appList');
    list.style.display = 'block';
    list.innerHTML = `
        <div class="process-item">Chrome - ID: 2345</div>
        <div class="process-item">VSCode - ID: 6789</div>
        <div class="process-item">Explorer - ID: 3456</div>
        <div class="process-item">Notepad - ID: 4567</div>
        <div class="process-item">Calculator - ID: 8901</div>
    `;
    setTimeout(() => list.classList.add('show'), 10);
    showToast('Application list loaded', 'success');
}

// Keylog functions
function hookKeylog() {
    console.log('Keylogger hooked');
    showToast('Keylogger hooked successfully', 'success');
}

function unhookKeylog() {
    console.log('Keylogger unhooked');
    showToast('Keylogger unhooked', 'success');
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
        [2025-12-13 10:30:19] Key pressed: W
        [2025-12-13 10:30:19] Key pressed: o
        [2025-12-13 10:30:20] Key pressed: r
        [2025-12-13 10:30:20] Key pressed: l
        [2025-12-13 10:30:21] Key pressed: d
    `;
    setTimeout(() => output.classList.add('show'), 10);
    showToast('Keylog data retrieved', 'success');
}

// Screenshot functions
function captureScreenshot() {
    const display = document.getElementById('screenshotDisplay');
    const actions = document.getElementById('screenshotActions');
    
    display.innerHTML = '<img src="https://via.placeholder.com/600x400/f5f5f0/666?text=Screenshot+Preview" alt="Screenshot">';
    setTimeout(() => display.classList.add('show'), 10);
    
    actions.style.display = 'flex';
    showToast('Screenshot captured', 'success');
}

function saveScreenshot() {
    showToast('Screenshot saved successfully', 'success');
}

function deleteScreenshot() {
    showConfirm(
        'Delete Screenshot',
        'Are you sure you want to delete this screenshot?',
        () => {
            const display = document.getElementById('screenshotDisplay');
            const actions = document.getElementById('screenshotActions');
            
            display.classList.remove('show');
            setTimeout(() => {
                display.innerHTML = '';
                actions.style.display = 'none';
            }, 400);
            
            showToast('Screenshot deleted', 'success');
        }
    );
}

// Webcam functions
function startWebcam() {
    const video = document.getElementById('webcamVideo');
    const liveIndicator = document.getElementById('liveIndicator');
    
    // Show video immediately
    video.classList.add('active');
    liveIndicator.classList.add('active');
    
    // Play video
    video.play().catch(err => {
        console.log('Video autoplay failed:', err);
    });
    
    console.log('Webcam recording started');
    showToast('Webcam streaming started - Live', 'success');
}

function endWebcam() {
    const video = document.getElementById('webcamVideo');
    const liveIndicator = document.getElementById('liveIndicator');
    
    // Hide video
    video.classList.remove('active');
    liveIndicator.classList.remove('active');
    
    // Pause video
    video.pause();
    
    console.log('Webcam recording ended');
    showToast('Webcam streaming ended', 'success');
}

// System Control
function shutdownServer() {
    showConfirm(
        'Shut Down Server',
        'Are you sure you want to shut down the server?',
        () => {
            console.log('Shutting down server...');
            showToast('Server shutdown initiated', 'success');
            setTimeout(() => disconnect(), 1500);
        }
    );
}

function restartServer() {
    showConfirm(
        'Restart Server',
        'Are you sure you want to restart the server?',
        () => {
            console.log('Restarting server...');
            showToast('Server restart initiated', 'success');
            setTimeout(() => disconnect(), 1500);
        }
    );
}

// Initialize
window.onload = () => {
    updateStatus(false);
    
    // Setup confirm dialog buttons
    document.getElementById('confirmYes').onclick = () => closeConfirm(true);
    document.getElementById('confirmNo').onclick = () => closeConfirm(false);
    
    // Setup swipe/scroll navigation
    setupSwipeNavigation();
};

// Swipe Navigation for Windows
function setupSwipeNavigation() {
    const container = document.getElementById('windowsWrapper');
    const windowsContainer = document.getElementById('windowsWrapper').parentElement;
    
    // Mouse events
    let mouseDown = false;
    let startX;
    let scrollLeft;
    
    windowsContainer.addEventListener('mousedown', (e) => {
        // Allow vertical scrolling in window content
        if (e.target.closest('.window-content')) return;
        if (e.target.tagName === 'BUTTON' || e.target.tagName === 'INPUT') return;
        
        mouseDown = true;
        startX = e.pageX;
        touchStartX = e.pageX;
        isDragging = false;
        container.style.transition = 'none';
    });
    
    windowsContainer.addEventListener('mousemove', (e) => {
        if (!mouseDown) return;
        e.preventDefault();
        isDragging = true;
        const x = e.pageX;
        const walk = (x - startX) * 2;
        const newTransform = -currentWindow * window.innerWidth + walk;
        container.style.transform = `translateX(${newTransform}px)`;
    });
    
    windowsContainer.addEventListener('mouseup', (e) => {
        if (!mouseDown) return;
        mouseDown = false;
        touchEndX = e.pageX;
        container.style.transition = 'transform 0.8s cubic-bezier(0.34, 1.56, 0.64, 1)';
        
        if (isDragging) {
            handleSwipe();
        }
    });
    
    windowsContainer.addEventListener('mouseleave', () => {
        if (mouseDown) {
            mouseDown = false;
            container.style.transition = 'transform 0.8s cubic-bezier(0.34, 1.56, 0.64, 1)';
            navigateTo(currentWindow);
        }
    });
    
    // Touch events
    let touchStartY = 0;
    let isVerticalScroll = false;
    
    windowsContainer.addEventListener('touchstart', (e) => {
        if (e.target.tagName === 'BUTTON' || e.target.tagName === 'INPUT') return;
        touchStartX = e.touches[0].clientX;
        touchStartY = e.touches[0].clientY;
        isDragging = false;
        isVerticalScroll = false;
        container.style.transition = 'none';
    });
    
    windowsContainer.addEventListener('touchmove', (e) => {
        const touchCurrentX = e.touches[0].clientX;
        const touchCurrentY = e.touches[0].clientY;
        const diffX = Math.abs(touchCurrentX - touchStartX);
        const diffY = Math.abs(touchCurrentY - touchStartY);
        
        // Determine if it's a vertical or horizontal scroll
        if (!isDragging && !isVerticalScroll) {
            if (diffY > diffX && diffY > 10) {
                isVerticalScroll = true;
                return; // Allow vertical scrolling
            } else if (diffX > 10) {
                isDragging = true;
            }
        }
        
        if (isDragging && !isVerticalScroll) {
            e.preventDefault();
            const x = touchCurrentX;
            const walk = (x - touchStartX) * 1.5;
            const newTransform = -currentWindow * window.innerWidth + walk;
            container.style.transform = `translateX(${newTransform}px)`;
        }
    }, { passive: false });
    
    windowsContainer.addEventListener('touchend', (e) => {
        if (isVerticalScroll) {
            isVerticalScroll = false;
            return;
        }
        
        touchEndX = e.changedTouches[0].clientX;
        container.style.transition = 'transform 0.8s cubic-bezier(0.34, 1.56, 0.64, 1)';
        
        if (isDragging) {
            handleSwipe();
        }
    });
    
    // Wheel/scroll event - only for horizontal navigation when not scrolling content
    windowsContainer.addEventListener('wheel', (e) => {
        // Check if target is scrollable content
        const target = e.target.closest('.window-content, .process-list, .keylog-output');
        
        if (target) {
            // Allow natural vertical scrolling in content areas
            return;
        }
        
        if (Math.abs(e.deltaX) > Math.abs(e.deltaY)) {
            // Horizontal scroll
            e.preventDefault();
            if (e.deltaX > 30 && currentWindow < 5) {
                navigateTo(currentWindow + 1);
            } else if (e.deltaX < -30 && currentWindow > 0) {
                navigateTo(currentWindow - 1);
            }
        } else if (Math.abs(e.deltaY) > 80) {
            // Strong vertical scroll converted to horizontal navigation
            // Only when not over scrollable content
            e.preventDefault();
            if (e.deltaY > 0 && currentWindow < 5) {
                navigateTo(currentWindow + 1);
            } else if (e.deltaY < 0 && currentWindow > 0) {
                navigateTo(currentWindow - 1);
            }
        }
    }, { passive: false });
}

function handleSwipe() {
    const swipeThreshold = 100;
    const diff = touchStartX - touchEndX;
    
    if (Math.abs(diff) > swipeThreshold) {
        if (diff > 0 && currentWindow < 5) {
            // Swipe left - go to next window
            navigateTo(currentWindow + 1);
        } else if (diff < 0 && currentWindow > 0) {
            // Swipe right - go to previous window
            navigateTo(currentWindow - 1);
        } else {
            // Not enough windows, return to current
            navigateTo(currentWindow);
        }
    } else {
        // Swipe too small, return to current window
        navigateTo(currentWindow);
    }
}
