// Smart Parking - Users list
//
// Reads every registered card from the backend (GET /api/users), displays it
// in a table, and lets you click "Edit" on any row to change name/vehicle/
// type/wallet inline - useful for recharging a wallet balance without going
// through the Register page again.

if (sessionStorage.getItem('sp_logged_in') !== 'true') {
    window.location.href = 'index.html';
}

let allUsers = {};
let editingUid = null;

function renderRow(uid, u) {
    const isEditing = uid === editingUid;

    if (!isEditing) {
        return `
            <tr>
                <td>${uid}</td>
                <td>${u.name || '-'}</td>
                <td>${u.vehicle || '-'}</td>
                <td>${u.type || '-'}</td>
                <td class="wallet">₹${u.wallet ?? 0}</td>
                <td><span class="badge ${u.status === 'INSIDE' ? 'inside' : 'outside'}">${u.status || 'OUTSIDE'}</span></td>
                <td class="row-actions">
                    <button class="action-btn" data-action="edit" data-uid="${uid}">Edit</button>
                </td>
            </tr>
        `;
    }

    return `
        <tr>
            <td>${uid}</td>
            <td><input class="row-input" id="edit-name" value="${u.name || ''}"></td>
            <td><input class="row-input" id="edit-vehicle" value="${u.vehicle || ''}"></td>
            <td>
                <select class="row-input" id="edit-type">
                    <option value="Car" ${u.type === 'Car' ? 'selected' : ''}>Car</option>
                    <option value="Bike" ${u.type === 'Bike' ? 'selected' : ''}>Bike</option>
                </select>
            </td>
            <td><input class="row-input" id="edit-wallet" type="number" value="${u.wallet ?? 0}"></td>
            <td><span class="badge ${u.status === 'INSIDE' ? 'inside' : 'outside'}">${u.status || 'OUTSIDE'}</span></td>
            <td class="row-actions">
                <button class="action-btn save" data-action="save" data-uid="${uid}">Save</button>
                <button class="action-btn" data-action="cancel">Cancel</button>
            </td>
        </tr>
    `;
}

function renderTable() {
    const tbody = document.getElementById('users-body');
    const entries = Object.entries(allUsers);

    if (entries.length === 0) {
        tbody.innerHTML = '<tr><td colspan="7" class="empty-row">No users registered yet.</td></tr>';
        return;
    }

    tbody.innerHTML = entries.map(([uid, u]) => renderRow(uid, u)).join('');
}

async function loadUsers() {
    const statusMsg = document.getElementById('status-msg');
    try {
        allUsers = await API.getUsers();
        renderTable();
    } catch (err) {
        statusMsg.textContent = 'Could not reach the backend. Check that server.js is running and API_BASE in js/api.js matches its IP.';
        statusMsg.className = 'status-msg error';
        statusMsg.hidden = false;
        document.getElementById('users-body').innerHTML = '';
    }
}

document.getElementById('users-body').addEventListener('click', async (e) => {
    const btn = e.target.closest('button[data-action]');
    if (!btn) return;

    const action = btn.dataset.action;
    const statusMsg = document.getElementById('status-msg');

    if (action === 'edit') {
        editingUid = btn.dataset.uid;
        renderTable();
    }

    if (action === 'cancel') {
        editingUid = null;
        renderTable();
    }

    if (action === 'save') {
        const uid = btn.dataset.uid;
        const updated = {
            name: document.getElementById('edit-name').value.trim(),
            vehicle: document.getElementById('edit-vehicle').value.trim(),
            type: document.getElementById('edit-type').value,
            wallet: Number(document.getElementById('edit-wallet').value),
        };

        try {
            await API.saveUser(uid, updated);
            allUsers[uid] = { ...allUsers[uid], ...updated };
            editingUid = null;
            renderTable();

            statusMsg.textContent = `Updated ${updated.name || uid}.`;
            statusMsg.className = 'status-msg success';
            statusMsg.hidden = false;
        } catch (err) {
            statusMsg.textContent = 'Could not save changes. Check the backend is running.';
            statusMsg.className = 'status-msg error';
            statusMsg.hidden = false;
        }
    }
});

loadUsers();