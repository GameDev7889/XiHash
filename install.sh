#!/usr/bin/env bash
set -euo pipefail

REPO_URL="https://github.com/GameDev7889/XiHash.git"
INSTALL_ROOT="${XIHASH_INSTALL_ROOT:-${HOME}/.local/share/xihash}"
SOURCE_DIR="${INSTALL_ROOT}/source"
BIN_DIR="${HOME}/.local/bin"
COMPILER="${BIN_DIR}/XiHash"

for command_name in git cmake python3; do
  if ! command -v "${command_name}" >/dev/null 2>&1; then
    echo "Error: ${command_name} is required. Install it and run this script again." >&2
    exit 1
  fi
done
if ! command -v c++ >/dev/null 2>&1 && ! command -v g++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1; then
  echo "Error: a C++ compiler is required (for example, g++ or clang++)." >&2
  exit 1
fi
if ! command -v node >/dev/null 2>&1 || ! command -v npm >/dev/null 2>&1; then
  echo "Error: Node.js and npm are required to package the VS Code extension." >&2
  exit 1
fi

mkdir -p "${INSTALL_ROOT}" "${BIN_DIR}"
if [[ -d "${SOURCE_DIR}/.git" ]]; then
  git -C "${SOURCE_DIR}" pull --ff-only
else
  git clone "${REPO_URL}" "${SOURCE_DIR}"
fi

cmake -S "${SOURCE_DIR}" -B "${INSTALL_ROOT}/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "${INSTALL_ROOT}/build" --config Release --parallel
install -m 755 "${INSTALL_ROOT}/build/XiHash" "${COMPILER}"

EXTENSION_DIR="${SOURCE_DIR}/examples/xi-hash-vscode"
VSIX_PATH="${INSTALL_ROOT}/xihash.vsix"
(cd "${EXTENSION_DIR}" && npx --yes @vscode/vsce package --out "${VSIX_PATH}")

if command -v code >/dev/null 2>&1; then
  CODE_COMMAND="$(command -v code)"
elif command -v code-insiders >/dev/null 2>&1; then
  CODE_COMMAND="$(command -v code-insiders)"
else
  echo "Compiler installed at: ${COMPILER}"
  echo "Extension package created at: ${VSIX_PATH}"
  echo "Install VS Code, then run: code --install-extension '${VSIX_PATH}'"
  exit 0
fi

"${CODE_COMMAND}" --install-extension "${VSIX_PATH}" --force

case "$(uname -s)" in
  Darwin)
    if [[ "${CODE_COMMAND}" == *insiders* ]]; then
      SETTINGS_PATH="${HOME}/Library/Application Support/Code - Insiders/User/settings.json"
    else
      SETTINGS_PATH="${HOME}/Library/Application Support/Code/User/settings.json"
    fi
    ;;
  *)
    if [[ "${CODE_COMMAND}" == *insiders* ]]; then
      SETTINGS_PATH="${HOME}/.config/Code - Insiders/User/settings.json"
    else
      SETTINGS_PATH="${HOME}/.config/Code/User/settings.json"
    fi
    ;;
esac

python3 - "${SETTINGS_PATH}" "${COMPILER}" <<'PY'
import json
import re
import sys
from pathlib import Path

settings_path = Path(sys.argv[1])
compiler_path = str(Path(sys.argv[2]).resolve())
settings_path.parent.mkdir(parents=True, exist_ok=True)
contents = settings_path.read_text() if settings_path.exists() else "{\n}\n"
encoded_path = json.dumps(compiler_path)
pattern = re.compile(r'("xihash\.compilerPath"\s*:\s*)("(?:\\.|[^"\\])*")')
if pattern.search(contents):
    contents = pattern.sub(lambda match: match.group(1) + encoded_path, contents, count=1)
else:
    # VS Code settings are JSON with comments; insert into the root object without reformatting it.
    closing = contents.rfind("}")
    if closing < 0:
        raise SystemExit(f"Could not find the closing brace in {settings_path}")
    before = contents[:closing]
    meaningful = before.rstrip()
    if meaningful.endswith("{") or meaningful.endswith(","):
        separator = ""
    else:
        separator = ","
    contents = before + separator + '\n  "xihash.compilerPath": ' + encoded_path + '\n' + contents[closing:]
settings_path.write_text(contents)
PY

echo "XiHash compiler installed at ${COMPILER}"
echo "VS Code extension installed and configured in ${SETTINGS_PATH}"
echo "Open an .xi file in VS Code to see syntax highlighting and compiler diagnostics."
