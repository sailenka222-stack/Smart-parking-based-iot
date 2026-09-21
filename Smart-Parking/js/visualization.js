// Smart Parking - Visualization
//
// CHANGED: originally this read live IR sensor data from GET /api/slots
// (pushed by the STM32's physical slot1/slot2 sensors). Since one physical
// sensor turned out to be unreliable, this now instead derives occupancy
// from who's currently marked INSIDE via RFID scan (GET /api/users) - the
// same reliable data source the Dashboard already uses. Whoever scanned in
// first shows in Slot 1, the next person in Slot 2.
//
// Slots 3/4 stay dimmed with a "Reserved" tag - not wired to anything yet.

if (sessionStorage.getItem('sp_logged_in') !== 'true') {
    window.location.href = 'index.html';
}

async function refreshSlots() {
    const statusMsg = document.getElementById('status-msg');
    try {
        const users = await API.getUsers();
        const insideUsers = Object.values(users || {}).filter(u => u.status === 'INSIDE');

        document.getElementById('slot-1').classList.toggle('occupied', insideUsers.length >= 1);
        document.getElementById('slot-2').classList.toggle('occupied', insideUsers.length >= 2);

        statusMsg.hidden = true;
    } catch (err) {
        statusMsg.textContent = 'Could not reach the backend. Check that server.js is running and API_BASE in js/api.js matches its IP.';
        statusMsg.hidden = false;
    }
}

refreshSlots();
setInterval(refreshSlots, 2000);