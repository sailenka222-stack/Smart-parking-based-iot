import { initializeApp } from "https://www.gstatic.com/firebasejs/12.0.0/firebase-app.js";
import { getDatabase } from "https://www.gstatic.com/firebasejs/12.0.0/firebase-database.js";

const firebaseConfig = {

    apiKey: "AIzaSyDLoyg4Vfm0brhjdNnqil4qZTCQmlQP6_Y",

    authDomain: "smart-parking-9f947.firebaseapp.com",

    databaseURL: "https://smart-parking-9f947-default-rtdb.asia-southeast1.firebasedatabase.app",

    projectId: "smart-parking-9f947",

    storageBucket: "smart-parking-9f947.firebasestorage.app",

    messagingSenderId: "336319795552",

    appId: "1:336319795552:web:928d48ae94db03ecb70e05"

};

const app = initializeApp(firebaseConfig);

export const db = getDatabase(app);