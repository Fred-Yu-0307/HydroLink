// Import the functions you need from the Firebase SDKs
import { initializeApp } from "https://www.gstatic.com/firebasejs/12.0.0/firebase-app.js";
import { getDatabase, ref, onValue, set, get, child } from "https://www.gstatic.com/firebasejs/12.0.0/firebase-database.js"; // Added 'child' for specific path access
import { getAuth, onAuthStateChanged, signOut } from "https://www.gstatic.com/firebasejs/12.0.0/firebase-auth.js";

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
    const database = getDatabase(app);
    const auth = getAuth(app);

    // --- IMPORTANT: Device ID ---
    // This will now be dynamically loaded from localStorage or Firebase
    let DEVICE_ID = null; 

    let currentUserId = null; // To store the current authenticated user's UID

    // --- UI Elements ---
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

    // Firebase references - will be initialized once DEVICE_ID is known
    let deviceStatusRef;
    let deviceSettingsRef;
    let manualRefillTargetRef; // Reference for manual refill command

    // --- Authentication State Listener ---
    onAuthStateChanged(auth, (user) => {
        if (user) {
            // User is signed in
            currentUserId = user.uid;
            console.log("User signed in:", user.email, "UID:", currentUserId);
            // Update UI for logged-in state (e.g., show dashboard, hide login form)
            const authNavLink = document.querySelector('.navbar-nav .nav-link[href="auth.html"]');
            if (authNavLink) {
                authNavLink.innerHTML = '<i class="bi bi-box-arrow-right"></i> Logout';
                authNavLink.onclick = handleLogout;
            }
            
            // Attempt to load the linked device ID
            loadLinkedDeviceId();

        } else {
            // User is signed out
            currentUserId = null;
            DEVICE_ID = null; // Clear device ID on logout
            console.log("User signed out.");
            // Update UI for logged-out state (e.g., redirect to login, clear dashboard)
            const authNavLink = document.querySelector('.navbar-nav .nav-link[href="auth.html"]');
            if (authNavLink) {
                authNavLink.innerHTML = '<i class="bi bi-person-circle"></i> Login';
                authNavLink.onclick = null; // Revert to default link behavior
            }
            window.location.href = __embed_url_prefix__ + 'auth.html'; // Redirect to login page
        }
    });

    // --- Handle Logout ---
    function handleLogout(e) {
        e.preventDefault();
        signOut(auth).then(() => {
            console.log("Signed out successfully.");
            // onAuthStateChanged will handle redirection
        }).catch((error) => {
            console.error("Error signing out:", error);
            alert("Error signing out. Please try again.");
        });
    }

    // Function to load the linked device ID from Firebase or localStorage
    async function loadLinkedDeviceId() {
        if (!currentUserId) return;

        // 1. Try to load from localStorage first (for quick access)
        let storedDeviceId = localStorage.getItem(`hydrolink_device_id_${currentUserId}`);
        if (storedDeviceId) {
            DEVICE_ID = storedDeviceId;
            console.log("Loaded DEVICE_ID from localStorage:", DEVICE_ID);
            initializeDeviceDataListeners();
            return;
        }

        // 2. If not in localStorage, check Firebase for user's linked devices
        const userDevicesRef = ref(database, `hydrolink/users/${currentUserId}/devices`);
        onValue(userDevicesRef, async (snapshot) => {
            const devices = snapshot.val();
            if (devices) {
                // Assuming one device per user for simplicity, take the first one
                const firstDeviceId = Object.keys(devices)[0];
                if (firstDeviceId) {
                    DEVICE_ID = firstDeviceId;
                    localStorage.setItem(`hydrolink_device_id_${currentUserId}`, DEVICE_ID); // Save to localStorage
                    console.log("Loaded DEVICE_ID from Firebase (user's linked devices):", DEVICE_ID);
                    initializeDeviceDataListeners();
                    return;
                }
            }
            
            console.log("No linked device found for this user. Checking MAC-to-UID mapping...");
            // If no device linked to user, try to find a device's Firebase UID via MAC address
            // This is a fallback, typically a user would link via setup.html
            const macToUidRef = ref(database, 'hydrolink/macToFirebaseUid');
            const macToUidSnapshot = await get(macToUidRef);
            const macToUidMapping = macToUidSnapshot.val();

            if (macToUidMapping) {
                // Iterate through the MAC-to-UID mapping to find a device not yet linked
                // This part might need refinement for multiple devices, but for a single device, it's a starting point
                for (const mac in macToUidMapping) {
                    const firebaseUid = macToUidMapping[mac];
                    // Check if this device is already linked to *any* user (optional, depending on multi-user strategy)
                    // For now, if we find any device in macToFirebaseUid, we'll try to link it to the current user
                    DEVICE_ID = firebaseUid;
                    localStorage.setItem(`hydrolink_device_id_${currentUserId}`, DEVICE_ID);
                    console.log("Found DEVICE_ID via MAC-to-UID mapping:", DEVICE_ID);
                    // Link this device to the current user in Firebase
                    await set(ref(database, `hydrolink/users/${currentUserId}/devices/${DEVICE_ID}`), true);
                    initializeDeviceDataListeners();
                    return;
                }
            }

            console.log("No linked device found for this user, and no unlinked device found via MAC mapping. Please go to setup.html to link a device.");
            // Optionally, redirect to setup page if no device is linked
            // window.location.href = __embed_url_prefix__ + 'setup.html';
            // For now, just show default empty dashboard
        }, { onlyOnce: true }); // Only fetch once on load
    }

    // --- Initialize Firebase Realtime Database Listeners once DEVICE_ID is known ---
    function initializeDeviceDataListeners() {
        if (!DEVICE_ID) {
            console.error("DEVICE_ID is not set. Cannot initialize Firebase listeners.");
            return;
        }

        // Assign Firebase references with the determined DEVICE_ID
        deviceStatusRef = ref(database, `hydrolink/devices/${DEVICE_ID}/status`);
        deviceSettingsRef = ref(database, `hydrolink/devices/${DEVICE_ID}/settings`);
        manualRefillTargetRef = ref(database, `hydrolink/devices/${DEVICE_ID}/settings/manualRefillTarget`);

        // Start listening for device data and settings
        listenForDeviceStatus();
        listenForDeviceSettings();

        // Initialize UI for settings based on current user's data
        initializeCustomizeModal();

        // Manual Refill button event listener needs to be set up after DEVICE_ID is known
        if (startManualRefillBtn) {
            refillPercentageInput.addEventListener('input', () => {
                refillPercentageValueSpan.innerText = `${refillPercentageInput.value}%`;
            });

            startManualRefillBtn.addEventListener('click', async () => {
                if (!currentUserId || !DEVICE_ID) {
                    alert("Please log in and link a device to send manual refill commands.");
                    return;
                }
                const targetPercentage = parseInt(refillPercentageInput.value);
                console.log(`Manual refill requested to ${targetPercentage}% for device ${DEVICE_ID}`);

                try {
                    // Send manual refill command to Firebase
                    await set(manualRefillTargetRef, targetPercentage);
                    alert(`Manual refill command sent to device: Target ${targetPercentage}%`);
                } catch (error) {
                    console.error("Error sending manual refill command:", error);
                    alert("Failed to send manual refill command. Please try again.");
                }
            });
        }
    }

    // --- Firebase Realtime Database Listeners ---

    // Listen for real-time device status updates from ESP32
    function listenForDeviceStatus() {
        if (!deviceStatusRef) return; // Ensure ref is initialized

        onValue(deviceStatusRef, (snapshot) => {
            const data = snapshot.val();
            if (data) {
                console.log("Received device status:", data);
                // Update System Status
                systemStatusText.innerText = data.systemStatus === 'online' ? 'Online' : 'Offline';
                systemStatusDot.style.backgroundColor = data.systemStatus === 'online' ? '#28a745' : '#dc3545'; // Green for online, Red for offline

                // Update Water Level Visualization
                const waterPercentage = data.waterPercentage || 0;
                waterPercentageDisplay.innerText = `${waterPercentage}%`;
                waterLevelFill.style.height = `${waterPercentage}%`;

                // Update Last Updated Timestamp
                if (data.lastUpdated) {
                    const date = new Date(data.lastUpdated);
                    lastUpdatedDisplay.innerText = date.toLocaleString();
                }

                // Update Battery Percentage (if available)
                // Assuming you'll add a UI element for battery
                // console.log("Battery:", data.batteryPercentage || 'N/A');

            } else {
                console.log("No device status data available.");
                systemStatusText.innerText = 'Offline';
                systemStatusDot.style.backgroundColor = '#dc3545';
                waterPercentageDisplay.innerText = '--%';
                waterLevelFill.style.height = '0%';
                lastUpdatedDisplay.innerText = 'N/A';
            }
        }, (error) => {
            console.error("Error listening to device status:", error);
            systemStatusText.innerText = 'Offline (Error)';
            systemStatusDot.style.backgroundColor = '#ffc107'; // Yellow for error
        });
    }

    // Listen for real-time device settings updates (from setup.html or other users)
    function listenForDeviceSettings() {
        if (!deviceSettingsRef) return; // Ensure ref is initialized

        onValue(deviceSettingsRef, (snapshot) => {
            const settings = snapshot.val();
            if (settings) {
                console.log("Received device settings:", settings);
                // Update dashboard UI elements with settings
                refillThresholdDisplay.innerText = `${settings.refillThresholdPercentage || '--'}%`;

                // Update Customize Modal inputs if it's open
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

    // --- Customize Modal Logic ---
    function initializeCustomizeModal() {
        // Update range slider values dynamically
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

        // Save Customize Settings button click
        if (saveCustomizeSettingsBtn) {
            saveCustomizeSettingsBtn.addEventListener('click', async () => {
                if (!currentUserId || !DEVICE_ID) {
                    alert("Please log in and link a device to save settings.");
                    return;
                }
                const refillThreshold = parseInt(autoRefillThresholdInput.value);
                const maxFillLevel = parseInt(maxFillLevelInput.value);

                try {
                    // Save settings to Firebase under the device's settings path
                    await set(ref(database, `hydrolink/devices/${DEVICE_ID}/settings/refillThresholdPercentage`), refillThreshold);
                    await set(ref(database, `hydrolink/devices/${DEVICE_ID}/settings/maxFillLevelPercentage`), maxFillLevel);
                    
                    alert("Settings saved successfully!");
                    // Close the modal
                    const customizeModal = bootstrap.Modal.getInstance(document.getElementById('customizeModal'));
                    if (customizeModal) {
                        customizeModal.hide();
                    }
                } catch (error) {
                    console.error("Error saving settings:", error);
                    alert("Failed to save settings. Please try again.");
                }
            });
        }
    }

    // Initial load time for availability logs
    if (initialLoadTime) {
        initialLoadTime.innerText = new Date().toLocaleString();
    }

    // Placeholder for other stats and history (will need more Firebase data)
    // totalWaterUsed, refillCount, lastRefillDate, notificationList, availabilityLogs, refillHistoryTable
    // These would typically be populated by more complex Firebase queries or Cloud Functions.
});
