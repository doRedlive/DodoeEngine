from __future__ import annotations

import argparse
from pathlib import Path


DEFAULT_EXTENSIONS = {".h", ".hpp", ".c", ".cpp", ".cs", ".inl"}
DEFAULT_TARGET_DIRS = ["engine/src/runtime", "engine/src/editor", "engine/src/scriptcore", "engine/src/sandbox"]


def count_file_lines(file_path: Path) -> tuple[int, int, int]:
	"""Return (total, non_empty, code) line counts for a source file."""
	total = 0
	non_empty = 0
	code = 0

	try:
		with file_path.open("r", encoding="utf-8", errors="ignore") as f:
			for raw_line in f:
				total += 1
				line = raw_line.strip()

				if not line:
					continue
				non_empty += 1

				# Ignore comment-only lines.
				if line.startswith("//") or line.startswith("/*") or line.startswith("*") or line.startswith("*/"):
					continue

				code += 1
	except OSError as e:
		print(f"[WARN] Skip unreadable file: {file_path} ({e})")

	return total, non_empty, code


def collect_files(root: Path, target_dirs: list[str], exts: set[str]) -> list[Path]:
	files: list[Path] = []
	for rel_dir in target_dirs:
		abs_dir = root / rel_dir
		if not abs_dir.exists():
			print(f"[WARN] Directory not found, skipped: {abs_dir}")
			continue

		for file_path in abs_dir.rglob("*"):
			if not file_path.is_file():
				continue
			if file_path.suffix.lower() in exts:
				files.append(file_path)
	return files


def main() -> None:
	parser = argparse.ArgumentParser(description="Count code lines in selected project directories.")
	parser.add_argument(
		"--root",
		default=str(Path(__file__).resolve().parent.parent),
		help="Project root directory. Defaults to the parent of scripts/.",
	)
	parser.add_argument(
		"--dirs",
		nargs="+",
		default=DEFAULT_TARGET_DIRS,
		help="Relative directories to count. Defaults to runtime+editor.",
	)
	parser.add_argument(
		"--ext",
		nargs="+",
		default=sorted(DEFAULT_EXTENSIONS),
		help="File extensions to include, e.g. .cpp .h .hpp",
	)
	parser.add_argument(
		"--details",
		action="store_true",
		help="Print per-file line counts.",
	)

	args = parser.parse_args()
	root = Path(args.root).resolve()
	exts = {e if e.startswith(".") else f".{e}" for e in args.ext}
	files = collect_files(root, args.dirs, exts)

	if not files:
		print("No matching files found.")
		return

	total_all = 0
	non_empty_all = 0
	code_all = 0
	by_dir: dict[str, tuple[int, int, int, int]] = {}

	for file_path in sorted(files):
		total, non_empty, code = count_file_lines(file_path)
		total_all += total
		non_empty_all += non_empty
		code_all += code

		rel_path = file_path.relative_to(root)
		top_scope = rel_path.parts[0] if rel_path.parts else str(rel_path)
		# Track by full requested scope when possible.
		scope_name = next((d for d in args.dirs if str(rel_path).startswith(d)), top_scope)
		f_count, t_count, n_count, c_count = by_dir.get(scope_name, (0, 0, 0, 0))
		by_dir[scope_name] = (f_count + 1, t_count + total, n_count + non_empty, c_count + code)

		if args.details:
			print(f"{rel_path}: total={total}, non_empty={non_empty}, code={code}")

	print("\n=== Directory Summary ===")
	for scope_name in sorted(by_dir):
		f_count, t_count, n_count, c_count = by_dir[scope_name]
		print(f"{scope_name}: files={f_count}, total={t_count}, non_empty={n_count}, code={c_count}")

	print("\n=== Overall ===")
	print(f"files={len(files)}, total={total_all}, non_empty={non_empty_all}, code={code_all}")


if __name__ == "__main__":
	main()
