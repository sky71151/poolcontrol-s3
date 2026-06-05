import json
import os
import re
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

Import("env")

CONFIG_FILE = Path(env.subst("$PROJECT_DIR")) / "include" / "config.h"
VERSION_DEFINE = "OTA_GITHUB_CURRENT_VERSION"

NEW_VERSION = None


def _log(message):
    print(f"[github-ota] {message}")


def _read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _write_text(path: Path, text: str):
    path.write_text(text, encoding="utf-8")


def _increment_semver(version: str) -> str:
    parts = version.split(".")
    if len(parts) != 3 or any(not p.isdigit() for p in parts):
        raise ValueError(f"Unsupported version format '{version}'. Expected x.y.z")

    major, minor, patch = [int(p) for p in parts]
    patch += 1
    return f"{major}.{minor}.{patch}"


def bump_version_before_build(*args, **kwargs):
    global NEW_VERSION

    if not CONFIG_FILE.exists():
        raise RuntimeError(f"Config file not found: {CONFIG_FILE}")

    text = _read_text(CONFIG_FILE)
    pattern = re.compile(rf'^(\s*#define\s+{re.escape(VERSION_DEFINE)}\s+")([^"]+)("\s*)$', re.MULTILINE)
    match = pattern.search(text)
    if not match:
        raise RuntimeError(
            f"Could not find #define {VERSION_DEFINE} in {CONFIG_FILE}"
        )

    current_version = match.group(2).strip()
    NEW_VERSION = _increment_semver(current_version)

    updated_text = pattern.sub(
        lambda m: f"{m.group(1)}{NEW_VERSION}{m.group(3)}",
        text,
        count=1,
    )
    _write_text(CONFIG_FILE, updated_text)

    _log(f"Version bumped: {current_version} -> {NEW_VERSION}")


def _github_request(method: str, url: str, token: str, data=None, headers=None):
    req_headers = {
        "Accept": "application/vnd.github+json",
        "Authorization": f"Bearer {token}",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "platformio-github-ota-script",
    }
    if headers:
        req_headers.update(headers)

    body = None
    if data is not None:
        if isinstance(data, (dict, list)):
            body = json.dumps(data).encode("utf-8")
            req_headers["Content-Type"] = "application/json"
        elif isinstance(data, bytes):
            body = data
        else:
            body = str(data).encode("utf-8")

    request = urllib.request.Request(url=url, data=body, headers=req_headers, method=method)

    try:
        with urllib.request.urlopen(request) as response:
            raw = response.read()
            if not raw:
                return None
            content_type = response.headers.get("Content-Type", "")
            if "application/json" in content_type:
                return json.loads(raw.decode("utf-8"))
            return raw
    except urllib.error.HTTPError as ex:
        response_body = ex.read().decode("utf-8", errors="ignore")
        raise RuntimeError(f"GitHub API error {ex.code} for {url}: {response_body}") from ex


def _get_or_create_release(repo: str, token: str, tag_name: str, release_name: str):
    api_base = f"https://api.github.com/repos/{repo}"

    try:
        return _github_request("GET", f"{api_base}/releases/tags/{tag_name}", token)
    except RuntimeError:
        _log(f"Release for tag {tag_name} not found, creating a new release")

    payload = {
        "tag_name": tag_name,
        "name": release_name,
        "draft": False,
        "prerelease": False,
        "generate_release_notes": False,
    }
    return _github_request("POST", f"{api_base}/releases", token, data=payload)


def _delete_existing_asset_if_needed(repo: str, token: str, release: dict, asset_name: str):
    api_base = f"https://api.github.com/repos/{repo}"
    for asset in release.get("assets", []):
        if asset.get("name") == asset_name and asset.get("id") is not None:
            asset_id = asset["id"]
            _log(f"Deleting existing release asset {asset_name}")
            _github_request("DELETE", f"{api_base}/releases/assets/{asset_id}", token)
            return


def _upload_asset(repo: str, token: str, release: dict, asset_name: str, asset_path: Path):
    if not asset_path.exists():
        _log(f"Skipping missing asset: {asset_path.name}")
        return

    _delete_existing_asset_if_needed(repo, token, release, asset_name)

    upload_url_template = release.get("upload_url", "")
    if not upload_url_template:
        raise RuntimeError("Release upload_url missing in GitHub response")

    upload_url = upload_url_template.split("{")[0]
    upload_url = f"{upload_url}?name={urllib.parse.quote(asset_name)}"

    _log(f"Uploading {asset_path.name} to release as {asset_name}")
    _github_request(
        "POST",
        upload_url,
        token,
        data=asset_path.read_bytes(),
        headers={"Content-Type": "application/octet-stream"},
    )


def upload_bin_after_build(source, target, env):
    global NEW_VERSION

    if NEW_VERSION is None:
        _log("No version bump detected in this run, skipping GitHub upload")
        return

    token = os.getenv("GITHUB_TOKEN", "").strip()
    repo = os.getenv("GITHUB_REPOSITORY", "").strip()

    if not token or not repo:
        _log("Skipping GitHub upload: set GITHUB_TOKEN and GITHUB_REPOSITORY")
        return

    release_prefix = os.getenv("GITHUB_RELEASE_PREFIX", "v")
    base_asset_name = os.getenv("GITHUB_ASSET_NAME", "poolcontrol")
    tag_name = f"{release_prefix}{NEW_VERSION}"
    release_name = f"PoolControl {NEW_VERSION}"

    if isinstance(source, list) and source:
        firmware_path = Path(str(source[0]))
    else:
        firmware_path = Path(env.subst("$BUILD_DIR/${PROGNAME}.bin"))

    if not firmware_path.exists():
        raise RuntimeError(f"Firmware binary not found: {firmware_path}")

    factory_path = firmware_path.with_suffix("").with_suffix(".factory.bin")

    release = _get_or_create_release(repo, token, tag_name, release_name)

    _log(f"Uploading artifacts to {repo} release {tag_name}")
    _upload_asset(repo, token, release, f"{base_asset_name}.bin", firmware_path)
    _upload_asset(repo, token, release, f"{base_asset_name}.factory.bin", factory_path)

    _log("GitHub upload completed")


if os.getenv("PIO_DISABLE_GITHUB_UPLOAD_SCRIPT", "0") != "1":
    env.AddPreAction("$BUILD_DIR/${PROGNAME}.bin", bump_version_before_build)
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", upload_bin_after_build)
else:
    _log("Build hooks disabled by PIO_DISABLE_GITHUB_UPLOAD_SCRIPT=1")
