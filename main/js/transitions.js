document.addEventListener('DOMContentLoaded', () => {
    const overlay = document.getElementById('pageTransition');
    if (!overlay) return;

    // Fade out original loader on page load
    setTimeout(() => {
        overlay.classList.add('loaded');
    }, 150);

    // Attach click listeners to all links for fade out
    const links = document.querySelectorAll('a[href]');
    
    links.forEach(link => {
        link.addEventListener('click', e => {
            const target = link.getAttribute('href');
            
            // Ignore hash links, javascript links, target="_blank", or empty hrefs
            if (!target || target.startsWith('#') || target.startsWith('javascript:') || link.target === '_blank') {
                return;
            }
            
            // Ignore if default prevented or Ctrl/Cmd pressed
            if (e.defaultPrevented || e.ctrlKey || e.metaKey || e.shiftKey) return;
            
            e.preventDefault();
            overlay.classList.remove('loaded'); // Fade back in
            
            setTimeout(() => {
                window.location.href = target;
            }, 500); // Wait for the fade-in animation to complete
        });
    });
});

window.addEventListener('pageshow', (event) => {
    // Re-hide overlay if page is loaded from bfcache (back button)
    if (event.persisted) {
        const overlay = document.getElementById('pageTransition');
        if (overlay) {
            overlay.classList.add('loaded');
        }
    }
});
