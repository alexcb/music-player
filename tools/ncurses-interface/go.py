#!/usr/bin/env python2.7
import argparse
import json
import requests
import sys
import threading
import time
import websocket

from collections import defaultdict

from key_mapping import get_ch, MOUSE_EVENT, KEY_EVENT

import locale
import curses
import curses.ascii



def get_parser():
    parser = argparse.ArgumentParser(description='control me the hits')
    parser.add_argument('--host', default='localhost')
    return parser

def get_library(host):
    r = requests.get('http://%s/library' % host)
    r.raise_for_status()
    return r.json()

def get_websocket_worker(host, cb):
    def on_message(ws, message):
        try:
            status = json.loads(message)
            track_id = None

            playing = 'Playing' if status['playing'] else 'Paused'
            text = '%s: %s - %s - %s - %s' % (playing, status['artist'], status['album'], status['track'], status['title'])
            track_id = status.get('id')

        except Exception as e:
            text = 'unknown state: %s; %s' % (str(message), str(e))

        cb(text, track_id)
    
    def on_close(ws):
        print "### closed ###"
    
    #websocket.enableTrace(True)
    ws = websocket.WebSocketApp('ws://%s/websocket' % host, on_message = on_message, on_close = on_close)
    wst = threading.Thread(target=ws.run_forever)
    wst.daemon = True
    wst.start()
    return ws.close

def uploadplaylist(host, playlist_name, tracks):
    #raise ValueError(repr(tracks))
    r = requests.post('http://%s/playlists' % host, data=json.dumps({
        'name': playlist_name,
        'playlist': tracks,
        }))
    r.raise_for_status()

def get_playlists(host):
    r = requests.get('http://%s/playlists' % host)
    r.raise_for_status()
    return r.json()['playlists']

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
    return [x[1] for x in sorted(collection.items(), key=lambda x: strip_prefix(x[0].lower(), "the").strip())]

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
    def __init__(self, screen, albums_by_artist, playlists, save_and_play_playlist, toggle_pause, save_playlist):
        self._screen = screen
        self._playing = ''

        self._toggle_pause = toggle_pause

        self._album_widget = AlbumsWidget(self, albums_by_artist, self._add_track)
        self._playlist_widget = PlaylistWidget(self, playlists, save_and_play_playlist, save_playlist)
        self._search_widget = SearchWidget(self)

        self._selected_widget = None
        self.set_focus(self._album_widget)

        self._buf = []

    def _on_search(self, text):
        self._album_widget.filter(text)

    def _on_new_playlist(self, text):
        self._playlist_widget.new_playlist(text)
        self._search_widget.clear()
        self.set_focus(self._playlist_widget)

    def _add_track(self, track):
        self._playlist_widget.add_track(track)

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
        while True:
            self._screen.clear()
            height, width = self._screen.getmaxyx()
            screenprint = self._get_print_func(0, 0)
            screenprint(0, 0, self._playing)

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
                elif key == ord('p') and self._selected_widget != self._search_widget:
                    self._toggle_pause()
                elif key == ord('q') and self._selected_widget != self._search_widget:
                    return
                else:
                    self._selected_widget.handle_key(key)

    def change_playing(self, x, track_id):
        self._playing = x
        self._playlist_widget.set_playing(track_id)
        with open("tmp", "w") as fp:
            fp.write(repr(x))
            fp.write(repr(track_id))

class MyMain(object):
    def __init__(self, host):
        self._host = host
        self._playlists = {}

    def _refresh_library(self):
        library = get_library(self._host)
        self._albums_by_artist = group_albums_by_artist(add_data_to_tracks(library['albums']))

        self._path_lookup = {}
        for album in library['albums']:
            for track in album['tracks']:
                self._path_lookup[track['path']] = track

    def _refresh_playlists(self):
        playlists = get_playlists(self._host)

        converted_playlists = {}
        for k,v in playlists.iteritems():
            converted = []
            for t in v['items']:
                tt = self._path_lookup[t['path']]
                t['album'] = tt['album']
                t['artist'] = tt['artist']
                t['title'] = tt['title']
                t['track_number'] = tt['track_number']
                t['year'] = tt['year']
                t['length'] = tt['length']
                converted.append(t)
            converted_playlists[k] = converted

        # ensure the same dict is kept, as its passed by ref
        self._playlists.clear()
        self._playlists['default'] = []
        self._playlists.update(converted_playlists)

    def save_and_play_playlist(self, playlist_name, tracks, index, when):
        self.save_playlist(playlist_name, tracks)
        playplaylist(self._host, playlist_name, index, when)

    def toggle_pause(self):
        r = requests.post('http://%s/pause' % self._host)
        r.raise_for_status()

    def save_playlist(self, playlist_name, tracks):
        uploadplaylist(self._host, playlist_name, tracks)
        self._refresh_playlists()

    def run(self):
        self._refresh_library()
        self._refresh_playlists()


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


        ui = UI(screen, self._albums_by_artist, self._playlists, self.save_and_play_playlist, self.toggle_pause, self.save_playlist)

        ws_close = get_websocket_worker(self._host, ui.change_playing)

        ui.run()
        #ws_close() # This is too slow, so we're going to just let python kill the thread when we exit

def main():
    args = get_parser().parse_args()
    mm = MyMain(args.host)
    mm.run()

if __name__ == '__main__':
    try:
        main()
    finally:
        try:
            curses.endwin()
        except:
            pass

