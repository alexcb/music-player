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

locale.setlocale(locale.LC_ALL,"")
screen = curses.initscr()
curses.noecho()
curses.cbreak()
curses.curs_set(0)

# this interupts special chars (althought might not work for ctrl-up?)
# stdscr.keypad(1)

# TODO add these before exiting
# curses.nocbreak(); stdscr.keypad(0); curses.echo()
# curses.endwin()

screen.nodelay(1)

curses.mousemask(1)
curses.start_color()
curses.use_default_colors()



#def chinput():
#    while 1:
#        tmp = get_ch()
#        if tmp is None:
#            time.sleep(0.1)
#            continue
#        if tmp[0] == key_mapping.MOUSE_EVENT:
#            print 'm', tmp[1]
#            continue
#        if tmp[0] == key_mapping.KEY_EVENT:
#            print 'k', tmp[1]
#            continue


def get_parser():
    parser = argparse.ArgumentParser(description='control me the hits')
    parser.add_argument('--host', default='localhost')
    return parser

def get_library(host):
    r = requests.get('http://%s/library' % host)
    r.raise_for_status()
    return r.json()

def websocket_worker(host, cb):
    while 1:
        try:
            ws = websocket.WebSocket()
            ws.connect("ws://%s/websocket" % host)
            while 1:
                status = json.loads(ws.recv())
                try:
                    text = '%s - %s' % (status['artist'], status['title'])
                except KeyError:
                    text = 'unknown state: %s' % str(status)
                cb(text)
        except Exception as e:
            pass
        print 'lost websocket connection'
        time.sleep(1)

def loadplaylist(host, playlist_name, tracks):
    print '---- loading ---'
    r = requests.post('http://%s/playlists' % host, data=json.dumps({
        'name': playlist_name,
        'playlist': tracks,
        }))
    print r.status_code
    print r.text
    r.raise_for_status()

def get_playlist(host, playlist_name):
    print '---- playing ---'
    r = requests.get('http://%s/playlists' % host)
    r.raise_for_status()
    playlists = {x['name']: x for x in r.json()['playlists']}
    return playlists[playlist_name]

def playplaylist(host, playlist_name, item_id):
    print '---- playing ---'
    print (playlist_name, item_id)
    r = requests.post('http://%s/play?playlist=%s&id=%s' % (host, playlist_name, item_id))
    print r.status_code
    print r.text
    r.raise_for_status()

def group_albums_by_artist(albums):
    collection = defaultdict(list)
    for x in albums:
        collection[x['artist']].append(x)
    return [x[1] for x in sorted(collection.items(), key=lambda x: x[0].lower())]

def add_data_to_tracks(albums):
    for album in albums:
        for track in album['tracks']:
            track['artist'] = album['artist']
            track['album'] = album['album']
    return albums


from album_widget import AlbumsWidget
from playlist_widget import PlaylistWidget
from search_widget import SearchWidget

class UI(object):
    def __init__(self, albums_by_artist, save_and_play_playlist):
        self._playing = None

        self._album_widget = AlbumsWidget(self, albums_by_artist, self._add_track)
        self._playlist_widget = PlaylistWidget(self, [], save_and_play_playlist)
        self._search_widget = SearchWidget(self, self._on_search)

        self._selected_widget = None
        self.set_focus(self._album_widget)

        self._buf = []

    def _on_search(self, text):
        self._album_widget.filter(text)

    def _add_track(self, track):
        self._playlist_widget.add_track(track)

    def _get_print_func(self, screen, x, y):
        def wrapped(xx, yy, s, attr=None):
            assert '\n' not in s
            if isinstance(s, unicode):
                s = s.encode('utf8')
            args = [y+yy, x+xx, s]
            if attr is not None:
                args.append(attr)
            screen.addstr(*args)
        return wrapped

    def set_focus(self, widget):
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
            screen.clear()
            height, width = screen.getmaxyx()
            col_size = (width - spacing) / 2

            self._album_widget.draw(screen, 0, 4, col_size, height - 4 - 1, self._get_print_func(screen, 0, 4))
            self._playlist_widget.draw(screen, col_size + spacing, 4 - 1, col_size, height - 4, self._get_print_func(screen, col_size + spacing, 4))
            self._search_widget.draw(screen, 0, height - 1, width, 1, self._get_print_func(screen, 0, height - 1))

            #screen.addstr(0,0, "%s - %s" % (int(time.time()), ch))
            screen.refresh()

            ch = get_ch(screen)
            if ch is None:
                time.sleep(0.1)
                continue
            key_type, key = ch
            if key_type is MOUSE_EVENT:
                asdf
            elif key_type is KEY_EVENT:
                if key == ord('/'):
                    self.set_focus(self._search_widget)
                else:
                    self._selected_widget.handle_key(key)

            #self._clear_buffer(t.width, t.height)
            #sys.stdout.write(t.clear() + t.hide_cursor)
            #self._selected_widget.got_cursor()

            ##print 'Playing: %s' % self._playing
            ##print ''




            #sys.stdout.write(t.clear() + t.move(0,0) + self._get_buf())
            #sys.stdout.flush()

            #num_rows = t.height - 4
            #first_displayed_i = max(min(selected - 5, len(self._lines) - num_rows), 0)
            #for y in xrange(0, num_rows):
            #    i = y + first_displayed_i
            #    if i == selected:
            #        sys.stdout.write(t.reverse)
            #    if i < len(self._lines):
            #        key, s = self._lines[i]
            #        print s
            #    sys.stdout.write(t.normal)

            #with t.cbreak():
            #    key = t.inkey(timeout=0.1)
            #    if repr(key) in ('KEY_RIGHT', 'KEY_LEFT'):
            #        # Currently there are only two widgets, so this code is super minimal
            #        self._selected_widget.lost_cursor()
            #        if self._selected_widget == self._album_widget:
            #            self._selected_widget = self._playlist_widget
            #        else:
            #            self._selected_widget = self._album_widget
            #    else:
            #        self._selected_widget.handle_key(key)



    def change_playing(self, x):
        self._playing = x

usecache = False

def main():
    print 'Loading albums'
    args = get_parser().parse_args()
    if not usecache:
        library = get_library(args.host)
        albums_by_artist = group_albums_by_artist(add_data_to_tracks(library['albums']))
        with open("tmp.cache", "w") as fp:
            fp.write(json.dumps(albums_by_artist))
    else:
        albums_by_artist = json.loads(open("tmp.cache", "r").read())

    def save_and_play_playlist(paths, index):
        loadplaylist(args.host, "quick album", paths)
        first_track = get_playlist(args.host, "quick album")['items'][index]['id']
        playplaylist(args.host, "quick album", first_track)

    ui = UI(albums_by_artist, save_and_play_playlist)

    t = threading.Thread(target=websocket_worker, args=(args.host, ui.change_playing))
    #t.start()

    ui.run()

if __name__ == '__main__':
    try:
        main()
    finally:
        curses.endwin()

