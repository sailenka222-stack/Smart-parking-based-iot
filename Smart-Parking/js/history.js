// Smart Parking - History
//
// IMPORTANT: this reads from GET /api/logs on the backend, which only shows
// entries that were actually posted to it via POST /api/log. Right now,
// we-10.c's WE10_CheckUser() only *reads* user data - it doesn't yet POST an
// entry/exit log when a card is scanned. So this page will show "no logs
// yet" until we add that logging call to the firmware. Ask if you want that
// added - it's a small addition to we-10.c and main.c.

if (sessionStorage.getItem('sp_logged_in') !== 'true') {
    window.location.href = 'index.html';
}

async function loadHistory() {
    const statusMsg = document.getElementById('status-msg');
    const infoMsg = document.getElementById('info-msg');
    const tbody = document.getElementById('logs-body');

    try {
        const logs = await API.getLogs();

        if (!logs || logs.length === 0) {
            tbody.innerHTML = '<tr><td colspan="4" class="empty-row">No logs yet.</td></tr>';
            infoMsg.textContent = 'Note: the firmware doesn\'t send entry/exit logs to the backend yet - this page will populate once that\'s added.';
            infoMsg.className = 'status-msg';
            infoMsg.style.color = 'var(--text-muted)';
            infoMsg.style.border = '1px solid var(--panel-line)';
            infoMsg.hidden = false;
            return;
        }

        // Show most recent first
        tbody.innerHTML = logs.slice().reverse().map(log => `
            <tr>
                <td class="timestamp">${new Date(log.timestamp).toLocaleString()}</td>
                <td>${log.uid || '-'}</td>
                <td><span class="badge ${log.event === 'ENTRY' ? 'inside' : 'outside'}">${log.event || '-'}</span></td>
                <td>${log.fee ? `₹${log.fee}` : '-'}</td>
            </tr>
        `).join('');
    } catch (err) {
        statusMsg.textContent = 'Could not reach the backend. Check that server.js is running and API_BASE in js/api.js matches its IP.';
        statusMsg.className = 'status-msg error';
        statusMsg.hidden = false;
        tbody.innerHTML = '';
    }
}

loadHistory();