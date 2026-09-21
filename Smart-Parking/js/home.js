// Smart Parking - Home hub
//
// This page just needs to (1) make sure someone is actually logged in before
// showing the control panel, and (2) let them sign out.

if (sessionStorage.getItem('sp_logged_in') !== 'true') {
    window.location.href = 'index.html';
}

document.getElementById('logout-btn').addEventListener('click', function () {
    sessionStorage.removeItem('sp_logged_in');
    window.location.href = 'index.html';
});