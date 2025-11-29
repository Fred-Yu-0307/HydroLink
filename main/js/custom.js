// =======================================================
// PART 1: FIREBASE MODULE IMPORTS & INITIALIZATION
// This uses the module syntax and is contained in an IIFE 
// (Immediately Invoked Function Expression) to manage scope.
// We expose necessary functions to the global window object.
// =======================================================

// --- Module Imports ---
// IMPORTANT: These imports will only work if the custom.js file is linked 
// in your HTML using <script type="module" src="js/custom.js"></script>
import { initializeApp } from "https://www.gstatic.com/firebasejs/12.0.0/firebase-app.js";
import { getDatabase, ref, onValue, get, set, remove } from "https://www.gstatic.com/firebasejs/12.0.0/firebase-database.js"; // ADDED get, set, remove
import { getAuth, onAuthStateChanged, updatePassword, EmailAuthProvider, reauthenticateWithCredential } from "https://www.gstatic.com/firebasejs/12.0.0/firebase-auth.js"; // ADDED auth-specific funcs

// --- Firebase Config ---
const firebaseConfig = {
    apiKey: "AIzaSyCmFInEL6TMoD-9JwdPy-e9niNGGL5SjHA",
    authDomain: "hydrolink-d3c57.firebaseapp.com",
    databaseURL: "https://hydrolink-d3c57-default-rtdb.asia-southeast1.firebasedatabase.app",
    projectId: "hydrolink-d3c57",
    storageBucket: "hydrolink-d3c57.firebasestorage.app",
    messagingSenderId: "770257381412",
    appId: "1:770257381412:web:a894f35854cb82274950b1"
};

const app = initializeApp(firebaseConfig);
const database = getDatabase(app);
const auth = getAuth(app);

// --- Global State Variables ---
let currentUserId = null;
let DEVICE_ID = null;
let lastUpdateCheckInterval = null; 

let currentUser = null; // Store the current authenticated user object
let currentDeviceBeingConfigured = null; // Stores the deviceId of the device opened in the modal
let isScanning = false; // Flag for modal scanning
let unsubscribeModalDrumHeight = null; // To store the unsubscribe function for modal drum height listener

// --- DOM References (Initialized after DOMContentLoaded) ---
let systemStatusText;
let systemStatusDot;
let lastUpdatedDisplay;



// Function to update system status
function updateSystemStatus(isOnline) {
    if (isOnline) {
        systemStatusText.textContent = "Online";
        systemStatusDot.classList.remove('offline');
        systemStatusDot.classList.add('online');
    } else {
        systemStatusText.textContent = "Offline";
        systemStatusDot.classList.remove('online');
        systemStatusDot.classList.add('offline');
    }
}

// Function to check device last update
function checkDeviceLastUpdate() {
    if (!DEVICE_ID) {
        console.warn("DEVICE_ID not available for status check.");
        updateSystemStatus(false); 
        lastUpdatedDisplay.textContent = "N/A";
        return;
    }

    const lastUpdatedRef = ref(database, `hydrolink/devices/${DEVICE_ID}/status/lastUpdated`);
    onValue(lastUpdatedRef, (snapshot) => {
        const lastUpdatedTimestamp = snapshot.val();
        const currentTime = Date.now(); 

        if (lastUpdatedTimestamp) {
            const timeDifferenceSeconds = (currentTime - lastUpdatedTimestamp) / 1000;
            const date = new Date(lastUpdatedTimestamp);
            lastUpdatedDisplay.textContent = date.toLocaleString(); 

            if (timeDifferenceSeconds < 60) { // Less than 1 minute
                updateSystemStatus(true); // Online
            } else {
                updateSystemStatus(false); // Offline
            }
        } else {
            updateSystemStatus(false); 
            lastUpdatedDisplay.textContent = "N/A";
        }
    }, { onlyOnce: false });
}

// Authentication state listener and Device ID logic
onAuthStateChanged(auth, (user) => {
    if (user) {
        currentUserId = user.uid;
        currentUser = user; // 🔑 CRUCIAL: Set the global user object for all handlers
        console.log("History/Dashboard/Settings: User signed in...", "UID:", currentUserId);
            
            const authNavLink = document.querySelector('.navbar-nav .nav-link[href="auth.html"]');
            if (authNavLink) {
                authNavLink.innerHTML = '<i class="bi bi-box-arrow-right"></i> Logout';
                authNavLink.onclick = handleLogout;
            }
        // 1. Check if we are on the settings page to load user data
        if (document.body.id === 'settings-page') {
            // Load all profile and address data from Firebase
            loadUserDataAndAddress(); 
        }
        
        // 2. Device ID and status logic (for Dashboard/other pages)
        const storedDeviceId = localStorage.getItem(`hydrolink_device_id_${currentUserId}`);
        if (storedDeviceId) {
            DEVICE_ID = storedDeviceId;
            console.log("Loaded device ID from localStorage:", DEVICE_ID);
            
            // Only run device status check if DOM elements are available
            if (systemStatusText && systemStatusDot && lastUpdatedDisplay) {
                checkDeviceLastUpdate();
                if (lastUpdateCheckInterval) clearInterval(lastUpdateCheckInterval); 
                lastUpdateCheckInterval = setInterval(checkDeviceLastUpdate, 30000); 
            }
            
            // --- NEW: History Page Logic (after DEVICE_ID is set) ---
            if (document.body.id === 'history-page') {
                fetchAndRenderHistory();
            }
            // --------------------------------------------------------

        } else {
            console.warn("No device ID found in localStorage. Device not linked yet.");
            if (systemStatusText) {
                updateSystemStatus(false); 
                lastUpdatedDisplay.textContent = "No device linked";
            }
        }
    } else {
        currentUserId = null;
        currentUser = null; // Good practice to clear
        DEVICE_ID = null;
        console.log("User signed out. Redirecting to login.");
        if (lastUpdateCheckInterval) clearInterval(lastUpdateCheckInterval);
        if (!window.location.pathname.endsWith('auth.html')) {
            window.location.href = 'auth.html';
        }
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

// PART 2: GLOBAL UTILITY/EXPORT FUNCTIONS (Must be exposed)

/**
 * Calculates the date cutoff based on the selected range (in days).
 */
function getDateCutoff(days) {
    if (days === 'All') {
        // Return a date far in the past to include all records
        return new Date(0); 
    }
    
    const daysNum = parseInt(days, 10);
    const today = new Date();
    const cutoff = new Date(today);
    cutoff.setDate(today.getDate() - daysNum);
    cutoff.setHours(0, 0, 0, 0); // Start of the cutoff day
    
    return cutoff;
}

/**
 * Sets the value of a select element and optionally triggers a 'change' event.
 */
function setSelectValueAndTriggerChange(selectElement, value) {
    if (selectElement && value) {
        selectElement.value = value;
        // Manually dispatch the 'change' event to trigger dependent logic
        selectElement.dispatchEvent(new Event('change')); 
    }
}

/**
 * Filters the refill history table based on the selected criteria.
 * This is called by 'onchange' or button clicks.
 */
function filterTable() {
    const dateRange = document.getElementById('dateRangeFilter')?.value || 'All';
    const refillType = document.getElementById('typeFilter')?.value || 'All';
    const status = document.getElementById('statusFilter')?.value || 'All';

    const tableBody = document.getElementById('refillTableBody');
    if (!tableBody) {
        console.warn("Table body 'refillTableBody' not found. Cannot filter.");
        return;
    }
    const rows = tableBody.getElementsByTagName('tr');
    const dateCutoff = getDateCutoff(dateRange);

    for (let i = 0; i < rows.length; i++) {
        const row = rows[i];
        // Skip informational rows (like 'No data' or 'Loading')
        if (row.querySelector('td[colspan="8"]')) {
            row.style.display = 'none'; 
            continue;
        }

        const rowDateStr = row.getAttribute('data-date'); 
        const rowType = row.getAttribute('data-type');
        const rowStatus = row.getAttribute('data-status');
        let showRow = true;

        // Date Filter Logic
        if (dateRange !== 'All' && rowDateStr) {
            const rowDate = new Date(rowDateStr);
            // We want to show the row if the rowDate is >= the cutoff date
            if (rowDate < dateCutoff) {
                showRow = false;
            }
        }

        // Type Filter Logic
        if (refillType !== 'All' && rowType !== refillType) {
            showRow = false;
        }

        // Status Filter Logic
        if (status !== 'All' && rowStatus !== status) {
            showRow = false;
        }

        // Show or hide the row
        row.style.display = showRow ? "" : "none";
    }
}

/**
 * Exports the currently visible table data (filtered rows) to a CSV file.
 */
function exportToSpreadsheet(event) {
    event.preventDefault(); 
    // ... your exportToSpreadsheet logic ... (The one you provided is fine)
    const table = document.querySelector('.table-hover');
    // ... (CSV generation and download logic) ...
    
    // START of your CSV logic
    let csv = [];
    const headerRow = table.querySelector('thead tr');
    const headers = Array.from(headerRow.querySelectorAll('th:not(:last-child)')).map(th => th.innerText.trim());
    csv.push(headers.join(','));

    const rows = table.querySelectorAll('tbody tr');
    rows.forEach(row => {
        if (row.style.display !== 'none' && !row.querySelector('td[colspan="8"]')) { // Skip hidden and informational rows
            const rowData = [];
            const cells = row.querySelectorAll('td:not(:last-child)');
            cells.forEach((cell, index) => {
                let cellText = '';
                
                if (index === 0) {
                    const strong = cell.querySelector('strong')?.innerText || '';
                    const small = cell.querySelector('small')?.innerText || '';
                    cellText = `${strong} ${small}`.trim();
                } 
                else if (index === 1 || index === 6) {
                    cellText = cell.querySelector('.badge')?.innerText.trim() || cell.innerText.trim();
                } 
                else {
                    cellText = cell.innerText.trim().replace(/[\n\r\t]/g, ' ').replace(/\s{2,}/g, ' ').replace('%', '').replace('L', '').trim();
                }
                
                rowData.push(`"${cellText.replace(/"/g, '""')}"`); // Handle quotes in data
            });
            csv.push(rowData.join(','));
        }
    });

    if (csv.length === 1) { // Only header row exists
        alert('No data to export after applying current filters.');
        return;
    }

    const csvContent = csv.join('\n');
    const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.setAttribute('href', url);
    link.setAttribute('download', 'Hydrolink_Refill_History.csv');
    link.style.visibility = 'hidden';
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    alert('Exported filtered data to Spreadsheet (.csv)!');
    // END of your CSV logic
}

/**
 * Exports the currently visible table data to a PDF file in Landscape orientation.
 */
function exportToPDF(event) {
    event.preventDefault(); 

    // ** jsPDF FIX **
    if (typeof window.jspdf !== 'undefined' && typeof window.jsPDF === 'undefined') {
        window.jsPDF = window.jspdf.jsPDF;
    }
    
    if (typeof jsPDF === 'undefined') {
        alert("The PDF Export feature is not fully loaded. Check your CDN links.");
        return;
    }

    const doc = new jsPDF({ 
        orientation: 'l', 
        unit: 'mm',
        format: 'a4'
    });
    
    if (typeof doc.autoTable !== 'function') {
        alert("jsPDF AutoTable plugin is missing or failed to load. Check the second CDN link.");
        return;
    }
    
    const table = document.querySelector('.table-hover');
    const head = [];
    const body = [];

    // 1. Prepare Header
    const headerRow = table.querySelector('thead tr');
    head.push(Array.from(headerRow.querySelectorAll('th:not(:last-child)')).map(th => th.innerText.trim()));

    // 2. Prepare Body (Visible Rows Only)
    const rows = table.querySelectorAll('tbody tr');
    rows.forEach(row => {
        if (row.style.display !== 'none' && !row.querySelector('td[colspan="8"]')) { // Skip hidden and informational rows
            const rowData = [];
            const cells = row.querySelectorAll('td:not(:last-child)');
            cells.forEach((cell, index) => {
                let cellText = '';
                
                if (index === 0) {
                    const strong = cell.querySelector('strong')?.innerText || '';
                    const small = cell.querySelector('small')?.innerText || '';
                    cellText = `${strong} | ${small}`.trim(); // Changed to a single line separator for PDF
                } else if (index === 1 || index === 6) {
                    cellText = cell.querySelector('.badge')?.innerText.trim() || cell.innerText.trim();
                } else {
                    cellText = cell.innerText.trim().replace(/[\n\r\t]/g, ' ').replace(/\s{2,}/g, ' ').trim();
                }
                
                rowData.push(cellText); 
            });
            body.push(rowData);
        }
    });

    if (body.length === 0) {
        alert('No data to export after applying current filters.');
        return;
    }

    // 3. Generate PDF
    doc.autoTable({
        head: head,
        body: body,
        startY: 15,
        styles: { fontSize: 8 },
        headStyles: { fillColor: [230, 230, 230], textColor: [0, 0, 0] },
        theme: 'grid'
    });

    doc.setProperties({ title: 'Hydrolink Refill History' });
    doc.save('Hydrolink_Refill_History.pdf');
    alert('Exported filtered data to PDF (Landscape)!');
}

// --- Expose functions to the global window object ---
// This allows your HTML to call them directly via onclick="..." or onchange="..."
window.filterTable = filterTable;
window.exportToSpreadsheet = exportToSpreadsheet;
window.exportToPDF = exportToPDF;


// =======================================================
// PART 3: PAGE INITIALIZATION (with pagination)
// =======================================================

let allRecords = [];        // Holds all fetched records
let currentPage = 1;        // Current page index
const recordsPerPage = 10;  // Show 10 per page

async function fetchAndRenderHistory() {
    if (!DEVICE_ID) {
        console.warn("Cannot fetch history: DEVICE_ID is null.");
        document.getElementById('refillTableBody').innerHTML =
            '<tr><td colspan="8" class="text-center text-muted">No device linked or history available.</td></tr>';
        return;
    }

    const historyRef = ref(database, `hydrolink/devices/${DEVICE_ID}/refillHistory`);
    const tableBody = document.getElementById('refillTableBody');
    tableBody.innerHTML =
        '<tr><td colspan="8" class="text-center"><span class="spinner-border spinner-border-sm me-2"></span>Loading History...</td></tr>';

    try {
        const snapshot = await get(historyRef);
        if (snapshot.exists()) {
            const historyRecords = snapshot.val();

            // Sort by timestamp descending
            allRecords = Object.entries(historyRecords)
                .sort(([, a], [, b]) => b.timestamp - a.timestamp);

            currentPage = 1;
            renderTablePage(); // Render the first page
        } else {
            tableBody.innerHTML = '<tr><td colspan="8" class="text-center text-muted">No refill records found for this device.</td></tr>';
        }
    } catch (error) {
        console.error("Error fetching refill history:", error);
        tableBody.innerHTML = '<tr><td colspan="8" class="text-center text-danger">Error loading data.</td></tr>';
    }
}

// =======================================================
// PAGINATION HANDLERS
// =======================================================
function renderTablePage() {
    const tableBody = document.getElementById('refillTableBody');

    // Determine slice range
    const startIndex = (currentPage - 1) * recordsPerPage;
    const endIndex = startIndex + recordsPerPage;
    const pageRecords = allRecords.slice(startIndex, endIndex);

    if (pageRecords.length === 0) {
        tableBody.innerHTML = '<tr><td colspan="8" class="text-center text-muted">No refill records found.</td></tr>';
        updatePagination();
        return;
    }

    let htmlContent = '';
    pageRecords.forEach(([recordId, record]) => {
        if (!record.timestamp || record.beforeLevelPct === undefined || record.afterLevelPct === undefined ||
            record.amountLitersAdded === undefined || record.durationMin === undefined || !record.status) return;

        const date = new Date(record.timestamp);
        const dateStr = date.toLocaleDateString('en-US', { month: 'short', day: 'numeric', year: 'numeric' });
        const timeStr = date.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' });
        const dataDate = date.toISOString().split('T')[0];

        const recordType =
            record.actionsLog?.includes('Auto') || record.actionsLog?.includes('Threshold') ? 'Automatic' :
            record.actionsLog?.includes('Manual') ? 'Manual' : 'Scheduled';

        const before = parseFloat(record.beforeLevelPct);
        const after = parseFloat(record.afterLevelPct);
        const added = parseFloat(record.amountLitersAdded);
        const duration = parseFloat(record.durationMin);

        const typeClass =
            recordType === 'Manual' ? 'bg-info' :
            recordType === 'Scheduled' ? 'bg-secondary' : 'bg-primary';

        const statusClass =
            record.status === 'Partial' ? 'bg-warning' :
            record.status === 'Failed' ? 'bg-danger' : 'bg-success';

        const beforeColor = before <= 25 ? 'bg-danger' : before <= 50 ? 'bg-warning' : 'bg-primary';
        const afterColor = after <= 25 ? 'bg-danger' : after <= 50 ? 'bg-warning' : 'bg-primary';

        htmlContent += `
        <tr data-date="${dataDate}" data-type="${recordType}" data-status="${record.status}">
            <td><strong>${dateStr}</strong><br><small class="text-muted">${timeStr}</small></td>
            <td><span class="badge ${typeClass}">${recordType}</span></td>
            <td>
                <div class="d-flex align-items-center">
                    <div class="progress me-2" style="width:60px;height:8px;">
                        <div class="progress-bar ${beforeColor}" style="width:${before}%"></div>
                    </div>
                    <span>${before}%</span>
                </div>
            </td>
            <td>
                <div class="d-flex align-items-center">
                    <div class="progress me-2" style="width:60px;height:8px;">
                        <div class="progress-bar ${afterColor}" style="width:${after}%"></div>
                    </div>
                    <span>${after}%</span>
                </div>
            </td>
            <td><strong>${added.toFixed(1)}L</strong></td>
            <td>${duration} min</td>
            <td><span class="badge ${statusClass}">${record.status}</span></td>
            <td>
                <button class="btn btn-sm btn-outline-primary me-1"><i class="fas fa-eye"></i></button>
                <button class="btn btn-sm btn-outline-danger" onclick="deleteRefillRecord('${recordId}')"><i class="fas fa-trash"></i></button>
            </td>
        </tr>`;
    });

    tableBody.innerHTML = htmlContent;
    updatePagination();
}

// =======================================================
// PAGINATION NAVIGATION
// =======================================================
function updatePagination() {
    const totalPages = Math.ceil(allRecords.length / recordsPerPage);
    const pagination = document.getElementById('pagination');
    if (!pagination) return;
    pagination.innerHTML = '';

    const maxVisible = 2; // show 2 pages before and after current

    // === PREVIOUS BUTTON ===
    pagination.innerHTML += `
        <li class="page-item ${currentPage === 1 ? 'disabled' : ''}">
            <a class="page-link" href="javascript:void(0)" onclick="changePage(${currentPage - 1}); return false;">
                <i class="fas fa-chevron-left"></i>
            </a>
        </li>`;

    // === PAGE NUMBERS ===
    let startPage = Math.max(1, currentPage - maxVisible);
    let endPage = Math.min(totalPages, currentPage + maxVisible);

    // Adjust if near start
    if (currentPage <= maxVisible) {
        endPage = Math.min(totalPages, 1 + maxVisible * 2);
    }

    // Adjust if near end
    if (currentPage > totalPages - maxVisible) {
        startPage = Math.max(1, totalPages - maxVisible * 2);
    }

    for (let i = startPage; i <= endPage; i++) {
        pagination.innerHTML += `
            <li class="page-item ${i === currentPage ? 'active' : ''}">
                <a class="page-link" href="javascript:void(0)" onclick="changePage(${i}); return false;">${i}</a>
            </li>`;
    }

    // === NEXT BUTTON ===
    pagination.innerHTML += `
        <li class="page-item ${currentPage === totalPages ? 'disabled' : ''}">
            <a class="page-link" href="javascript:void(0)" onclick="changePage(${currentPage + 1}); return false;">
                <i class="fas fa-chevron-right"></i>
            </a>
        </li>`;

    // === PAGE INFO LABEL ===
    const pageInfo = document.getElementById('pageInfo');
    if (pageInfo) {
        const totalRecords = allRecords.length;
        const startRecord = totalRecords === 0 ? 0 : (currentPage - 1) * recordsPerPage + 1;
        const endRecord = Math.min(startRecord + recordsPerPage - 1, totalRecords);
        pageInfo.textContent = `Results ${startRecord} - ${endRecord} of ${totalRecords}`;
    }
}



function changePage(page) {
    const totalPages = Math.ceil(allRecords.length / recordsPerPage);
    if (page < 1 || page > totalPages) return;
    currentPage = page;
    renderTablePage();
}

// Expose functions to global scope so inline onclick handlers work while using <script type="module">
window.changePage = changePage;
window.updatePagination = updatePagination;
window.renderTablePage = renderTablePage;
window.fetchAndRenderHistory = fetchAndRenderHistory;
window.deleteRefillRecord = deleteRefillRecord; // if not already exposed




// =======================================================
// DELETE REFILL HISTORY RECORD (with Bootstrap Modals)
// =======================================================
let recordIdToDelete = null;

async function deleteRefillRecord(recordId) {
    if (!DEVICE_ID) {
        showInfoModal("Device ID not found. Cannot delete record.", "danger");
        return;
    }

    // Store which record to delete and show confirmation modal
    recordIdToDelete = recordId;
    const deleteModalEl = document.getElementById("deleteConfirmModal");

    // Ensure Bootstrap modal is ready
    const deleteModal = new bootstrap.Modal(deleteModalEl);
    deleteModal.show();

    // Ensure event listener is only attached once
    const confirmBtn = document.getElementById("confirmDeleteBtn");
    confirmBtn.onclick = async () => {
        try {
            await remove(ref(database, `hydrolink/devices/${DEVICE_ID}/refillHistory/${recordIdToDelete}`));
            deleteModal.hide();
            showInfoModal("Record deleted successfully!", "success");
            fetchAndRenderHistory();
        } catch (error) {
            console.error("Error deleting record:", error);
            deleteModal.hide();
            showInfoModal("Failed to delete record. Check console for details.", "danger");
        } finally {
            recordIdToDelete = null;
        }
    };
}

// Show an information modal instead of alert()
function showInfoModal(message, type = "info") {
    const infoModalEl = document.getElementById("infoModal");
    const infoModalMessage = document.getElementById("infoModalMessage");
    const header = infoModalEl.querySelector(".modal-header");

    // Update color theme
    if (type === "success") {
        header.className = "modal-header bg-success text-white";
    } else if (type === "danger") {
        header.className = "modal-header bg-danger text-white";
    } else {
        header.className = "modal-header bg-primary text-white";
    }

    infoModalMessage.textContent = message;

    const infoModal = new bootstrap.Modal(infoModalEl);
    infoModal.show();
}

// Make available to HTML buttons
window.deleteRefillRecord = deleteRefillRecord;


// --- NEW FUNCTION FOR HISTORY PAGE INITIALIZATION ---
function initHistoryPage() {
    console.log("Initializing History Page.");
    
    // 2. Attach Event Listeners
    const dateFilter = document.getElementById('dateRangeFilter');
    const typeFilter = document.getElementById('typeFilter');
    const statusFilter = document.getElementById('statusFilter');
    
    if (dateFilter) dateFilter.addEventListener('change', filterTable);
    if (typeFilter) typeFilter.addEventListener('change', filterTable);
    if (statusFilter) statusFilter.addEventListener('change', filterTable);
}
// ----------------------------------------------------


// =======================================================
// PART 4: SETTINGS PAGE CORE FUNCTIONS (Devices, Modal, Address Data)
// =======================================================

// --- Address Data (Move this near the address logic or keep it global) ---
const addressData = {
    // Paste the ENTIRE addressData object here (from source 360 to 400)
    ncr: { /* ... */ },
    region9: { /* ... */ },
    region10: { /* ... */ },
    region7: { /* ... */ }
};


// --- Device Management Functions ---

async function loadLinkedDevices() {
    // PASTE the full loadLinkedDevices function (Source 275 to 301)
    if (!currentUserId) {
        // ... (rest of the function) ...
    }
    // ... (rest of the function including onValue listener and event handlers) ...
}

async function confirmDisconnect(deviceId) {
    // PASTE the full confirmDisconnect function (Source 302 to 312)
    const confirmAction = confirm("Are you sure you want to disconnect this device? This action cannot be undone.");
    if (!confirmAction) {
        return;
    }
    // ... (rest of the function) ...
}


// --- Modal Scan Logic Functions ---

async function triggerModalDrumMeasurement() {
    // PASTE the full triggerModalDrumMeasurement function (Source 315 to 341)
    if (!currentUserId || !currentDeviceBeingConfigured) {
        // ... (rest of the function) ...
    }
    // ... (rest of the function including the 8-second delay and Firebase promise) ...
}

function completeModalDrumHeightScan(heightCm) {
    // PASTE the full completeModalDrumHeightScan function (Source 342 to 346)
    // ... (function logic) ...
}

function resetModalScanSection() {
    // PASTE the full resetModalScanSection function (Source 347 to 351)
    // ... (function logic) ...
}

async function saveModalDrumHeight() {
    // PASTE the full saveModalDrumHeight function (Source 352 to 359)
    // ... (function logic) ...
}

// =======================================================
// PART X: NEW UTILITY FUNCTIONS (Settings Page)
// =======================================================

// Message Box Function (replaces alert/confirm)
function displayMessage(message, type = 'info', duration = 4000) {
    const messageBoxContainer = document.getElementById('messageBoxContainer');
    if (!messageBoxContainer) return; // Prevent errors if not on a page with the container
    
    // Remove any existing message boxes to ensure only one is visible at a time
    const existingMessageBox = messageBoxContainer.querySelector('.message-box');
    if (existingMessageBox) {
        existingMessageBox.remove();
    }

    const alertDiv = document.createElement('div');
    alertDiv.className = `alert alert-${type} message-box`;
    alertDiv.setAttribute('role', 'alert');
    alertDiv.innerHTML = `<div>${message}</div>`;
    messageBoxContainer.appendChild(alertDiv);
    setTimeout(() => {
        alertDiv.remove();
    }, duration);
}

// Helper function to populate a select element
function populateSelect(selectElement, options = []) {
    // Clear existing options except the first placeholder
    selectElement.innerHTML = `<option value="" selected disabled>Select ${selectElement.id.charAt(0).toUpperCase() + selectElement.id.slice(1).replace('zipcode', 'Zipcode')}</option>`;
    if (options && options.length > 0) {
        selectElement.disabled = false;
        options.forEach(option => {
            const optionElement = document.createElement('option');
            optionElement.value = option;
            optionElement.textContent = option;
            selectElement.appendChild(optionElement);
        });
    } else {
        selectElement.disabled = true;
    }
}

// Clear and disable dependent selects
function clearDependentSelects(...selects) {
    selects.forEach(select => {
        select.innerHTML = `<option value="" selected disabled>Select ${select.id.charAt(0).toUpperCase() + select.id.slice(1).replace('zipcode', 'Zipcode')}</option>`;
        select.disabled = true;
    });
}

// =======================================================
// NEW FUNCTION TO LOAD PROFILE AND ADDRESS DATA
// =======================================================

/**
 * Loads the current user's profile and address data from Firebase
 * and populates the forms on the Settings page.
 */
async function loadUserDataAndAddress() {
    if (!currentUserId || !currentUser) { 
        displayMessage("User data not found. Cannot load data.", "warning");
        return;
    }
    
    // Check if the address data structure is available in the module scope
    if (typeof addressData === 'undefined') {
        console.error("FATAL ERROR: addressData structure is not defined in custom.js scope!");
        displayMessage("Cannot load address details. Location data structure is missing.", "danger");
        return;
    }

    // --- 0. Load Email ---
    const emailInput = document.getElementById('email');
    if (emailInput) {
        emailInput.value = currentUser.email || 'N/A';
        emailInput.disabled = true; 
    }

    // --- 1. Load Profile Data ---
    const profileRef = ref(database, `hydrolink/users/${currentUserId}/profile`);
    const profileSnapshot = await get(profileRef);

    if (profileSnapshot.exists()) {
        const profileData = profileSnapshot.val();
        document.getElementById('firstName').value = profileData.firstName || '';
        document.getElementById('middleName').value = profileData.middleName || '';
        document.getElementById('lastName').value = profileData.lastName || '';
        document.getElementById('phone').value = profileData.phone || '';
        document.getElementById('gender').value = profileData.gender || '';
        document.getElementById('birthdate').value = profileData.birthdate || '';
        document.getElementById('bio').value = profileData.bio || '';
    }

    // --- 2. Load Address Data ---
    const addressRef = ref(database, `hydrolink/users/${currentUserId}/address`);
    const addressSnapshot = await get(addressRef);

    if (addressSnapshot.exists()) {
        const addressDataFirebase = addressSnapshot.val(); // Renamed to avoid confusion

        // Populate simple fields
        document.getElementById('streetName').value = addressDataFirebase.streetName || '';
        document.getElementById('addressNotes').value = addressDataFirebase.addressNotes || '';

        // --- Populate Dependent Dropdowns in order (MANUAL CASCADE) ---
        const regionSelect = document.getElementById('region');
        const provinceSelect = document.getElementById('province');
        const citySelect = document.getElementById('city');
        const barangaySelect = document.getElementById('barangay');
        const zipcodeSelect = document.getElementById('zipcode');

        // A. Region
        if (addressDataFirebase.region && addressData[addressDataFirebase.region]) {
            regionSelect.value = addressDataFirebase.region;
            
            // Manually populate Province options using the local addressData structure
            const provinces = Object.keys(addressData[addressDataFirebase.region].provinces);
            populateSelect(provinceSelect, provinces);
            provinceSelect.value = addressDataFirebase.province || ''; // Set saved value
        }

        // B. Province
        if (addressDataFirebase.province) {
            // Manually populate City options
            const cities = Object.keys(addressData[addressDataFirebase.region].provinces[addressDataFirebase.province].cities);
            populateSelect(citySelect, cities);
            citySelect.value = addressDataFirebase.city || ''; // Set saved value
        }
        
        // C. City
        if (addressDataFirebase.city) {
            const cityData = addressData[addressDataFirebase.region].provinces[addressDataFirebase.province].cities[addressDataFirebase.city];

            // Manually populate Barangay options
            const barangays = cityData.barangays || [];
            populateSelect(barangaySelect, barangays);
            barangaySelect.value = addressDataFirebase.barangay || ''; // Set saved value
            
            // Manually populate Zipcode
            if (cityData.zipcode) {
                populateSelect(zipcodeSelect, [cityData.zipcode]);
                zipcodeSelect.value = addressDataFirebase.zipcode || ''; // Set saved value
            }
        }
    }
}

// =======================================================
// PART 5: PAGE-SPECIFIC INITIALIZATION LOGIC (Settings Page)
// =======================================================

function initSettingsPage() {
    console.log("Initializing Settings Page logic.");
    
    // Check if the page has the necessary containers before running logic
    if (!document.getElementById('deviceListContainer')) {
        console.warn("Not all Settings DOM elements found. Skipping initialization.");
        return;
    }

    // --- DOM Elements (Local to this init) ---
    // Note: The global ones like currentUser are set in onAuthStateChanged
    const updatePasswordBtn = document.getElementById('updatePasswordBtn');
    
    // Modal DOM Elements (for attaching listeners)
    const configureDeviceModalEl = document.getElementById('configureDeviceModal');
    
    // Address Form Elements
    const regionSelect = document.getElementById('region');
    const provinceSelect = document.getElementById('province');
    const citySelect = document.getElementById('city');
    const barangaySelect = document.getElementById('barangay');
    const zipcodeSelect = document.getElementById('zipcode');

    
    // --- Attach Modal Listeners ---
    if (configureDeviceModalEl) {
        // Event listener for when the modal is hidden
        configureDeviceModalEl.addEventListener('hidden.bs.modal', function () {
            resetModalScanSection();
            // Also unsubscribe the listener if it's still active when modal closes
            if (unsubscribeModalDrumHeight) {
                unsubscribeModalDrumHeight();
                unsubscribeModalDrumHeight = null; // Clear the reference
            }
        });
        
        // Modal button listeners
        const modalScanButton = document.getElementById('modalScanButton');
        const modalRescanButton = document.getElementById('modalRescanButton');
        const modalSaveDrumHeightButton = document.getElementById('modalSaveDrumHeightButton');

        if (modalScanButton) modalScanButton.addEventListener('click', triggerModalDrumMeasurement);
        if (modalRescanButton) modalRescanButton.addEventListener('click', resetModalScanSection);
        if (modalSaveDrumHeightButton) modalSaveDrumHeightButton.addEventListener('click', saveModalDrumHeight);
        
        // Listener for when the modal is shown (to load data/reset UI)
        configureDeviceModalEl.addEventListener('show.bs.modal', async (event) => {
            const button = event.relatedTarget; 
            currentDeviceBeingConfigured = button.getAttribute('data-device-id');
            const deviceName = button.getAttribute('data-device-name');
            document.getElementById('modalDeviceName').textContent = deviceName;

            resetModalScanSection();
            
            // Load existing drum height for the device (Logic from Source 293-298)
            if (currentDeviceBeingConfigured) {
                const deviceSettingsRef = ref(database, `hydrolink/devices/${currentDeviceBeingConfigured}/settings/drumHeightCm`);
                const snapshot = await get(deviceSettingsRef);
                const currentHeight = snapshot.val();
                
                const modalDrumHeightDisplay = document.getElementById('modalDrumHeightDisplay');
                const modalScanSection = document.getElementById('modalScanSection');
                const modalScanResults = document.getElementById('modalScanResults');
                const modalScanIcon = document.getElementById('modalScanIcon');
                const modalScanTitle = document.getElementById('modalScanTitle');
                const modalScanDescription = document.getElementById('modalScanDescription');
                const modalSaveDrumHeightButton = document.getElementById('modalSaveDrumHeightButton');

                if (currentHeight !== null && currentHeight > 0) { 
                    modalDrumHeightDisplay.textContent = `${currentHeight.toFixed(1)} cm`;
                    modalScanSection.classList.add('completed');
                    modalScanResults.classList.remove('hidden');
                    modalScanIcon.className = 'fas fa-check-circle icon-large text-success';
                    modalScanTitle.textContent = 'Current Drum Height';
                    modalScanDescription.textContent = 'This device is already calibrated. You can re-measure if needed.';
                    modalScanButton.style.display = 'none';
                    modalSaveDrumHeightButton.textContent = 'Update Drum Height';
                } else {
                    modalDrumHeightDisplay.textContent = '0.0 cm';
                    modalSaveDrumHeightButton.textContent = 'Save Drum Height';
                    modalScanButton.style.display = 'inline-block';
                }
            }
        });
    }

    // --- Profile Tab Functionality ---
    document.getElementById('profileForm')?.addEventListener('submit', async (e) => {
        e.preventDefault();
        if (!currentUser) {
            displayMessage("You must be logged in to update your profile.", "danger");
            return;
        }

        const firstName = document.getElementById('firstName').value.trim();
        const middleName = document.getElementById('middleName').value.trim();
        const lastName = document.getElementById('lastName').value.trim();
        const phone = document.getElementById('phone').value.trim();
        const gender = document.getElementById('gender').value;
        const birthdate = document.getElementById('birthdate').value;
        const bio = document.getElementById('bio').value.trim();

        const profileData = { firstName, middleName, lastName, phone, gender, birthdate, bio, lastUpdated: Date.now() };
        
        const saveProfileBtn = document.getElementById('saveProfileBtn');
        saveProfileBtn.disabled = true;
        saveProfileBtn.innerHTML = '<span class="spinner-border spinner-border-sm me-2" role="status"></span>Saving...';

        try {
            await set(ref(database, `hydrolink/users/${currentUserId}/profile`), profileData);
            if (firstName || lastName) {
                const fullName = `${firstName} ${middleName ? middleName + ' ' : ''}${lastName}`.trim();
                await currentUser.updateProfile({ displayName: fullName });
            }
            displayMessage("Profile updated successfully!", "success");
        } catch (error) {
            console.error("Error updating profile:", error);
            displayMessage(`Failed to update profile: ${error.message}`, "danger");
        } finally {
            saveProfileBtn.disabled = false;
            saveProfileBtn.innerHTML = '<span class="btn-text">Save Changes</span><span class="btn-icon"><i class="bi bi-check-circle"></i></span>';
        }
    });

    // --- Security Tab Functionality (Change Password) ---
// --- Security Tab Functionality (Change Password) ---
updatePasswordBtn?.addEventListener('click', async (e) => {
    e.preventDefault();
    
    // 🔑 THE FIX: Retrieve the current authenticated user object
    const userToUpdate = getAuth().currentUser; 

    if (!userToUpdate || !userToUpdate.email) {
        displayMessage("You must be logged in with an email to change your password.", "danger");
        return;
    }

    const currentPassword = document.getElementById('currentPassword').value;
    const newPassword = document.getElementById('newPassword').value;
    const confirmNewPassword = document.getElementById('confirmNewPassword').value;

    if (!currentPassword || !newPassword || !confirmNewPassword) {
        displayMessage("Please fill in all password fields.", "danger");
        return;
    }
    if (newPassword !== confirmNewPassword) {
        displayMessage("New passwords do not match.", "danger");
        return;
    }
    if (newPassword.length < 6) {
        displayMessage("New password must be at least 6 characters long.", "danger");
        return;
    }

    updatePasswordBtn.disabled = true;
    updatePasswordBtn.innerHTML = '<span class="spinner-border spinner-border-sm me-2" role="status"></span>Updating...';

    try {
        // Use the locally fetched userToUpdate object for credential checks and updates
        const credential = EmailAuthProvider.credential(userToUpdate.email, currentPassword);
        await reauthenticateWithCredential(userToUpdate, credential);
        
        await updatePassword(userToUpdate, newPassword);
        displayMessage("Password updated successfully!", "success");
        // Clear password fields
        document.getElementById('currentPassword').value = '';
        document.getElementById('newPassword').value = '';
        document.getElementById('confirmNewPassword').value = '';
    } catch (error) {
        console.error("Error updating password:", error);
        let errorMessage = `Failed to update password: ${error.message}`;
        if (error.code === 'auth/wrong-password') {
            errorMessage = "Incorrect current password.";
        } else if (error.code === 'auth/requires-recent-login') {
            errorMessage = "Please log in again to update your password (for security reasons).";
        } else if (error.code === 'auth/weak-password') {
            errorMessage = "The new password is too weak. Please choose a stronger one.";
        }
        displayMessage(errorMessage, "danger");
    } finally {
        updatePasswordBtn.disabled = false;
        updatePasswordBtn.innerHTML = 'Update Password';
    }
});

    // --- Address Tab Functionality ---
    if (regionSelect) regionSelect.addEventListener('change', function() {
        const selectedRegion = this.value;
        clearDependentSelects(provinceSelect, citySelect, barangaySelect, zipcodeSelect);
        
        if (selectedRegion && addressData[selectedRegion]) {
            const provinces = Object.keys(addressData[selectedRegion].provinces);
            populateSelect(provinceSelect, provinces);
        }
    });

    if (provinceSelect) provinceSelect.addEventListener('change', function() {
        const selectedRegion = regionSelect.value;
        const selectedProvince = this.value;
        clearDependentSelects(citySelect, barangaySelect, zipcodeSelect);
        
        if (selectedRegion && selectedProvince && addressData[selectedRegion].provinces[selectedProvince]) {
            const cities = Object.keys(addressData[selectedRegion].provinces[selectedProvince].cities);
            populateSelect(citySelect, cities);
        }
    });

    if (citySelect) citySelect.addEventListener('change', function() {
        const selectedRegion = regionSelect.value;
        const selectedProvince = provinceSelect.value;
        const selectedCity = this.value;
        clearDependentSelects(barangaySelect, zipcodeSelect);
        
        if (selectedRegion && selectedProvince && selectedCity && 
            addressData[selectedRegion].provinces[selectedProvince].cities[selectedCity]) {
            
            const cityData = addressData[selectedRegion].provinces[selectedProvince].cities[selectedCity];
            
            // Populate barangays
            const barangays = cityData.barangays || [];
            populateSelect(barangaySelect, barangays);
            
            // Populate zipcode
            if (cityData.zipcode) {
                populateSelect(zipcodeSelect, [cityData.zipcode]);
            }
        }
    });

    // Address form submission handler
    document.getElementById('addressForm')?.addEventListener('submit', function(e) {
        e.preventDefault();
        
        const formData = {
            region: regionSelect.value,
            province: provinceSelect.value,
            city: citySelect.value,
            barangay: barangaySelect.value,
            streetName: document.getElementById('streetName').value,
            zipcode: zipcodeSelect.value,
            addressNotes: document.getElementById('addressNotes').value
        };
        
        if (!currentUserId) {
            displayMessage("You must be logged in to save your address.", "danger");
            return;
        }

        const addressDataToSave = {
            ...formData, // Spread all form data
            lastUpdated: Date.now()
        };

        const saveAddressBtn = e.target.querySelector('button[type="submit"]');
        saveAddressBtn.disabled = true;
        saveAddressBtn.innerHTML = '<span class="spinner-border spinner-border-sm me-2" role="status"></span>Saving...';
        
        set(ref(database, `hydrolink/users/${currentUserId}/address`), addressDataToSave)
            .then(() => {
                displayMessage("Address saved successfully!", "success");
            })
            .catch((error) => {
                console.error("Error saving address:", error);
                displayMessage(`Failed to save address: ${error.message}`, "danger");
            })
            .finally(() => {
                saveAddressBtn.disabled = false;
                saveAddressBtn.innerHTML = '<span class="btn-text">Save Address</span><span class="btn-icon"><i class="bi bi-check-circle"></i></span>';
            });
    });

}



// =======================================================
// PART 6: MAIN EXECUTION ENTRY POINT
// This determines which page-specific code to run.
// =======================================================

document.addEventListener('DOMContentLoaded', function() {
    // 1. Get DOM references once the page is ready (Ensure system status refs are still here)
    systemStatusText = document.getElementById('systemStatusText');
    systemStatusDot = document.getElementById('systemStatusDot');
    lastUpdatedDisplay = document.getElementById('lastUpdatedDisplay');
    
    // We'll check the <body> tag's ID to know which page we are on.
    const bodyId = document.body.id;

    if (bodyId === 'history-page') {
        initHistoryPage();
    } else if (bodyId === 'home-page') {
        initHomePage();
    } else if (bodyId === 'settings-page') { // ADDED SETTINGS PAGE CHECK
        initSettingsPage();
    } else {
        console.warn("No specific initializer found for this page.");
    }
});