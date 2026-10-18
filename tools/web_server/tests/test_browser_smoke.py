"""
Browser smoke tests for the MoLab web panel (real Chromium through Playwright).

They load the actual front-end against a real ``MoLabWebServer`` (scratch project tree +
fake IPC simulator, see ``test_web_api.ApiTestCase``) and verify what a developer would
check by hand with DevTools open:

* the page initializes with NO console errors/warnings, page errors or failed requests;
* the plugin switch changes the BACKEND state, survives a reload, and cannot enable a
  plugin whose library is not built;
* gravity is sent under ``environment.gravity`` and the UI warns when nothing applies it;
* a simulation started from the UI completes and shows the simulator log.

Chart.js is loaded from a CDN by app.html. These tests run offline, so external hosts are
answered with a tiny stub; chart *rendering* is therefore not covered here.

The module is skipped when Playwright or Chromium is not installed:
    pip install playwright && playwright install chromium
"""
import json
import unittest

try:  # pragma: no cover - environment dependent
    from playwright.sync_api import sync_playwright
    HAVE_PLAYWRIGHT = True
except ImportError:  # pragma: no cover
    HAVE_PLAYWRIGHT = False

try:  # run as part of the package (python -m unittest web_server.tests...)
    from .test_web_api import ApiTestCase, lib_filename
except ImportError:  # run with the tests directory as the top level (discover -s / pytest)
    from test_web_api import ApiTestCase, lib_filename  # noqa: E402

STUB_CHART_JS = (
    "window.Chart=function(){this.destroy=function(){};this.resize=function(){};"
    "this.update=function(){}};window.Chart.defaults={};window.Chart.register=function(){};"
)


@unittest.skipUnless(HAVE_PLAYWRIGHT, "playwright is not installed")
class BrowserSmokeTests(ApiTestCase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls._pw = sync_playwright().start()
        try:
            cls._browser = cls._pw.chromium.launch()
        except Exception as exc:  # pragma: no cover - no browser binary
            cls._pw.stop()
            raise unittest.SkipTest(f"Chromium is not available: {exc}")

    @classmethod
    def tearDownClass(cls):
        cls._browser.close()
        cls._pw.stop()
        super().tearDownClass()

    # ---------------------------------------------------------------- helpers
    def open_page(self, block_chart_cdn=False):
        self.problems = []      # console errors/warnings, page errors, failed/4xx/5xx requests
        self.external = []
        page = self._browser.new_page()
        self.addCleanup(page.close)

        page.on("console", lambda m: self.problems.append(f"console.{m.type}: {m.text}")
                if m.type in ("error", "warning") else None)
        page.on("pageerror", lambda e: self.problems.append(f"pageerror: {e}"))
        page.on("requestfailed", lambda r: self.problems.append(f"requestfailed: {r.url} {r.failure}"))
        page.on("response", lambda r: self.problems.append(f"HTTP {r.status}: {r.url}")
                if r.status >= 400 else None)
        page.on("dialog", lambda d: d.accept())

        local = f"http://127.0.0.1:{self.port}"

        def route(route):
            url = route.request.url
            if url.startswith(local):
                return route.continue_()
            self.external.append(url)
            if block_chart_cdn and url.endswith(".js"):
                return route.abort()
            if url.endswith(".js"):
                return route.fulfill(status=200, content_type="application/javascript", body=STUB_CHART_JS)
            return route.fulfill(status=200, content_type="text/css", body="")

        page.route("**/*", route)
        page.goto(f"{local}/app.html")
        page.wait_for_function("() => document.querySelector('.plugin-card') !== null")
        page.wait_for_function("() => !document.querySelector('.plugin-card.busy')")
        return page

    def settle(self, page):
        page.wait_for_function("() => !document.querySelector('.plugin-card.busy')")

    def open_tab(self, page, name):
        page.click(f'.tab[data-tab="{name}"]')

    def switch(self, page, plugin_id):
        return page.locator(f'input[data-action="toggle"][data-plugin-id="{plugin_id}"]')

    def toggle_in_ui(self, page, plugin_id):
        with page.expect_response(f"**/api/plugins/{plugin_id}/toggle"):
            self.switch(page, plugin_id).click()
        self.settle(page)

    def backend_enabled(self, plugin_id):
        return self.plugin(plugin_id)["enabled"]

    # ------------------------------------------------------------------ tests
    def test_page_initializes_without_console_errors(self):
        page = self.open_page()
        page.wait_for_timeout(500)
        self.assertEqual(self.problems, [], "the page must load clean")
        # The only third-party hosts are the Chart.js CDN and the font stylesheet.
        self.assertTrue(all(("jsdelivr" in u) or ("fonts.googleapis" in u) for u in self.external),
                        self.external)

    def test_status_bar_reports_a_ready_system(self):
        page = self.open_page()
        page.wait_for_function("() => document.getElementById('status').textContent.includes('System ready')")
        self.assertEqual(self.problems, [])

    def test_switch_changes_backend_and_survives_reload(self):
        page = self.open_page()
        self.open_tab(page, "plugins")
        self.assertFalse(self.backend_enabled("example_plugin"))
        self.assertFalse(self.switch(page, "example_plugin").is_checked())

        self.toggle_in_ui(page, "example_plugin")
        self.assertTrue(self.backend_enabled("example_plugin"), "backend must follow the switch")
        self.assertTrue(self.switch(page, "example_plugin").is_checked())

        # The panel refreshes /api/environment after every change. Reloading while that request
        # is in flight aborts it (ERR_ABORTED), which is harmless but would be recorded as a
        # problem; wait until the page is quiet so the test checks what it means to check.
        page.wait_for_load_state("networkidle")
        page.reload()
        page.wait_for_function("() => document.querySelector('.plugin-card') !== null")
        self.open_tab(page, "plugins")
        self.assertTrue(self.switch(page, "example_plugin").is_checked(), "state must survive a reload")

        self.toggle_in_ui(page, "example_plugin")
        self.assertFalse(self.backend_enabled("example_plugin"))
        self.assertEqual(self.problems, [])

    def test_unbuilt_plugin_cannot_be_enabled_from_the_ui(self):
        page = self.open_page()
        self.open_tab(page, "plugins")
        self.assertTrue(self.switch(page, "environment").is_disabled())
        self.assertIn("Not built", page.locator('[data-plugin-id="environment"].plugin-card').inner_text())
        self.assertFalse(self.backend_enabled("environment"))

    def test_backend_rejection_is_shown_and_ui_resyncs(self):
        page = self.open_page()
        self.open_tab(page, "plugins")
        # Delete the library behind the UI's back: the UI still thinks it is built.
        (self.tmp / "build" / "bin" / "plugins" / lib_filename("example_plugin")).unlink()
        with page.expect_response("**/api/plugins/example_plugin/toggle") as info:
            self.switch(page, "example_plugin").click()
        self.assertEqual(info.value.status, 409)
        self.settle(page)

        status = page.locator("#status").inner_text()
        self.assertIn("library was not found", status)
        self.assertFalse(self.switch(page, "example_plugin").is_checked(), "UI must show the real state")
        self.assertFalse(self.backend_enabled("example_plugin"))
        # A rejected request is logged on purpose (console.error + the browser's own 409 line).
        # What must NOT happen is a script error or a log that is unrelated to this toggle.
        self.assertEqual([p for p in self.problems if p.startswith("pageerror")], [])
        unrelated = [p for p in self.problems if "toggle" not in p and "409" not in p]
        self.assertEqual(unrelated, [])

    def test_parameter_edit_is_sent_to_backend(self):
        page = self.open_page()
        self.open_tab(page, "plugins")
        self.toggle_in_ui(page, "example_plugin")

        field = page.locator('input[data-action="param"][data-key="drag_coefficient"]')
        with page.expect_response("**/api/plugins/example_plugin/parameters"):
            field.fill("0.75")
            field.blur()
        self.settle(page)
        self.assertEqual(self.plugin("example_plugin")["parameters"]["drag_coefficient"], 0.75)

        box = page.locator('input[data-action="param"][data-key="apply_gravity"]')
        with page.expect_response("**/api/plugins/example_plugin/parameters"):
            box.click()
        self.settle(page)
        self.assertTrue(self.plugin("example_plugin")["parameters"]["apply_gravity"])
        self.assertEqual(self.problems, [])

    def test_reset_button_restores_defaults(self):
        page = self.open_page()
        self.open_tab(page, "plugins")
        self.toggle_in_ui(page, "example_plugin")
        with page.expect_response("**/api/plugins/reset"):
            page.click('[data-action="reset-plugins"]')
        page.wait_for_function("() => !document.querySelector('.plugin-card.busy')")
        self.assertFalse(self.backend_enabled("example_plugin"))
        self.assertFalse(self.switch(page, "example_plugin").is_checked())

    def test_gravity_notice_follows_plugins(self):
        page = self.open_page()
        self.open_tab(page, "physics")
        notice = page.locator("#gravity-notice")
        page.wait_for_function("() => document.getElementById('gravity-notice').textContent.length > 0")
        self.assertIn("nothing applies it", notice.inner_text())
        self.assertIn("not built", notice.inner_text())

        page.uncheck("#enable-gravity")
        page.wait_for_function("() => document.getElementById('gravity-notice').textContent.includes('Gravity is off')")
        page.check("#enable-gravity")

        self.open_tab(page, "plugins")
        self.toggle_in_ui(page, "example_plugin")
        box = page.locator('input[data-action="param"][data-key="apply_gravity"]')
        with page.expect_response("**/api/plugins/example_plugin/parameters"):
            box.click()
        self.settle(page)

        self.open_tab(page, "physics")
        page.wait_for_function(
            "() => document.getElementById('gravity-notice').textContent.includes('applied by example_plugin')")

    def test_gravity_applied_twice_is_warned_in_the_panel(self):
        self.build_environment_library()
        page = self.open_page()
        self.open_tab(page, "plugins")
        # The module explains its limitation on its own card.
        self.assertIn("does not read any parameters",
                      page.locator('[data-plugin-id="environment"].plugin-card .plugin-note').inner_text())

        self.toggle_in_ui(page, "environment")
        self.toggle_in_ui(page, "example_plugin")
        box = page.locator('input[data-action="param"][data-key="apply_gravity"]')
        with page.expect_response("**/api/plugins/example_plugin/parameters"):
            box.click()
        self.settle(page)

        self.open_tab(page, "physics")
        page.wait_for_function(
            "() => document.getElementById('gravity-notice').textContent.includes('more than one plugin')")

        page.uncheck("#enable-gravity")
        page.wait_for_function(
            "() => document.getElementById('gravity-notice').textContent.includes('more than one plugin')")
        self.assertEqual(self.problems, [])

    def test_environment_module_alone_explains_it_ignores_the_vector(self):
        self.build_environment_library()
        page = self.open_page()
        self.open_tab(page, "plugins")
        self.toggle_in_ui(page, "environment")
        self.open_tab(page, "physics")
        page.wait_for_function(
            "() => document.getElementById('gravity-notice').textContent.includes('own gravity model')")
        page.uncheck("#enable-gravity")
        page.wait_for_function(
            "() => document.getElementById('gravity-notice').textContent.includes('cannot be switched off')")

    def test_template_applies_gravity_and_reports_skipped_plugins(self):
        page = self.open_page()
        page.click('.template-card[data-template="suborbital"]')
        # suborbital asks for the Environment module, which is not built in this tree.
        page.wait_for_function(
            "() => document.getElementById('status').textContent.includes('not built and were skipped')")
        self.assertIn("Environment", page.locator("#status").inner_text())
        self.assertEqual(page.input_value("#gravity-magnitude"), "9.81")
        self.assertEqual(page.input_value("#gravity-dir-z"), "-1")
        self.assertFalse(self.backend_enabled("environment"))

    def test_run_from_ui_sends_environment_gravity_and_shows_log(self):
        page = self.open_page()
        self.open_tab(page, "physics")
        page.fill("#gravity-magnitude", "3.7")
        page.fill("#gravity-dir-x", "0")
        page.fill("#gravity-dir-z", "-1")

        self.open_tab(page, "plugins")
        self.toggle_in_ui(page, "example_plugin")

        with page.expect_response("**/api/run") as info:
            page.click("#run-btn")
        payload = json.loads(info.value.request.post_data)
        self.assertNotIn("plugins", payload, "plugin state must come from the backend registry")
        self.assertEqual(payload["environment"]["gravity"],
                         {"enabled": True, "magnitude": 3.7, "direction": [0.0, 0.0, -1.0]})
        self.assertNotIn("enable_gravity", payload["physics"])

        page.wait_for_function("() => document.getElementById('job-log').textContent.includes('plugins:')",
                               timeout=15000)
        # The log sits inside a collapsed <details>; inner_text() would be empty for hidden text.
        log = page.locator("#job-log").text_content()
        self.assertIn("example_plugin", log)
        self.assertIn("structures", log)

        self.assertAlmostEqual(self.last_run()["initial_state"]["gravity"]["z"], -3.7, places=5)

        # After the run the panel loads the results tab (2 s later) without errors.
        page.wait_for_function("() => document.getElementById('results-tab').classList.contains('active')",
                               timeout=15000)
        page.wait_for_timeout(300)
        self.assertEqual(self.problems, [])

    def test_missing_chart_library_does_not_break_the_panel(self):
        page = self.open_page(block_chart_cdn=True)
        self.open_tab(page, "plugins")
        self.toggle_in_ui(page, "example_plugin")
        self.assertTrue(self.backend_enabled("example_plugin"))
        script_errors = [p for p in self.problems if p.startswith("pageerror")]
        self.assertEqual(script_errors, [], "an unreachable CDN must not cause script errors")


if __name__ == "__main__":
    unittest.main()
