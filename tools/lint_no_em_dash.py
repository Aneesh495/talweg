#!/usr/bin/env python3
"""
Lint tool to enforce that no em dashes (U+2014) or en dashes (U+2013)
exist in any source, comment, document, or configuration file.
"""

import sys
from pathlib import Path

FORBIDDEN_CHARS = {
    '\u2014': 'Em dash (U+2014)',
    '\u2013': 'En dash (U+2013)',
}

EXCLUDED_DIRS = {'.git', 'build', 'bin', '__pycache__', '.pytest_cache'}
EXCLUDED_EXTS = {'.flash', '.journal', '.o', '.a', '.so', '.dylib', '.png', '.jpg', '.pdf'}

def check_file(path: Path) -> list:
    violations = []
    try:
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            for line_no, line in enumerate(f, start=1):
                for char, name in FORBIDDEN_CHARS.items():
                    if char in line:
                        col = line.index(char) + 1
                        violations.append((path, line_no, col, name, line.strip()))
    except Exception as e:
        pass
    return violations

def main():
    root = Path(__file__).resolve().parent.parent
    all_violations = []

    for path in root.rglob('*'):
        if path.is_file():
            if any(part in EXCLUDED_DIRS for part in path.parts):
                continue
            if path.suffix in EXCLUDED_EXTS:
                continue
            v = check_file(path)
            all_violations.extend(v)

    if all_violations:
        print(f"FAILED: Found {len(all_violations)} forbidden dash violations:")
        for path, line_no, col, name, text in all_violations:
            rel = path.relative_to(root)
            print(f"  {rel}:{line_no}:{col} - {name}: {text}")
        sys.exit(1)
    else:
        print("OK: Zero em dashes or en dashes found across all files.")
        sys.exit(0)

if __name__ == '__main__':
    main()
