import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

YTM = Path(__file__).resolve().parent.parent / "bin" / "omp-ytm"

# A stand-in for ytmusicapi: enough of its shape for the helper, with fixed answers.
STUB = {
    "ytmusicapi/__init__.py": '''
import os
from ytmusicapi.exceptions import YTMusicUserError

class YTMusic:
    def __init__(self, auth=None):
        if "signedout" in (auth or ""):
            raise KeyError("Unable to find path on {'messageRenderer': {'button': {'signInEndpoint': {'hack': True}}}}")
        if "browser" in os.environ.get("OMP_TEST_MODE", "") and "authorization" not in (auth or ""):
            # what the real library says when the headers lack "authorization": it takes them for OAuth
            raise YTMusicUserError("oauth JSON provided via auth argument, but oauth_credentials not provided")
        if "browser" in os.environ.get("OMP_TEST_MODE", "") and ("SID=g" in auth or "gp" in auth or "SID=s" in auth or "SID=m" not in auth or "__Secure-3PAPISID=p" not in auth):
            raise YTMusicUserError("wrong cookies")
        if "x=y" in (auth or ""):
            raise YTMusicUserError("wrong cookies")
        if "bad" in (auth or ""):
            raise YTMusicUserError("Please provide authentication before using this function")
    def get_account_info(self):
        return {"accountName": "Amy"}
    def get_liked_songs(self, limit=None):
        return {"tracks": [
            {"videoId": "aaa", "title": "Liked One", "artists": [{"name": "Zed"}, {"name": "Yan"}],
             "album": {"name": "Gamma"}, "duration_seconds": 200,
             "thumbnails": [{"url": "https://lh3.googleusercontent.com/x=w60-h60-l90-rj"}]},
            {"videoId": "vid", "title": "A Video", "artists": [{"name": "Zed"}], "album": None, "duration_seconds": 90, "thumbnails": []},
            {"videoId": None, "title": "Gone", "artists": [], "isAvailable": False},
        ]}
    def get_library_songs(self, limit=None):
        return [{"videoId": "aaa", "title": "Liked One", "artists": [{"name": "Zed"}], "album": {"name": "Gamma"}},
                {"videoId": "bbb", "title": "Library Two", "artists": [{"name": "Zed"}], "album": {"name": "Gamma"}, "duration_seconds": 100}]
    def get_library_albums(self, limit=None):
        return [{"browseId": "MPREb_1"}]
    def get_album(self, browse_id):
        return {"title": "Delta", "year": "2019", "tracks": [
            {"videoId": "ccc", "title": "Album Song", "artists": [{"name": "Wen"}], "duration_seconds": 61, "trackNumber": 1,
             "thumbnails": [{"url": "https://i.ytimg.com/vi/ccc/hq.jpg"}]}]}
    def get_library_playlists(self, limit=None):
        return [{"playlistId": "LM", "title": "Liked Music"}, {"playlistId": "SE", "title": "Episodes for Later"},
                {"playlistId": "PL1", "title": "Mix"}]
    def get_playlist(self, playlist_id, limit=None):
        return {"tracks": [{"videoId": "ddd", "title": "Only In Playlist", "artists": [{"name": "Vic"}], "album": None},
                           {"videoId": "bbb", "title": "Library Two", "artists": [{"name": "Zed"}]}]}
''',
    "yt_dlp/__init__.py": "",
    "yt_dlp/cookies.py": '''
import json, os
from types import SimpleNamespace

def extract_cookies_from_browser(browser, profile=None, *args, **kw):
    # yt-dlp guesses the keyring from the desktop; on Hyprland it guesses a plain-text one and cannot read Brave's cookies
    if browser != "firefox" and kw.get("keyring") != "GNOMEKEYRING":
        return []
    if browser == "nosuch":
        raise ValueError("could not find nosuch cookies database")
    return [SimpleNamespace(**c) for c in json.loads(os.environ["OMP_TEST_COOKIES"])]
''',
    "ytmusicapi/exceptions.py": '''
class YTMusicError(Exception): pass
class YTMusicUserError(YTMusicError): pass
class YTMusicServerError(YTMusicError): pass
''',
    "ytmusicapi/auth/__init__.py": "",
    "ytmusicapi/auth/browser.py": '''
import json
from ytmusicapi.exceptions import YTMusicUserError

def setup_browser(filepath=None, headers_raw=None):
    headers = {}
    for line in headers_raw.split("\\n"):
        if ": " in line:
            k, v = line.split(": ", 1)
            headers[k.lower()] = v
    if "cookie" not in headers:
        raise YTMusicUserError("missing cookie")
    return json.dumps(headers)
''',
}


COOKIES = json.dumps([
    {"domain": ".youtube.com", "name": "__Secure-3PAPISID", "value": "p"},
    {"domain": ".youtube.com", "name": "SID", "value": "s"},
    {"domain": ".google.com", "name": "SID", "value": "g"},          # the same name, another value: not for music.youtube.com
    {"domain": ".google.com", "name": "__Secure-3PAPISID", "value": "gp"},
    {"domain": "music.youtube.com", "name": "SID", "value": "m"},   # a more specific cookie wins over .youtube.com's
    {"domain": ".other.com", "name": "x", "value": "y"},
])


def run(cmd, stdin, stub_dir, **extra):
    env = dict(os.environ, PYTHONPATH=str(stub_dir), OMP_TEST_COOKIES=COOKIES)
    env.update(extra)
    return subprocess.run([sys.executable, str(YTM), cmd], input=stdin, capture_output=True, text=True, env=env)


class OmpYtm(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        for name, body in STUB.items():
            p = Path(cls.tmp.name) / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text(body)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def sync(self):
        r = run("sync", '{"cookie": "ok"}', self.tmp.name)
        self.assertEqual(r.returncode, 0, r.stderr)
        return [json.loads(l) for l in r.stdout.splitlines()]

    def test_sync_lists_each_song_once_with_the_fields_omp_reads(self):
        tracks = [o for o in self.sync() if o["kind"] == "track"]
        self.assertEqual([t["path"] for t in tracks], ["ytm:aaa", "ytm:vid", "ytm:bbb", "ytm:ccc", "ytm:ddd"])
        one = tracks[0]
        self.assertEqual((one["title"], one["artist"], one["album_artist"], one["album"]), ("Liked One", "Zed, Yan", "Zed", "Gamma"))
        self.assertEqual(one["duration"], 200)
        self.assertIn("=w544-h544", one["cover_id"])                       # a bigger picture
        self.assertEqual(tracks[1]["album"], "YouTube Music")              # a video has no album
        self.assertNotIn("cover_id", tracks[1])
        album_song = tracks[3]
        self.assertEqual((album_song["album"], album_song["year"], album_song["track"]), ("Delta", 2019, 1))
        self.assertEqual(album_song["cover_id"], "https://i.ytimg.com/vi/ccc/hq.jpg")   # not a Google thumbnail: left alone

    def test_playlists_list_their_songs_and_skip_episodes(self):
        lists = {o["id"]: o for o in self.sync() if o["kind"] == "playlist"}
        self.assertEqual(sorted(lists), ["LM", "PL1"])
        self.assertEqual(lists["LM"]["name"], "Liked Music")
        self.assertEqual(lists["LM"]["paths"], ["ytm:aaa", "ytm:vid"])    # unavailable songs are left out
        self.assertEqual(lists["PL1"]["paths"], ["ytm:ddd", "ytm:bbb"])

    def test_progress_ends_at_the_total(self):
        progress = [o for o in self.sync() if o["kind"] == "progress"]
        self.assertEqual((progress[-1]["done"], progress[-1]["total"]), (3, 3))   # one album, two playlists

    def test_check_and_a_login_that_no_longer_works(self):
        ok = run("check", '{"cookie": "ok"}', self.tmp.name)
        self.assertEqual(json.loads(ok.stdout), {"ok": True, "name": "Amy"})
        bad = run("check", '{"cookie": "bad"}', self.tmp.name)
        self.assertEqual(bad.returncode, 2)
        self.assertEqual(json.loads(bad.stderr)["code"], "auth")

    def test_a_signed_out_page_counts_as_a_login_that_no_longer_works(self):
        r = run("sync", '{"cookie": "signedout"}', self.tmp.name)
        self.assertEqual(r.returncode, 2)
        self.assertEqual(json.loads(r.stderr)["code"], "auth")

    def test_a_browser_login_reads_the_cookies_fresh_and_only_youtube_and_google_ones(self):
        r = run("sync", "browser:brave:/home/ks/.config/BraveSoftware/Brave-Origin/Default", self.tmp.name, OMP_TEST_MODE="browser")
        self.assertEqual(r.returncode, 0, r.stderr)                              # the stub refuses cookies of other sites
        self.assertEqual(json.loads(run("check", "browser:brave:/x", self.tmp.name, OMP_TEST_MODE="browser").stdout), {"ok": True, "name": "Amy"})

    def test_a_browser_that_is_not_signed_in_counts_as_an_expired_login(self):
        only_other = json.dumps([{"domain": ".other.com", "name": "x", "value": "y"}])
        r = run("sync", "browser:brave:/x", self.tmp.name, OMP_TEST_COOKIES=only_other)
        self.assertEqual(r.returncode, 2)
        self.assertEqual(json.loads(r.stderr)["code"], "auth")
        only_google = json.dumps([{"domain": ".google.com", "name": "__Secure-3PAPISID", "value": "gp"}])   # YouTube never saw the login
        self.assertEqual(run("sync", "browser:brave:/x", self.tmp.name, OMP_TEST_COOKIES=only_google).returncode, 2)
        bad = run("sync", "browser:nosuch:/x", self.tmp.name)
        self.assertEqual(bad.returncode, 1)
        self.assertIn("cannot read the cookies of nosuch", json.loads(bad.stderr)["error"])

    def test_browsers_lists_the_profiles_that_have_cookies(self):
        with tempfile.TemporaryDirectory() as home:
            for rel in ("BraveSoftware/Brave-Origin/Default/Cookies", "BraveSoftware/Brave-Browser/Profile 1/Network/Cookies",
                        "google-chrome/Default/Network/Cookies"):
                f = Path(home, ".config", rel)
                f.parent.mkdir(parents=True)
                f.write_text("")
            Path(home, ".config", "chromium", "Default").mkdir(parents=True)             # no cookie file: skipped
            f = Path(home, ".mozilla", "firefox", "abc.default-release", "cookies.sqlite")
            f.parent.mkdir(parents=True)
            f.write_text("")
            r = run("browsers", "", self.tmp.name, HOME=home)
            self.assertEqual(r.returncode, 0, r.stderr)
            found = {o["label"]: o["spec"] for o in json.loads(r.stdout)}
            self.assertEqual(sorted(found), ["Brave Browser (Profile 1)", "Brave Origin", "Chrome", "Firefox"])
            self.assertEqual(found["Brave Origin"], "browser:brave:%s/.config/BraveSoftware/Brave-Origin/Default" % home)
            self.assertTrue(found["Firefox"].startswith("browser:firefox:") and found["Firefox"].endswith("abc.default-release"))

    def test_login_turns_a_pasted_curl_into_the_login(self):
        paste = "curl 'https://music.youtube.com/youtubei/v1/browse' \\\n  -H 'x-goog-authuser: 0' \\\n  -b 'SID=abc; HSID=def' \\\n  -H 'origin: https://music.youtube.com'"
        r = run("login", paste, self.tmp.name)
        self.assertEqual(r.returncode, 0, r.stderr)
        login = json.loads(r.stdout)
        self.assertEqual(login["cookie"], "SID=abc; HSID=def")             # -b becomes the cookie header
        self.assertEqual(login["x-goog-authuser"], "0")
        self.assertEqual(run("login", "curl 'https://x'", self.tmp.name).returncode, 2)   # no cookie in it

    def test_a_missing_module_is_reported(self):
        broken = tempfile.TemporaryDirectory()   # a module that will not import, whether or not the real one is installed
        Path(broken.name, "ytmusicapi").mkdir()
        Path(broken.name, "ytmusicapi", "__init__.py").write_text("raise ImportError('not installed')")
        env = dict(os.environ, PYTHONPATH=broken.name)
        r = subprocess.run([sys.executable, str(YTM), "check"], input="x", capture_output=True, text=True, env=env)
        self.assertEqual(r.returncode, 3)
        self.assertEqual(json.loads(r.stderr)["code"], "missing")


if __name__ == "__main__":
    unittest.main()
