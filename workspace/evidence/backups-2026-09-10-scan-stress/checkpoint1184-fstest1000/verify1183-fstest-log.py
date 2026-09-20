"""Review original fstest log; only exempt exact dot-directory listing lines.

fstest_directory labels every non-file entry 'Error', including . and ..;
that branch returns OK. Preserve and reject all other error diagnostics.
Does not open UART or restart the ongoing1181 workload.
"""
import argparse
import importlib.util
from pathlib import Path
import re

D = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('original', D / 'run1181-fstest1000-large.py')
original = importlib.util.module_from_spec(spec)
spec.loader.exec_module(original)


class Verdict(original.Verdict):
    def __init__(self):
        super().__init__()
        self.dot_entries = 0

    def line(self, text):
        if re.fullmatch(r'\s*\d+\. Type\[4\]: Error Name: \.{1,2}\s*', text):
            self.dot_entries += 1
            return
        super().line(text)


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    v = Verdict()
    with args.log.open(errors='replace') as log:
        for line in log:
            v.line(line)
    v.check(require)
    print(f'ORIGINAL_FSTEST_RESULT=2000_OK_0_FAILED dot_listing_lines={v.dot_entries}')
    print('Board exit status and post-test cleanup require separate evidence.')
