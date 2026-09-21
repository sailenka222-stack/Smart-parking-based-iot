// Smart Parking - Dashboard
//
// NOTE: this reads "who's inside" from your backend's user records
// (status: "INSIDE"/"OUTSIDE"), which the WE10 module updates when it
// checks a card. Total slot count comes from CONFIG.TOTAL_SLOTS.
//
// Slot occupancy here is DERIVED from user status, not read live from the
// STM32's IR sensors - see visualization.html for the live sensor-based view.

if (sessionStorage.getItem('sp_logged_in') !== 'true') {
    window.location.href = 'index.html';
}

let lastSeenLogCount = null;

function showToast(name, fee) {
    const container = document.getElementById('toast-container');
    const toast = document.createElement('div');
    toast.className = 'toast';
    toast.innerHTML = `
        <div class="toast__icon">₹</div>
        <div>
            <p class="toast__title">Payment received</p>
            <p class="toast__body">You received ₹${fee} from ${name || 'a user'}.</p>
        </div>
    `;
    container.appendChild(toast);

    setTimeout(() => {
        toast.classList.add('leaving');
        setTimeout(() => toast.remove(), 250);
    }, 4500);
}

async function checkForNewPayments() {
    try {
        const logs = await API.getLogs();

        if (lastSeenLogCount === null) {
            // First load: just remember the count, don't fire toasts for
            // history that already existed before this page was opened.
            lastSeenLogCount = logs.length;
            return;
        }

        if (logs.length > lastSeenLogCount) {
            const newEntries = logs.slice(lastSeenLogCount);
            newEntries.forEach(entry => {
                if (entry.event === 'EXIT' && entry.fee) {
                    showToast(entry.uid, entry.fee);
                }
            });
            lastSeenLogCount = logs.length;
        }
    } catch (err) {
        // Silent - the main loadDashboard() call already surfaces connection errors.
    }
}

async function loadDashboard() {
    const statusMsg = document.getElementById('status-msg');
    try {
        const users = await API.getUsers();
        const logs = await API.getLogs();
        const userList = Object.entries(users || {}).map(([uid, u]) => ({ uid, ...u }));

        const insideUsers = userList.filter(u => u.status === 'INSIDE');
        const total = CONFIG.TOTAL_SLOTS;
        const occupied = Math.min(insideUsers.length, total);
        const available = Math.max(total - occupied, 0);

        // Total revenue = sum of every EXIT event's fee
        const totalRevenue = logs
            .filter(l => l.event === 'EXIT')
            .reduce((sum, l) => sum + (l.fee || 0), 0);

        document.getElementById('stat-total').textContent = total;
        document.getElementById('stat-occupied').textContent = occupied;
        document.getElementById('stat-available').textContent = available;
        document.getElementById('stat-revenue').textContent = `₹${totalRevenue}`;

        const tbody = document.getElementById('inside-body');
        if (insideUsers.length === 0) {
            tbody.innerHTML = '<tr><td colspan="3" class="empty-row">No one currently inside.</td></tr>';
        } else {
            tbody.innerHTML = insideUsers.map(u => `
                <tr>
                    <td>${u.uid}</td>
                    <td>${u.name || '-'}</td>
                    <td>${u.vehicle || '-'}</td>
                </tr>
            `).join('');
        }
    } catch (err) {
        statusMsg.textContent = 'Could not reach the backend. Check that server.js is running and API_BASE in js/api.js matches its IP.';
        statusMsg.className = 'status-msg error';
        statusMsg.hidden = false;
    }
}

loadDashboard();
checkForNewPayments();
setInterval(loadDashboard, 5000);
setInterval(checkForNewPayments, 5000);