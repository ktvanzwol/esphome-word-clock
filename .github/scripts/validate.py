"""Run CI validation using official ESP-IDF release asset mirrors."""

import os
from pathlib import Path
import subprocess
import sys
from urllib.parse import urlparse

import requests
import yaml

from esphome.components.esp32 import ESP_IDF_FRAMEWORK_VERSION_LOOKUP


def framework_asset_url() -> str:
    """Resolve the configured framework's official GitHub release asset."""
    config = yaml.safe_load(Path("tests/word-clock.yaml").read_text())
    framework = config["esp32"]["framework"]
    if framework["type"] != "esp-idf":
        raise ValueError("CI validation expects an ESP-IDF framework")
    selected = str(framework.get("version", "recommended"))
    version = str(ESP_IDF_FRAMEWORK_VERSION_LOOKUP.get(selected, selected))
    headers = {"Accept": "application/vnd.github+json"}
    if token := os.environ.get("GITHUB_TOKEN"):
        headers["Authorization"] = f"Bearer {token}"

    release_url = f"https://api.github.com/repos/esphome-libs/esp-idf/releases/tags/v{version}"
    release = requests.get(release_url, headers=headers, timeout=30)
    release.raise_for_status()
    expected_name = f"esp-idf-v{version}.tar.xz"
    assets = [asset for asset in release.json()["assets"] if asset["name"] == expected_name]
    if len(assets) != 1:
        raise ValueError(f"Expected exactly one official framework asset named {expected_name}")
    asset_url = assets[0]["url"]
    if urlparse(asset_url).hostname != "api.github.com":
        raise ValueError("Unexpected GitHub release asset API host")

    headers["Accept"] = "application/octet-stream"
    response = requests.get(asset_url, headers=headers, allow_redirects=False, timeout=30)
    response.raise_for_status()
    if response.status_code != 302 or "Location" not in response.headers:
        raise ValueError("GitHub did not redirect the ESP-IDF release asset download")
    location = response.headers["Location"]
    parsed = urlparse(location)
    if parsed.scheme != "https" or parsed.hostname != "release-assets.githubusercontent.com":
        raise ValueError("Unexpected GitHub release asset download host")
    return location


def main() -> int:
    mirror = framework_asset_url()
    if os.environ.get("GITHUB_ACTIONS") == "true":
        print(f"::add-mask::{mirror}", flush=True)
    env = os.environ.copy()
    env.pop("GITHUB_TOKEN", None)
    env["ESPHOME_IDF_FRAMEWORK_MIRRORS"] = mirror
    env["IDF_GITHUB_ASSETS"] = "dl.espressif.com/github_assets"
    print("Using the official ESPHome framework release and Espressif tool mirror.", flush=True)
    return subprocess.call(
        [sys.executable, "tests/validate.py", "--work", "/tmp/word-clock-validation", "--compile"],
        env=env,
    )


if __name__ == "__main__":
    sys.exit(main())
