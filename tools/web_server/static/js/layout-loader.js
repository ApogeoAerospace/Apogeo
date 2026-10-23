/**
 * Layout loader
 * Loads large static HTML fragments before app startup to keep index.html compact.
 */
(function () {
    window.__APOGEO_LAYOUT_LOADER__ = true;

    async function loadFragment(slotId, path) {
        const slot = document.getElementById(slotId);
        if (!slot) {
            return;
        }

        const response = await fetch(path, { cache: 'no-cache' });
        if (!response.ok) {
            throw new Error(`Failed to load ${path}: HTTP ${response.status}`);
        }

        slot.outerHTML = await response.text();
    }

    async function bootstrapLayout() {
        try {
            await Promise.all([
                loadFragment('sidebar-sprite-slot', 'fragments/sidebar-sprite.html'),
                loadFragment('navigation-tabs-slot', 'fragments/navigation-tabs.html'),
                loadFragment('tabs-core-slot', 'fragments/tabs-core.html'),
                loadFragment('results-tab-slot', 'fragments/results-tab.html'),
                loadFragment('control-panel-slot', 'fragments/control-panel.html'),
                loadFragment('html-templates-slot', 'fragments/html-templates.html')
            ]);

            window.__APOGEO_LAYOUT_READY__ = true;
            document.dispatchEvent(new Event('apogeo:layout-ready'));

            if (typeof window.startApogeoApp === 'function') {
                window.startApogeoApp();
            }
        } catch (error) {
            console.error('Layout loader error:', error);
            window.__APOGEO_LAYOUT_READY__ = true;
            document.dispatchEvent(new Event('apogeo:layout-ready'));

            if (typeof window.startApogeoApp === 'function') {
                window.startApogeoApp();
            }
        }
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', bootstrapLayout, { once: true });
    } else {
        bootstrapLayout();
    }
})();
