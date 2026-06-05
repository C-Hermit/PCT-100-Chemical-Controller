// Scroll spy: 滚动时自动高亮当前目录项
document.addEventListener('DOMContentLoaded', function () {
    var sections = document.querySelectorAll('section[id]');
    var navLinks = document.querySelectorAll('.sidebar-nav a');
    if (!sections.length || !navLinks.length) return;

    var navHeight = 80;
    var offsets = [];

    function updateOffsets() {
        offsets = [];
        for (var i = 0; i < sections.length; i++) {
            offsets.push({
                id: sections[i].id,
                top: sections[i].offsetTop,
                bottom: sections[i].offsetTop + sections[i].offsetHeight
            });
        }
    }

    function doHighlight() {
        var threshold = window.scrollY + navHeight + 10;
        var activeId = null;

        // 正向查找：第一个尚未完全滚出导航栏补偿区的章节
        for (var i = 0; i < offsets.length; i++) {
            if (offsets[i].bottom > threshold) {
                activeId = offsets[i].id;
                break;
            }
        }

        // 全部滚完 → 高亮最后一个
        if (!activeId && offsets.length > 0) {
            activeId = offsets[offsets.length - 1].id;
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
