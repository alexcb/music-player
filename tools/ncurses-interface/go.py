#!/usr/bin/env python2.7
import argparse
import json
import requests
import sys
import threading
import time
import websocket
import os
import traceback
from log import log, log_duration


from collections import defaultdict

from key_mapping import get_ch, MOUSE_EVENT, KEY_EVENT
from spinner import get_random_spinner
from playlist import Playlist

import locale
import curses
import curses.ascii


def get_parser():
    parser = argparse.ArgumentParser(description='control me the hits')
    parser.add_argument('--host', default='localhost')
    return parser

def get_websocket_worker(host, cb):
    def on_message(ws, message):
        log('got websocket message: %s' % (message,))
        track = None
        playing = None
        try:
            status = json.loads(message)
            if status.get('type') == 'welcome':
                return
            playing = status['playing']
            del status['playing']
            if status:
                track = status
        except Exception as e:
            log(e)
            pass
            #text = 'unknown state: %s; %s' % (str(message), str(e))
        cb(playing, track)

    def on_close(ws):
        pass

    #websocket.enableTrace(True)
    ws = websocket.WebSocketApp('ws://%s/websocket' % host, on_message = on_message, on_close = on_close)
    wst = threading.Thread(target=ws.run_forever)
    wst.daemon = True
    wst.start()
    return ws.close

@log_duration
def get_library(host):
    r = requests.get('http://%s/library' % host)
    r.raise_for_status()
    return r.json()

@log_duration
def uploadplaylist(host, playlist_name, tracks):
    #raise ValueError(repr(tracks))
    r = requests.post('http://%s/playlists' % host, data=json.dumps({
        'name': playlist_name,
        'playlist': tracks,
        }))
    r.raise_for_status()

@log_duration
def get_playlists(host):
    r = requests.get('http://%s/playlists' % host)
    r.raise_for_status()
    return r.json()['playlists']

@log_duration
def playplaylist(host, playlist_name, track_num, when):
    url = 'http://%s/play?playlist=%s&track=%s&when=%s' % (host, playlist_name, track_num, when)
    r = requests.post(url)
    r.raise_for_status()

def strip_prefix(s, prefix):
    if s.startswith(prefix):
        return s[len(prefix):]
    return s

def group_albums_by_artist(albums):
    collection = defaultdict(list)
    for x in albums:
        collection[x['artist']].append(x)

    l = []
    for k, v in sorted(collection.items(), key=lambda x: strip_prefix(x[0].lower(), "the").strip()):
        l.append(sorted(v, key=lambda x: (x['year'], x['album'])))
    return l

def add_data_to_tracks(albums):
    for album in albums:
        for track in album['tracks']:
            track['artist'] = album['artist']
            track['album'] = album['album']
            track['year'] = album['year']
    return albums




from album_widget import AlbumsWidget
from playlist_widget import PlaylistWidget
from search_widget import SearchWidget

class UI(object):
    def __init__(self, screen, model_ctrl):
        self._screen = screen
        self._mc = model_ctrl

        self._album_widget = AlbumsWidget(self)
        self._playlist_widget = PlaylistWidget(self)
        self._search_widget = SearchWidget(self)

        self._selected_widget = None
        self.set_focus(self._album_widget)

        self._buf = []

    def _on_search(self, text):
        self._album_widget.filter(text)

    def _on_new_playlist(self, text):
        self._mc.new_playlist(text)
        self._search_widget.clear()
        self._playlist_widget._selected_playlist = text
        self.set_focus(self._playlist_widget)

    def _get_print_func(self, x, y):
        def wrapped(xx, yy, s, attr=None):
            assert '\n' not in s
            if isinstance(s, unicode):
                s = s.encode('utf8')
            args = [y+yy, x+xx, s]
            if attr is not None:
                args.append(attr)
            self._screen.addstr(*args)
        return wrapped

    def set_focus(self, widget):
        assert widget
        if widget != self._selected_widget:
            if self._selected_widget:
                self._selected_widget.lost_cursor()
            self._selected_widget = widget
            self._selected_widget.got_cursor()

    def run(self):
        first_line = 0
        selected = 0
        spacing = 3
        ch = None

        loading_spinner = get_random_spinner()

        while True:
            self._screen.clear()
            height, width = self._screen.getmaxyx()
            screenprint = self._get_print_func(0, 0)

            if not self._mc.ready():
                screenprint(0, 0, 'loading %s' % loading_spinner.get_ch())
                time.sleep(loading_spinner.get_speed())
            else:
                t = self._mc.get_current_track()
                if t:
                    if 'stream' in t:
                        track = t['stream']
                    else:
                        track = '%s - %s - %s' % (
                            t['artist'],
                            t['album'],
                            t['title'],
                            )
                    text = '[%(playing)s] %(track_text)s' % {
                        'playing': 'Playing' if self._mc._playing else 'Paused',
                        'track_text': track,
                        }
                else:
                    text = 'nothing'
                screenprint(0, 0, text)

                col1 = 0
                col1_width = width / 3
                col2 = col1_width + 2
                col2_width = width - col2
                bottom_row = height - 1
                main_height = height - 2

                self._album_widget.draw(   self._screen, col1, 1, col1_width, main_height, self._get_print_func(col1, 1))
                self._playlist_widget.draw(self._screen, col2, 1, col2_width, main_height, self._get_print_func(col2, 1))

                self._search_widget.draw(  self._screen,    0, bottom_row, width, 1, self._get_print_func(0, height - 1))

                #screen.addstr(0,0, "%s - %s" % (int(time.time()), ch))

            self._screen.refresh()

            ch = get_ch(self._screen)
            if ch is None:
                time.sleep(0.1)
                continue
            key_type, key = ch
            if key_type is MOUSE_EVENT:
                pass
            elif key_type is KEY_EVENT:
                if key == ord('/') and self._selected_widget != self._search_widget:
                    self._search_widget.set_mode('Search', self._on_search, None, self._album_widget)
                    self.set_focus(self._search_widget)
                elif key == ord('n') and self._selected_widget != self._search_widget:
                    self._search_widget.set_mode('New Playlist', None, self._on_new_playlist, self._album_widget)
                    self.set_focus(self._search_widget)
                elif key == ord('g') and self._selected_widget != self._search_widget:
                    self._playlist_widget.toggle_mode()
                elif key == 'ctrl-left':
                    self._playlist_widget.prev_playlist()
                elif key == 'ctrl-right':
                    self._playlist_widget.next_playlist()
                elif key == ord('p') and self._selected_widget != self._search_widget:
                    self._mc.toggle_pause()
                elif key == ord('q') and self._selected_widget != self._search_widget:
                    return
                else:
                    self._selected_widget.handle_key(key)


class ModelCtrl(object):
    def __init__(self, host):
        self._host = host
        self._playing = None
        self._playlists = None
        self._albums_by_artist = None
        self._active_playlist = 'default'
        self._current_track = None

    def ready(self):
        if self._playing is None:
            log("not ready 1")
            return False
        if self._albums_by_artist is None:
            log("not ready 2")
            return False
        if self._playlists is None:
            log("not ready 3")
            return False
        return True

    def refresh_library(self):
        library = get_library(self._host)
        self._albums_by_artist = group_albums_by_artist(add_data_to_tracks(library['albums']))

        self._path_lookup = {}
        for album in library['albums']:
            for track in album['tracks']:
                self._path_lookup[track['path']] = {
                    'path':         track['path'],
                    'artist':       album['artist'],
                    'album':        album['album'],
                    'year':         album['year'],
                    'title':        track['title'],
                    'length':       track['length'],
                    'track_number': track['track_number'],
                    }

    def refresh_playlists(self):
        playlists = {
            'default': Playlist(),
            }
        for k, v in get_playlists(self._host).iteritems():
            tracks = []
            for x in v['items']:
                if 'path' in x:
                    xx = self._path_lookup[x['path']].copy()
                    xx['id'] = x['id']
                    tracks.append(xx)
                elif 'stream' in x:
                    x['length'] = 999
                    x['track_number'] = 0
                    x['year'] = 0
                    x['album'] = ''
                    x['artist'] = x['stream']
                    x['path'] = x['stream']
                    tracks.append(x)
                else:
                    assert 0, 'unsupported playlist type'
            playlists[k] = Playlist(tracks, self.get_current_track_id())
        self._playlists = playlists

    def new_playlist(self, playlist):
        if playlist not in self._playlists:
            self._playlists[playlist] = Playlist()

    def change_playing(self, playing, track_data):
        log("change_playing: %s %s" % (playing, track_data))
        self._playing = playing
        self._current_track = track_data
        for p in self._playlists.itervalues():
            p.set_playing(track_data.get('id'))

    def get_track_meta(self, track_id):
        path = None
        for v in self._playlists.itervalues():
            for t in v._tracks:
                if t['id'] == track_id:
                    path = t['path']
        if path is None:
            raise KeyError(track_id)
        return self._path_lookup[path]

    def get_current_track(self):
        return self._current_track

    def get_current_track_id(self):
        if self._current_track is None:
            return None
        return self._current_track.get('id')

    def save_and_play_playlist(self, playlist_name, tracks, index, when):
        log("call to save_and_play_playlist")
        self.save_playlist(playlist_name, tracks)
        log("calling playplaylist")
        playplaylist(self._host, playlist_name, index, when)
        log("calling playplaylist done")

    def toggle_pause(self):
        r = requests.post('http://%s/pause' % self._host)
        r.raise_for_status()

    def save_playlist(self, playlist_name, tracks):
        uploadplaylist(self._host, playlist_name, tracks)
        self.refresh_playlists()

    def add_track(self, track, playlist_name):
        self._playlists[playlist_name].append(track)

def run(host):
    model_ctrl = ModelCtrl(host)

    def refresh():
        try:
            log('starting refresh')
            model_ctrl.refresh_library()
            model_ctrl.refresh_playlists()
            log('refresh done')
        except Exception as e:
            tb = traceback.format_exc()
            log(tb)
            def wrapped():
                raise RuntimeError(str(e)+str(tb))
            model_ctrl.ready = wrapped
    load_thread = threading.Thread(target=refresh)
    load_thread.start()

    # curses setup
    locale.setlocale(locale.LC_ALL,"")
    screen = curses.initscr()
    curses.noecho()
    curses.cbreak()
    curses.curs_set(0)

    screen.nodelay(1)

    #availmask, oldmask = curses.mousemask(1)
    #assert availmask == 1, "mouse mode not available"
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(1, curses.COLOR_RED, -1)

    ui = UI(screen, model_ctrl)
    model_ctrl.ui = ui
    ws_close = get_websocket_worker(host, model_ctrl.change_playing)

    ui.run()

    #ws_close() # This is too slow, so we're going to just let python kill the thread when we exit


if __name__ == '__main__':
    try:
        args = get_parser().parse_args()
        run(args.host)
    finally:
        try:
            curses.endwin()
        except:
            pass

