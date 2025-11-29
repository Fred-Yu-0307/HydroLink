// Import the functions you need from the Firebase SDKs
import { initializeApp } from "https://www.gstatic.com/firebasejs/12.0.0/firebase-app.js";
import { getAuth, onAuthStateChanged, signOut } from "https://www.gstatic.com/firebasejs/12.0.0/firebase-auth.js";
import { 
    getDatabase, 
    ref, 
    onValue, 
    set, 
    get, 
    child, 
    query, 
    orderByKey, 
    limitToLast,
    endBefore
} from "https://www.gstatic.com/firebasejs/12.0.0/firebase-database.js";


// =======================================================
// CUSTOM ALERT MODAL FUNCTION (Replaces native alert())
// =======================================================
/**
 * Shows a custom, centered modal notification with a message and an animated icon.
 * @param {string} message - The message to display in the modal.
 * @param {'success'|'error'|'info'} type - The type of alert for icon and styling.
 */
window.showCustomAlert = function(message, type = 'info') {
    const modalElement = document.getElementById('customAlertModal');
    const messageElement = document.getElementById('alertMessage');
    const iconContainer = document.getElementById('alertIconContainer');

    if (!modalElement || !messageElement || !iconContainer) {
        console.warn("Custom alert elements not found. Falling back to console log.");
        console.log(`ALERT (${type.toUpperCase()}): ${message}`);
        return;
    }

    messageElement.textContent = message;
    iconContainer.className = 'alert-icon-container'; // Reset classes
    iconContainer.innerHTML = ''; // Clear previous icon

    let iconHtml = '';
    let iconClass = '';

    switch (type) {
        case 'success':
            iconHtml = '<i class="bi bi-check-circle-fill"></i>';
            iconClass = 'alert-icon-success';
            break;
        case 'error':
            iconHtml = '<i class="bi bi-x-octagon-fill"></i>';
            iconClass = 'alert-icon-error';
            break;
        case 'info':
        default:
            iconHtml = '<i class="bi bi-info-circle-fill"></i>';
            iconClass = 'alert-icon-info';
            break;
    }

    iconContainer.classList.add(iconClass);
    iconContainer.innerHTML = iconHtml;

    // Use Bootstrap's JavaScript API to show the modal
    const customAlertModal = new bootstrap.Modal(modalElement);
    customAlertModal.show();
};


// =======================================================
// VISUALIZATION FUNCTION
// =======================================================
/**
 * Updates the e-card wave visualization based on a water percentage value.
 * @param {number|string} percentageValue - The current water percentage (e.g., 54 or "54").
 */
function updateWaterLevelVisualization(percentageValue) {
    const card = document.querySelector('.e-card');
    const display = document.getElementById('waterPercentageDisplay');
    
    if (!card || !display) return;

    const percentage = parseInt(String(percentageValue).replace('%', '').trim());

    if (isNaN(percentage)) {
        display.textContent = `--%`;
        return;
    }
    
    display.textContent = `${percentage}%`;

    card.classList.remove('level-1', 'level-2', 'level-3', 'level-4');

    if (percentage >= 76) {
        card.classList.add('level-4', 'playing');
    } else if (percentage >= 51) {
        card.classList.add('level-3', 'playing');
    } else if (percentage >= 26) {
        card.classList.add('level-2', 'playing');
    } else { 
        card.classList.add('level-1', 'playing');
    }
}
// =======================================================
/**
 * Generates the HTML string for a single notification item.
 * @param {string} iconClass - e.g., 'bi bi-droplet-fill'
 * @param {string} bgClass - e.g., 'bg-danger'
 * @param {string} title - The main heading.
 * @param {string} content - The detailed content line.
 * @param {string} timeText - The timestamp text.
 * @param {boolean} isUnread - Whether to apply the 'unread' class.
 * @returns {string} The HTML for the list item.
 */
function createNotificationHtml(iconClass, bgClass, title, content, timeText, isUnread = true) {
    const unreadClass = isUnread ? 'unread' : '';
    return `
        <li class="notification-item ${unreadClass}">
            <div class="notification-icon ${bgClass}">
                <i class="${iconClass}"></i>
            </div>
            <div class="notification-content">
                <h6>${title}</h6>
                <p>${content}</p>
                <small class="text-muted">${timeText}</small>
            </div>
        </li>
    `;
}

// =======================================================
// FIREBASE REALTIME DATABASE LISTENERS FOR STATS (FINAL ROBUST LOGIC)
// =======================================================
/**
 * Sets up listeners to populate the Water Usage Statistics card.
 */
function listenForWaterStats() {
    // DEVICE_ID and database are expected to be available from the scope below
    if (!DEVICE_ID || !database) return;

    // ------------------------------------------
    // 1. Sensor Data Listener (Liters Used & Water Available)
    // ------------------------------------------
    const sensorDataRef = ref(database, `hydrolink/devices/${DEVICE_ID}/sensorData`);
    onValue(sensorDataRef, (snapshot) => {
        const data = snapshot.val();
        if (data) {
            // 1. Total Liters Used (Today)
            const litersUsedToday = data.totalLitersUsedToday || 0;
            if (totalWaterUsed) {
                totalWaterUsed.innerText = parseFloat(litersUsedToday).toFixed(2) + 'L'; 
            }

            // 4. Water Available (Boolean status)
            const isWaterAvailable = data.waterAvailable;
            if (totalrefillhours) {
                totalrefillhours.innerText = isWaterAvailable ? 'Yes' : 'No';
            }
        } else {
             if (totalWaterUsed) totalWaterUsed.innerText = '0L';
             if (totalrefillhours) totalrefillhours.innerText = 'N/A';
        }
    });

    // ------------------------------------------
    // 2. Refill History Listener (Count & Last Refill Date)
    // ------------------------------------------
    const refillHistoryRef = ref(database, `hydrolink/devices/${DEVICE_ID}/refillHistory`);
    onValue(refillHistoryRef, (snapshot) => {
        
        const now = new Date();
        const currentMonth = now.getMonth(); // 0-indexed
        const currentYear = now.getFullYear();
        let count = 0;
        let latestTimestamp = 0;
        let latestDateString = 'N/A';
        
        // Use snapshot.forEach() for reliable iteration over dynamic keys
        snapshot.forEach((childSnapshot) => {
            const entry = childSnapshot.val();

            if (entry && entry.timestamp) {
                
                // Get timestamp and ensure it is a number
                let timestampValue = Number(entry.timestamp);

                if (isNaN(timestampValue) || timestampValue === 0) return;

                // If timestamp is in seconds (10 digits), convert to milliseconds (13 digits)
                // This is the robust fix for timestamp format differences
                if (String(timestampValue).length < 13) {
                    timestampValue *= 1000;
                }
                
                const entryDate = new Date(timestampValue);
                
                if (isNaN(entryDate.getTime())) return; 

                // 2. Refills This Month (Check month and year)
                if (entryDate.getMonth() === currentMonth && entryDate.getFullYear() === currentYear) {
                    count++;
                }

                // 3. Last Refill Date (Find the highest timestamp)
                if (timestampValue > latestTimestamp) { 
                    latestTimestamp = timestampValue;
                    latestDateString = entryDate.toLocaleDateString();
                }
            }
        });

        // Update DOM elements
        if (refillCount) {
            refillCount.innerText = count;
        }
        if (lastRefillDate) {
            lastRefillDate.innerText = latestDateString;
        }

        if (count === 0 && refillCount) {
            refillCount.innerText = '0';
            if (lastRefillDate) lastRefillDate.innerText = 'N/A';
        }

    });
}
// =======================================================

let notificationsLoaded = 0;
const PAGE_SIZE = 10;
let lastNotificationKey = null;
let isLoadingMore = false;


function listenForRefillNotifications() {
    if (!DEVICE_ID || !database) return;

    const refillsRef = ref(database, `hydrolink/devices/${DEVICE_ID}/refillHistory`);

    onValue(refillsRef, (snapshot) => {
        snapshot.forEach(child => {
            const entry = child.val();
            const key = child.key;

            if (!entry || entry.notified) return; // prevent duplicates

            const ts = Number(entry.timestamp);
            const messageTime = ts ? new Date(ts).toLocaleString() : '';

            let notif = {
                title: "Refill Update",
                message: `Refill: ${entry.beforeLevelPct}% ➜ ${entry.afterLevelPct}% (Status: ${entry.status})`,
                type: "refill",
                timestamp: entry.timestamp || Date.now(),
                read: false
            };

            if (entry.status === "Failed" &&
                entry.actionsLog.includes("No Water Detected")) {
                notif.title = "No Water Detected";
                notif.message = entry.actionsLog;
                notif.type = "no_water";
            }

            set(ref(database, `hydrolink/devices/${DEVICE_ID}/notifications/${key}`), notif);
            set(ref(database, `hydrolink/devices/${DEVICE_ID}/refillHistory/${key}/notified`), true);
        });
    });
}

function listenForBatteryNotifications() {
    if (!DEVICE_ID || !database) return;

    const batteryRef = ref(database, `hydrolink/devices/${DEVICE_ID}/status/batteryPercentage`);
    const statusFlagRef = ref(database, `hydrolink/devices/${DEVICE_ID}/status/batteryNotifyStatus`);

    onValue(batteryRef, async (snapshot) => {
        const battery = snapshot.val();
        if (battery == null) return;

        // Check last notification status
        const statusSnapshot = await get(statusFlagRef);
        const lastStatus = statusSnapshot.exists() ? statusSnapshot.val() : "normal";

        let notif = null;
        let newStatus = lastStatus;

        // Battery Low Trigger
        if (battery <= 20 && lastStatus !== "low") {
            notif = {
                title: "Battery Low",
                message: `Battery level is critically low (${battery}%). Please recharge.`,
                type: "battery_low",
                timestamp: Date.now(),
                read: false
            };
            newStatus = "low";
        }

        // Battery Full Trigger
        if (battery >= 100 && lastStatus !== "full") {
            notif = {
                title: "Battery Full",
                message: `Battery is fully charged (${battery}%) ✔`,
                type: "battery_full",
                timestamp: Date.now(),
                read: false
            };
            newStatus = "full";
        }

        // Reset when within normal range
        if (battery > 20 && battery < 100 && lastStatus !== "normal") {
            await set(statusFlagRef, "normal");
            return;
        }

        // Save notification & new status
        if (notif) {
            const newNotifRef = ref(database, `hydrolink/devices/${DEVICE_ID}/notifications/${Date.now()}`);
            await set(newNotifRef, notif);
            await set(statusFlagRef, newStatus);
        }
    });
}


async function loadNotifications(initial = false) {
    if (!DEVICE_ID || !database || isLoadingMore) return;

    isLoadingMore = true;

    const notificationsRef = ref(database, `hydrolink/devices/${DEVICE_ID}/notifications`);

    let queryRef;

    if (initial) {
        queryRef = query(notificationsRef, orderByKey(), limitToLast(PAGE_SIZE));
    } else {
        if (!lastNotificationKey) {
            isLoadingMore = false;
            return;
        }
        queryRef = query(notificationsRef, orderByKey(), endBefore(lastNotificationKey), limitToLast(PAGE_SIZE));
    }

    const snapshot = await get(queryRef);

    if (!snapshot.exists()) {
        isLoadingMore = false;
        return;
    }

    const notifications = [];
    snapshot.forEach(child => {
        notifications.push({ key: child.key, ...child.val() });
    });

    notifications.sort((a, b) => b.key.localeCompare(a.key));

    const container = document.getElementById("notificationsList");
    const badge = document.getElementById("notificationBadge");

    if (initial) container.innerHTML = "";

    notifications.forEach(notif => {
        renderNotificationItem(notif, container);
    });

    notificationsLoaded += notifications.length;
    lastNotificationKey = notifications.length > 0 ? notifications[notifications.length - 1].key : null;

    updateBadgeCount();

    isLoadingMore = false;
}


function renderNotificationItem(notif, container) {
    const ts = notif.timestamp ? new Date(notif.timestamp).toLocaleString() : "Unknown";
    const unreadClass = notif.read ? "" : "unread";

    let icon = "bi bi-bell";
    let theme = "bg-secondary";

    switch(notif.type) {
        case "battery_low": icon = "bi bi-battery-half"; theme = "bg-danger"; break;
        case "battery_full": icon = "bi bi-battery-full"; theme = "bg-success"; break;
        case "refill": icon = "bi bi-droplet-fill"; theme = "bg-primary"; break;
        case "no_water": icon = "bi bi-exclamation-triangle-fill"; theme = "bg-warning"; break;
    }

    container.innerHTML += `
        <li class="notification-item ${unreadClass}">
            <div class="notification-icon ${theme}">
                <i class="${icon}"></i>
            </div>
            <div class="notification-content">
                <h6>${notif.title}</h6>
                <p>${notif.message}</p>
                <small class="text-muted">${ts}</small>
            </div>
        </li>
    `;
}

function updateBatteryDisplay(batteryPercentage) {
    const batteryLevel = document.getElementById("batteryLevel");
    if (!batteryLevel) return;

    const percentage = Math.max(0, Math.min(100, Number(batteryPercentage)));

    // Update width
    batteryLevel.style.width = `${percentage}%`;

    // Update color depending on percentage
    if (percentage <= 25) {
        batteryLevel.style.backgroundColor = "#FF3B30"; // Red — Low
    } else if (percentage <= 60) {
        batteryLevel.style.backgroundColor = "#FFC107"; // Yellow — Medium
    } else {
        batteryLevel.style.backgroundColor = "#28A745"; // Green — Good
    }
}




async function updateBadgeCount() {
    const snapshot = await get(ref(database, `hydrolink/devices/${DEVICE_ID}/notifications`));

    let unread = 0;
    snapshot.forEach(child => {
        if (!child.val().read) unread++;
    });

    document.getElementById("notificationBadge").innerText = `${unread} New`;
}

const notificationsListEl = document.getElementById("notificationsList");

notificationsListEl.addEventListener("scroll", () => {
    if (notificationsListEl.scrollTop + notificationsListEl.clientHeight >= notificationsListEl.scrollHeight - 5) {
        loadNotifications(false);
    }
});




document.addEventListener('DOMContentLoaded', function() {
    // Your web app's Firebase configuration
    const firebaseConfig = {
        apiKey: "AIzaSyCmFInEL6TMoD-9JwdPy-e9niNGGL5SjHA",
        authDomain: "hydrolink-d3c57.firebaseapp.com",
        databaseURL: "https://hydrolink-d3c57-default-rtdb.asia-southeast1.firebasedatabase.app",
        projectId: "hydrolink-d3c57",
        storageBucket: "hydrolink-d3c57.firebasestorage.app",
        messagingSenderId: "770257381412",
        appId: "1:770257381412:web:a894f35854cb82274950b1"
    };

    // Initialize Firebase
    const app = initializeApp(firebaseConfig);
    // Make database and auth globally accessible to the functions above
    window.database = getDatabase(app); 
    const auth = getAuth(app);

    // --- IMPORTANT: Device ID ---
    let DEVICE_ID = null; 
    let currentUserId = null; 
    window.DEVICE_ID = DEVICE_ID; // Expose to global scope for listenForWaterStats

    // --- UI Elements (Made globally accessible within this scope) ---
    const systemStatusDot = document.getElementById('systemStatusDot');
    const systemStatusText = document.getElementById('systemStatusText');
    const waterLevelFill = document.getElementById('waterLevelFill'); 
    const waterPercentageDisplay = document.getElementById('waterPercentageDisplay');
    const refillThresholdDisplay = document.getElementById('refillThresholdDisplay');
    const lastUpdatedDisplay = document.getElementById('lastUpdatedDisplay');
    const refillPercentageInput = document.getElementById('refillPercentage');
    const refillPercentageValueSpan = document.getElementById('refillPercentageValue');
    const startManualRefillBtn = document.getElementById('startManualRefill');
    const initialLoadTime = document.getElementById('initialLoadTime');
    const saveCustomizeSettingsBtn = document.getElementById('saveCustomizeSettings');
    const autoRefillThresholdInput = document.getElementById('autoRefillThreshold');
    const thresholdValueSpan = document.getElementById('thresholdValue');
    const maxFillLevelInput = document.getElementById('maxFillLevel');
    const maxFillValueSpan = document.getElementById('maxFillValue');
    
    // --- STATS UI Elements (Made globally accessible within this scope for listenForWaterStats) ---
    window.totalWaterUsed = document.getElementById('totalWaterUsed');
    window.refillCount = document.getElementById('refillCount');
    window.lastRefillDate = document.getElementById('lastRefillDate');
    window.totalrefillhours = document.getElementById('totalrefillhours');
    // ----------------------------

    // Firebase references - will be initialized once DEVICE_ID is known
    let deviceStatusRef;
    let deviceSettingsRef;
    let manualRefillTargetRef; 


    // --- Authentication State Listener ---
    onAuthStateChanged(auth, (user) => {
        if (user) {
            currentUserId = user.uid;
            console.log("User signed in:", user.email, "UID:", currentUserId);
            
            const authNavLink = document.querySelector('.navbar-nav .nav-link[href="auth.html"]');
            if (authNavLink) {
                authNavLink.innerHTML = '<i class="bi bi-box-arrow-right"></i> Logout';
                authNavLink.onclick = handleLogout;
            }
            
            loadLinkedDeviceId();

        } else {
            currentUserId = null;
            DEVICE_ID = null; 
            window.DEVICE_ID = null;
            console.log("User signed out.");
            
            const authNavLink = document.querySelector('.navbar-nav .nav-link[href="auth.html"]');
            if (authNavLink) {
                authNavLink.innerHTML = '<i class="bi bi-person-circle"></i> Login';
                authNavLink.onclick = null; 
            }
            window.location.href = 'auth.html'; 
        }
    });

    // --- Handle Logout ---
    function handleLogout(e) {
        e.preventDefault();
        signOut(auth).then(() => {
            console.log("Signed out successfully.");
        }).catch((error) => {
            console.error("Error signing out:", error);
            showCustomAlert("Error signing out. Please try again.", 'error'); // Replaced alert
        });
    }

    // Function to load the linked device ID from Firebase or localStorage
    async function loadLinkedDeviceId() {
        if (!currentUserId) return;

        let storedDeviceId = localStorage.getItem(`hydrolink_device_id_${currentUserId}`);
        if (storedDeviceId) {
            DEVICE_ID = storedDeviceId;
            window.DEVICE_ID = DEVICE_ID;
            console.log("Loaded DEVICE_ID from localStorage:", DEVICE_ID);
            initializeDeviceDataListeners();
            return;
        }

        const userDevicesRef = ref(database, `hydrolink/users/${currentUserId}/devices`);
        onValue(userDevicesRef, async (snapshot) => {
            const devices = snapshot.val();
            if (devices) {
                const firstDeviceId = Object.keys(devices)[0];
                if (firstDeviceId) {
                    DEVICE_ID = firstDeviceId;
                    window.DEVICE_ID = DEVICE_ID;
                    localStorage.setItem(`hydrolink_device_id_${currentUserId}`, DEVICE_ID); 
                    console.log("Loaded DEVICE_ID from Firebase (user's linked devices):", DEVICE_ID);
                    initializeDeviceDataListeners();
                    return;
                }
            }
            
            console.log("No linked device found for this user. Checking MAC-to-UID mapping...");
            const macToUidRef = ref(database, 'hydrolink/macToFirebaseUid');
            const macToUidSnapshot = await get(macToUidRef);
            const macToUidMapping = macToUidSnapshot.val();

            if (macToUidMapping) {
                for (const mac in macToUidMapping) {
                    const firebaseUid = macToUidMapping[mac];
                    DEVICE_ID = firebaseUid;
                    window.DEVICE_ID = DEVICE_ID;
                    localStorage.setItem(`hydrolink_device_id_${currentUserId}`, DEVICE_ID);
                    console.log("Found DEVICE_ID via MAC-to-UID mapping:", DEVICE_ID);
                    await set(ref(database, `hydrolink/users/${currentUserId}/devices/${DEVICE_ID}`), true);
                    initializeDeviceDataListeners();
                    return;
                }
            }

            console.log("No linked device found for this user, and no unlinked device found via MAC mapping. Please go to setup.html to link a device.");
        }, { onlyOnce: true });
    }

    // --- Initialize Firebase Realtime Database Listeners once DEVICE_ID is known ---
    function initializeDeviceDataListeners() {
        if (!DEVICE_ID) {
            console.error("DEVICE_ID is not set. Cannot initialize Firebase listeners.");
            return;
        }

        deviceStatusRef = ref(database, `hydrolink/devices/${DEVICE_ID}/status`);
        deviceSettingsRef = ref(database, `hydrolink/devices/${DEVICE_ID}/settings`);
        manualRefillTargetRef = ref(database, `hydrolink/devices/${DEVICE_ID}/settings/manualRefillTarget`);

        listenForDeviceStatus();
        listenForDeviceSettings();
        listenForWaterStats(); 
        initializeCustomizeModal();
        listenForRefillNotifications();
        listenForBatteryNotifications();
        loadNotifications(true);
        listenForBatteryLevel();


        if (startManualRefillBtn) {
            refillPercentageInput.addEventListener('input', () => {
                refillPercentageValueSpan.innerText = `${refillPercentageInput.value}%`;
            });

            startManualRefillBtn.addEventListener('click', async () => {
                if (!currentUserId || !DEVICE_ID) {
                    showCustomAlert("Please log in and link a device to send manual refill commands.", 'info'); // Replaced alert
                    return;
                }
                const targetPercentage = parseInt(refillPercentageInput.value);
                console.log(`Manual refill requested to ${targetPercentage}% for device ${DEVICE_ID}`);

                try {
                    await set(manualRefillTargetRef, targetPercentage);
                    showCustomAlert(`Manual refill command sent to device: Target ${targetPercentage}%`, 'success'); // Replaced alert
                } catch (error) {
                    console.error("Error sending manual refill command:", error);
                    showCustomAlert("Failed to send manual refill command. Please try again.", 'error'); // Replaced alert
                }
            });
        }
    }

//  Online/Offline Detection
let lastUpdateTime = Date.now();
const OFFLINE_TIMEOUT = 15000; // 15s no updates = offline

function setDeviceOnline() {
    lastUpdateTime = Date.now();
    systemStatusText.innerText = "Online";
    systemStatusDot.classList.remove("offline");
    systemStatusDot.classList.add("online");
}

function setDeviceOffline() {
    systemStatusText.innerText = "Offline";
    systemStatusDot.classList.remove("online");
    systemStatusDot.classList.add("offline");
}

//  Periodically check offline status
setInterval(() => {
    if (Date.now() - lastUpdateTime > OFFLINE_TIMEOUT) {
        setDeviceOffline();
    }
}, 5000);


//  Main Device Status Listener
function listenForDeviceStatus() {
    if (!deviceStatusRef) return;

    onValue(deviceStatusRef, (snapshot) => {
        const data = snapshot.val();
        if (data) {
            console.log("Received device status:", data);

            // New update from device = online
            setDeviceOnline();

            // water % (not in status)
            const waterPercentage = data.waterPercentage ?? 0;
            updateWaterLevelVisualization(waterPercentage);

            // timestamp conversion
            if (data.lastUpdated) {
                let ts = Number(data.lastUpdated);
                if (String(ts).length <= 10) ts *= 1000;
                lastUpdatedDisplay.innerText = new Date(ts).toLocaleString();
            }
        } else {
            console.log("No device status data available.");
            setDeviceOffline();
            updateWaterLevelVisualization(0);
            lastUpdatedDisplay.innerText = "N/A";
        }
    }, (error) => {
        console.error("Error listening to device status:", error);
        systemStatusText.innerText = "Offline (Error)";
        systemStatusDot.classList.remove("online");
        systemStatusDot.classList.add("offline");
    });
}


    function listenForDeviceSettings() {
        if (!deviceSettingsRef) return;

        onValue(deviceSettingsRef, (snapshot) => {
            const settings = snapshot.val();
            if (settings) {
                console.log("Received device settings:", settings);
                refillThresholdDisplay.innerText = `${settings.refillThresholdPercentage || '--'}%`;

                const thresholdDisplay = document.getElementById('thresholdDisplay');
                if (thresholdDisplay) {
                    thresholdDisplay.innerText = `${settings.refillThresholdPercentage || '--'}%`;
                }

                if (autoRefillThresholdInput) {
                    autoRefillThresholdInput.value = settings.refillThresholdPercentage || 25;
                    thresholdValueSpan.innerText = `${settings.refillThresholdPercentage || 25}%`;
                }
                if (maxFillLevelInput) {
                    maxFillLevelInput.value = settings.maxFillLevelPercentage || 75;
                    maxFillValueSpan.innerText = `${settings.maxFillLevelPercentage || 75}%`;
                }
            } else {
                console.log("No device settings data available.");
                refillThresholdDisplay.innerText = '--%';
            }
        }, (error) => {
            console.error("Error listening to device settings:", error);
        });
    }

    function listenForBatteryLevel() {
    if (!DEVICE_ID || !database) return;

    const batteryRef = ref(database, `hydrolink/devices/${DEVICE_ID}/sensorData/batteryPercentage`);

    onValue(batteryRef, (snapshot) => {
        const battery = snapshot.val();

        if (battery == null) return;
        updateBatteryDisplay(battery);
    });
}


    // --- Customize Modal Logic ---
    function initializeCustomizeModal() {
        if (autoRefillThresholdInput) {
            autoRefillThresholdInput.addEventListener('input', () => {
                thresholdValueSpan.innerText = `${autoRefillThresholdInput.value}%`;
            });
        }
        if (maxFillLevelInput) {
            maxFillLevelInput.addEventListener('input', () => {
                maxFillValueSpan.innerText = `${maxFillLevelInput.value}%`;
            });
        }

        if (saveCustomizeSettingsBtn) {
            saveCustomizeSettingsBtn.addEventListener('click', async () => {
                if (!currentUserId || !DEVICE_ID) {
                    showCustomAlert("Please log in and link a device to save settings.", 'info'); // Replaced alert
                    return;
                }
                const refillThreshold = parseInt(autoRefillThresholdInput.value);
                const maxFillLevel = parseInt(maxFillLevelInput.value);

                try {
                    await set(ref(database, `hydrolink/devices/${DEVICE_ID}/settings/refillThresholdPercentage`), refillThreshold);
                    await set(ref(database, `hydrolink/devices/${DEVICE_ID}/settings/maxFillLevelPercentage`), maxFillLevel);
                    
                    showCustomAlert("Settings saved successfully!", 'success'); // Replaced alert
                    
                    // Hide the customize modal after saving
                    const customizeModalElement = document.getElementById('customizeModal');
                    const customizeModal = bootstrap.Modal.getInstance(customizeModalElement);
                    if (customizeModal) {
                        customizeModal.hide();
                    }
                } catch (error) {
                    console.error("Error saving settings:", error);
                    showCustomAlert("Failed to save settings. Please try again.", 'error'); // Replaced alert
                }
            });
        }
    }

    if (initialLoadTime) {
        initialLoadTime.innerText = new Date().toLocaleString();
    }
});
