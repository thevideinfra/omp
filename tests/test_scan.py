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


def scan_raw(*args, env=None):
    result = subprocess.run(
        [sys.executable, str(SCAN), *args], capture_output=True, text=True, check=True,
        env=env,
    )
    return [json.loads(line) for line in result.stdout.splitlines()]


def scan(*args):
    return [e for e in scan_raw(*args) if "offline" not in e]


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

    def test_missing_and_empty_folders_are_reported_offline(self):
        with tempfile.TemporaryDirectory() as d:
            self.assertEqual(
                scan_raw("/nonexistent/omamusic-test", d),
                [{"offline": "/nonexistent/omamusic-test"}, {"offline": d}],
            )

    def test_a_hung_folder_goes_offline_and_the_next_one_is_scanned(self):
        # Load the script as a module and make the first folder's listing hang.
        import importlib.machinery
        import importlib.util
        import io
        import time
        from contextlib import redirect_stdout

        loader = importlib.machinery.SourceFileLoader("omascan", str(SCAN))
        spec = importlib.util.spec_from_loader("omascan", loader)
        mod = importlib.util.module_from_spec(spec)
        loader.exec_module(mod)
        real_listdir = os.listdir

        def listdir(path):
            if path == "/hung":
                time.sleep(5)
            return real_listdir(path)

        mod.os = type("OS", (), {"__getattr__": lambda self, n: getattr(os, n), "listdir": staticmethod(listdir)})()
        os.environ["OMAMUSIC_SCAN_STALL"] = "0.5"
        try:
            buf = io.StringIO()
            started = time.time()
            with redirect_stdout(buf):
                mod.main(["/hung", "/nonexistent/omamusic-test"])
            self.assertLess(time.time() - started, 3)
        finally:
            del os.environ["OMAMUSIC_SCAN_STALL"]
        lines = [json.loads(l) for l in buf.getvalue().splitlines()]
        self.assertEqual(lines, [{"offline": "/hung"}, {"offline": "/nonexistent/omamusic-test"}])

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
                        "year": 0,
                    }
                )
                + "\n"
            )
            self.assertEqual(scan("--cache", str(cache), d)[0]["title"], "Cached")

    def test_reads_the_year_from_the_date_or_year_tag(self):
        with tempfile.TemporaryDirectory() as d:
            make_flac(Path(d) / "a.flac", title="A", date="2019-05-01")
            make_flac(Path(d) / "b.flac", title="B", year="1987")
            make_flac(Path(d) / "c.flac", title="C")
            years = {e["title"]: e["year"] for e in scan(d)}
            self.assertEqual(years, {"A": 2019, "B": 1987, "C": 0})

    def test_a_cache_entry_from_before_years_is_probed_again(self):
        with tempfile.TemporaryDirectory() as d:
            f = Path(d) / "a.flac"
            make_flac(f, title="Real", date="2001")
            st = f.stat()
            cache = Path(d) / "cache.jsonl"
            cache.write_text(
                json.dumps({"path": str(f), "title": "Old", "size": st.st_size, "mtime": int(st.st_mtime)}) + "\n"
            )
            e = scan("--cache", str(cache), d)[0]
            self.assertEqual(e["title"], "Real")
            self.assertEqual(e["year"], 2001)

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
