// Smart Parking - Register new user
//
// Saves a new card via PUT /api/user/:uid on the backend. This is exactly
// what the WE10 will look up later when that card is scanned.

if (sessionStorage.getItem('sp_logged_in') !== 'true') {
    window.location.href = 'index.html';
}

document.getElementById('register-form').addEventListener('submit', async function (e) {
    e.preventDefault();
    const statusMsg = document.getElementById('status-msg');

    const uid = document.getElementById('uid').value.trim();
    const name = document.getElementById('name').value.trim();
    const vehicle = document.getElementById('vehicle').value.trim();
    const type = document.getElementById('type').value;
    const wallet = Number(document.getElementById('wallet').value);

    if (!uid || !name || !vehicle) {
        statusMsg.textContent = 'Please fill in all required fields.';
        statusMsg.className = 'status-msg error';
        statusMsg.hidden = false;
        return;
    }

    try {
        await API.saveUser(uid, { name, vehicle, type, wallet, status: 'OUTSIDE' });

        statusMsg.textContent = `User "${name}" registered with card ${uid}.`;
        statusMsg.className = 'status-msg success';
        statusMsg.hidden = false;

        document.getElementById('register-form').reset();
        document.getElementById('wallet').value = 500;
    } catch (err) {
        statusMsg.textContent = 'Could not reach the backend. Check that server.js is running and API_BASE in js/api.js matches its IP.';
        statusMsg.className = 'status-msg error';
        statusMsg.hidden = false;
    }
});