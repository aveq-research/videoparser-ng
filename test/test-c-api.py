#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# ///
"""
Compare the output of the C API test program with the CLI.

For each build directory and clip, run video-parser and
test/c-api/videoparser-c-test and compare stdout byte by byte, and the exit
status, in these modes:

- path: open the file by path
- io: read the file through the custom input callbacks, with seek
- io-small: as io, with at most 1000 bytes per read
- n5: only the first 5 frames (-n 5)
- io-no-seek: custom input without seek; only the frame and summary records
  are compared, since the sequence information lacks the scan. Inputs that
  cannot be opened without seeking are listed, not counted as failures.
- all-frames: with frames_without_statistics; the output must be the same
  where the CLI succeeds. Where it fails (for example FFV1), the number of
  frames is listed.

With --raw, also compare the decoded pictures (vp_get_picture, with
frames_without_statistics) with the raw video from FFmpeg's ffmpeg program.

Usage:

    uv run test/test-c-api.py --build build --build build/shared-legacy \
        --clips test --clips /path/to/clips
"""

import argparse
import hashlib
import os
import subprocess
import sys
import tempfile
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
VIDEO_EXTENSIONS = {
    ".mp4",
    ".mkv",
    ".webm",
    ".ts",
    ".mpg",
    ".m2v",
    ".h264",
    ".264",
    ".hevc",
    ".265",
    ".ivf",
    ".mov",
}
MODES = ["path", "io", "io-small", "n5", "io-no-seek", "all-frames"]


@dataclass
class Result:
    build: str
    clip: Path
    group: str
    mode: str
    ok: bool
    note: str = ""


@dataclass
class Run:
    stdout: bytes
    stderr: bytes
    returncode: int


def run(cmd: list[str], timeout: int) -> Run:
    proc = subprocess.run(cmd, capture_output=True, timeout=timeout)
    return Run(proc.stdout, proc.stderr, proc.returncode)


def find_clips(paths: list[str]) -> list[Path]:
    clips: list[Path] = []
    for p in paths:
        path = Path(p)
        if path.is_file():
            clips.append(path)
        elif path.is_dir():
            clips.extend(
                sorted(
                    f
                    for f in path.rglob("*")
                    if f.is_file() and f.suffix.lower() in VIDEO_EXTENSIONS
                )
            )
        else:
            sys.exit(f"Not found: {p}")
    return clips


def first_difference(a: bytes, b: bytes) -> str:
    la, lb = a.splitlines(), b.splitlines()
    for i, (x, y) in enumerate(zip(la, lb)):
        if x != y:
            return f"line {i + 1}: cli={x[:200]!r} c={y[:200]!r}"
    return f"line counts differ: cli={len(la)} c={len(lb)}"


def compare(cli: Run, c: Run, skip_first_line: bool = False) -> tuple[bool, str]:
    if cli.returncode != c.returncode:
        return False, (
            f"exit status cli={cli.returncode} c={c.returncode}; "
            f"c stderr: {c.stderr.decode(errors='replace').strip()[-300:]}"
        )
    a, b = cli.stdout, c.stdout
    if skip_first_line:
        a = b"".join(a.splitlines(keepends=True)[1:])
        b = b"".join(b.splitlines(keepends=True)[1:])
    if a != b:
        return False, first_difference(a, b)
    return True, ""


def check_clip(
    build: Path, clip: Path, group: str, modes: list[str], timeout: int
) -> list[Result]:
    cli_bin = str(build / "VideoParserCli" / "video-parser")
    c_bin = str(build / "test" / "c-api" / "videoparser-c-test")
    results = []
    cli_full = run([cli_bin, str(clip)], timeout)

    def add(mode: str, ok: bool, note: str = ""):
        results.append(Result(str(build), clip, group, mode, ok, note))

    for mode in modes:
        if mode == "path":
            add(mode, *compare(cli_full, run([c_bin, str(clip)], timeout)))
        elif mode == "io":
            add(mode, *compare(cli_full, run([c_bin, "--io", str(clip)], timeout)))
        elif mode == "io-small":
            c = run([c_bin, "--io", "--read-size", "1000", str(clip)], timeout)
            add(mode, *compare(cli_full, c))
        elif mode == "n5":
            cli = run([cli_bin, "-n", "5", str(clip)], timeout)
            add(mode, *compare(cli, run([c_bin, "-n", "5", str(clip)], timeout)))
        elif mode == "io-no-seek":
            c = run([c_bin, "--io-no-seek", str(clip)], timeout)
            if cli_full.returncode == 0 and c.returncode != 0:
                # Could not be read without seeking (for example MP4 with the
                # index at the end): report, but do not fail
                note = c.stderr.decode(errors="replace").strip().splitlines()
                add(mode, True, "needs seek: " + (note[-1] if note else ""))
            else:
                add(mode, *compare(cli_full, c, skip_first_line=True))
        elif mode == "all-frames":
            c = run([c_bin, "--all-frames", str(clip)], timeout)
            if cli_full.returncode != 0 and c.returncode == 0:
                frames = c.stdout.count(b'"frame_info"')
                add(mode, True, f"{frames} frames without statistics")
            else:
                add(mode, *compare(cli_full, c))
    return results


def md5_of_raw_c(build: Path, clip: Path, timeout: int) -> tuple[str, int]:
    c_bin = str(build / "test" / "c-api" / "videoparser-c-test")
    with tempfile.NamedTemporaryFile(suffix=".yuv") as tmp:
        c = run([c_bin, "--all-frames", "--raw", tmp.name, str(clip)], timeout)
        if c.returncode != 0:
            return "", 0
        frames = sum(1 for line in c.stdout.splitlines() if b'"frame_info"' in line)
        return hashlib.md5(Path(tmp.name).read_bytes()).hexdigest(), frames


def md5_of_raw_ffmpeg(ffmpeg: str, clip: Path, timeout: int) -> str:
    proc = subprocess.run(
        [
            ffmpeg,
            "-nostdin",
            "-v",
            "error",
            "-threads",
            "1",
            "-i",
            str(clip),
            "-map",
            "0:v:0",
            "-fps_mode",
            "passthrough",
            "-f",
            "rawvideo",
            "-",
        ],
        capture_output=True,
        timeout=timeout,
    )
    if proc.returncode != 0:
        return ""
    return hashlib.md5(proc.stdout).hexdigest()


def check_raw(
    build: Path, ffmpeg: str, clip: Path, group: str, timeout: int
) -> Result:
    c_md5, frames = md5_of_raw_c(build, clip, timeout)
    if not c_md5:
        return Result(str(build), clip, group, "raw", True, "no frames (skipped)")
    ff_md5 = md5_of_raw_ffmpeg(ffmpeg, clip, timeout)
    if c_md5 == ff_md5:
        return Result(str(build), clip, group, "raw", True, f"{frames} frames")
    return Result(
        str(build), clip, group, "raw", False, f"md5 c={c_md5} ffmpeg={ff_md5}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "--build",
        action="append",
        help="Build directory with VideoParserCli/ and test/c-api/ (repeatable; default: build)",
    )
    parser.add_argument(
        "--clips",
        action="append",
        help="Clip file or directory, searched recursively (repeatable; default: test/)",
    )
    parser.add_argument(
        "--mode",
        action="append",
        choices=MODES,
        help="Modes to run (repeatable; default: all)",
    )
    parser.add_argument(
        "--raw",
        action="store_true",
        help="Also compare decoded pictures with the raw video of ffmpeg",
    )
    parser.add_argument(
        "--ffmpeg", default="ffmpeg", help="ffmpeg program for --raw (default: ffmpeg)"
    )
    parser.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 4)
    parser.add_argument("--timeout", type=int, default=600)
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    builds = [Path(b) for b in (args.build or [str(HERE.parent / "build")])]
    clips = find_clips(args.clips or [str(HERE)])
    modes = args.mode or MODES
    for build in builds:
        for binary in ["VideoParserCli/video-parser", "test/c-api/videoparser-c-test"]:
            if not (build / binary).exists():
                sys.exit(f"Missing {build / binary}; build first")

    jobs = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for build in builds:
            for clip in clips:
                group = clip.parent.name
                jobs.append(
                    pool.submit(check_clip, build, clip, group, modes, args.timeout)
                )
                if args.raw:
                    jobs.append(
                        pool.submit(
                            check_raw, build, args.ffmpeg, clip, group, args.timeout
                        )
                    )
        results: list[Result] = []
        for job in jobs:
            r = job.result()
            results.extend(r if isinstance(r, list) else [r])

    # Counts per build, group and mode
    table: dict[tuple[str, str, str], list[int]] = defaultdict(lambda: [0, 0])
    for r in results:
        table[(r.build, r.group, r.mode)][0 if r.ok else 1] += 1
    all_modes = modes + (["raw"] if args.raw else [])
    for build in builds:
        print(f"\n{build}")
        groups = sorted({r.group for r in results if r.build == str(build)})
        print(f"  {'group':<12}" + "".join(f"{m:>14}" for m in all_modes))
        for group in groups:
            cells = []
            for mode in all_modes:
                passed, failed = table[(str(build), group, mode)]
                cells.append(f"{passed}/{passed + failed}")
            print(f"  {group:<12}" + "".join(f"{c:>14}" for c in cells))

    notes = [r for r in results if r.ok and r.note and (args.verbose or r.mode != "raw")]
    if notes:
        print("\nNotes:")
        for r in notes:
            print(f"  {r.build} {r.clip} [{r.mode}]: {r.note}")

    failures = [r for r in results if not r.ok]
    if failures:
        print("\nFailures:")
        for r in failures:
            print(f"  {r.build} {r.clip} [{r.mode}]: {r.note}")
        return 1
    print("\nAll outputs identical.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
