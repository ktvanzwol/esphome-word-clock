"""Run isolated ESPHome configuration tests and optional firmware builds in WSLC."""

import argparse
import copy
from pathlib import Path
import shutil
import subprocess

import yaml


class ExampleLoader(yaml.SafeLoader):
    pass


ExampleLoader.add_constructor("!secret", lambda loader, node: "validation-only")


def run_cli(config: Path, command: str, work: Path, generate: bool = False) -> subprocess.CompletedProcess:
    args = ["esphome", command, str(config)]
    if generate:
        args.append("--only-generate")
    result = subprocess.run(args, capture_output=True, text=True)
    (work / f"{config.stem}-{command}.log").write_text(result.stdout + result.stderr)
    return result


def require_success(result: subprocess.CompletedProcess, label: str) -> None:
    if result.returncode:
        raise RuntimeError(f"{label} failed:\n" + "\n".join((result.stdout + result.stderr).splitlines()[-60:]))
    print(f"PASS: {label}", flush=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--compile", action="store_true")
    parser.add_argument("--native", action="store_true", help="Build and execute native ESPHome runtime tests")
    args = parser.parse_args()
    source = Path(__file__).resolve().parent.parent
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    for directory in ("components", "example", "tests"):
        shutil.copytree(source / directory, work / directory, dirs_exist_ok=True, ignore=shutil.ignore_patterns("__pycache__"))
    (work / "example" / "secrets.yaml").write_text(
        "ota_password: validation-only\nwifi_ssid_iot: validation-only\nwifi_password_iot: validation-only\n"
    )
    example_source = work / "example" / "word-clock.yaml"
    example_config = yaml.load(example_source.read_text(), Loader=ExampleLoader)
    example_config["external_components"][0]["source"] = {
        "type": "local",
        "path": "../components",
    }
    example_source.write_text(yaml.safe_dump(example_config, sort_keys=False))
    subprocess.run(
        ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", str(work / "tests" / "dutch_clock_test.cpp"),
         str(work / "components" / "word_clock" / "dutch_clock.cpp"), "-o", str(work / "dutch-clock-test")],
        check=True,
    )
    subprocess.run([str(work / "dutch-clock-test")], check=True)

    example = example_source
    require_success(run_cli(example, "compile", work, generate=True), "full example configuration and code generation")
    data = yaml.load(example.read_text(), Loader=ExampleLoader)
    matrix_only = copy.deepcopy(data)
    matrix_only["esphome"]["name"] = "word-clock-matrix-only"
    del matrix_only["light"][2]["dots_display_id"]
    del matrix_only["light"][0]
    del matrix_only["display"][0]
    matrix_path = work / "example" / "matrix-only.yaml"
    matrix_path.write_text(yaml.safe_dump(matrix_only, sort_keys=False))
    require_success(run_cli(matrix_path, "compile", work, generate=True), "matrix-only configuration and code generation")

    cases = []
    variant = copy.deepcopy(data)
    variant["light"][2]["matrix_display_id"] = "neopixel_text"
    cases.append(("wrong-type", variant, "AddressableLightDisplay"))
    variant = copy.deepcopy(data)
    variant["display"][1]["width"] = 10
    cases.append(("wrong-dimensions", variant, "logical dimensions 11x10"))
    variant = copy.deepcopy(data)
    variant["display"][0]["width"] = 3
    cases.append(("wrong-dot-dimensions", variant, "logical dimensions 4x1"))
    variant = copy.deepcopy(data)
    variant["display"][1]["lambda"] = "it.clear();"
    cases.append(("competing-writer", variant, "cannot have a lambda or pages"))
    variant = copy.deepcopy(data)
    variant["light"][1]["gamma_correct"] = 2.8
    cases.append(("double-gamma", variant, "gamma_correct: 1.0"))
    variant = copy.deepcopy(data)
    variant["light"][1]["internal"] = False
    variant["light"][1]["name"] = "Exposed hardware"
    cases.append(("exposed-hardware", variant, "must be internal"))
    variant = copy.deepcopy(data)
    variant["light"][1]["num_leds"] = 109
    cases.append(("insufficient-leds", variant, "fewer LEDs"))
    variant = copy.deepcopy(data)
    variant["display"][0]["addressable_light_id"] = "neopixel_text"
    cases.append(("shared-backing", variant, "another display"))
    variant = copy.deepcopy(data)
    duplicate = copy.deepcopy(variant["light"][2])
    duplicate.update(id="second_clock", name="Second Clock")
    variant["light"].append(duplicate)
    cases.append(("shared-clock", variant, "cannot be shared"))
    variant = copy.deepcopy(data)
    variant["light"][2]["time_id"] = "neopixel_text"
    cases.append(("wrong-time-type", variant, "RealTimeClock"))
    for name, variant, expected in cases:
        path = work / "example" / f"{name}.yaml"
        path.write_text(yaml.safe_dump(variant, sort_keys=False))
        result = run_cli(path, "config", work)
        if result.returncode == 0 or expected not in result.stdout + result.stderr:
            details = "Configuration unexpectedly passed" if result.returncode == 0 else (
                "\n".join((result.stdout + result.stderr).splitlines()[-25:])
            )
            raise RuntimeError(f"Negative schema test {name} did not report {expected!r}:\n{details}")
        print(f"PASS: rejects {name}", flush=True)
    if args.native or args.compile:
        host = work / "tests" / "host.yaml"
        require_success(run_cli(host, "compile", work), "native ESPHome runtime test build")
        programs = list((work / "tests" / ".esphome" / "build" / "word-clock-native-test").rglob("program"))
        if len(programs) != 1:
            raise RuntimeError(f"Expected exactly one native test executable, found {programs}")
        subprocess.run([str(programs[0])], check=True, timeout=20, cwd=work)
    if args.compile:
        for path in (example, matrix_path):
            require_success(run_cli(path, "compile", work), f"{path.stem} firmware build")


if __name__ == "__main__":
    main()
