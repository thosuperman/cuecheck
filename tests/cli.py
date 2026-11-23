"""Black-box contract tests against the executable, including Windows paths."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

EXE = Path(sys.argv.pop()).resolve()
T = "00:00:01,000 --> 00:00:02,000"


class CliTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def call(self, *args, status=0):
        result = subprocess.run([EXE, *args], cwd=self.root, capture_output=True)
        self.assertEqual(result.returncode, status, (result.stdout[:1000], result.stderr))
        return result.stdout.decode("utf-8").replace("\r\n", "\n")

    def write(self, contents, name="sub.srt"):
        (self.root / name).write_bytes(contents.encode() if isinstance(contents, str) else contents)
        return name

    def check(self, data, *args, status=0):
        file = self.write(data)
        report, = json.loads(self.call("--json", *args, file, status=status))
        self.assertEqual((self.root / file).read_bytes(), data.encode() if isinstance(data, str) else data)
        self.assertEqual(report["file"], file)
        return [(x["line"], x["code"]) for x in report["issues"]]

    def test_clean_unicode_multiline(self):
        for newline in ("\r\n", "\r", "\n"):
            with self.subTest(newline=newline):
                self.assertEqual(self.check(newline.join(["\ufeff1", T, "Café", "「字幕」", " \t", "2",
                    "00:00:02,000 --> 00:00:03,000", "123"])), [])

    def test_malformed_time_and_range(self):
        for stamp in ["00:60:00,000", "00:00:60,000", "00:00:01.000", "0:00:01,000", "ab:00:01,000"]:
            self.assertEqual(self.check(f"1\n{stamp} --> 00:00:03,000\nA", status=1), [(2, "timestamp")])
        for end in ["00:00:01,000", "00:00:00,999"]:
            self.assertEqual(self.check(f"1\n00:00:01,000 --> {end}\nA", status=1), [(2, "duration")])

    def test_no_index_empty_text_or_truncated_block(self):
        self.assertEqual(self.check(f"{T}\nHi", status=1), [(1, "missing-number")])
        self.assertEqual(self.check(f"1\n{T}\n\n2", status=1), [(2, "empty-text"), (4, "missing-timing")])
        self.assertEqual(self.check("\ufeff \r\n", status=1), [(1, "empty-file")])

    def test_missing_separator_recovers(self):
        t2 = "00:00:02,000 --> 00:00:03,000"
        self.assertEqual(self.check(f"1\n{T}\n2\n{t2}\nNext", status=1),
                         [(2, "empty-text"), (3, "missing-separator")])

    def test_overlaps_nested_unsorted_and_touching(self):
        def cue(i, a, b):
            return f"{i}\n00:00:{a:02},000 --> 00:00:{b:02},000\nTest"
        file = "\n\n".join([cue(1, 5, 10), cue(2, 1, 2), cue(3, 6, 7), cue(4, 8, 9), cue(5, 10, 11)])
        expected = [(6, "out-of-order"), (10, "overlap"), (14, "overlap")]
        self.assertEqual(self.check(file), expected)
        self.assertEqual(self.check(file, "--strict", status=1), expected)

    def test_readability_counts_characters_not_bytes_or_style_tags(self):
        file = f"1\n{T}\n<i>字幕</i>\n123"
        self.assertEqual(self.check(file, "--max-cps", "6", "--max-line", "3"), [])
        self.assertEqual(self.check(file, "--max-cps", "5", "--max-line", "2"), [(2, "reading-speed"), (4, "line-length")])

    def test_numbering_warning_and_severity(self):
        file = self.write(f"42\n{T}\nHi")
        issue = json.loads(self.call("--json", file))[0]["issues"][0]
        self.assertEqual((issue["severity"], issue["code"]), ("warning", "numbering"))
        self.assertIn("Expected cue number 1", self.call("--strict", file, status=1))

    def test_invalid_encoding(self):
        for bad in (b"\xff\xfe", b"\x00", b"\xc0\x80", b"\xed\xa0\x80", b"\xf4\x90\x80\x80", b"\xf0\x9f"):
            self.assertEqual(self.check(b"1\n" + T.encode() + b"\n" + bad, status=1), [(3, "encoding")])

    def test_batch_precedence_and_unicode_path(self):
        name = self.write(f"1\n{T}\nHi", "cut – hội thoại.srt")
        broken = self.write("")
        result = json.loads(self.call("--json", "absent.srt", broken, name, status=2))
        self.assertEqual([x["file"] for x in result], ["absent.srt", broken, name])
        self.assertEqual([x["cues"] for x in result], [0, 0, 1])
        self.assertEqual(result[0]["issues"][0]["code"], "input")
        self.assertEqual(result[2]["issues"], [])

    def test_options_and_limits(self):
        self.assertIn("Usage:", self.call("--help"))
        self.assertRegex(self.call("--version"), r"^CueCheck \d+\.\d+\.\d+\n$")
        for args in [[], ["--bogus"], ["--json"], ["--max-cps", "0"], ["--max-line", "4.5"], ["--max-cps"]]:
            self.call(*args, status=2)
        name = self.write(f"1\n{T}\nOK", "-draft.srt")
        self.assertIn("0 issue(s)", self.call("--", name))
        self.assertEqual(self.check(b" " * (4 * 1024 * 1024 + 1), status=2), [(0, "input")])

    @unittest.skipUnless(sys.platform == "win32", "Windows namespace syntax")
    def test_windows_namespace_path_is_rejected_before_lookup(self):
        for name in [r"\\localhost\share\cut.srt", "//localhost/share/cut.srt", r"\\?\C:\cut.srt"]:
            issue = json.loads(self.call("--json", name, status=2))[0]["issues"][0]
            self.assertIn("UNC and device paths", issue["message"])


if __name__ == "__main__":
    unittest.main()
