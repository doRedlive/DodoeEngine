#!/usr/bin/env python3

import sys

def find_marker(content, marker_name):
    for style in [f"// === {marker_name} ===", f"/* === {marker_name} === */"]:
        if style in content:
            return content.find(style), style
    return -1, None

def line_end_after(content, pos):
    line_end = content.find("\n", pos)
    return len(content) if line_end == -1 else line_end + 1

def splice(target_path, generated_path, start_marker, end_marker):
    with open(target_path, 'r', encoding='utf-8') as f:
        target = f.read()
    with open(generated_path, 'r', encoding='utf-8') as f:
        generated = f.read()

    start_pos, start_tag = find_marker(target, start_marker)
    if start_tag is None:
        print(f"ERROR: start marker '// === {start_marker} ===' or '/* === {start_marker} === */' not found in {target_path}")
        sys.exit(1)
    end_pos, end_tag = find_marker(target, end_marker)
    if end_tag is None:
        print(f"ERROR: end marker '// === {end_marker} ===' or '/* === {end_marker} === */' not found in {target_path}")
        sys.exit(1)

    before = target[:line_end_after(target, start_pos + len(start_tag))]
    after  = target[end_pos:]
    new_content = before + generated.strip() + "\n" + after

    if new_content != target:
        with open(target_path, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f"Updated: {target_path}")
    else:
        print(f"No change: {target_path}")

if __name__ == '__main__':
    if len(sys.argv) != 5:
        print("Usage: splice_generated.py <target> <generated> <start_marker> <end_marker>")
        sys.exit(1)
    splice(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
