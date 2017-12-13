#!/usr/bin/env python2.7
from blessed import Terminal
import argparse
import json
import requests
import sys
import threading
import time
import websocket

from collections import defaultdict

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


def main():
    print 'Loading albums'
    args = get_parser().parse_args()
    library = get_library(args.host)
    albums_by_artist = group_albums_by_artist(add_data_to_tracks(library['albums']))

    def save_and_play_playlist(paths, index):
        loadplaylist(args.host, "quick album", paths)
        first_track = get_playlist(args.host, "quick album")['items'][index]['id']
        playplaylist(args.host, "quick album", first_track)

    ui = UI(albums_by_artist, save_and_play_playlist)

    t = threading.Thread(target=websocket_worker, args=(args.host, ui.change_playing))
    #t.start()

    ui.run()



from album_widget import AlbumsWidget
from playlist_widget import PlaylistWidget

class UI(object):
    def __init__(self, albums_by_artist, save_and_play_playlist):
        self._playing = None

        self._album_widget = AlbumsWidget(albums_by_artist, self._add_track)
        self._playlist_widget = PlaylistWidget([], save_and_play_playlist)

        self._selected_widget = self._album_widget

        self._buf = []

    def _add_track(self, track):
        self._playlist_widget.add_track(track)

    #def _clear_buffer(self, width, height):
    #    self._buf_width = width
    #    self._buf_height = height
    #    self._buf = [' '] * (width * height)

    #def _get_print_func(self, t, x, y):
    #    def wrapped(xx, yy, s):
    #        assert '\n' not in s
    #        yyy = y + yy
    #        xxx = x + xx
    #        assert  0 <= xxx < self._buf_width
    #        assert  0 <= yyy < self._buf_height
    #        i = self._buf_width * yyy + xxx
    #        j = i + len(s)
    #        self._buf[i:j] = s
    #    return wrapped

    #def _get_buf(self):
    #     return ''.join(self._buf)

    def _get_print_func(self, t, x, y):
        def wrapped(xx, yy, s):
            assert '\n' not in s
            sys.stdout.write(t.move(y+yy,x+xx) + s)
            sys.stdout.flush()
        return wrapped

    def run(self):
        t = Terminal()
        first_line = 0
        selected = 0
        spacing = 3
        with t.fullscreen():
            while True:
                #self._clear_buffer(t.width, t.height)
                sys.stdout.write(t.clear() + t.hide_cursor)
                self._selected_widget.got_cursor()

                #print 'Playing: %s' % self._playing
                #print ''

                col_size = (t.width - spacing) / 2

                self._album_widget.draw(t, 0, 4, col_size, t.height - 4, self._get_print_func(t, 0, 4))

                self._playlist_widget.draw(t, col_size + spacing, 4, col_size, t.height - 4, self._get_print_func(t, col_size + spacing, 4))


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

                with t.cbreak():
                    key = t.inkey(timeout=0.1)
                    if repr(key) in ('KEY_RIGHT', 'KEY_LEFT'):
                        # Currently there are only two widgets, so this code is super minimal
                        self._selected_widget.lost_cursor()
                        if self._selected_widget == self._album_widget:
                            self._selected_widget = self._playlist_widget
                        else:
                            self._selected_widget = self._album_widget
                    else:
                        self._selected_widget.handle_key(key)



    def change_playing(self, x):
        self._playing = x


def chinput():
    term = Terminal()
    screen = {}

    def get_ch(timeout=0.1):
        key = term.inkey(timeout=timeout)
        if not key:
            return None
        if len(key) > 1:
            return key
        if ord(key) == 27:
            nextkey = term.inkey(timeout=0.0001)
            if not nextkey:
                # THIS IS REALLY SLOW
                return 'ESC'
            raise KeyError(repr(nextkey))
        return key

    with term.hidden_cursor(), \
            term.raw(), \
            term.location(), \
            term.fullscreen(), \
            term.keypad():
        inp = None
        while True:
            ch = get_ch()
            if ch:
                print ch
            #inp = term.inkey()

            #if ord(inp) == 27:
            #    print ord(inp)
            #    while 1:
            #        inp = term.inkey()
            #        print 'follow: %s' % ord(inp)

            #        # ctrl-up
            #        #27
            #        #  follow: 91
            #        #            follow: 49
            #        #                      follow: 59
            #        #                                follow: 53
            #        #                                          follow: 65

            #        # ctrl-down
            #        #27
            #        #  follow: 91
            #        #            follow: 49
            #        #                      follow: 59
            #        #                                follow: 53
            #        #                                          follow: 66



            #if len(inp) == 1:
            #    print ord(inp)
            #    continue
            #if inp.is_sequence:
            #    print 'is seq'
            #print repr(inp)
            #if inp == chr(3):
            #    # ^c exits
            #    break

            #elif inp == chr(19):
            #    save
            #    continue

            #elif inp == chr(12):
            #    # ^l refreshes
            #    redraw

            #else:
            #    n_csr = lookup_move(inp.code, csr, term)

            #if n_csr != csr:
            #    # erase old cursor,
            #    echo_yx(csr, screen.get((csr.y, csr.x), u' '))
            #    csr = n_csr

            #elif input_filter(inp):
            #    echo_yx(csr, inp)
            #    screen[(csr.y, csr.x)] = inp.__str__()
            #    n_csr = right_of(csr, 1)
            #    if n_csr == csr:
            #        # wrap around margin
            #        n_csr = home(below(csr, 1))
            #    csr = n_csr

if __name__ == '__main__':
    #chinput()
    main()

