// Smart Parking - API connector
//
// This is the ONE place that knows your backend's address. Every other page
// (dashboard, users, register, history) calls the functions below instead of
// writing fetch() calls directly - so if your backend's IP ever changes,
// you only update it here.

// ↓↓↓ CHANGE THIS to the IP address of the PC/server running server.js ↓↓↓
const API_BASE = 'http://10.10.50.117';

const API = {
    async getUsers() {
        const res = await fetch(`${API_BASE}/api/users`);
        return res.json();
    },

    async getUser(uid) {
        const res = await fetch(`${API_BASE}/api/user/${uid}`);
        if (res.status === 404) return null;
        return res.json();
    },

    async saveUser(uid, data) {
        const res = await fetch(`${API_BASE}/api/user/${uid}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data),
        });
        return res.json();
    },

    async setStatus(uid, patch) {
        const res = await fetch(`${API_BASE}/api/user/${uid}/status`, {
            method: 'PATCH',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(patch),
        });
        return res.json();
    },

    async getLogs() {
        const res = await fetch(`${API_BASE}/api/logs`);
        return res.json();
    },

    async deleteLogs() {
        const res = await fetch(`${API_BASE}/api/logs`, { method: 'DELETE' });
        return res.json();
    },

    async getSlots() {
        const res = await fetch(`${API_BASE}/api/slots`);
        return res.json();
    },

    async addLog(entry) {
        const res = await fetch(`${API_BASE}/api/log`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(entry),
        });
        return res.json();
    },
};