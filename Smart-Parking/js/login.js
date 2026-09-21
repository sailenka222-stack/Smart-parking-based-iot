// Smart Parking - Login
//
// This is a simple single-admin login: it checks the entered email/password
// against CONFIG (js/config.js). There's no backend involved in login itself -
// once signed in, every other page (dashboard, users, history) talks to your
// own backend server (see js/api.js) instead of Firebase.

document.getElementById('login-form').addEventListener('submit', function (e) {
    e.preventDefault();

    const email = document.getElementById('email').value.trim();
    const password = document.getElementById('password').value;
    const errorBox = document.getElementById('error-msg');

    if (email === CONFIG.EMAIL && password === CONFIG.PASSWORD) {
        sessionStorage.setItem('sp_logged_in', 'true');
        window.location.href = 'home.html';
    } else {
        errorBox.textContent = 'Incorrect email or password.';
        errorBox.hidden = false;
    }
});

// If already logged in this session, skip straight to the dashboard.
if (sessionStorage.getItem('sp_logged_in') === 'true') {
    window.location.href = 'home.html';
}