// Scroll spy: 滚动时自动高亮当前目录项
document.addEventListener('DOMContentLoaded', function () {
    var sections = document.querySelectorAll('section[id]');
    var navLinks = document.querySelectorAll('.sidebar-nav a');
    if (!sections.length || !navLinks.length) return;

    var navHeight = 80;
    var sectionOffsets = [];

    function updateOffsets() {
        sectionOffsets = [];
        for (var i = 0; i < sections.length; i++) {
            sectionOffsets.push({
                id: sections[i].id,
                top: sections[i].offsetTop
            });
        }
    }

    function setActive(id) {
        for (var j = 0; j < navLinks.length; j++) {
            var link = navLinks[j];
            if (link.getAttribute('href') === '#' + id) {
                link.classList.add('active');
            } else {
                link.classList.remove('active');
            }
        }
    }

    function highlightCurrent() {
        var scrollY = window.scrollY + navHeight + 10;
        var activeId = null;

        for (var i = sectionOffsets.length - 1; i >= 0; i--) {
            if (sectionOffsets[i].top <= scrollY) {
                activeId = sectionOffsets[i].id;
                break;
            }
        }

        // 兜底：取第一个章节
        if (!activeId && sectionOffsets.length > 0) {
            activeId = sectionOffsets[0].id;
        }

        if (activeId) {
            setActive(activeId);
        }
    }

    // 点击目录时立即高亮，不等滚动事件
    for (var k = 0; k < navLinks.length; k++) {
        navLinks[k].addEventListener('click', function () {
            var id = this.getAttribute('href').substring(1);
            setActive(id);
        });
    }

    updateOffsets();

    window.addEventListener('scroll', highlightCurrent, { passive: true });
    window.addEventListener('resize', function () {
        updateOffsets();
        highlightCurrent();
    });

    highlightCurrent();
});
