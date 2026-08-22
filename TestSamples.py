#!/usr/bin/env python3
"""Corpus regression runner for TinyEXIF.

Runs the TinyEXIFdemo binary against every .jpg sample under Samples/
(recursively) and compares its exit code + output against a checked-in
baseline (Samples/**/<name>.expected).

Check mode (default):
    TestSamples.py --binary /path/to/TinyEXIFdemo
    (or: TINYEXIF_DEMO=/path/to/TinyEXIFdemo TestSamples.py)
Exits 0 if every sample matches its baseline, 1 otherwise. Prints a unified
diff for every mismatch and a final "N samples, M mismatches" summary.

Update mode (regenerates baselines from the current binary's output):
    TestSamples.py --binary /path/to/TinyEXIFdemo --update
Use only when a change to the parser legitimately alters its output.
"""
import argparse
import difflib
import os
import shutil
import subprocess
import sys

SAMPLE_EXT = '.jpg'
BASELINE_EXT = '.expected'

# A legitimate sample parses in ~10ms on this machine; 30s gives a huge margin for a slow or
# ASAN-instrumented CI runner while still bounding how long a single hostile/hanging sample
# (e.g. a future fuzzer find that loops instead of crashing) can stall the whole run.
DEMO_TIMEOUT_SECONDS = 30
TIMEOUT_MARKER = 'TIMEOUT'


def find_binary(binary_arg):
	path = binary_arg or os.environ.get('TINYEXIF_DEMO')
	if not path:
		sys.exit(
			"error: TinyEXIFdemo binary not specified; "
			"pass --binary PATH or set the TINYEXIF_DEMO environment variable"
		)
	if not os.path.isfile(path) or not os.access(path, os.X_OK):
		sys.exit("error: TinyEXIFdemo binary not found or not executable: {}".format(path))
	return path


def find_samples(samples_dir):
	"""Yield (jpg_path, baseline_path) for every sample under samples_dir, recursively.

	Paths are built from `root` (not the top-level samples_dir), so samples in
	subdirectories resolve correctly.
	"""
	for root, dirs, filenames in os.walk(samples_dir):
		dirs.sort()  # deterministic traversal order once there is more than one subdirectory
		for f in sorted(filenames):
			base, extension = os.path.splitext(f)
			if extension.lower() != SAMPLE_EXT:
				continue
			fullpath = os.path.join(root, f)
			baseline_path = os.path.join(root, base + BASELINE_EXT)
			yield fullpath, baseline_path


def find_orphan_baselines(samples_dir, samples):
	"""Yield every baseline under samples_dir that no sample in `samples` claims.

	find_samples() enumerates samples and derives the baseline path from them, so a
	baseline whose sample was deleted or renamed is never looked at: the run just
	reports a smaller sample count and still succeeds. Rather than re-deriving the
	sample name from the baseline -- a second derivation that could drift from the
	first -- this compares against the exact baseline paths find_samples() produced,
	so the two directions cannot disagree.
	"""
	claimed = set(baseline_path for _, baseline_path in samples)
	for root, dirs, filenames in os.walk(samples_dir):
		dirs.sort()
		for f in sorted(filenames):
			if os.path.splitext(f)[1].lower() != BASELINE_EXT:
				continue
			baseline_path = os.path.join(root, f)
			if baseline_path not in claimed:
				yield baseline_path


def run_demo(binary, sample_path, timeout=DEMO_TIMEOUT_SECONDS):
	"""Run the demo synchronously and return (exit_code, combined stdout+stderr text).

	exit_code is the process's real exit code, or TIMEOUT_MARKER if the demo did not finish
	within `timeout` seconds. A hang (e.g. a hostile sample that loops instead of crashing)
	is treated as a hard mismatch, never as an indefinite stall.
	"""
	try:
		proc = subprocess.run(
			[binary, sample_path],
			stdout=subprocess.PIPE,
			stderr=subprocess.STDOUT,
			timeout=timeout,
		)
	except subprocess.TimeoutExpired as exc:
		# On POSIX, subprocess.run does not recover partial output after a timeout kill.
		return TIMEOUT_MARKER, (exc.output or b'').decode('utf-8', errors='replace')
	return proc.returncode, proc.stdout.decode('utf-8', errors='replace')


def render_baseline(exit_code, output):
	return "exit_code: {}\n{}".format(exit_code, output)


def is_crash_exit(exit_code):
	"""True if exit_code says the demo died on a signal rather than exiting normally.

	subprocess reports a POSIX signal death as a negative code; shells and CI runners
	surface the same event as 128+signum, so both forms are treated as a crash.
	"""
	return isinstance(exit_code, int) and (exit_code < 0 or exit_code >= 128)


def print_exiftool_cross_check(exiftool, sample_path):
	"""Best-effort, informational only: not compared, not checked in."""
	print("--- exiftool cross-check for {} (informational only, not compared) ---".format(sample_path))
	try:
		proc = subprocess.run(
			[exiftool, '-n', '-s', '-G', sample_path],
			stdout=subprocess.PIPE,
			stderr=subprocess.STDOUT,
			timeout=DEMO_TIMEOUT_SECONDS,
		)
	except subprocess.TimeoutExpired:
		# This fires on a mismatch, i.e. on the samples most likely to be hostile --
		# the same crafted files the fuzzer produced. An informational extra must not
		# be able to stall the run until the CI job limit.
		print("exiftool did not finish within {}s; skipping the cross-check".format(
			DEMO_TIMEOUT_SECONDS))
		return
	sys.stdout.write(proc.stdout.decode('utf-8', errors='replace'))


def main(argv):
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument('--binary', help='path to TinyEXIFdemo (or set TINYEXIF_DEMO env var)')
	parser.add_argument(
		'--samples',
		default=os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Samples'),
		help='path to the Samples directory (default: Samples/ next to this script)',
	)
	parser.add_argument(
		'--update',
		action='store_true',
		help='regenerate baselines from the current binary output instead of checking them',
	)
	args = parser.parse_args(argv)

	binary = find_binary(args.binary)

	if not os.path.isdir(args.samples):
		sys.exit("error: samples directory not found: {}".format(args.samples))

	samples = list(find_samples(args.samples))
	if not samples:
		sys.exit("error: no {} samples found under {}".format(SAMPLE_EXT, args.samples))

	exiftool = shutil.which('exiftool')

	if args.update:
		timed_out = []
		crashed = []
		for sample_path, baseline_path in samples:
			exit_code, output = run_demo(binary, sample_path)
			if exit_code == TIMEOUT_MARKER:
				# Never bake a hang into a baseline -- that would turn a real bug into
				# permanently "expected" behaviour instead of surfacing it.
				print("TIMEOUT: {} did not finish within {}s; not writing a baseline for it".format(
					sample_path, DEMO_TIMEOUT_SECONDS))
				timed_out.append(sample_path)
				continue
			if is_crash_exit(exit_code):
				# Same reasoning as the timeout guard above: a signal death is a bug, and
				# writing it out would make the crash permanently "expected" -- the corpus
				# gate would then go green on a segfault forever.
				print("CRASH: {} died with exit code {}; not writing a baseline for it".format(
					sample_path, exit_code))
				crashed.append(sample_path)
				continue
			with open(baseline_path, 'w') as fh:
				fh.write(render_baseline(exit_code, output))
			print("updated " + baseline_path)
		if timed_out:
			print("update aborted: {} sample(s) timed out, see above".format(len(timed_out)))
		if crashed:
			print("update aborted: {} sample(s) crashed, see above".format(len(crashed)))
		if timed_out or crashed:
			return 1
		print("updated {} baseline(s)".format(len(samples)))
		return 0

	mismatches = 0
	for sample_path, baseline_path in samples:
		exit_code, output = run_demo(binary, sample_path)
		actual = render_baseline(exit_code, output)

		if not os.path.isfile(baseline_path):
			print("MISSING BASELINE: {} (expected at {})".format(sample_path, baseline_path))
			mismatches += 1
			continue

		with open(baseline_path, 'r') as fh:
			expected = fh.read()

		if actual != expected:
			mismatches += 1
			if exit_code == TIMEOUT_MARKER:
				print("TIMEOUT: {} did not finish within {}s".format(sample_path, DEMO_TIMEOUT_SECONDS))
			else:
				print("MISMATCH: " + sample_path)
			diff = difflib.unified_diff(
				expected.splitlines(keepends=True),
				actual.splitlines(keepends=True),
				fromfile=baseline_path,
				tofile=sample_path + ' (actual)',
			)
			sys.stdout.writelines(diff)
			if exiftool:
				print_exiftool_cross_check(exiftool, sample_path)

	for baseline_path in find_orphan_baselines(args.samples, samples):
		# The corpus shrank: a checked-in baseline has lost its sample. Counted as a
		# mismatch so the gate fails instead of quietly testing fewer files.
		print("ORPHAN BASELINE: {} (no matching {} sample)".format(baseline_path, SAMPLE_EXT))
		mismatches += 1

	print("{} samples, {} mismatches".format(len(samples), mismatches))
	return 1 if mismatches else 0


if __name__ == '__main__':
	sys.exit(main(sys.argv[1:]))
