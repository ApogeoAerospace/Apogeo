"""
Plugin registry for the MoLab Web Server.

The registry is the single source of truth for the plugin list shown in the
web panel. It merges three layers, from lowest to highest priority:

1. The base configuration (``data/config/web_last_config.json`` when present,
   otherwise ``data/defaults/default_config.json``).
2. A small persisted overlay (``data/config/web_plugin_state.json``) holding the
   ``enabled`` flag and parameter overrides chosen from the web panel.
3. Build information discovered at request time (is the plugin library built?).

The overlay lives outside the versioned defaults on purpose: toggling a plugin
from the UI must never rewrite ``default_config.json``.
"""
from __future__ import annotations

import copy
import json
import os
import platform
import threading
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional

from .logger import setup_logger
from .utils import get_simulator_path, read_json_file


logger = setup_logger(__name__)


class PluginRegistryError(Exception):
    """Error raised by registry operations, carrying an HTTP status and a code."""

    def __init__(self, message: str, status_code: int = 400, code: str = "plugin_error"):
        super().__init__(message)
        self.message = message
        self.status_code = status_code
        self.code = code


# UI-oriented metadata. Runtime configuration always comes from the JSON files.
PLUGIN_DEFINITIONS: Dict[str, Dict[str, str]] = {
    "example_plugin": {
        "display_name": "Example Plugin",
        "description": (
            "Reference plugin: host logger, configurable parameters, state "
            "mutation and force output (gravity, drag)"
        ),
    },
    "aerodynamics": {
        "display_name": "Aerodynamics",
        "description": "Real aerodynamic forces: drag, lift, compressibility effects",
    },
    "propulsion": {
        "display_name": "Propulsion",
        "description": "Rocket/jet propulsion with fuel consumption and altitude compensation",
    },
    "structures": {
        "display_name": "Structures",
        "description": "Mass tracking, center of gravity, and structural analysis",
    },
    "environment": {
        "display_name": "Environment",
        "description": "Atmospheric model, wind effects, and gravity",
        "notes": (
            "In its current version this module does not read any parameters (its "
            "configure step does nothing) and computes gravity with its own model, "
            "ignoring GeneralState.gravity. Gravity cannot be switched off while it is enabled."
        ),
    },
    "programming": {
        "display_name": "Programming",
        "description": "Programmable logic and control sequences",
    },
}

LIBRARY_EXTENSIONS: Dict[str, List[str]] = {
    "Windows": [".dll", ".pyd"],
    "Darwin": [".dylib", ".so"],
    "Linux": [".so"],
}

PLUGIN_TYPE_SEQUENTIAL = 0
PLUGIN_TYPE_PARALLEL = 1

# How a plugin turns gravity into a force. The core only CARRIES ``GeneralState.gravity``
# (it never applies it to the dynamics), so gravity affects a trajectory only when an
# enabled plugin applies it, in one of two ways:
#
#   "state_vector"  reads GeneralState.gravity, i.e. the value seeded from the web panel.
#   "own_model"     computes gravity itself and ignores GeneralState.gravity
#                   (the Environment module: inverse-square law from the position).
#
# Two providers at once add their forces, so the vehicle would fall too fast.
GRAVITY_PROVIDERS = {
    "environment": lambda parameters: "own_model",
    "example_plugin": lambda parameters: "state_vector" if parameters.get("apply_gravity", False) else None,
}


class PluginRegistry:
    """Thread-safe plugin registry backed by config files and a small overlay."""

    def __init__(self, config):
        self.config = config
        self._lock = threading.RLock()
        self.state_file: Path = config.project_root / "data" / "config" / "web_plugin_state.json"
        self.base_config_candidates: List[Path] = [
            config.project_root / "data" / "config" / "web_last_config.json",
            config.project_root / "data" / "defaults" / "default_config.json",
        ]

    # ------------------------------------------------------------------ #
    # Overlay persistence
    # ------------------------------------------------------------------ #
    def _read_overlay(self) -> Dict[str, Dict[str, Any]]:
        data = read_json_file(self.state_file)
        if not isinstance(data, dict):
            return {}
        plugins = data.get("plugins")
        return plugins if isinstance(plugins, dict) else {}

    def _write_overlay(self, overlay: Dict[str, Dict[str, Any]]) -> None:
        try:
            self.state_file.parent.mkdir(parents=True, exist_ok=True)
            tmp = self.state_file.with_suffix(".json.tmp")
            with open(tmp, "w", encoding="utf-8") as handle:
                json.dump({"version": 1, "plugins": overlay}, handle, indent=2)
            os.replace(tmp, self.state_file)
        except OSError as exc:
            raise PluginRegistryError(
                f"Could not persist plugin state: {exc}", 500, "persist_failed"
            ) from exc

    # ------------------------------------------------------------------ #
    # Base configuration
    # ------------------------------------------------------------------ #
    def _base_plugins(self) -> List[Dict[str, Any]]:
        for candidate in self.base_config_candidates:
            data = read_json_file(candidate)
            if isinstance(data, dict) and isinstance(data.get("plugins"), list):
                return [p for p in data["plugins"] if isinstance(p, dict) and p.get("name")]
        return []

    # ------------------------------------------------------------------ #
    # Library discovery
    # ------------------------------------------------------------------ #
    def _library_search_bases(self) -> List[Path]:
        build_dir = self.config.build_dir
        bases: List[Path] = [
            self.config.project_root,
            build_dir,
            build_dir / "bin",
            build_dir / "bin" / "plugins",
            build_dir / "lib",
            build_dir / "lib" / "plugins",
            build_dir / "plugins",
        ]
        # Multi-config / Visual Studio layouts: look next to the simulator binary.
        simulator = get_simulator_path(build_dir)
        if simulator.exists():
            sim_dir = simulator.parent
            bases.extend([sim_dir, sim_dir / "plugins", sim_dir.parent / "lib",
                          sim_dir.parent / "lib" / "plugins"])
        seen, unique = set(), []
        for base in bases:
            key = str(base)
            if key not in seen:
                seen.add(key)
                unique.append(base)
        return unique

    def resolve_library(self, library_path: str) -> Optional[Path]:
        """Resolve a configured ``library_path`` to an existing library file."""
        if not library_path:
            return None

        extensions = LIBRARY_EXTENSIONS.get(platform.system(), [".so"])
        raw = Path(library_path)

        if raw.is_absolute():
            if raw.is_file():
                return raw
            bases = [raw.parent]
            raw_name = raw.name
        else:
            direct = self.config.project_root / raw
            if direct.is_file():
                return direct
            bases = self._library_search_bases()
            raw_name = raw.name

        names = {raw_name}
        names.add(raw_name[3:] if raw_name.startswith("lib") else f"lib{raw_name}")

        for base in bases:
            for name in sorted(names):
                for ext in ("", *extensions):
                    candidate = base / f"{name}{ext}"
                    if candidate.is_file():
                        return candidate
        return None

    # ------------------------------------------------------------------ #
    # Public API
    # ------------------------------------------------------------------ #
    def _merge(self, base: Dict[str, Any], overlay: Dict[str, Any]) -> Dict[str, Any]:
        name = str(base.get("name"))
        metadata = PLUGIN_DEFINITIONS.get(name, {})
        resolved = self.resolve_library(str(base.get("library_path", "")))

        parameters = copy.deepcopy(base.get("parameters") or {})
        override_params = overlay.get("parameters")
        if isinstance(override_params, dict):
            parameters.update(override_params)

        enabled = overlay.get("enabled", base.get("enabled", False))

        return {
            "id": name,
            "name": metadata.get("display_name", name),
            "description": metadata.get("description", f"{name} plugin"),
            "notes": metadata.get("notes", ""),
            "type": int(base.get("type", PLUGIN_TYPE_SEQUENTIAL)),
            "enabled": bool(enabled),
            "parameters": parameters,
            "default_enabled": bool(base.get("enabled", False)),
            "default_parameters": copy.deepcopy(base.get("parameters") or {}),
            "library_path": resolved.as_posix() if resolved else str(base.get("library_path", "")),
            "library_found": resolved is not None,
        }

    def list_plugins(self) -> List[Dict[str, Any]]:
        with self._lock:
            overlay = self._read_overlay()
            plugins = [self._merge(base, overlay.get(str(base["name"]), {}))
                       for base in self._base_plugins()]

        if plugins:
            return plugins

        # No configuration available: expose the known plugins, all disabled.
        return [
            {
                "id": name,
                "name": meta.get("display_name", name),
                "description": meta.get("description", f"{name} plugin"),
                "notes": meta.get("notes", ""),
                "type": PLUGIN_TYPE_SEQUENTIAL,
                "enabled": False,
                "parameters": {},
                "default_enabled": False,
                "default_parameters": {},
                "library_path": "",
                "library_found": False,
            }
            for name, meta in PLUGIN_DEFINITIONS.items()
        ]

    def get_plugin(self, plugin_id: str) -> Dict[str, Any]:
        for plugin in self.list_plugins():
            if plugin["id"] == plugin_id:
                return plugin
        raise PluginRegistryError(f"Unknown plugin: {plugin_id}", 404, "unknown_plugin")

    def set_enabled(self, plugin_id: str, enabled: Optional[bool] = None) -> Dict[str, Any]:
        """Enable/disable a plugin. ``enabled=None`` flips the current state."""
        with self._lock:
            plugin = self.get_plugin(plugin_id)
            target = (not plugin["enabled"]) if enabled is None else bool(enabled)

            if target and not plugin["library_found"]:
                raise PluginRegistryError(
                    f"Plugin '{plugin_id}' cannot be enabled: its library was not found "
                    f"(looked for '{plugin['library_path']}'). Build the plugin first.",
                    409,
                    "library_not_found",
                )

            overlay = self._read_overlay()
            entry = overlay.setdefault(plugin_id, {})
            entry["enabled"] = target
            self._write_overlay(overlay)
            logger.info("Plugin '%s' %s", plugin_id, "enabled" if target else "disabled")
            return self.get_plugin(plugin_id)

    def set_parameters(self, plugin_id: str, parameters: Dict[str, Any]) -> Dict[str, Any]:
        """Update parameter overrides. Only known keys with matching types are accepted."""
        if not isinstance(parameters, dict) or not parameters:
            raise PluginRegistryError("'parameters' must be a non-empty object", 400, "invalid_parameters")

        with self._lock:
            plugin = self.get_plugin(plugin_id)
            known = plugin["default_parameters"]
            cleaned: Dict[str, Any] = {}

            for key, value in parameters.items():
                if key not in known:
                    raise PluginRegistryError(
                        f"Unknown parameter '{key}' for plugin '{plugin_id}'", 400, "unknown_parameter"
                    )
                reference = known[key]
                if isinstance(reference, bool):
                    if not isinstance(value, bool):
                        raise PluginRegistryError(f"Parameter '{key}' must be a boolean", 400, "invalid_parameters")
                elif isinstance(reference, (int, float)):
                    if isinstance(value, bool) or not isinstance(value, (int, float)):
                        raise PluginRegistryError(f"Parameter '{key}' must be a number", 400, "invalid_parameters")
                    if value != value or value in (float("inf"), float("-inf")):
                        raise PluginRegistryError(f"Parameter '{key}' must be finite", 400, "invalid_parameters")
                elif isinstance(reference, str):
                    if not isinstance(value, str):
                        raise PluginRegistryError(f"Parameter '{key}' must be a string", 400, "invalid_parameters")
                cleaned[key] = value

            overlay = self._read_overlay()
            entry = overlay.setdefault(plugin_id, {})
            entry.setdefault("parameters", {}).update(cleaned)
            self._write_overlay(overlay)
            return self.get_plugin(plugin_id)

    def apply_preset(self, enabled_ids: Iterable[str],
                     overrides: Optional[Dict[str, Dict[str, Any]]] = None) -> List[Dict[str, Any]]:
        """Replace the enabled set in one step (used by mission templates).

        Plugins whose library is not built are skipped and reported in the
        ``skipped`` list of each returned entry instead of failing the preset.
        """
        wanted = {str(p) for p in enabled_ids}
        overrides = overrides or {}

        with self._lock:
            plugins = self.list_plugins()
            known_ids = {p["id"] for p in plugins}
            unknown = wanted - known_ids
            if unknown:
                raise PluginRegistryError(
                    f"Unknown plugin(s): {', '.join(sorted(unknown))}", 404, "unknown_plugin"
                )

            overlay = self._read_overlay()
            for plugin in plugins:
                pid = plugin["id"]
                entry = overlay.setdefault(pid, {})
                entry["enabled"] = pid in wanted and plugin["library_found"]
                extra = overrides.get(pid)
                if pid in wanted and isinstance(extra, dict):
                    allowed = {k: v for k, v in extra.items() if k in plugin["default_parameters"]}
                    if allowed:
                        entry.setdefault("parameters", {}).update(allowed)
            self._write_overlay(overlay)

            result = self.list_plugins()
            for plugin in result:
                plugin["skipped"] = plugin["id"] in wanted and not plugin["enabled"]
            return result

    def reset(self) -> List[Dict[str, Any]]:
        """Drop every override and return to the configuration defaults."""
        with self._lock:
            try:
                if self.state_file.exists():
                    self.state_file.unlink()
            except OSError as exc:
                raise PluginRegistryError(f"Could not reset plugin state: {exc}", 500, "persist_failed") from exc
            return self.list_plugins()

    def gravity_providers(self, plugins: Optional[List[Dict[str, Any]]] = None) -> List[Dict[str, str]]:
        """Enabled plugins that will apply gravity during a run, with how they do it."""
        plugins = plugins if plugins is not None else self.list_plugins()
        providers = []
        for plugin in plugins:
            rule = GRAVITY_PROVIDERS.get(plugin["id"])
            if plugin.get("enabled") and rule:
                mode = rule(plugin.get("parameters") or {})
                if mode:
                    providers.append({"id": plugin["id"], "mode": mode})
        return providers

    def gravity_consumers(self, plugins: Optional[List[Dict[str, Any]]] = None) -> List[str]:
        """Ids of the plugins returned by :meth:`gravity_providers`."""
        return [provider["id"] for provider in self.gravity_providers(plugins)]

    def resolve_run_plugins(self, requested: Optional[List[Any]] = None) -> List[Dict[str, Any]]:
        """Plugin configs to hand to the simulator for one run.

        Without ``requested`` the persisted registry state is used (this is what
        the web panel relies on). A legacy client may still send an explicit
        ``plugins`` list: it overrides the enabled flags for that run only, but it
        is validated against the registry so unknown or unbuilt plugins fail
        early with a clear message instead of failing inside the simulator.
        """
        if requested is None:
            return self.simulator_plugin_configs()

        if not isinstance(requested, list):
            raise PluginRegistryError("'plugins' must be a list", 400, "invalid_plugins")

        known = {p["id"]: p for p in self.list_plugins()}
        configs: List[Dict[str, Any]] = []
        for entry in requested:
            if not isinstance(entry, dict) or not entry.get("name"):
                raise PluginRegistryError("Each plugin entry needs a 'name'", 400, "invalid_plugins")
            name = str(entry["name"])
            if name not in known:
                raise PluginRegistryError(f"Unknown plugin: {name}", 400, "unknown_plugin")
            base = known[name]
            enabled = bool(entry.get("enabled", True))
            if enabled and not base["library_found"]:
                raise PluginRegistryError(
                    f"Plugin '{name}' library was not found; build it or disable the plugin.",
                    409, "library_not_found",
                )
            parameters = copy.deepcopy(base["parameters"])
            if isinstance(entry.get("parameters"), dict):
                parameters.update(entry["parameters"])
            configs.append({
                "name": name,
                "type": base["type"],
                "library_path": base["library_path"],
                "enabled": enabled,
                "parameters": parameters,
            })
        return configs

    def simulator_plugin_configs(self) -> List[Dict[str, Any]]:
        """Plugin entries in the simulator config schema, from the effective state."""
        configs = []
        for plugin in self.list_plugins():
            configs.append({
                "name": plugin["id"],
                "type": plugin["type"],
                "library_path": plugin["library_path"],
                "enabled": plugin["enabled"],
                "parameters": plugin["parameters"],
            })
        return configs
