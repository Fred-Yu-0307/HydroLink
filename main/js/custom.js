import { initializeApp } from "https://www.gstatic.com/firebasejs/12.0.0/firebase-app.js";
import { getDatabase, ref, onValue, get, set, remove } from "https://www.gstatic.com/firebasejs/12.0.0/firebase-database.js"; // ADDED get, set, remove
import {
    getAuth,
    onAuthStateChanged,
    signOut,  // < This was missing!
    updatePassword,
    EmailAuthProvider,
    reauthenticateWithCredential
} from "https://www.gstatic.com/firebasejs/12.0.0/firebase-auth.js";

//  Firebase Config 
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

//  Global State Variables 
let currentUserId = null;
let DEVICE_ID = null;
let lastUpdateCheckInterval = null;

let currentUser = null;
let currentDeviceBeingConfigured = null;
let isScanning = false;
let unsubscribeModalDrumHeight = null;

//  DOM References (Initialized after DOMContentLoaded) 
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

            if (timeDifferenceSeconds < 60) {
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
        currentUser = user; // Set the global user object for all handlers
        console.log("History/Dashboard/Settings: User signed in...", "UID:", currentUserId);

        const authNavLink = document.querySelector('.navbar-nav .nav-link[href="auth.html"]');
        if (authNavLink) {
            authNavLink.innerHTML = '<i class="bi bi-box-arrow-right"></i>';
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

            //  NEW: History Page Logic (after DEVICE_ID is set) 
            if (document.body.id === 'history-page') {
                fetchAndRenderHistory();
            }
            // 

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

//  Handle Logout 
function handleLogout(e) {
    e.preventDefault();
    signOut(auth).then(() => {
        console.log("Signed out successfully.");
    }).catch((error) => {
        console.error("Error signing out:", error);
        showCustomAlert("Error signing out. Please try again.", 'error'); // Replaced alert
    });
}

//GLOBAL UTILITY/EXPORT FUNCTIONS (Must be exposed)

// Calculates the date cutoff based on the selected range (in days).
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

//Sets the value of a select element and optionally triggers a 'change' event.
function setSelectValueAndTriggerChange(selectElement, value) {
    if (selectElement && value) {
        selectElement.value = value;
        // Manually dispatch the 'change' event to trigger dependent logic
        selectElement.dispatchEvent(new Event('change'));
    }
}

//Filters the refill history table based on the selected criteria. This is called by 'onchange' or button clicks.
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

//Exports the currently visible table data (filtered rows) to a CSV file.
function exportToSpreadsheet(event) {
    event.preventDefault();
    const table = document.querySelector('.table-hover');
    //CSV generation and download logic

    //CSV logic
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
    // END of CSV logic
}

// Exports the currently visible table data to a PDF file in Landscape orientation.
function exportToPDF(event) {
    event.preventDefault();

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

//  Expose functions to the global window object 
window.filterTable = filterTable;
window.exportToSpreadsheet = exportToSpreadsheet;
window.exportToPDF = exportToPDF;



// PAGE INITIALIZATION (with pagination)


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


// PAGINATION HANDLERS

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

// PAGINATION NAVIGATION
function updatePagination() {
    const totalPages = Math.ceil(allRecords.length / recordsPerPage);
    const pagination = document.getElementById('pagination');
    if (!pagination) return;
    pagination.innerHTML = '';

    const maxVisible = 2; // show 2 pages before and after current

    //PREVIOUS BUTTON
    pagination.innerHTML += `
        <li class="page-item ${currentPage === 1 ? 'disabled' : ''}">
            <a class="page-link" href="javascript:void(0)" onclick="changePage(${currentPage - 1}); return false;">
                <i class="fas fa-chevron-left"></i>
            </a>
        </li>`;

    // PAGE NUMBERS
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

    // NEXT BUTTON
    pagination.innerHTML += `
        <li class="page-item ${currentPage === totalPages ? 'disabled' : ''}">
            <a class="page-link" href="javascript:void(0)" onclick="changePage(${currentPage + 1}); return false;">
                <i class="fas fa-chevron-right"></i>
            </a>
        </li>`;

    //PAGE INFO LABEL
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

// DELETE REFILL HISTORY RECORD (with Bootstrap Modals)

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

function initHistoryPage() {
    console.log("Initializing History Page.");

    // Attach Event Listeners
    const dateFilter = document.getElementById('dateRangeFilter');
    const typeFilter = document.getElementById('typeFilter');
    const statusFilter = document.getElementById('statusFilter');

    if (dateFilter) dateFilter.addEventListener('change', filterTable);
    if (typeFilter) typeFilter.addEventListener('change', filterTable);
    if (statusFilter) statusFilter.addEventListener('change', filterTable);
}

// UTILITY FUNCTIONS (Settings Page)

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

function initSettingsPage() {
    console.log("Initializing Page logic.");

    // Check if the page has the necessary containers before running logic
    if (!document.getElementById('deviceListContainer')) {
        console.warn("Not all Settings DOM elements found. Skipping initialization.");
        return;
    }

    //DOM Elements (Local to this init) 
    // The global ones like currentUser are set in onAuthStateChanged
    const updatePasswordBtn = document.getElementById('updatePasswordBtn');

    // Modal DOM Elements (for attaching listeners)
    const configureDeviceModalEl = document.getElementById('configureDeviceModal');

    // Address Form Elements
    const regionSelect = document.getElementById('region');
    const provinceSelect = document.getElementById('province');
    const citySelect = document.getElementById('city');
    const barangaySelect = document.getElementById('barangay');
    const zipcodeSelect = document.getElementById('zipcode');


    // Attach Modal Listeners 
    if (configureDeviceModalEl) {
        // Event listener for when the modal is hidden
        configureDeviceModalEl.addEventListener('hidden.bs.modal', function () {
            resetModalScanSection();
            // Also unsubscribe the listener if it's still active when modal closes
            if (unsubscribeModalDrumHeight) {
                unsubscribeModalDrumHeight();
                unsubscribeModalDrumHeight = null;
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

            // Load existing drum height for the device
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
    } }


//MAIN EXECUTION ENTRY POINT
// This determines which page-specific code to run.
document.addEventListener('DOMContentLoaded', function () {
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