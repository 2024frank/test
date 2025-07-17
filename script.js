// Wait for DOM to be fully loaded
document.addEventListener('DOMContentLoaded', function() {
    const checkboxes = document.querySelectorAll('.checklist-item input[type="checkbox"]');
    const progressFill = document.getElementById('progressFill');
    const progressText = document.getElementById('progressText');
    
    // Function to update progress
    function updateProgress() {
        const totalTasks = checkboxes.length;
        const completedTasks = document.querySelectorAll('.checklist-item input[type="checkbox"]:checked').length;
        
        const percentage = (completedTasks / totalTasks) * 100;
        
        progressFill.style.width = percentage + '%';
        progressText.textContent = `${completedTasks} of ${totalTasks} completed`;
        
        // Add some visual feedback when all tasks are completed
        if (completedTasks === totalTasks) {
            progressText.style.color = '#4CAF50';
            progressText.textContent = '🎉 All tasks completed!';
        } else {
            progressText.style.color = '#1976D2';
        }
    }
    
    // Add event listeners to all checkboxes
    checkboxes.forEach(checkbox => {
        checkbox.addEventListener('change', updateProgress);
    });
    
    // Initialize progress on page load
    updateProgress();
});

// Function to redirect to appliance controller
function redirectToAppliance() {
    // Add a small delay for the button animation
    setTimeout(() => {
        window.location.href = 'http://10.17.192.136/';
    }, 200);
} 