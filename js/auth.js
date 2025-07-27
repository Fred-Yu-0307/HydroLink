// Import the functions you need from the SDKs you need
import { initializeApp } from "https://www.gstatic.com/firebasejs/12.0.0/firebase-app.js";
import { getAuth, createUserWithEmailAndPassword, signInWithEmailAndPassword } from "https://www.gstatic.com/firebasejs/12.0.0/firebase-auth.js";

document.addEventListener('DOMContentLoaded', function() {
    // Your web app's Firebase configuration (from your script.js)
    const firebaseConfig = {
        apiKey: "AIzaSyCmFInEL6TMoD-9JwdPy-e9niNGGL5SjHA",
        authDomain: "hydrolink-d3c57.firebaseapp.com",
        projectId: "hydrolink-d3c57",
        storageBucket: "hydrolink-d3c57.firebasestorage.app",
        messagingSenderId: "770257381412",
        appId: "1:770257381412:web:a894f35854cb82274950b1"
    };

    // Initialize Firebase
    const app = initializeApp(firebaseConfig);
    const auth = getAuth(app);

    // Toggle between login and signup forms
    const toggleButtons = document.querySelectorAll('.auth-toggle-btn');
    const toggleIndicator = document.querySelector('.auth-toggle-indicator');
    const loginForm = document.getElementById('login-form');
    const signupForm = document.getElementById('signup-form');
    const formSwitchLinks = document.querySelectorAll('.form-switch');
    
    // Set initial form heights to ensure smooth transitions
    function setFormContainerHeight() {
        const activeForm = document.querySelector('.auth-form.active');
        const formContainer = document.querySelector('.auth-form-container');
        if (activeForm && formContainer) {
            formContainer.style.minHeight = `${activeForm.offsetHeight}px`;
        }
    }
    
    // Call on page load
    setFormContainerHeight();
    
    // Toggle form when clicking the toggle buttons
    toggleButtons.forEach(button => {
        button.addEventListener('click', function() {
            const formType = this.getAttribute('data-form');
            switchForm(formType);
        });
    });
    
    // Toggle form when clicking the form switch links
    formSwitchLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            const formType = this.getAttribute('data-target');
            switchForm(formType);
        });
    });
    
    // Function to switch between forms with improved animation
    function switchForm(formType) {
        // Update toggle buttons
        toggleButtons.forEach(btn => {
            if (btn.getAttribute('data-form') === formType) {
                btn.classList.add('active');
            } else {
                btn.classList.remove('active');
            }
        });
        
        // Move toggle indicator with smoother animation
        if (formType === 'signup') {
            toggleIndicator.style.left = '50%';
        } else {
            toggleIndicator.style.left = '0';
        }
        
        // Hide current form with fade out animation
        const currentForm = document.querySelector('.auth-form.active');
        currentForm.style.transition = 'opacity 0.3s ease, transform 0.3s cubic-bezier(0.68, -0.55, 0.27, 1.55)';
        currentForm.style.opacity = '0';
        currentForm.style.transform = 'translateY(20px)';
        
        // Show new form with fade in animation after a short delay
        setTimeout(() => {
            currentForm.classList.remove('active');
            
            if (formType === 'signup') {
                signupForm.classList.add('active');
                signupForm.style.opacity = '1';
                signupForm.style.transform = 'translateY(0)';
            } else {
                loginForm.classList.add('active');
                loginForm.style.opacity = '1';
                loginForm.style.transform = 'translateY(0)';
            }
            
            // Update container height for the new form
            setFormContainerHeight();
        }, 300);
    }
    
    // Password visibility toggle
    const passwordToggles = document.querySelectorAll('.password-toggle');
    
    passwordToggles.forEach(toggle => {
        toggle.addEventListener('click', function() {
            const passwordField = this.parentElement.querySelector('input');
            const icon = this.querySelector('i');
            
            if (passwordField.type === 'password') {
                passwordField.type = 'text';
                icon.classList.remove('bi-eye');
                icon.classList.add('bi-eye-slash');
            } else {
                passwordField.type = 'password';
                icon.classList.remove('bi-eye-slash');
                icon.classList.add('bi-eye');
            }
        });
    });
    
    // Form validation and Firebase integration
    const loginFormElement = loginForm.querySelector('form');
    const signupFormElement = signupForm.querySelector('form');
    
    loginFormElement.addEventListener('submit', async function(e) {
        e.preventDefault();
        
        // Get form values
        const email = document.getElementById('loginEmail').value;
        const password = document.getElementById('loginPassword').value;
        
        // Simple validation
        let isValid = true;
        
        if (!validateEmail(email)) {
            showError('loginEmail', 'Please enter a valid email address');
            isValid = false;
        } else {
            clearError('loginEmail');
        }
        
        if (password.length < 6) {
            showError('loginPassword', 'Password must be at least 6 characters');
            isValid = false;
        } else {
            clearError('loginPassword');
        }
        
        if (isValid) {
            try {
                // Firebase Login
                await signInWithEmailAndPassword(auth, email, password);
                simulateSuccessfulAuth('login');
            } catch (error) {
                const errorCode = error.code;
                const errorMessage = error.message;
                console.error("Login Error:", errorCode, errorMessage);
                // Display error to the user
                if (errorCode === 'auth/user-not-found' || errorCode === 'auth/wrong-password') {
                    showError('loginEmail', 'Invalid email or password.');
                    showError('loginPassword', 'Invalid email or password.');
                } else {
                    // Generic error for other issues
                    showError('loginEmail', 'Login failed. Please try again.');
                }
            }
        }
    });
    
    signupFormElement.addEventListener('submit', async function(e) {
        e.preventDefault();
        
        // Get form values
        // const firstName = document.getElementById('firstName').value; // Commented out as per auth.html
        // const middleName = document.getElementById('middleName').value; // Commented out as per auth.html
        // const lastName = document.getElementById('lastName').value; // Commented out as per auth.html
        const email = document.getElementById('email').value;
        const password = document.getElementById('signupPassword').value;
        const confirmPassword = document.getElementById('confirmPassword').value;
        const termsCheck = document.getElementById('termsCheck').checked;
        
        // Simple validation
        let isValid = true;
        
        // if (firstName.trim() === '') { // Commented out as per auth.html
        //     showError('firstName', 'Please enter your first name');
        //     isValid = false;
        // } else {
        //     clearError('firstName');
        // }
        
        // if (lastName.trim() === '') { // Commented out as per auth.html
        //     showError('lastName', 'Please enter your last name');
        //     isValid = false;
        // } else {
        //     clearError('lastName');
        // }
        
        if (!validateEmail(email)) {
            showError('email', 'Please enter a valid email address');
            isValid = false;
        } else {
            clearError('email');
        }
        
        if (password.length < 6) {
            showError('signupPassword', 'Password must be at least 6 characters');
            isValid = false;
        } else {
            clearError('signupPassword');
        }
        
        if (password !== confirmPassword) {
            showError('confirmPassword', 'Passwords do not match');
            isValid = false;
        } else {
            clearError('confirmPassword');
        }
        
        if (!termsCheck) {
            showError('termsCheck', 'You must agree to the Terms of Service');
            isValid = false;
        } else {
            clearError('termsCheck');
        }
        
        if (isValid) {
            try {
                // Firebase Account Creation
                await createUserWithEmailAndPassword(auth, email, password);
                simulateSuccessfulAuth('signup');
            } catch (error) {
                const errorCode = error.code;
                const errorMessage = error.message;
                console.error("Signup Error:", errorCode, errorMessage);
                // Display error to the user
                if (errorCode === 'auth/email-already-in-use') {
                    showError('email', 'This email address is already in use.');
                } else if (errorCode === 'auth/invalid-email') {
                    showError('email', 'The email address is not valid.');
                } else if (errorCode === 'auth/weak-password') {
                    showError('signupPassword', 'The password is too weak.');
                } else {
                    // Generic error for other issues
                    showError('email', 'Account creation failed. Please try again.');
                }
            }
        }
    });
    
    // Helper functions
    function validateEmail(email) {
        const re = /^(([^<>()\[\]\\.,;:\s@"]+(\.[^<>()\[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-1]{3}\.[0-9]{1,3}\])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/;
        return re.test(String(email).toLowerCase());
    }
    
    function showError(inputId, message) {
        const input = document.getElementById(inputId);
        // Ensure input exists before trying to add classes/elements
        if (!input) {
            console.error(`Error: Input element with ID '${inputId}' not found.`);
            return;
        }

        input.classList.add('is-invalid');
        input.classList.remove('is-valid');
        
        // Remove existing error message if any
        let existingFeedback = input.nextElementSibling;
        if (existingFeedback && existingFeedback.classList.contains('invalid-feedback')) {
            existingFeedback.textContent = message;
        } else {
            const errorDiv = document.createElement('div');
            errorDiv.className = 'invalid-feedback';
            errorDiv.textContent = message;
            
            // Insert after input for regular inputs, or after label for checkboxes
            if (input.type === 'checkbox') {
                input.parentElement.appendChild(errorDiv);
            } else {
                input.parentNode.insertBefore(errorDiv, input.nextSibling);
            }
        }
    }
    
    function clearError(inputId) {
        const input = document.getElementById(inputId);
        // Ensure input exists before trying to add classes/elements
        if (!input) {
            return;
        }

        input.classList.remove('is-invalid');
        input.classList.add('is-valid');
        
        // Remove error message if exists
        let errorDiv = input.nextElementSibling;
        if (errorDiv && errorDiv.classList.contains('invalid-feedback')) {
            errorDiv.remove();
        }
    }
    
    function simulateSuccessfulAuth(type) {
        // Hide the form
        const form = type === 'login' ? loginForm : signupForm;
        form.innerHTML = ''; // Clear form content
        
        // Show success message with improved animation
        const successMessage = document.createElement('div');
        successMessage.className = 'text-center';
        successMessage.innerHTML = `
            <svg class="checkmark" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 52 52">
                <circle class="checkmark__circle" cx="26" cy="26" r="25" fill="none"/>
                <path class="checkmark__check" fill="none" d="M14.1 27.2l7.1 7.2 16.7-16.8"/>
            </svg>
            <h3 class="auth-title">${type === 'login' ? 'Login Successful!' : 'Account Created!'}</h3>
            <p class="auth-subtitle">${type === 'login' ? 'Redirecting you to your dashboard...' : 'Your account has been created successfully.'}</p>
            <div class="mt-4">
                <div class="spinner-border text-primary" role="status">
                    <span class="visually-hidden">Loading...</span>
                </div>
            </div>
        `;
        
        form.appendChild(successMessage);
        
        // Update container height for the success message
        setFormContainerHeight();
        
        // Simulate redirect after 2 seconds
        setTimeout(() => {
            window.location.href = 'landing-page-no-connection.html';
        }, 2000);
    }
    
    // Add ripple effect to buttons
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
    
    // Handle window resize to adjust form container height
    window.addEventListener('resize', setFormContainerHeight);
});
