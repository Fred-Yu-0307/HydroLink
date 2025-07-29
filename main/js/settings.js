document.addEventListener('DOMContentLoaded', function() {
    // Initialize tab functionality with animations
    initTabs();
    
    // Initialize form submissions with animations
    initFormSubmissions();
    
    // Initialize address form with cascading dropdowns
    initAddressForm();
    
    // Initialize password strength meter
    initPasswordStrength();
    
    // Add ripple effect to buttons
    addRippleEffect();
    
    // Initialize form field animations
    initFormFieldAnimations();
});

// Initialize tab functionality with smooth animations
function initTabs() {
    const tabLinks = document.querySelectorAll('.list-group-item[data-bs-toggle="list"]');
    
    tabLinks.forEach(tabLink => {
        tabLink.addEventListener('click', function(e) {
            // Add active class to clicked tab
            tabLinks.forEach(link => link.classList.remove('active'));
            this.classList.add('active');
            
            // Get target tab pane
            const target = document.querySelector(this.getAttribute('href'));
            
            // Hide all tab panes with animation
            document.querySelectorAll('.tab-pane').forEach(pane => {
                if (pane.classList.contains('show')) {
                    pane.style.opacity = '0';
                    pane.style.transform = 'translateY(20px)';
                    
                    setTimeout(() => {
                        pane.classList.remove('show');
                        pane.classList.remove('active');
                    }, 300);
                }
            });
            
            // Show target tab pane with animation
            setTimeout(() => {
                target.classList.add('active');
                target.classList.add('show');
                
                setTimeout(() => {
                    target.style.opacity = '1';
                    target.style.transform = 'translateY(0)';
                }, 50);
            }, 300);
            
            // Update URL hash
            history.pushState(null, null, this.getAttribute('href'));
            
            e.preventDefault();
        });
    });
    
    // Check for hash in URL and activate corresponding tab
    if (window.location.hash) {
        const activeTab = document.querySelector(`.list-group-item[href="${window.location.hash}"]`);
        if (activeTab) {
            activeTab.click();
        }
    }
}

// Initialize form submissions with animations
function initFormSubmissions() {
    const forms = document.querySelectorAll('form');
    
    forms.forEach(form => {
        form.addEventListener('submit', function(e) {
            e.preventDefault();
            
            // Get save button
            const saveBtn = this.querySelector('.save-btn');
            
            if (saveBtn) {
                // Add saving class for animation
                saveBtn.classList.add('saving');
                saveBtn.disabled = true;
                
                // Simulate form submission
                setTimeout(() => {
                    saveBtn.classList.remove('saving');
                    saveBtn.classList.add('saved');
                    
                    // Show success notification
                    showNotification('Settings Saved', 'Your changes have been saved successfully.', 'success');
                    
                    // Reset button state after delay
                    setTimeout(() => {
                        saveBtn.classList.remove('saved');
                        saveBtn.disabled = false;
                    }, 2000);
                }, 1500);
            }
        });
    });
}

// Initialize address form with cascading dropdowns
function initAddressForm() {
    const regionSelect = document.getElementById('region');
    const provinceSelect = document.getElementById('province');
    const citySelect = document.getElementById('city');
    const barangaySelect = document.getElementById('barangay');
    const zipcodeSelect = document.getElementById('zipcode');
    
    if (regionSelect) {
        // When region changes, update provinces
        regionSelect.addEventListener('change', function() {
            // Enable province dropdown
            provinceSelect.disabled = false;
            
            // Clear and disable dependent dropdowns
            clearSelect(provinceSelect, 'Select Province');
            clearSelect(citySelect, 'Select City/Municipality');
            clearSelect(barangaySelect, 'Select Barangay');
            clearSelect(zipcodeSelect, 'Select Zipcode');
            
            citySelect.disabled = true;
            barangaySelect.disabled = true;
            zipcodeSelect.disabled = true;
            
            // Get selected region
            const selectedRegion = this.value;
            
            // Simulate loading provinces for the selected region
            simulateLoading(provinceSelect);
            
            setTimeout(() => {
                // Add provinces based on selected region
                if (selectedRegion === 'ncr') {
                    addOption(provinceSelect, 'metro_manila', 'Metro Manila');
                } else if (selectedRegion === 'region4a') {
                    addOption(provinceSelect, 'cavite', 'Cavite');
                    addOption(provinceSelect, 'laguna', 'Laguna');
                    addOption(provinceSelect, 'batangas', 'Batangas');
                    addOption(provinceSelect, 'rizal', 'Rizal');
                    addOption(provinceSelect, 'quezon', 'Quezon');
                } else {
                    // Add dummy provinces for other regions
                    addOption(provinceSelect, 'province1', 'Province 1');
                    addOption(provinceSelect, 'province2', 'Province 2');
                    addOption(provinceSelect, 'province3', 'Province 3');
                }
                
                // Add animation to the dropdown
                animateDropdown(provinceSelect);
            }, 800);
        });
        
        // When province changes, update cities
        provinceSelect.addEventListener('change', function() {
            // Enable city dropdown
            citySelect.disabled = false;
            
            // Clear and disable dependent dropdowns
            clearSelect(citySelect, 'Select City/Municipality');
            clearSelect(barangaySelect, 'Select Barangay');
            clearSelect(zipcodeSelect, 'Select Zipcode');
            
            barangaySelect.disabled = true;
            zipcodeSelect.disabled = true;
            
            // Get selected province
            const selectedProvince = this.value;
            
            // Simulate loading cities for the selected province
            simulateLoading(citySelect);
            
            setTimeout(() => {
                // Add cities based on selected province
                if (selectedProvince === 'metro_manila') {
                    addOption(citySelect, 'manila', 'Manila');
                    addOption(citySelect, 'quezon_city', 'Quezon City');
                    addOption(citySelect, 'makati', 'Makati');
                    addOption(citySelect, 'pasig', 'Pasig');
                    addOption(citySelect, 'taguig', 'Taguig');
                } else if (selectedProvince === 'cavite') {
                    addOption(citySelect, 'bacoor', 'Bacoor');
                    addOption(citySelect, 'imus', 'Imus');
                    addOption(citySelect, 'dasmarinas', 'Dasmariñas');
                    addOption(citySelect, 'cavite_city', 'Cavite City');
                } else {
                    // Add dummy cities for other provinces
                    addOption(citySelect, 'city1', 'City 1');
                    addOption(citySelect, 'city2', 'City 2');
                    addOption(citySelect, 'city3', 'City 3');
                }
                
                // Add animation to the dropdown
                animateDropdown(citySelect);
            }, 800);
        });
        
        // When city changes, update barangays
        citySelect.addEventListener('change', function() {
            // Enable barangay dropdown
            barangaySelect.disabled = false;
            
            // Clear and disable dependent dropdowns
            clearSelect(barangaySelect, 'Select Barangay');
            clearSelect(zipcodeSelect, 'Select Zipcode');
            
            zipcodeSelect.disabled = true;
            
            // Get selected city
            const selectedCity = this.value;
            
            // Simulate loading barangays for the selected city
            simulateLoading(barangaySelect);
            
            setTimeout(() => {
                // Add barangays based on selected city
                if (selectedCity === 'manila') {
                    addOption(barangaySelect, 'barangay1', 'Barangay 1');
                    addOption(barangaySelect, 'barangay2', 'Barangay 2');
                    addOption(barangaySelect, 'barangay3', 'Barangay 3');
                } else if (selectedCity === 'quezon_city') {
                    addOption(barangaySelect, 'barangay4', 'Barangay 4');
                    addOption(barangaySelect, 'barangay5', 'Barangay 5');
                    addOption(barangaySelect, 'barangay6', 'Barangay 6');
                } else {
                    // Add dummy barangays for other cities
                    addOption(barangaySelect, 'barangay7', 'Barangay 7');
                    addOption(barangaySelect, 'barangay8', 'Barangay 8');
                    addOption(barangaySelect, 'barangay9', 'Barangay 9');
                }
                
                // Add animation to the dropdown
                animateDropdown(barangaySelect);
            }, 800);
        });
        
        // When barangay changes, update zipcodes
        barangaySelect.addEventListener('change', function() {
            // Enable zipcode dropdown
            zipcodeSelect.disabled = false;
            
            // Clear zipcode dropdown
            clearSelect(zipcodeSelect, 'Select Zipcode');
            
            // Get selected barangay
            const selectedBarangay = this.value;
            
            // Simulate loading zipcodes for the selected barangay
            simulateLoading(zipcodeSelect);
            
            setTimeout(() => {
                // Add zipcodes based on selected city and barangay
                const selectedCity = citySelect.value;
                
                if (selectedCity === 'manila') {
                    addOption(zipcodeSelect, '1000', '1000');
                    addOption(zipcodeSelect, '1001', '1001');
                    addOption(zipcodeSelect, '1002', '1002');
                } else if (selectedCity === 'quezon_city') {
                    addOption(zipcodeSelect, '1100', '1100');
                    addOption(zipcodeSelect, '1101', '1101');
                    addOption(zipcodeSelect, '1102', '1102');
                } else {
                    // Add dummy zipcodes for other cities
                    addOption(zipcodeSelect, '1200', '1200');
                    addOption(zipcodeSelect, '1300', '1300');
                    addOption(zipcodeSelect, '1400', '1400');
                }
                
                // Add animation to the dropdown
                animateDropdown(zipcodeSelect);
            }, 800);
        });
    }
}

// Helper function to clear select options
function clearSelect(select, placeholderText) {
    select.innerHTML = '';
    const placeholderOption = document.createElement('option');
    placeholderOption.value = '';
    placeholderOption.textContent = placeholderText;
    placeholderOption.disabled = true;
    placeholderOption.selected = true;
    select.appendChild(placeholderOption);
}

// Helper function to add option to select
function addOption(select, value, text) {
    const option = document.createElement('option');
    option.value = value;
    option.textContent = text;
    select.appendChild(option);
}

// Simulate loading state for dropdowns
function simulateLoading(select) {
    // Add loading class
    select.classList.add('loading');
    
    // Add loading option
    clearSelect(select, 'Loading...');
    
    // Disable select during loading
    select.disabled = true;
}

// Animate dropdown when options are loaded
function animateDropdown(select) {
    // Remove loading class
    select.classList.remove('loading');
    
    // Enable select
    select.disabled = false;
    
    // Animate options
    const options = select.querySelectorAll('option');
    options.forEach((option, index) => {
        option.style.opacity = '0';
        option.style.transform = 'translateY(10px)';
        
        setTimeout(() => {
            option.style.transition = 'opacity 0.3s ease, transform 0.3s ease';
            option.style.opacity = '1';
            option.style.transform = 'translateY(0)';
        }, 50 * index);
    });
}

// Initialize password strength meter
function initPasswordStrength() {
    const newPassword = document.getElementById('newPassword');
    const progressBar = document.querySelector('.password-strength .progress-bar');
    const strengthText = document.getElementById('passwordStrength');
    
    if (newPassword && progressBar && strengthText) {
        newPassword.addEventListener('input', function() {
            const password = this.value;
            let strength = 0;
            
            // Calculate password strength
            if (password.length >= 8) strength += 25;
            if (password.match(/[a-z]/) && password.match(/[A-Z]/)) strength += 25;
            if (password.match(/\d/)) strength += 25;
            if (password.match(/[^a-zA-Z\d]/)) strength += 25;
            
            // Update progress bar
            progressBar.style.width = `${strength}%`;
            
            // Update color based on strength
            if (strength < 25) {
                progressBar.className = 'progress-bar bg-danger';
                strengthText.textContent = 'Weak';
            } else if (strength < 50) {
                progressBar.className = 'progress-bar bg-warning';
                strengthText.textContent = 'Fair';
            } else if (strength < 75) {
                progressBar.className = 'progress-bar bg-info';
                strengthText.textContent = 'Good';
            } else {
                progressBar.className = 'progress-bar bg-success';
                strengthText.textContent = 'Strong';
            }
            
            // Add animation to the progress bar
            progressBar.style.transition = 'width 0.5s ease, background-color 0.5s ease';
        });
    }
}

// Add ripple effect to buttons
function addRippleEffect() {
    const buttons = document.querySelectorAll('.btn');
    
    buttons.forEach(button => {
        button.addEventListener('click', function(e) {
            const x = e.clientX - e.target.getBoundingClientRect().left;
            const y = e.clientY - e.target.getBoundingClientRect().top;
            
            const ripple = document.createElement('span');
            ripple.className = 'ripple';
            ripple.style.left = `${x}px`;
            ripple.style.top = `${y}px`;
            
            this.appendChild(ripple);
            
            setTimeout(() => {
                ripple.remove();
            }, 600);
        });
    });
}

// Initialize form field animations
function initFormFieldAnimations() {
    const formControls = document.querySelectorAll('.form-control, .form-select');
    
    formControls.forEach(control => {
        // Add focus animation
        control.addEventListener('focus', function() {
            this.parentElement.classList.add('focused');
        });
        
        // Remove focus animation
        control.addEventListener('blur', function() {
            this.parentElement.classList.remove('focused');
        });
        
        // Add initial class if field has value
        if (control.value) {
            control.parentElement.classList.add('has-value');
        }
        
        // Update has-value class on input
        control.addEventListener('input', function() {
            if (this.value) {
                this.parentElement.classList.add('has-value');
            } else {
                this.parentElement.classList.remove('has-value');
            }
        });
    });
}

// Show notification with modern styling
function showNotification(title, message, type) {
    // Create notification element
    const notification = document.createElement('div');
    notification.className = `toast align-items-center border-0`;
    notification.style.backgroundColor = type === 'success' ? '#f0f9f0' : 
                                         type === 'info' ? '#f0f4fa' : 
                                         type === 'warning' ? '#fff8e6' : 
                                         type === 'danger' ? '#fef0f0' : '#ffffff';
    notification.style.color = '#333';
    notification.style.borderRadius = '12px';
    notification.style.boxShadow = '0 5px 15px rgba(0, 0, 0, 0.08)';
    notification.setAttribute('role', 'alert');
    notification.setAttribute('aria-live', 'assertive');
    notification.setAttribute('aria-atomic', 'true');
    
    const iconClass = type === 'success' ? 'bi-check-circle-fill text-success' : 
                      type === 'info' ? 'bi-info-circle-fill text-primary' : 
                      type === 'warning' ? 'bi-exclamation-triangle-fill text-warning' : 
                      'bi-x-circle-fill text-danger';
    
    notification.innerHTML = `
        <div class="d-flex">
            <div class="toast-body">
                <div class="d-flex align-items-center">
                    <i class="bi ${iconClass} me-2" style="font-size: 1.5rem;"></i>
                    <div>
                        <strong>${title}</strong><br>
                        ${message}
                    </div>
                </div>
            </div>
            <button type="button" class="btn-close me-2 m-auto" data-bs-dismiss="toast" aria-label="Close"></button>
        </div>
    `;
    
    // Add to container
    const toastContainer = document.createElement('div');
    toastContainer.className = 'toast-container position-fixed top-0 end-0 p-3';
    toastContainer.style.zIndex = '1060';
    toastContainer.appendChild(notification);
    document.body.appendChild(toastContainer);
    
    // Initialize and show toast
    const toast = new bootstrap.Toast(notification, {
        autohide: true,
        delay: 5000
    });
    toast.show();
    
    // Add entrance animation
    notification.style.transform = 'translateY(-20px)';
    notification.style.opacity = '0';
    
    setTimeout(() => {
        notification.style.transition = 'transform 0.3s ease, opacity 0.3s ease';
        notification.style.transform = 'translateY(0)';
        notification.style.opacity = '1';
    }, 50);
    
    // Remove from DOM after hiding
    notification.addEventListener('hidden.bs.toast', function() {
        document.body.removeChild(toastContainer);
    });
}
