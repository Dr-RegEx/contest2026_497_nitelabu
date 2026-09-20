"""Run a local diagnostic while removing a hidden input from all output."""
import contextlib
import getpass
import runpy
import sys

secret = getpass.getpass('Value to redact (hidden): ')
escaped = ''.join('\\' + c if c in " \t\\'\"`$#;|<>&" else c for c in secret)


class Output:
    def __init__(self, stream):
        self.stream = stream

    def write(self, text):
        rendered = text
        for value in sorted(set((secret, escaped)), key=len, reverse=True):
            if value:
                rendered = rendered.replace(value, '<redacted>')
        self.stream.write(rendered)
        self.stream.flush()
        return len(text)

    def flush(self):
        self.stream.flush()


with open(sys.argv[2], 'x', buffering=1) as log:
    output = Output(log)
    with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
        runpy.run_path(sys.argv[1], run_name='__main__')
print('REDACTED_RUN_COMPLETED')
