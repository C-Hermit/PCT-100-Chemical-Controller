// Scroll spy: 滚动时自动高亮当前目录项
document.addEventListener('DOMContentLoaded', function () {
    const sections = document.querySelectorAll('section[id]');
    const navLinks = document.querySelectorAll('.sidebar-nav a');

    if (!sections.length || !navLinks.length) return;

    const observer = new IntersectionObserver(function (entries) {
        let activeId = null;

        for (const entry of entries) {
            if (entry.isIntersecting) {
                activeId = entry.target.id;
            }
        }

        if (activeId) {
            for (const link of navLinks) {
                link.classList.remove('active');
                if (link.getAttribute('href') === '#' + activeId) {
                    link.classList.add('active');
                }
            }
        }
    }, {
        rootMargin: '-80px 0px -60% 0px'
    });

    for (const section of sections) {
        observer.observe(section);
    }
});
