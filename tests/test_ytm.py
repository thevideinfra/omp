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
import json, os
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
        return {"title": "Delta", "year": "2019", "audioPlaylistId": "OLAK5_delta", "tracks": [
            {"videoId": "ccc", "title": "Album Song", "artists": [{"name": "Wen"}], "duration_seconds": 61, "trackNumber": 1,
             "thumbnails": [{"url": "https://i.ytimg.com/vi/ccc/hq.jpg"}]}]}
    def search(self, query, filter=None, limit=20, ignore_spelling=False):
        if filter == "songs":
            return [{"videoId": "s1", "title": "Found %s %d" % (query, limit), "artists": [{"name": "Zed"}], "album": {"name": "Gamma", "id": "MPREb_g"},
                     "duration_seconds": 215, "thumbnails": [{"url": "https://lh3.googleusercontent.com/s=w60-h60-l90-rj"}]},
                    {"videoId": None, "title": "Gone", "artists": []}]
        return [{"browseId": "MPREb_g", "title": "Gamma", "artists": [{"name": "Zed"}], "year": "2019", "type": "Album",
                 "thumbnails": [{"url": "https://lh3.googleusercontent.com/a=w60-h60-l90-rj"}]},
                {"browseId": "MPREb_s", "title": "Tiny", "artists": [{"name": "Yan"}], "year": None, "type": "Single", "thumbnails": []}]
    def _log(self, name, *args):
        if os.environ.get("OMP_TEST_LOG"):
            with open(os.environ["OMP_TEST_LOG"], "a") as f:
                f.write(json.dumps([name, *args]) + "\\n")
    def create_playlist(self, title, description, privacy_status="PRIVATE", video_ids=None, source_playlist=None):
        self._log("create_playlist", title, video_ids)
        return "PLnew"
    def add_playlist_items(self, playlistId, videoIds=None, source_playlist=None, duplicates=False):
        self._log("add_playlist_items", playlistId, videoIds, duplicates)
        if "FAIL" in playlistId:
            return {"status": "STATUS_FAILED", "playlistEditResults": []}
        return {"status": "STATUS_SUCCEEDED", "playlistEditResults": [
            {"playlistEditVideoAddedResultData": {"videoId": v, "setVideoId": "set-" + v}} for v in videoIds]}
    def remove_playlist_items(self, playlistId, videos):
        self._log("remove_playlist_items", playlistId, videos)
        return "STATUS_SUCCEEDED"
    def edit_playlist(self, playlistId, title=None, description=None, privacyStatus=None, moveItem=None, addPlaylistId=None, addToTop=None):
        self._log("edit_playlist", playlistId, title)
        return "STATUS_SUCCEEDED"
    def delete_playlist(self, playlistId):
        self._log("delete_playlist", playlistId)
        return "STATUS_SUCCEEDED"
    def rate_song(self, videoId, rating="INDIFFERENT"):
        self._log("rate_song", videoId, rating)
        return {}
    def rate_playlist(self, playlistId, rating="INDIFFERENT"):
        self._log("rate_playlist", playlistId, rating)
        return {}
    def get_library_playlists(self, limit=None):
        return [{"playlistId": "LM", "title": "Liked Music"}, {"playlistId": "SE", "title": "Episodes for Later"},
                {"playlistId": "PL1", "title": "Mix"}, {"playlistId": "PLsaved", "title": "Saved from someone"}]
    def get_playlist(self, playlist_id, limit=None):
        if playlist_id == "PL1":
            return {"owned": True, "tracks": [{"videoId": "ddd", "setVideoId": "sv1", "title": "Only In Playlist", "artists": [{"name": "Vic"}], "album": None},
                                              {"videoId": "bbb", "setVideoId": "sv2", "title": "Library Two", "artists": [{"name": "Zed"}]}]}
        if playlist_id == "PLnew":
            return {"owned": True, "tracks": [{"videoId": "x1", "setVideoId": "n1", "title": "N", "artists": []}, {"videoId": "x2", "setVideoId": "n2", "title": "M", "artists": []}]}
        return {"owned": False, "tracks": []}
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
        self.assertEqual(sorted(lists), ["LM", "PL1", "PLsaved"])
        self.assertEqual(lists["LM"]["name"], "Liked Music")
        self.assertEqual(lists["LM"]["paths"], ["ytm:aaa", "ytm:vid"])    # unavailable songs are left out
        self.assertEqual(lists["PL1"]["paths"], ["ytm:ddd", "ytm:bbb"])
        self.assertEqual(lists["PL1"]["set_ids"], ["sv1", "sv2"])                  # to take a song out of the playlist later
        self.assertTrue(lists["PL1"]["editable"])
        self.assertFalse(lists["LM"]["editable"])                                  # liked songs are changed by liking
        self.assertFalse(lists["PLsaved"]["editable"])                             # someone else's playlist

    def test_progress_ends_at_the_total(self):
        progress = [o for o in self.sync() if o["kind"] == "progress"]
        self.assertEqual((progress[-1]["done"], progress[-1]["total"]), (4, 4))   # one album, three playlists

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

    def test_the_songs_in_liked_music_are_marked_liked(self):
        tracks = {o["path"]: o for o in self.sync() if o["kind"] == "track"}
        self.assertTrue(tracks["ytm:aaa"].get("starred"))
        self.assertTrue(tracks["ytm:vid"].get("starred"))
        self.assertFalse(tracks["ytm:bbb"].get("starred", False))                  # only in the library

    def edit(self, *args):
        log = os.path.join(self.tmp.name, "calls.log")
        if os.path.exists(log):
            os.remove(log)
        r = subprocess.run([sys.executable, str(YTM), *args], input='{"cookie": "ok"}', capture_output=True, text=True,
                           env=dict(os.environ, PYTHONPATH=str(self.tmp.name), OMP_TEST_LOG=log))
        calls = [json.loads(l) for l in open(log)] if os.path.exists(log) else []
        return r, calls

    def test_a_new_playlist_comes_back_with_its_id_and_the_ids_of_its_items(self):
        r, calls = self.edit("playlist-create", "Road trip", "x1", "x2")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(json.loads(r.stdout), {"ok": True, "id": "PLnew", "set_ids": {"x1": "n1", "x2": "n2"}})
        self.assertEqual(calls, [["create_playlist", "Road trip", ["x1", "x2"]]])
        r, calls = self.edit("playlist-create", "Empty")
        self.assertEqual(json.loads(r.stdout), {"ok": True, "id": "PLnew", "set_ids": {}})
        self.assertEqual(calls, [["create_playlist", "Empty", None]])

    def test_songs_are_added_to_and_removed_from_a_playlist(self):
        r, calls = self.edit("playlist-add", "PL1", "a1", "a2")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(json.loads(r.stdout), {"ok": True, "set_ids": {"a1": "set-a1", "a2": "set-a2"}})
        self.assertEqual(calls, [["add_playlist_items", "PL1", ["a1", "a2"], False]])
        bad, _ = self.edit("playlist-add", "PLFAIL", "a1")
        self.assertEqual(bad.returncode, 1)
        r, calls = self.edit("playlist-remove", "PL1", "ddd/sv1", "bbb/sv2")
        self.assertEqual(json.loads(r.stdout), {"ok": True})
        self.assertEqual(calls, [["remove_playlist_items", "PL1", [{"videoId": "ddd", "setVideoId": "sv1"}, {"videoId": "bbb", "setVideoId": "sv2"}]]])

    def test_a_playlist_is_renamed_and_deleted(self):
        r, calls = self.edit("playlist-rename", "PL1", "New name")
        self.assertEqual((r.returncode, calls), (0, [["edit_playlist", "PL1", "New name"]]))
        r, calls = self.edit("playlist-delete", "PL1")
        self.assertEqual((r.returncode, calls), (0, [["delete_playlist", "PL1"]]))

    def test_songs_are_liked_and_unliked_and_an_album_goes_into_the_library(self):
        _, calls = self.edit("like", "a1", "a2")
        self.assertEqual(calls, [["rate_song", "a1", "LIKE"], ["rate_song", "a2", "LIKE"]])
        _, calls = self.edit("unlike", "a1")
        self.assertEqual(calls, [["rate_song", "a1", "INDIFFERENT"]])
        r, calls = self.edit("like-album", "MPREb_1")
        self.assertEqual((r.returncode, calls), (0, [["rate_playlist", "OLAK5_delta", "LIKE"]]))   # an album is its audio playlist

    def test_search_lists_songs_and_albums_with_what_omp_needs(self):
        r = subprocess.run([sys.executable, str(YTM), "search", "gamma ray"], input='{"cookie": "ok"}', capture_output=True, text=True,
                           env=dict(os.environ, PYTHONPATH=str(self.tmp.name)))
        self.assertEqual(r.returncode, 0, r.stderr)
        out = [json.loads(l) for l in r.stdout.splitlines()]
        songs = [o for o in out if o["kind"] == "track"]
        albums = [o for o in out if o["kind"] == "album"]
        self.assertEqual([t["path"] for t in songs], ["ytm:s1"])                  # a song with no id is left out
        self.assertEqual((songs[0]["title"], songs[0]["album"], songs[0]["duration"]), ("Found gamma ray 40", "Gamma", 215))
        self.assertIn("=w544-h544", songs[0]["cover_id"])
        self.assertEqual(songs[0]["album_id"], "MPREb_g")                          # to play the whole album later
        self.assertEqual([a["id"] for a in albums], ["MPREb_g", "MPREb_s"])
        self.assertEqual((albums[0]["title"], albums[0]["artist"], albums[0]["year"], albums[0]["type"]), ("Gamma", "Zed", 2019, "Album"))
        self.assertIn("=w544-h544", albums[0]["cover_id"])
        self.assertNotIn("cover_id", albums[1])
        self.assertEqual(albums[1]["year"], 0)

    def test_search_takes_how_many_results_to_ask_for(self):
        r = subprocess.run([sys.executable, str(YTM), "search", "x", "120"], input='{"cookie": "ok"}', capture_output=True, text=True,
                           env=dict(os.environ, PYTHONPATH=str(self.tmp.name)))
        self.assertEqual(json.loads(r.stdout.splitlines()[0])["title"], "Found x 120")
        bad = subprocess.run([sys.executable, str(YTM), "search", "x", "lots"], input='{"cookie": "ok"}', capture_output=True, text=True,
                             env=dict(os.environ, PYTHONPATH=str(self.tmp.name)))
        self.assertEqual(json.loads(bad.stdout.splitlines()[0])["title"], "Found x 40")      # not a number: the default

    def test_an_empty_search_is_refused_and_an_album_lists_its_songs(self):
        env = dict(os.environ, PYTHONPATH=str(self.tmp.name))
        r = subprocess.run([sys.executable, str(YTM), "search", "  "], input='{"cookie": "ok"}', capture_output=True, text=True, env=env)
        self.assertEqual(r.returncode, 1)
        a = subprocess.run([sys.executable, str(YTM), "album", "MPREb_1"], input='{"cookie": "ok"}', capture_output=True, text=True, env=env)
        self.assertEqual(a.returncode, 0, a.stderr)
        tracks = [json.loads(l) for l in a.stdout.splitlines()]
        self.assertEqual([(t["path"], t["album"], t["year"], t["track"]) for t in tracks], [("ytm:ccc", "Delta", 2019, 1)])
        self.assertEqual(tracks[0]["album_id"], "MPREb_1")

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
