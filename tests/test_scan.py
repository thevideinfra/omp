import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCAN = Path(__file__).resolve().parent.parent / "bin" / "omamusic-scan"


def make_flac(path, **tags):
    meta = []
    for key, value in tags.items():
        meta += ["-metadata", f"{key}={value}"]
    subprocess.run(
        ["ffmpeg", "-v", "error", "-f", "lavfi", "-i", "sine=d=0.3", *meta, str(path)],
        check=True,
    )


def scan(*args):
    result = subprocess.run(
        [sys.executable, str(SCAN), *args], capture_output=True, text=True, check=True
    )
    return [json.loads(line) for line in result.stdout.splitlines()]


class ScanTest(unittest.TestCase):
    def test_reads_tags_and_stat(self):
        with tempfile.TemporaryDirectory() as d:
            sub = Path(d) / "Album"
            sub.mkdir()
            make_flac(
                sub / "01.flac",
                title="One",
                artist="Amy",
                album="Beta",
                album_artist="Amy",
                track="3/12",
                disc="1",
            )
            (sub / "notes.txt").write_text("skip me")
            entries = scan(d)
            self.assertEqual(len(entries), 1)
            e = entries[0]
            self.assertEqual(e["path"], str(sub / "01.flac"))
            self.assertEqual(e["title"], "One")
            self.assertEqual(e["artist"], "Amy")
            self.assertEqual(e["album"], "Beta")
            self.assertEqual(e["album_artist"], "Amy")
            self.assertEqual(e["track"], 3)
            self.assertEqual(e["disc"], 1)
            self.assertGreater(e["duration"], 0.1)
            self.assertEqual(e["size"], (sub / "01.flac").stat().st_size)

    def test_missing_folder_is_skipped(self):
        self.assertEqual(scan("/nonexistent/omamusic-test"), [])

    def test_cache_hit_reuses_entry(self):
        with tempfile.TemporaryDirectory() as d:
            f = Path(d) / "a.flac"
            make_flac(f, title="Real")
            st = f.stat()
            cache = Path(d) / "cache.jsonl"
            cache.write_text(
                json.dumps(
                    {
                        "path": str(f),
                        "title": "Cached",
                        "size": st.st_size,
                        "mtime": int(st.st_mtime),
                    }
                )
                + "\n"
            )
            self.assertEqual(scan("--cache", str(cache), d)[0]["title"], "Cached")

    def test_cache_miss_reprobes(self):
        with tempfile.TemporaryDirectory() as d:
            f = Path(d) / "a.flac"
            make_flac(f, title="Real")
            cache = Path(d) / "cache.jsonl"
            cache.write_text(
                json.dumps({"path": str(f), "title": "Cached", "size": 1, "mtime": 1})
                + "\n"
            )
            self.assertEqual(scan("--cache", str(cache), d)[0]["title"], "Real")

    def test_non_utf8_filename_is_skipped(self):
        with tempfile.TemporaryDirectory() as d:
            make_flac(Path(d) / "ok.flac", title="Ok")
            make_flac(Path(d) / "tmp.flac", title="Bad")
            os.rename(
                os.path.join(os.fsencode(d), b"tmp.flac"),
                os.path.join(os.fsencode(d), b"caf\xe9.flac"),
            )
            entries = scan(d)
            self.assertEqual([e["path"] for e in entries], [str(Path(d) / "ok.flac")])


if __name__ == "__main__":
    unittest.main()
