"""Tests for CI's official framework asset resolution."""

import importlib.util
import os
from pathlib import Path
import unittest
from unittest.mock import Mock, patch


SCRIPT = Path(__file__).resolve().parent.parent / ".github" / "scripts" / "validate.py"
SPEC = importlib.util.spec_from_file_location("ci_validation", SCRIPT)
ci_validation = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ci_validation)


def response(*, body=None, status=200, location=None):
    result = Mock()
    result.json.return_value = body
    result.status_code = status
    result.headers = {"Location": location} if location else {}
    return result


class CIValidationTest(unittest.TestCase):
    def setUp(self):
        self.config = "esp32:\n  framework:\n    type: esp-idf\n"
        self.asset = {
            "name": "esp-idf-v5.5.6.tar.xz",
            "url": "https://api.github.com/repos/esphome-libs/esp-idf/releases/assets/123",
        }
        self.location = "https://release-assets.githubusercontent.com/official-asset"

    @patch.object(ci_validation, "ESP_IDF_FRAMEWORK_VERSION_LOOKUP", {"recommended": "5.5.6"})
    def test_resolves_recommended_asset_with_github_token(self):
        with patch.object(Path, "read_text", return_value=self.config), \
             patch.dict(os.environ, {"GITHUB_TOKEN": "test-token"}, clear=True), \
             patch.object(ci_validation.requests, "get") as get:
            get.side_effect = [
                response(body={"assets": [self.asset]}),
                response(status=302, location=self.location),
            ]
            self.assertEqual(ci_validation.framework_asset_url(), self.location)
            self.assertIn("/tags/v5.5.6", get.call_args_list[0].args[0])
            self.assertEqual(get.call_args_list[1].kwargs["headers"]["Authorization"], "Bearer test-token")
            self.assertFalse(get.call_args_list[1].kwargs["allow_redirects"])

    def test_rejects_missing_asset_and_untrusted_redirect(self):
        with patch.object(Path, "read_text", return_value=self.config), \
             patch.dict(os.environ, {}, clear=True), \
             patch.object(ci_validation, "ESP_IDF_FRAMEWORK_VERSION_LOOKUP", {"recommended": "5.5.6"}), \
             patch.object(ci_validation.requests, "get") as get:
            get.return_value = response(body={"assets": []})
            with self.assertRaisesRegex(ValueError, "exactly one"):
                ci_validation.framework_asset_url()
            get.side_effect = [
                response(body={"assets": [self.asset]}),
                response(status=302, location="https://example.com/archive"),
            ]
            with self.assertRaisesRegex(ValueError, "download host"):
                ci_validation.framework_asset_url()

    def test_rejects_non_idf_framework(self):
        with patch.object(Path, "read_text", return_value="esp32:\n  framework:\n    type: arduino\n"):
            with self.assertRaisesRegex(ValueError, "expects an ESP-IDF"):
                ci_validation.framework_asset_url()

    def test_runs_both_firmware_builds_without_exposing_token(self):
        with patch.object(ci_validation, "framework_asset_url", return_value=self.location), \
             patch.dict(os.environ, {"GITHUB_TOKEN": "test-token"}, clear=True), \
             patch.object(ci_validation.subprocess, "call", return_value=0) as call:
            self.assertEqual(ci_validation.main(), 0)
            command = call.call_args.args[0]
            env = call.call_args.kwargs["env"]
            self.assertIn("--compile", command)
            self.assertEqual(env["ESPHOME_IDF_FRAMEWORK_MIRRORS"], self.location)
            self.assertEqual(env["IDF_GITHUB_ASSETS"], "dl.espressif.com/github_assets")
            self.assertNotIn("GITHUB_TOKEN", env)


if __name__ == "__main__":
    unittest.main()
