document.addEventListener('DOMContentLoaded', function() {
    // Initialize water level visualization
    updateWaterLevel(68);
    
    // Initialize charts with modern styling
    initWaterUsageChart();
    initConsumptionChart();
    
    // Initialize event listeners
    initEventListeners();
    
    // Simulate real-time updates
    startSimulation();
    
    // Add ripple effect to buttons
    addRippleEffect();
});

// Water Level Visualization
function updateWaterLevel(percentage) {
    const waterLevel = document.getElementById('waterLevel');
    const waterPercentage = document.getElementById('waterPercentage');
    
    if (waterLevel && waterPercentage) {
        waterLevel.style.height = `${percentage}%`;
        waterPercentage.textContent = `${percentage}%`;
        
        // Change color based on level with smoother gradients
        if (percentage < 25) {
            waterLevel.style.background = 'linear-gradient(to bottom, #ff9f7f, #ff7272)';
        } else if (percentage < 50) {
            waterLevel.style.background = 'linear-gradient(to bottom, #ffd166, #ff9f7f)';
        } else {
            waterLevel.style.background = 'linear-gradient(to bottom, #6eb6ff, #4a9ff5)';
        }
    }
}

// Initialize Water Usage Chart with modern styling
function initWaterUsageChart() {
    const ctx = document.getElementById('waterUsageChart');
    
    if (ctx) {
        // Set Chart.js defaults for a more modern look
        Chart.defaults.font.family = "'Segoe UI', Tahoma, Geneva, Verdana, sans-serif";
        Chart.defaults.font.size = 12;
        Chart.defaults.color = '#6c757d';
        
        const waterUsageChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'],
                datasets: [{
                    label: 'Daily Usage (Liters)',
                    data: [12, 19, 8, 15, 12, 18, 22],
                    backgroundColor: 'rgba(74, 159, 245, 0.1)',
                    borderColor: '#4a9ff5',
                    borderWidth: 2,
                    tension: 0.4,
                    fill: true,
                    pointBackgroundColor: '#ffffff',
                    pointBorderColor: '#4a9ff5',
                    pointBorderWidth: 2,
                    pointRadius: 4,
                    pointHoverRadius: 6
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        display: false
                    },
                    tooltip: {
                        mode: 'index',
                        intersect: false,
                        backgroundColor: 'rgba(255, 255, 255, 0.9)',
                        titleColor: '#333',
                        bodyColor: '#666',
                        borderColor: 'rgba(74, 159, 245, 0.3)',
                        borderWidth: 1,
                        cornerRadius: 8,
                        padding: 12,
                        boxPadding: 6,
                        usePointStyle: true,
                        callbacks: {
                            labelPointStyle: function(context) {
                                return {
                                    pointStyle: 'circle',
                                    rotation: 0
                                };
                            }
                        }
                    }
                },
                scales: {
                    y: {
                        beginAtZero: true,
                        grid: {
                            color: 'rgba(0, 0, 0, 0.05)',
                            drawBorder: false
                        },
                        ticks: {
                            padding: 10
                        }
                    },
                    x: {
                        grid: {
                            display: false,
                            drawBorder: false
                        },
                        ticks: {
                            padding: 10
                        }
                    }
                },
                elements: {
                    line: {
                        tension: 0.4
                    }
                }
            }
        });
        
        // Add event listeners for chart period buttons
        const periodButtons = document.querySelectorAll('[data-period]');
        periodButtons.forEach(button => {
            button.addEventListener('click', function() {
                // Remove active class from all buttons
                periodButtons.forEach(btn => btn.classList.remove('active'));
                // Add active class to clicked button
                this.classList.add('active');
                
                // Update chart data based on selected period
                const period = this.getAttribute('data-period');
                updateChartData(waterUsageChart, period);
            });
        });
    }
}

// Update chart data based on selected period
function updateChartData(chart, period) {
    let labels, data;
    
    switch(period) {
        case 'week':
            labels = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
            data = [12, 19, 8, 15, 12, 18, 22];
            break;
        case 'month':
            labels = ['Week 1', 'Week 2', 'Week 3', 'Week 4'];
            data = [65, 72, 58, 80];
            break;
        case 'year':
            labels = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
            data = [220, 190, 210, 180, 230, 250, 270, 240, 220, 200, 230, 240];
            break;
        default:
            labels = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'];
            data = [12, 19, 8, 15, 12, 18, 22];
    }
    
    chart.data.labels = labels;
    chart.data.datasets[0].data = data;
    chart.update();
}

// Initialize Consumption Breakdown Chart with modern styling
function initConsumptionChart() {
    const ctx = document.getElementById('consumptionChart');
    
    if (ctx) {
        const consumptionChart = new Chart(ctx, {
            type: 'doughnut',
            data: {
                labels: ['Data 1', 'Data 2', 'Data 3', 'Data 4'],
                datasets: [{
                    data: [40, 30, 20, 10],
                    backgroundColor: [
                        '#4a9ff5',
                        '#5cb85c',
                        '#ffc107',
                        '#5bc0de'
                    ],
                    borderColor: '#ffffff',
                    borderWidth: 2
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        position: 'bottom',
                        labels: {
                            padding: 20,
                            boxWidth: 12,
                            usePointStyle: true,
                            pointStyle: 'circle'
                        }
                    },
                    tooltip: {
                        backgroundColor: 'rgba(255, 255, 255, 0.9)',
                        titleColor: '#333',
                        bodyColor: '#666',
                        borderColor: 'rgba(0, 0, 0, 0.1)',
                        borderWidth: 1,
                        cornerRadius: 8,
                        padding: 12,
                        callbacks: {
                            label: function(context) {
                                const label = context.label || '';
                                const value = context.raw || 0;
                                return `${label}: ${value}%`;
                            }
                        }
                    }
                },
                cutout: '70%'
            }
        });
    }
}

// Initialize Event Listeners
function initEventListeners() {
    // Customize button
    const customizeBtn = document.getElementById('customizeBtn');
    const customizeModalElement = document.getElementById('customizeModal');
    let customizeModal;

    if (customizeModalElement) {
        customizeModal = new bootstrap.Modal(customizeModalElement);
    }
    
    if (customizeBtn && customizeModal) {
        customizeBtn.addEventListener('click', function() {
            customizeModal.show();
        });
    }
    
    // Auto-refill threshold slider
    const autoRefillThreshold = document.getElementById('autoRefillThreshold');
    const thresholdValue = document.getElementById('thresholdValue');
    
    if (autoRefillThreshold && thresholdValue) {
        autoRefillThreshold.addEventListener('input', function() {
            thresholdValue.textContent = `${this.value}%`;
        });
    }
    
    // Max fill level slider
    const maxFillLevel = document.getElementById('maxFillLevel');
    const maxFillValue = document.getElementById('maxFillValue');
    
    if (maxFillLevel && maxFillValue) {
        maxFillLevel.addEventListener('input', function() {
            maxFillValue.textContent = `${this.value}%`;
        });
    }
    
    // Save customize settings button
    const saveCustomizeSettings = document.getElementById('saveCustomizeSettings');
    
    if (saveCustomizeSettings && customizeModal) {
        saveCustomizeSettings.addEventListener('click', function() {
            // Get values from form
            const threshold = autoRefillThreshold.value;
            const maxFill = maxFillLevel.value;
            
            // Update UI
            document.getElementById('refillThreshold').textContent = `${threshold}%`;
            
            // Show success notification
            showNotification('Settings Updated', 'Your customization settings have been saved successfully.', 'success');
            
            // Close modal
            customizeModal.hide();
        });
    }
    
    // Refill percentage slider
    const refillPercentage = document.getElementById('refillPercentage');
    const refillPercentageValue = document.getElementById('refillPercentageValue');
    
    if (refillPercentage && refillPercentageValue) {
        refillPercentage.addEventListener('input', function() {
            refillPercentageValue.textContent = `${this.value}%`;
        });
    }
    
    // Save refill settings button
    const saveRefillSettings = document.getElementById('saveRefillSettings');
    
    if (saveRefillSettings) {
        saveRefillSettings.addEventListener('click', function() {
            showNotification('Refill Settings Updated', 'Your refill percentage has been updated to ' + refillPercentage.value + '%', 'info');
        });
    }
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
    
    // Remove from DOM after hiding
    notification.addEventListener('hidden.bs.toast', function() {
        document.body.removeChild(toastContainer);
    });
}

// Simulate real-time updates
function startSimulation() {
    // Update water level randomly every 10 seconds
    setInterval(function() {
        const currentLevel = parseInt(document.getElementById('waterPercentage').textContent);
        let newLevel = currentLevel + (Math.random() > 0.7 ? -1 : (Math.random() > 0.7 ? 1 : 0));
        
        // Keep within bounds
        newLevel = Math.max(15, Math.min(95, newLevel));
        
        updateWaterLevel(newLevel);
        
        // Update last updated time
        document.getElementById('lastUpdated').textContent = 'Just now';
        
        // If level drops below threshold, trigger refill
        const threshold = parseInt(document.getElementById('refillThreshold').textContent);
        if (newLevel <= threshold && Math.random() > 0.7) {
            simulateRefill();
        }
    }, 10000);
    
    // Update timestamp every minute
    setInterval(function() {
        const now = new Date();
        const timeString = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
        document.getElementById('lastUpdated').textContent = timeString;
    }, 60000);
}

// Simulate water refill
function simulateRefill() {
    const startLevel = parseInt(document.getElementById('waterPercentage').textContent);
    const targetLevel = parseInt(document.getElementById('refillPercentageValue').textContent);
    
    // Show notification
    showNotification('Refill Started', 'Automatic refill initiated. Current level: ' + startLevel + '%', 'info');
    
    // Simulate gradual refill
    let currentLevel = startLevel;
    const interval = setInterval(function() {
        currentLevel += 1;
        updateWaterLevel(currentLevel);
        
        if (currentLevel >= targetLevel) {
            clearInterval(interval);
            
            // Update refill history
            updateRefillHistory(startLevel, targetLevel);
            
            // Show completion notification
            showNotification('Refill Complete', 'Tank refilled to ' + targetLevel + '% capacity', 'success');
        }
    }, 500);
}

// Update refill history
function updateRefillHistory(startLevel, endLevel) {
    const table = document.getElementById('refillHistoryTable');
    
    if (table) {
        const now = new Date();
        const dateString = now.toLocaleDateString('en-US', { 
            year: 'numeric', 
            month: 'short', 
            day: 'numeric' 
        });
        const timeString = now.toLocaleTimeString([], { 
            hour: '2-digit', 
            minute: '2-digit' 
        });
        
        const volume = Math.round((endLevel - startLevel) * 1);
        const duration = Math.round(volume / 4);
        
        const newRow = table.insertRow(0);
        newRow.innerHTML = `
            <td>${dateString} - ${timeString}</td>
            <td>${startLevel}%</td>
            <td>${endLevel}%</td>
            <td>${volume} L</td>
            <td>${duration} min</td>
            <td><span class="badge bg-success">Completed</span></td>
        `;
        
        // Add fade-in animation to the new row
        newRow.style.opacity = '0';
        newRow.style.transition = 'opacity 0.5s ease';
        setTimeout(() => {
            newRow.style.opacity = '1';
        }, 10);
        
        // Remove last row if more than 5
        if (table.rows.length > 5) {
            table.deleteRow(5);
        }
        
        // Update total water used
        const totalWaterUsed = document.getElementById('totalWaterUsed');
        if (totalWaterUsed) {
            const currentTotal = parseInt(totalWaterUsed.textContent);
            totalWaterUsed.textContent = currentTotal + volume;
            
            // Add animation to the updated value
            totalWaterUsed.classList.add('highlight-update');
            setTimeout(() => {
                totalWaterUsed.classList.remove('highlight-update');
            }, 1500);
        }
        
        // Update refill count
        const refillCount = document.getElementById('refillCount');
        if (refillCount) {
            const currentCount = parseInt(refillCount.textContent);
            refillCount.textContent = currentCount + 1;
            
            // Add animation to the updated value
            refillCount.classList.add('highlight-update');
            setTimeout(() => {
                refillCount.classList.remove('highlight-update');
            }, 1500);
        }
        
        // Update last refill date
        const lastRefillDate = document.getElementById('lastRefillDate');
        if (lastRefillDate) {
            lastRefillDate.textContent = dateString.split(',')[0];
            
            // Add animation to the updated value
            lastRefillDate.classList.add('highlight-update');
            setTimeout(() => {
                lastRefillDate.classList.remove('highlight-update');
            }, 1500);
        }
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