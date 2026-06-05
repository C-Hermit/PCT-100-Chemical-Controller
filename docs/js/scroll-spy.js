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

    function highlightCurrent() {
        var scrollY = window.scrollY;
        var viewportBottom = scrollY + window.innerHeight;
        var pageBottom = document.body.scrollHeight;
        var activeId = null;

        // 到底了 → 高亮最后一个
        if (viewportBottom >= pageBottom - 2 && offsets.length > 0) {
            activeId = offsets[offsets.length - 1].id;
        } else {
            // 从前向后遍历，取最后一个部分可见的章节
            // "部分可见" = 章节底部 > scrollY（未完全滚出顶部）
            for (var i = 0; i < offsets.length; i++) {
                if (offsets[i].bottom > scrollY + navHeight) {
                    activeId = offsets[i].id;
                }
            }
        }

        // 兜底
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

    window.addEventListener('scroll', highlightCurrent, { passive: true });
    window.addEventListener('resize', function () {
        updateOffsets();
        highlightCurrent();
    });

    // 点击目录链接后延迟重算（等浏览器完成锚点滚动）
    for (var k = 0; k < navLinks.length; k++) {
        navLinks[k].addEventListener('click', function () {
            setTimeout(highlightCurrent, 100);
        });
    }

    highlightCurrent();
});
