#!/usr/bin/env bash
# LOCUS A-07 Windows/Git Bash launcher.
# Pull -> configure -> build -> self-test -> audio smoke -> interactive run -> logs.
# The launched program is a diagnostic SPIKE, not the playable LOCUS game.
set -Eeuo pipefail

say() { printf '\n[A-07] %s\n' "$*"; }
fail() { printf '\n[A-07] ERROR: %s\n' "$*" >&2; exit 1; }

case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) ;;
  *) fail "Use Git Bash on Windows. This probe builds a Windows executable, not a WSL/Linux binary." ;;
esac

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
[[ -d "$ROOT/.git" ]] || fail "This folder is not a Git clone. Clone LOCUS first; see README.md."

for command_name in git cmake tar tee; do
  command -v "$command_name" >/dev/null 2>&1 ||
    fail "Missing $command_name in PATH. See the Windows prerequisites in README.md."
done

[[ "$(git -C "$ROOT" rev-parse --is-inside-work-tree 2>/dev/null)" == "true" ]] ||
  fail "Cannot access the Git repository."
# All generated data stays Git-ignored; even failed pulls get a log bundle.
RUNS="$ROOT/.locus-runs"
RUN_DIR="$RUNS/a07-$(date -u +%Y%m%dT%H%M%SZ)-$$"
mkdir -p "$RUN_DIR"
chmod 700 "$RUN_DIR" 2>/dev/null || true

bundle_logs() {
  local status=$?
  trap - EXIT
  {
    printf 'script_exit_code=%s\n' "$status"
    printf 'readme=README.md\n'
    printf 'test_report=mvp/architecture/STACK_EVALUATION.md\n'
  } >> "$RUN_DIR/run-info.txt" 2>/dev/null || true

  local items=()
  local item
  for item in run-info.txt git-update.log configure.log build.log self-test.log audio-smoke.log \
              probe-console.log raylib_probe_log.csv user-feedback.txt; do
    [[ ! -f "$RUN_DIR/$item" ]] || items+=("$item")
  done
  if ((${#items[@]})); then
    tar -czf "$RUN_DIR/LOCUS-A07-logs.tar.gz" -C "$RUN_DIR" "${items[@]}" ||
      printf '[A-07] WARNING: log bundle creation failed.\n' >&2
  fi

  printf '\n==============================================================\n'
  printf 'A-07 RUN FINISHED (exit code: %s)\n' "$status"
  printf 'Log folder: %s\n' "$RUN_DIR"
  printf 'UPLOAD TO CHAT: %s\n' "$RUN_DIR/LOCUS-A07-logs.tar.gz"
  printf 'The log archive excludes your generated audio and build files.\n'
  printf 'Review logs before sharing; compiler output can contain local paths.\n'
  printf '==============================================================\n'
}
trap bundle_logs EXIT

if [[ -n "$(git -C "$ROOT" status --porcelain)" ]]; then
  printf 'Repository contains local changes; pull blocked to protect your edits.\n' \
    | tee "$RUN_DIR/git-update.log" >&2
  fail "Commit or stash local changes before pulling; nothing was overwritten."
fi

say "Updating the current Git branch (fast-forward only; never force/reset)."
if ! git -C "$ROOT" pull --ff-only 2>&1 | tee "$RUN_DIR/git-update.log"; then
  fail "Git pull failed. Check network access and the configured upstream branch. Send the log bundle."
fi

CMAKE_GENERATOR_NAME="${LOCUS_CMAKE_GENERATOR:-Visual Studio 17 2022}"
{
  printf 'task=A-07\n'
  printf 'component=isolated-raylib-audio-probe-not-the-game\n'
  printf 'git_commit=%s\n' "$(git -C "$ROOT" rev-parse HEAD)"
  printf 'git_branch=%s\n' "$(git -C "$ROOT" branch --show-current)"
  printf 'platform=%s\n' "$(uname -s)"
  printf 'generator=%s\n' "$CMAKE_GENERATOR_NAME"
  printf 'cmake_version=%s\n' "$(cmake --version | head -n 1)"
  printf 'runner=local-Windows-Git-Bash\n'
} > "$RUN_DIR/run-info.txt"

SOURCE="$ROOT/mvp/spikes/raylib_audio"
BUILD="$ROOT/build-a07"
[[ -f "$SOURCE/CMakeLists.txt" ]] ||
  fail "The A-07 probe is missing after git pull: $SOURCE"

say "Configuring the standalone audio probe."
cmake_args=(-S "$SOURCE" -B "$BUILD" -G "$CMAKE_GENERATOR_NAME")
if [[ "$CMAKE_GENERATOR_NAME" == Visual\ Studio* ]]; then
  cmake_args+=(-A x64)
fi
if ! cmake "${cmake_args[@]}" 2>&1 | tee "$RUN_DIR/configure.log"; then
  fail "CMake configuration failed. Install Visual Studio 2022 with Desktop development with C++, or set LOCUS_CMAKE_GENERATOR for an already configured toolchain. Share the log bundle."
fi

say "Compiling the Windows Release executable."
if ! cmake --build "$BUILD" --config Release --parallel 2 2>&1 | tee "$RUN_DIR/build.log"; then
  fail "Compilation failed. Send the log bundle so I can inspect the compiler error."
fi

EXE="$BUILD/Release/locus_raylib_audio_spike.exe"
[[ -f "$EXE" ]] || EXE="$BUILD/locus_raylib_audio_spike.exe"
[[ -f "$EXE" ]] || fail "Build completed but executable was not found in build-a07."

printf 'probe_exe_bytes=%s\n' "$(wc -c < "$EXE" | tr -d '[:space:]')" \
  >> "$RUN_DIR/run-info.txt"

# The probe deliberately uses relative WAV/CSV paths. Keep all output per run.
cd "$RUN_DIR"

say "Checking the generated metronome WAV without starting audio."
if ! "$EXE" --self-test 2>&1 | tee "$RUN_DIR/self-test.log"; then
  fail "WAV/size self-test failed. Send the log bundle."
fi

say "Checking whether raylib can open the local audio device and seek/pause."
if ! "$EXE" --audio-smoke 2>&1 | tee "$RUN_DIR/audio-smoke.log"; then
  fail "Audio smoke test failed (possibly no audio device). Send the log bundle; the graphical probe was not started."
fi

say "Starting the interactive A-07 probe on this computer."
printf '\n'
printf 'LISTEN for 120 BPM clicks (every 0.5 s).\n'
printf 'Press A / D on the clicks to record input timestamps.\n'
printf 'SPACE or P = pause/resume; J / L = seek -/+2 s; R = restart.\n'
printf 'ESC = close. The track is 16 seconds; press R to repeat.\n'
printf 'This is NOT the playable LOCUS game.\n'
printf '\n'
if ! "$EXE" 2>&1 | tee "$RUN_DIR/probe-console.log"; then
  fail "Interactive probe exited with an error. Send the log bundle."
fi

# Optional notes are automatically included alongside the objective CSV.
if [[ -t 0 && "${LOCUS_NO_PROMPT:-0}" != "1" ]]; then
  say "Optional feedback (press Enter to skip each question)."
  {
    printf 'A-07 manual observation (not a calibrated latency measurement)\n'
    read -r -p "Were the clicks clearly audible and evenly spaced? " answer || true
    printf 'audible_and_even=%s\n' "${answer:-not supplied}"
    read -r -p "Did pause, resume, seek, and restart work? " answer || true
    printf 'transport_behavior=%s\n' "${answer:-not supplied}"
    read -r -p "How did A/D tapping feel relative to the clicks? " answer || true
    printf 'input_feel=%s\n' "${answer:-not supplied}"
    read -r -p "Any glitches or extra observations? " answer || true
    printf 'other_notes=%s\n' "${answer:-not supplied}"
  } > "$RUN_DIR/user-feedback.txt"
fi

say "Finished. The EXIT handler is packaging the logs now."
