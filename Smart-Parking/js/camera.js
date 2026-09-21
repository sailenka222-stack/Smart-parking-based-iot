// Smart Parking - Camera
//
// PLACEHOLDER: this currently shows THIS device's own webcam (laptop/phone
// camera), just so the page isn't blank. It is NOT yet connected to an
// actual camera mounted at the parking gate, since nothing in the project
// so far sets up a real gate camera (e.g. an ESP32-CAM or IP camera stream).
//
// If you have a physical camera at the gate, tell me what it is (ESP32-CAM,
// USB webcam on the Pi, an IP camera with an RTSP/HTTP stream, etc.) and
// this file can be updated to show that feed instead.

if (sessionStorage.getItem('sp_logged_in') !== 'true') {
    window.location.href = 'index.html';
}

async function startCamera() {
    const statusMsg = document.getElementById('status-msg');
    try {
        const stream = await navigator.mediaDevices.getUserMedia({ video: true });
        document.getElementById('cam-feed').srcObject = stream;
    } catch (err) {
        statusMsg.textContent = 'Could not access a camera on this device. Grant camera permission, or connect a real gate camera feed here.';
        statusMsg.className = 'status-msg error';
        statusMsg.hidden = false;
    }
}

startCamera();