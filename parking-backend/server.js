// Smart Parking Backend
//
// This replaces Firebase entirely. It stores user data in a simple local
// JSON file and serves it over PLAIN HTTP - both your frontend page and
// your WE10 module talk to this same server.
//
// No HTTPS anywhere in this flow, so the WE10's limited TLS stack is never
// involved - it just makes a normal HTTP GET/POST, which it's perfectly
// capable of.

const express = require('express');
const fs = require('fs');
const path = require('path');

const app = express();
const PORT = 80;
const DB_FILE = path.join(__dirname, 'data.json');

app.use(express.json());

// Allow the frontend page (running in a browser, possibly on a different
// origin/port) to call this server.
app.use((req, res, next) => {
    res.header('Access-Control-Allow-Origin', '*');
    res.header('Access-Control-Allow-Methods', 'GET,POST,PUT,DELETE,OPTIONS');
    res.header('Access-Control-Allow-Headers', 'Content-Type');
    if (req.method === 'OPTIONS') return res.sendStatus(200);
    next();
});

// ---- Simple file-based storage -------------------------------------------
function loadDB() {
    if (!fs.existsSync(DB_FILE)) {
        return { users: {}, logs: [], slots: { slot1: false, slot2: false } };
    }
    const db = JSON.parse(fs.readFileSync(DB_FILE, 'utf8'));
    if (!db.slots) db.slots = { slot1: false, slot2: false };
    return db;
}

function saveDB(db) {
    fs.writeFileSync(DB_FILE, JSON.stringify(db, null, 2));
}

// Seed the database with your existing user if it doesn't exist yet.
(function seed() {
    const db = loadDB();
    if (!db.users['43017215']) {
        db.users['43017215'] = {
            name: 'Mohit',
            status: 'OUTSIDE',
            type: 'Car',
            vehicle: 'AP31EK41',
            wallet: 500,
        };
        saveDB(db);
        console.log('Seeded initial user 43017215 (Mohit).');
    }
})();
// ---------------------------------------------------------------------------

/**
 * GET /api/user/:uid
 * Used by the WE10 (WE10_CheckUser). Returns the user record as JSON,
 * or 404 if the card isn't registered.
 */
app.get('/api/user/:uid', (req, res) => {
    const db = loadDB();
    const user = db.users[req.params.uid];

    console.log(`[${new Date().toISOString()}] GET /api/user/${req.params.uid}`);

    if (!user) {
        return res.status(404).json({ error: 'not_found' });
    }
    res.status(200).json(user);
});

/**
 * PUT /api/user/:uid
 * Used by your frontend to create or update a user (name, vehicle, wallet, etc).
 * Body: { "name": "...", "type": "Car", "vehicle": "...", "wallet": 500, "status": "OUTSIDE" }
 */
app.put('/api/user/:uid', (req, res) => {
    const db = loadDB();
    const uid = req.params.uid;

    db.users[uid] = { ...(db.users[uid] || {}), ...req.body };
    saveDB(db);

    console.log(`[${new Date().toISOString()}] PUT /api/user/${uid}: ${JSON.stringify(req.body)}`);
    res.status(200).json(db.users[uid]);
});

/**
 * PATCH /api/user/:uid/status
 * Used by the WE10 to flip a user's status on entry/exit, and to deduct wallet
 * balance on exit. Body: { "status": "INSIDE" | "OUTSIDE", "wallet": <new_value> }
 */
app.patch('/api/user/:uid/status', (req, res) => {
    const db = loadDB();
    const uid = req.params.uid;

    if (!db.users[uid]) {
        return res.status(404).json({ error: 'not_found' });
    }

    db.users[uid] = { ...db.users[uid], ...req.body };
    saveDB(db);

    console.log(`[${new Date().toISOString()}] PATCH /api/user/${uid}/status: ${JSON.stringify(req.body)}`);
    res.status(200).json(db.users[uid]);
});

/**
 * GET /api/users
 * Used by your frontend dashboard to list every registered user.
 */
app.get('/api/users', (req, res) => {
    const db = loadDB();
    res.status(200).json(db.users);
});

/**
 * POST /api/log
 * Optional: records an entry/exit event, useful for a history page later.
 * Body: { "uid": "...", "event": "ENTRY" | "EXIT", "fee": 0 }
 */
app.post('/api/log', (req, res) => {
    const db = loadDB();
    db.logs.push({ ...req.body, timestamp: new Date().toISOString() });
    saveDB(db);
    res.status(200).json({ ok: true });
});

app.get('/api/logs', (req, res) => {
    const db = loadDB();
    res.status(200).json(db.logs);
});

/**
 * DELETE /api/logs
 * Wipes the entry/exit history. Used by the "Clear History" button.
 */
app.delete('/api/logs', (req, res) => {
    const db = loadDB();
    db.logs = [];
    saveDB(db);
    console.log(`[${new Date().toISOString()}] History cleared.`);
    res.status(200).json({ ok: true });
});

/**
 * GET /api/slots
 * Used by visualization.html to show live occupancy per physical slot.
 */
app.get('/api/slots', (req, res) => {
    const db = loadDB();
    res.status(200).json(db.slots);
});

/**
 * PUT /api/slots
 * Used by the WE10/STM32 to push live IR sensor readings for slot1/slot2.
 * Body: { "slot1": true/false, "slot2": true/false }
 */
app.put('/api/slots', (req, res) => {
    const db = loadDB();
    db.slots = { ...db.slots, ...req.body };
    saveDB(db);
    console.log(`[${new Date().toISOString()}] PUT /api/slots: ${JSON.stringify(req.body)}`);
    res.status(200).json(db.slots);
});

// Health check
app.get('/health', (req, res) => {
    res.status(200).json({ status: 'ok' });
});

app.listen(PORT, '0.0.0.0', () => {
    console.log(`Smart Parking backend listening on http://0.0.0.0:${PORT}`);
    console.log(`Health check:  http://<server-ip>:${PORT}/health`);
    console.log(`User lookup:   http://<server-ip>:${PORT}/api/user/43017215`);
    console.log(`All users:     http://<server-ip>:${PORT}/api/users`);
});