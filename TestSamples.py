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
	for root, _dirs, filenames in os.walk(samples_dir):
		for f in sorted(filenames):
			base, extension = os.path.splitext(f)
			if extension.lower() != SAMPLE_EXT:
				continue
			fullpath = os.path.join(root, f)
			baseline_path = os.path.join(root, base + BASELINE_EXT)
			yield fullpath, baseline_path


def run_demo(binary, sample_path):
	"""Run the demo synchronously and return (exit_code, combined stdout+stderr text)."""
	proc = subprocess.run(
		[binary, sample_path],
		stdout=subprocess.PIPE,
		stderr=subprocess.STDOUT,
	)
	return proc.returncode, proc.stdout.decode('utf-8', errors='replace')


def render_baseline(exit_code, output):
	return "exit_code: {}\n{}".format(exit_code, output)


def print_exiftool_cross_check(exiftool, sample_path):
	"""Best-effort, informational only: not compared, not checked in."""
	proc = subprocess.run(
		[exiftool, '-n', '-s', '-G', sample_path],
		stdout=subprocess.PIPE,
		stderr=subprocess.STDOUT,
	)
	print("--- exiftool cross-check for {} (informational only, not compared) ---".format(sample_path))
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
		for sample_path, baseline_path in samples:
			exit_code, output = run_demo(binary, sample_path)
			with open(baseline_path, 'w') as fh:
				fh.write(render_baseline(exit_code, output))
			print("updated " + baseline_path)
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

	print("{} samples, {} mismatches".format(len(samples), mismatches))
	return 1 if mismatches else 0


if __name__ == '__main__':
	sys.exit(main(sys.argv[1:]))
