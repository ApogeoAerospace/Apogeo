"""
Integration tests for the MoLab web API.

A real ``MoLabWebServer`` is started on a free port against a scratch project tree
(``project_root`` override). The simulator is replaced by ``fake_simulator.py``,
which speaks the real ``--ipc stdio`` protocol, so these tests cover the whole
panel -> HTTP -> job manager -> IPC path without needing the C++ build.

Run with either runner:
    python -m unittest discover -s tools/web_server/tests -v
    pytest tools/web_server/tests
"""
import json
import os
import platform
import re
import shutil
import socket
import sys
import tempfile
import threading
import time
import unittest
import urllib.error
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
TOOLS_DIR = HERE.parent.parent
REAL_ROOT = TOOLS_DIR.parent
sys.path.insert(0, str(TOOLS_DIR))

from web_server import ServerConfig, MoLabWebServer  # noqa: E402
from web_server.utils import (  # noqa: E402
    normalize_config_for_simulator,
    resolve_gravity,
    validate_config,
)


def lib_filename(name: str) -> str:
    """File name of a plugin library on the current platform (as CMake names it)."""
    system = platform.system()
    if system == "Windows":
        return f"{name}.dll"
    if system == "Darwin":
        return f"lib{name}.dylib"
    return f"lib{name}.so"


def simulator_filename() -> str:
    return "simulator.exe" if platform.system() == "Windows" else "simulator"


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


class ApiTestCase(unittest.TestCase):
    """Starts a server on a scratch project tree for every test."""

    LIBS = ("structures", "example_plugin")

    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="molab_web_test_"))
        root = self.tmp

        defaults = root / "data" / "defaults"
        defaults.mkdir(parents=True)
        for name in ("default_config.json", "default_state.json", "web_config.json"):
            shutil.copy(REAL_ROOT / "data" / "defaults" / name, defaults / name)

        bin_dir = root / "build" / "bin"
        (bin_dir / "plugins").mkdir(parents=True)
        # Placeholder binary: the server only checks that it exists and is executable
        # (/api/status). The process that really runs is `simulator_command` below, so no
        # shebang or .exe is needed and the fixture behaves the same on every OS.
        simulator = bin_dir / simulator_filename()
        simulator.write_bytes(b"")
        simulator.chmod(0o755)
        for lib in self.LIBS:
            (bin_dir / "plugins" / lib_filename(lib)).write_bytes(b"")

        self.port = free_port()
        self.config = ServerConfig(
            port=self.port, auto_open_browser=False, project_root=root,
            simulator_command=[sys.executable, str(HERE / "fake_simulator.py")],
        )
        self.server = MoLabWebServer(self.config)
        self.thread = threading.Thread(target=self.server.start, daemon=True)
        self.thread.start()
        self._wait_for_port()

        self.addCleanup(self._shutdown)

    def _shutdown(self):
        self.server.stop()
        self.thread.join(timeout=5)
        shutil.rmtree(self.tmp, ignore_errors=True)

    def _wait_for_port(self):
        deadline = time.time() + 5
        while time.time() < deadline:
            try:
                with socket.create_connection(("127.0.0.1", self.port), timeout=0.2):
                    return
            except OSError:
                time.sleep(0.05)
        self.fail("server did not start")

    # ---------------------------------------------------------------- helpers
    def request(self, method, path, body=None, headers=None):
        data = json.dumps(body).encode("utf-8") if body is not None else None
        req = urllib.request.Request(
            f"http://127.0.0.1:{self.port}{path}", data=data, method=method,
            headers={"Content-Type": "application/json", **(headers or {})},
        )
        try:
            with urllib.request.urlopen(req, timeout=10) as resp:
                raw = resp.read()
                return resp.status, dict(resp.headers), raw
        except urllib.error.HTTPError as err:
            return err.code, dict(err.headers), err.read()

    def api(self, method, path, body=None, headers=None):
        status, resp_headers, raw = self.request(method, path, body, headers)
        parsed = json.loads(raw) if raw else None
        return status, parsed, resp_headers

    def plugin(self, plugin_id):
        _, plugins, _ = self.api("GET", "/api/plugins")
        return next(p for p in plugins if p["id"] == plugin_id)

    def build_environment_library(self):
        """Pretend the Environment module has been built."""
        (self.tmp / "build" / "bin" / "plugins" / lib_filename("environment")).write_bytes(b"")

    def run_config(self, **overrides):
        cfg = {
            "simulation": {"time_step": 0.1, "duration": 10.0},
            "physics": {"integrator_type": "runge_kutta_4"},
            "initial_state": {"position": [0, 0, 0], "velocity": [10, 0, 10], "mass": 1000},
        }
        cfg.update(overrides)
        return cfg

    def wait_job(self, job_id, terminal=("completed", "failed", "cancelled"), timeout=20):
        deadline = time.time() + timeout
        status = None
        while time.time() < deadline:
            _, status, _ = self.api("GET", f"/api/simulation/status/{job_id}")
            if status["status"] in terminal:
                return status
            time.sleep(0.05)
        self.fail(f"job did not finish, last status: {status}")

    def last_run(self):
        return json.loads((self.tmp / "output" / "fake_run.json").read_text(encoding="utf-8"))


# ======================================================================== plugins
class PluginToggleTests(ApiTestCase):
    def test_list_reports_library_state(self):
        structures = self.plugin("structures")
        self.assertTrue(structures["library_found"])
        self.assertTrue(structures["library_path"].endswith(lib_filename("structures")))
        # The old resolver returned the *source directory* plugins/<name>; a library
        # path must always be a file.
        self.assertTrue(Path(structures["library_path"]).is_file())
        self.assertFalse(self.plugin("environment")["library_found"])

    def test_toggle_flips_and_is_reflected_by_backend(self):
        self.assertFalse(self.plugin("example_plugin")["enabled"])

        status, body, _ = self.api("POST", "/api/plugins/example_plugin/toggle")
        self.assertEqual(status, 200)
        self.assertTrue(body["success"])
        self.assertTrue(body["plugin"]["enabled"])
        self.assertTrue(self.plugin("example_plugin")["enabled"])

        status, body, _ = self.api("POST", "/api/plugins/example_plugin/toggle")
        self.assertFalse(body["plugin"]["enabled"])
        self.assertFalse(self.plugin("example_plugin")["enabled"])

    def test_explicit_enabled_is_idempotent(self):
        for _ in range(2):
            status, body, _ = self.api("POST", "/api/plugins/example_plugin/toggle", {"enabled": True})
            self.assertEqual(status, 200)
            self.assertTrue(body["plugin"]["enabled"])

    def test_toggle_survives_server_restart(self):
        self.api("POST", "/api/plugins/example_plugin/toggle", {"enabled": True})
        self.assertTrue((self.tmp / "data" / "config" / "web_plugin_state.json").is_file())

        from web_server.plugin_registry import PluginRegistry
        fresh = PluginRegistry(self.config)
        self.assertTrue(next(p for p in fresh.list_plugins() if p["id"] == "example_plugin")["enabled"])

    def test_defaults_file_is_never_rewritten(self):
        defaults = self.tmp / "data" / "defaults" / "default_config.json"
        before = defaults.read_bytes()
        self.api("POST", "/api/plugins/example_plugin/toggle", {"enabled": True})
        self.assertEqual(defaults.read_bytes(), before)

    def test_enabling_unbuilt_plugin_is_rejected_and_state_unchanged(self):
        status, body, _ = self.api("POST", "/api/plugins/environment/toggle", {"enabled": True})
        self.assertEqual(status, 409)
        self.assertEqual(body["code"], "library_not_found")
        self.assertFalse(body["success"])
        self.assertFalse(self.plugin("environment")["enabled"])

    def test_disabling_unbuilt_plugin_is_allowed(self):
        status, body, _ = self.api("POST", "/api/plugins/environment/toggle", {"enabled": False})
        self.assertEqual(status, 200)

    def test_unknown_plugin_is_404(self):
        status, body, _ = self.api("POST", "/api/plugins/nope/toggle", {"enabled": True})
        self.assertEqual(status, 404)
        self.assertEqual(body["code"], "unknown_plugin")

    def test_enabled_must_be_boolean(self):
        status, body, _ = self.api("POST", "/api/plugins/example_plugin/toggle", {"enabled": "yes"})
        self.assertEqual(status, 400)

    def test_parameters_update_and_validation(self):
        status, body, _ = self.api("POST", "/api/plugins/example_plugin/parameters",
                                   {"parameters": {"drag_coefficient": 0.5}})
        self.assertEqual(status, 200)
        self.assertEqual(self.plugin("example_plugin")["parameters"]["drag_coefficient"], 0.5)

        status, body, _ = self.api("POST", "/api/plugins/example_plugin/parameters",
                                   {"parameters": {"does_not_exist": 1}})
        self.assertEqual(status, 400)
        self.assertEqual(body["code"], "unknown_parameter")

        status, body, _ = self.api("POST", "/api/plugins/example_plugin/parameters",
                                   {"parameters": {"apply_gravity": "yes"}})
        self.assertEqual(status, 400)

        status, body, _ = self.api("POST", "/api/plugins/example_plugin/parameters",
                                   {"parameters": {"drag_coefficient": True}})
        self.assertEqual(status, 400, "booleans must not pass as numbers")

    def test_every_example_plugin_parameter_is_editable_from_the_api(self):
        for key, value in self.plugin("example_plugin")["default_parameters"].items():
            status, body, _ = self.api("POST", "/api/plugins/example_plugin/parameters",
                                       {"parameters": {key: value}})
            self.assertEqual(status, 200, f"{key}: {body}")
        self.assertIn("reference_area", self.plugin("example_plugin")["parameters"])

    def test_preset_skips_unbuilt_plugins(self):
        status, body, _ = self.api("POST", "/api/plugins/preset", {
            "enabled": ["structures", "environment"],
            "overrides": {"example_plugin": {"drag_coefficient": 9}},
        })
        self.assertEqual(status, 200)
        by_id = {p["id"]: p for p in body["plugins"]}
        self.assertTrue(by_id["structures"]["enabled"])
        self.assertFalse(by_id["environment"]["enabled"])
        self.assertTrue(by_id["environment"]["skipped"])
        self.assertFalse(by_id["example_plugin"]["enabled"])
        # Overrides only apply to plugins that are part of the preset.
        self.assertNotEqual(by_id["example_plugin"]["parameters"]["drag_coefficient"], 9)

    def test_preset_with_unknown_plugin_changes_nothing(self):
        before = self.plugin("structures")["enabled"]
        status, body, _ = self.api("POST", "/api/plugins/preset", {"enabled": ["ghost"]})
        self.assertEqual(status, 404)
        self.assertEqual(self.plugin("structures")["enabled"], before)

    def test_reset_restores_defaults(self):
        self.api("POST", "/api/plugins/structures/toggle", {"enabled": False})
        self.assertFalse(self.plugin("structures")["enabled"])
        status, body, _ = self.api("POST", "/api/plugins/reset")
        self.assertEqual(status, 200)
        self.assertTrue(self.plugin("structures")["enabled"])


# ======================================================================== runs
class SimulationRunTests(ApiTestCase):
    def start(self, config):
        status, body, _ = self.api("POST", "/api/run", config)
        self.assertEqual(status, 200, body)
        self.assertTrue(body["success"])
        return body

    def test_run_uses_registry_state_not_client_flags(self):
        self.api("POST", "/api/plugins/example_plugin/toggle", {"enabled": True})
        body = self.start(self.run_config())  # note: no "plugins" key
        job = self.wait_job(body["job_id"])
        self.assertEqual(job["status"], "completed", job)
        self.assertEqual(sorted(job["plugins"]), ["example_plugin", "structures"])

        sent = {p["name"]: p for p in self.last_run()["config"]["plugins"]}
        self.assertTrue(sent["example_plugin"]["enabled"])
        self.assertTrue(sent["structures"]["enabled"])
        self.assertFalse(sent["aerodynamics"]["enabled"])
        # library paths reach the simulator as real files, not source directories
        self.assertTrue(Path(sent["structures"]["library_path"]).is_file())

    def test_simulator_runs_from_project_root(self):
        job = self.wait_job(self.start(self.run_config())["job_id"])
        self.assertEqual(job["status"], "completed", job)
        self.assertEqual(Path(self.last_run()["cwd"]).resolve(), self.tmp.resolve())

    def test_environment_gravity_reaches_initial_state(self):
        cfg = self.run_config(environment={"gravity": {"enabled": True, "magnitude": 3.7,
                                                       "direction": [0, -1, 0]}})
        job = self.wait_job(self.start(cfg)["job_id"])
        self.assertEqual(job["status"], "completed", job)
        gravity = self.last_run()["initial_state"]["gravity"]
        self.assertAlmostEqual(gravity["x"], 0.0)
        self.assertAlmostEqual(gravity["y"], -3.7)
        self.assertAlmostEqual(gravity["z"], 0.0)
        self.assertEqual(job["gravity"]["source"], "environment")

    def test_legacy_physics_gravity_still_supported(self):
        cfg = self.run_config(physics={"enable_gravity": True, "gravity_magnitude": 5.0,
                                       "integrator_type": "euler"})
        job = self.wait_job(self.start(cfg)["job_id"])
        self.assertEqual(self.last_run()["initial_state"]["gravity"]["z"], -5.0)
        self.assertEqual(job["gravity"]["source"], "legacy_physics")

    def test_disabled_gravity_seeds_zero_vector(self):
        cfg = self.run_config(environment={"gravity": {"enabled": False}})
        self.wait_job(self.start(cfg)["job_id"])
        self.assertEqual(self.last_run()["initial_state"]["gravity"], {"x": 0.0, "y": 0.0, "z": 0.0})

    def test_environment_block_wins_over_legacy_physics(self):
        cfg = self.run_config(physics={"enable_gravity": False, "gravity_magnitude": 1.0},
                              environment={"gravity": {"enabled": True, "magnitude": 2.0}})
        self.wait_job(self.start(cfg)["job_id"])
        self.assertEqual(self.last_run()["initial_state"]["gravity"]["z"], -2.0)

    def test_default_state_fallback_uses_environment_gravity(self):
        cfg = self.run_config(environment={"gravity": {"magnitude": 4.0}})
        del cfg["initial_state"]
        self.wait_job(self.start(cfg)["job_id"])
        self.assertEqual(self.last_run()["initial_state"]["gravity"]["z"], -4.0)

    def test_warning_when_no_plugin_applies_gravity(self):
        body = self.start(self.run_config())
        self.assertEqual(len(body["warnings"]), 1)
        self.assertIn("no enabled plugin applies it", body["warnings"][0])
        job = self.wait_job(body["job_id"])
        self.assertEqual(job["warnings"], body["warnings"])

    def test_no_warning_when_example_plugin_applies_gravity(self):
        self.api("POST", "/api/plugins/example_plugin/parameters", {"parameters": {"apply_gravity": True}})
        self.api("POST", "/api/plugins/example_plugin/toggle", {"enabled": True})
        body = self.start(self.run_config())
        self.assertEqual(body["warnings"], [])

    def test_no_warning_when_gravity_is_off(self):
        body = self.start(self.run_config(environment={"gravity": {"enabled": False}}))
        self.assertEqual(body["warnings"], [])

    def test_plugin_logs_are_captured_from_ipc_events(self):
        job = self.wait_job(self.start(self.run_config())["job_id"])
        messages = [entry["message"] for entry in job["logs"]]
        self.assertTrue(any(m.startswith("plugins:") for m in messages), messages)
        warning = next(e for e in job["logs"] if e["message"] == "run finished")
        self.assertEqual(warning["level"], "WARNING")
        self.assertEqual(warning["component"], "FakePlugin")

    def test_legacy_explicit_plugin_list_is_validated(self):
        status, body, _ = self.api("POST", "/api/run", self.run_config(
            plugins=[{"name": "ghost", "enabled": True}]))
        self.assertEqual(status, 400)
        self.assertEqual(body["code"], "unknown_plugin")

        status, body, _ = self.api("POST", "/api/run", self.run_config(
            plugins=[{"name": "environment", "enabled": True}]))
        self.assertEqual(status, 409)
        self.assertEqual(body["code"], "library_not_found")

    def test_legacy_explicit_plugin_list_overrides_registry_for_one_run(self):
        body = self.start(self.run_config(plugins=[{"name": "example_plugin", "enabled": True}]))
        job = self.wait_job(body["job_id"])
        self.assertEqual(job["plugins"], ["example_plugin"])
        self.assertFalse(self.plugin("example_plugin")["enabled"], "registry must not change")

    def test_failed_initialize_is_reported(self):
        os.environ["FAKE_SIM_FAIL_INIT"] = "1"
        self.addCleanup(os.environ.pop, "FAKE_SIM_FAIL_INIT", None)
        job = self.wait_job(self.start(self.run_config())["job_id"])
        self.assertEqual(job["status"], "failed")
        self.assertIn("Failed to initialize", job["error"])

    def test_environment_module_applies_its_own_gravity_model(self):
        self.build_environment_library()
        self.api("POST", "/api/plugins/environment/toggle", {"enabled": True})
        _, env, _ = self.api("GET", "/api/environment")
        self.assertEqual(env["gravity_providers"], [{"id": "environment", "mode": "own_model"}])
        self.assertFalse(env["gravity_conflict"])
        body = self.start(self.run_config())
        self.assertEqual(body["warnings"], [], "defaults match the module's nominal gravity")

    def test_gravity_applied_twice_is_flagged(self):
        self.build_environment_library()
        self.api("POST", "/api/plugins/environment/toggle", {"enabled": True})
        self.api("POST", "/api/plugins/example_plugin/parameters", {"parameters": {"apply_gravity": True}})
        self.api("POST", "/api/plugins/example_plugin/toggle", {"enabled": True})

        _, env, _ = self.api("GET", "/api/environment")
        self.assertTrue(env["gravity_conflict"])
        self.assertEqual(sorted(p["id"] for p in env["gravity_providers"]), ["environment", "example_plugin"])

        body = self.start(self.run_config())
        self.assertEqual(len(body["warnings"]), 1)
        self.assertIn("more than one plugin", body["warnings"][0])

    def test_gravity_off_cannot_switch_off_the_environment_module(self):
        self.build_environment_library()
        self.api("POST", "/api/plugins/environment/toggle", {"enabled": True})
        body = self.start(self.run_config(environment={"gravity": {"enabled": False}}))
        self.assertEqual(len(body["warnings"]), 1)
        self.assertIn("cannot be switched off", body["warnings"][0])

    def test_custom_gravity_vector_is_flagged_as_ignored_by_the_environment_module(self):
        self.build_environment_library()
        self.api("POST", "/api/plugins/environment/toggle", {"enabled": True})
        body = self.start(self.run_config(environment={"gravity": {"magnitude": 3.7}}))
        self.assertEqual(len(body["warnings"]), 1)
        self.assertIn("ignores them", body["warnings"][0])

    def test_plugin_parameters_reach_the_simulator_untouched(self):
        # Plugin parameters are plugin-owned: the web server must not add gravity keys.
        self.build_environment_library()
        self.api("POST", "/api/plugins/environment/toggle", {"enabled": True})
        job = self.wait_job(self.start(self.run_config(
            environment={"gravity": {"magnitude": 3.7}}))["job_id"])
        self.assertEqual(job["status"], "completed", job)
        sent = {p["name"]: p for p in self.last_run()["config"]["plugins"]}
        # default_config.json declares no parameters for it; none may be added.
        self.assertEqual(sent["environment"]["parameters"], {})

    def test_environment_module_note_is_exposed(self):
        self.assertIn("does not read any parameters", self.plugin("environment")["notes"])
        self.assertEqual(self.plugin("structures")["notes"], "")

    def test_failure_during_run_is_reported_instead_of_hanging(self):
        # The real simulator sends no ack when run_full fails mid-run.
        os.environ["FAKE_SIM_FAIL_RUN"] = "1"
        self.addCleanup(os.environ.pop, "FAKE_SIM_FAIL_RUN", None)
        job = self.wait_job(self.start(self.run_config())["job_id"], timeout=10)
        self.assertEqual(job["status"], "failed", job)
        self.assertIn("Integration diverged", job["error"])

    def test_unresponsive_simulator_times_out(self):
        os.environ["FAKE_SIM_HANG"] = "1"
        self.addCleanup(os.environ.pop, "FAKE_SIM_HANG", None)
        self.config.max_simulation_timeout = 2
        job = self.wait_job(self.start(self.run_config())["job_id"], timeout=15)
        self.assertEqual(job["status"], "failed", job)
        self.assertIn("did not answer", job["error"])

    def test_cancel_while_waiting_for_the_simulator(self):
        os.environ["FAKE_SIM_HANG"] = "1"
        self.addCleanup(os.environ.pop, "FAKE_SIM_HANG", None)
        job_id = self.start(self.run_config())["job_id"]
        self.wait_job(job_id, terminal=("running",))
        self.api("POST", f"/api/simulation/cancel/{job_id}")
        job = self.wait_job(job_id, timeout=10)
        self.assertEqual(job["status"], "cancelled", job)

    def test_cancel_reports_cancelled_not_failed(self):
        os.environ["FAKE_SIM_TICK_DELAY"] = "0.05"
        self.addCleanup(os.environ.pop, "FAKE_SIM_TICK_DELAY", None)
        job_id = self.start(self.run_config())["job_id"]

        self.wait_job(job_id, terminal=("running",))
        status, body, _ = self.api("POST", f"/api/simulation/cancel/{job_id}")
        self.assertEqual(status, 200)

        job = self.wait_job(job_id, timeout=10)
        self.assertEqual(job["status"], "cancelled", job)

    def test_cancel_unknown_or_finished_job_is_an_error(self):
        status, body, _ = self.api("POST", "/api/simulation/cancel/" + "0" * 8)
        self.assertEqual(status, 400)

        job_id = self.start(self.run_config())["job_id"]
        self.wait_job(job_id)
        status, body, _ = self.api("POST", f"/api/simulation/cancel/{job_id}")
        self.assertEqual(status, 400)

    def test_temp_files_are_cleaned_up(self):
        self.wait_job(self.start(self.run_config())["job_id"])
        time.sleep(0.2)
        leftovers = list((self.tmp / "data" / "config" / "tmp").glob("temp_web_*"))
        self.assertEqual(leftovers, [])
        self.assertEqual(list((self.tmp / "data" / "defaults").glob("temp_web_*")), [])

    def test_results_are_listed(self):
        self.wait_job(self.start(self.run_config())["job_id"])
        status, results, _ = self.api("GET", "/api/results")
        self.assertIn("fake_run.csv", [r["filename"] for r in results])

    def test_validation_errors(self):
        cases = [
            self.run_config(environment={"gravity": {"magnitude": -1}}),
            self.run_config(environment={"gravity": {"magnitude": 1e9}}),
            self.run_config(environment={"gravity": {"direction": [0, 0, 0]}}),
            self.run_config(environment={"gravity": {"direction": [1, 2]}}),
            self.run_config(environment={"gravity": {"enabled": "yes"}}),
            self.run_config(environment="space"),
            self.run_config(physics={"gravity_magnitude": "abc"}),
            self.run_config(physics={"enable_gravity": 1}),
            self.run_config(plugins="all"),
        ]
        for cfg in cases:
            status, body, _ = self.api("POST", "/api/run", cfg)
            self.assertEqual(status, 400, cfg)
            self.assertFalse(body["success"])


# ======================================================================== misc API
class StatusEnvironmentCorsTests(ApiTestCase):
    def test_status_reports_executable_simulator(self):
        status, body, _ = self.api("GET", "/api/status")
        self.assertTrue(body["simulator_built"])
        self.assertTrue(body["simulator_executable"])

    def test_environment_endpoint_tracks_gravity_consumers(self):
        _, env, _ = self.api("GET", "/api/environment")
        self.assertFalse(env["applies_gravity"])
        self.assertEqual(env["gravity_consumers"], [])
        self.assertFalse(env["environment_plugin"]["available"])
        self.assertEqual(env["gravity"]["direction"], [0.0, 0.0, -1.0])

        self.api("POST", "/api/plugins/example_plugin/parameters", {"parameters": {"apply_gravity": True}})
        self.api("POST", "/api/plugins/example_plugin/toggle", {"enabled": True})
        _, env, _ = self.api("GET", "/api/environment")
        self.assertTrue(env["applies_gravity"])
        self.assertEqual(env["gravity_consumers"], ["example_plugin"])
        self.assertEqual(env["gravity_providers"], [{"id": "example_plugin", "mode": "state_vector"}])
        self.assertFalse(env["gravity_conflict"])

    def test_cors_allows_only_listed_origins(self):
        ok = f"http://localhost:{self.port}"
        _, _, headers = self.api("GET", "/api/status", headers={"Origin": ok})
        self.assertEqual(headers.get("Access-Control-Allow-Origin"), ok)

        _, _, headers = self.api("GET", "/api/status", headers={"Origin": "http://evil.example"})
        self.assertNotIn("Access-Control-Allow-Origin", headers)

        _, _, headers = self.api("GET", "/api/status")
        self.assertNotIn("Access-Control-Allow-Origin", headers)

    def test_cors_preflight(self):
        ok = f"http://127.0.0.1:{self.port}"
        status, headers, _ = self.request("OPTIONS", "/api/run", headers={
            "Origin": ok, "Access-Control-Request-Method": "POST"})
        self.assertEqual(status, 204)
        self.assertEqual(headers.get("Access-Control-Allow-Origin"), ok)
        self.assertIn("POST", headers.get("Access-Control-Allow-Methods", ""))

        status, _, _ = self.request("OPTIONS", "/api/run", headers={"Origin": "http://evil.example"})
        self.assertEqual(status, 403)

    def test_oversized_body_is_rejected(self):
        big = {"simulation": {"duration": 1}, "padding": "x" * (1024 * 1024 + 10)}
        status, body, _ = self.api("POST", "/api/run", big)
        self.assertEqual(status, 413)
        self.assertEqual(body["code"], "payload_too_large")


class StaticAssetTests(ApiTestCase):
    def test_every_local_script_and_stylesheet_of_the_app_is_served(self):
        status, _, raw = self.request("GET", "/app.html")
        self.assertEqual(status, 200)
        html = raw.decode("utf-8")
        refs = re.findall(r'(?:src|href)="((?!https?:|#)[^"]+)"', html)
        self.assertTrue(refs)
        for ref in refs:
            status, _, _ = self.request("GET", "/" + ref.split("?")[0])
            self.assertEqual(status, 200, f"{ref} is referenced by app.html but not served")

    def test_every_layout_fragment_is_served(self):
        loader = (self.config.static_dir / "js" / "layout-loader.js").read_text(encoding="utf-8")
        for path in re.findall(r"'(fragments/[^']+)'", loader):
            status, _, _ = self.request("GET", "/" + path)
            self.assertEqual(status, 200, path)

    def test_favicon_does_not_404(self):
        status, _, _ = self.request("GET", "/favicon.ico")
        self.assertIn(status, (200, 204))

    def test_path_traversal_is_blocked(self):
        status, _, _ = self.request("GET", "/../config.py")
        self.assertIn(status, (403, 404))
        status, _, _ = self.request("GET", "/%2e%2e/config.py")
        self.assertIn(status, (403, 404))


# ======================================================================== server robustness
class ClientDisconnectTests(unittest.TestCase):
    """A browser that closes the connection mid-request is not a server error."""

    def _handle(self, error):
        import contextlib
        import io
        from web_server.server import _ThreadingServer

        server = _ThreadingServer(("127.0.0.1", 0), lambda *a, **k: None)
        self.addCleanup(server.server_close)
        captured = io.StringIO()
        with contextlib.redirect_stderr(captured):
            try:
                raise error
            except Exception:
                server.handle_error(None, ("127.0.0.1", 50000))
        return captured.getvalue()

    def test_connection_drops_are_not_printed_as_tracebacks(self):
        for error in (BrokenPipeError(32, "Broken pipe"),
                      ConnectionResetError(10054, "reset by peer"),
                      ConnectionAbortedError(10053, "aborted")):
            with self.subTest(type(error).__name__):
                self.assertEqual(self._handle(error), "")

    def test_real_errors_are_still_reported(self):
        self.assertIn("ValueError", self._handle(ValueError("boom")))


# ======================================================================== platform layouts
class PlatformLayoutTests(unittest.TestCase):
    """The simulator and the plugin libraries are named differently on each OS.

    Each case builds the layout of one platform/toolchain in a scratch tree and asks the
    server code to find it, with ``platform.system`` patched. The names are the ones CMake
    produces, so a naming mistake is caught on any machine, not only on the affected OS.
    """

    CASES = [
        # (id, platform, build dir (relative), simulator dir, plugin dir, library file name)
        ("linux/ninja", "Linux", "build", "bin", "bin/plugins", "libstructures.so"),
        ("macos/ninja", "Darwin", "build", "bin", "bin/plugins", "libstructures.dylib"),
        ("windows/ninja", "Windows", "build", "bin", "bin/plugins", "structures.dll"),
        ("windows/mingw", "Windows", "build", "bin", "bin/plugins", "libstructures.dll"),
        ("windows/msvc-multiconfig", "Windows", "build", "bin/Release", "bin/Release/plugins", "structures.dll"),
        ("windows/vs-preset", "Windows", "out/build", "x64-Debug/bin", "x64-Debug/bin/plugins", "structures.dll"),
    ]

    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="molab_layout_test_"))
        self.addCleanup(shutil.rmtree, self.tmp, True)

    def touch(self, relative: str) -> Path:
        path = self.tmp / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"")
        return path

    def test_simulator_and_library_are_found_for_each_layout(self):
        from unittest import mock
        from web_server.config import discover_build_dir
        from web_server.plugin_registry import PluginRegistry
        from web_server.utils import get_simulator_path

        for case_id, system, build, sim_dir, _plugin_dir, library in self.CASES:
            with self.subTest(case_id):
                root = self.tmp / case_id.replace("/", "_")
                root.mkdir()
                exe = "simulator.exe" if system == "Windows" else "simulator"
                sim = root / build / sim_dir / exe
                lib = root / build / _plugin_dir / library
                for path in (sim, lib):
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(b"")

                with mock.patch("platform.system", return_value=system):
                    build_dir = discover_build_dir(root)
                    self.assertEqual(build_dir, root / build, "build directory discovery")
                    self.assertEqual(get_simulator_path(build_dir), sim)

                    config = ServerConfig(port=8082, auto_open_browser=False,
                                          project_root=root, build_dir=build_dir)
                    found = PluginRegistry(config).resolve_library("plugins/structures")
                    self.assertEqual(found, lib)

    def test_a_library_of_another_platform_is_not_taken_for_this_one(self):
        from unittest import mock
        from web_server.plugin_registry import PluginRegistry

        root = self.tmp / "mixed"
        root.mkdir()
        self.touch("mixed/build/bin/plugins/libstructures.so")  # a Linux build artefact
        config = ServerConfig(port=8082, auto_open_browser=False, project_root=root,
                              build_dir=root / "build")
        with mock.patch("platform.system", return_value="Windows"):
            self.assertIsNone(PluginRegistry(config).resolve_library("plugins/structures"))


# ======================================================================== config/plugin sync
class ExamplePluginConfigSyncTests(unittest.TestCase):
    """The C++ plugin and data/defaults/default_config.json must declare the same keys.

    The panel only lets the user edit keys present in the defaults, and the plugin only reads
    the keys it knows; a key missing on either side is silently useless.
    """

    def test_every_parameter_the_plugin_reads_is_declared_in_the_defaults(self):
        source = (REAL_ROOT / "plugins" / "example_plugin" / "example_plugin.cpp").read_text(encoding="utf-8")
        read_by_plugin = set(re.findall(r'read(?:Bool|Number|Integer)\(json, "([a-z_]+)"', source))
        self.assertTrue(read_by_plugin, "could not find the parameter readers in example_plugin.cpp")

        config = json.loads((REAL_ROOT / "data" / "defaults" / "default_config.json").read_text(encoding="utf-8"))
        entry = next(p for p in config["plugins"] if p["name"] == "example_plugin")
        declared = set(entry["parameters"])

        # use_host_logger is consumed by the host, not by the plugin.
        self.assertEqual(declared - {"use_host_logger"}, read_by_plugin)

    def test_declared_defaults_are_accepted_by_the_registry_validation(self):
        # Round-trips every declared default through the parameters endpoint contract.
        from web_server.plugin_registry import PluginRegistry
        config = json.loads((REAL_ROOT / "data" / "defaults" / "default_config.json").read_text(encoding="utf-8"))
        entry = next(p for p in config["plugins"] if p["name"] == "example_plugin")
        self.assertEqual(entry["type"], 1, "force output (gravity, drag) needs a parallel plugin")
        for key, value in entry["parameters"].items():
            self.assertIsInstance(value, (bool, int, float), key)


# ======================================================================== unit: utils
class GravityResolutionTests(unittest.TestCase):
    def test_defaults(self):
        g = resolve_gravity({})
        self.assertTrue(g["enabled"])
        self.assertEqual(g["vector"], [0.0, 0.0, -9.81])
        self.assertEqual(g["source"], "default")

    def test_direction_is_normalised(self):
        g = resolve_gravity({"environment": {"gravity": {"magnitude": 10, "direction": [0, 3, 4]}}})
        self.assertAlmostEqual(g["vector"][1], 6.0)
        self.assertAlmostEqual(g["vector"][2], 8.0)

    def test_normalize_does_not_touch_plugin_parameters(self):
        cfg = {
            "simulation": {"duration": 10, "time_step": 0.1},
            "physics": {"enable_gravity": True, "gravity_magnitude": 3.0},
            "plugins": [{"name": "environment", "enabled": True, "parameters": {"enable_wind_effects": False}},
                        {"name": "structures", "enabled": True, "parameters": {}}],
        }
        out = normalize_config_for_simulator(cfg)
        env = next(p for p in out["plugins"] if p["name"] == "environment")
        self.assertEqual(env["parameters"], {"enable_wind_effects": False})
        self.assertEqual(next(p for p in out["plugins"] if p["name"] == "structures")["parameters"], {})
        # The legacy fields are folded into the canonical block that seeds the state.
        self.assertEqual(out["environment"]["gravity"]["magnitude"], 3.0)
        # idempotent: the manager normalises a second time
        self.assertEqual(normalize_config_for_simulator(out)["environment"], out["environment"])

    def test_validate_accepts_current_and_legacy_payloads(self):
        base = {"simulation": {"duration": 5}}
        self.assertEqual(validate_config({**base, "environment": {"gravity": {"magnitude": 9.81}}}), (True, None))
        self.assertEqual(validate_config({**base, "physics": {"gravity_magnitude": 9.81}}), (True, None))


if __name__ == "__main__":
    unittest.main()
