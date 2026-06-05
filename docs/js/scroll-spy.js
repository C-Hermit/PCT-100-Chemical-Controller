// Scroll spy: 滚动时自动高亮当前目录项
document.addEventListener('DOMContentLoaded', function () {
    var sections = document.querySelectorAll('section[id]');
    var navLinks = document.querySelectorAll('.sidebar-nav a');
    if (!sections.length || !navLinks.length) return;

    var offsets = [];

    function updateOffsets() {
        offsets = [];
        for (var i = 0; i < sections.length; i++) {
            offsets.push({
                id: sections[i].id,
                top: sections[i].offsetTop
            });
        }
    }

    function doHighlight() {
        var viewportBottom = window.scrollY + window.innerHeight;
        var activeId = null;

        // 从后往前找：最后一个顶部仍在可视区内的章节
        for (var i = offsets.length - 1; i >= 0; i--) {
            if (offsets[i].top < viewportBottom) {
                activeId = offsets[i].id;
                break;
            }
        }

        if (!activeId && offsets.length > 0) {
            activeId = offsets[0].id;
        }

        if (activeId) {
            for (var j = 0; j < navLinks.length; j++) {
                var link = navLinks[j];
                if (link.getAttribute('href') === '#' + activeId) {
                    link.classList.add('active');
                } else {
                    link.classList.remove('active');
                }
            }
        }
    }

    updateOffsets();

    window.addEventListener('scroll', doHighlight, { passive: true });
    window.addEventListener('resize', function () {
        updateOffsets();
        doHighlight();
    });

    doHighlight();
});
