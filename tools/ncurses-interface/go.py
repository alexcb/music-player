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


def main():
    print 'Loading albums'
    args = get_parser().parse_args()
    library = get_library(args.host)
    albums_by_artist = group_albums_by_artist(library['albums'])

    def change_album(selected_path):
        tracks = [x['tracks'] for x in albums if x['path'] == selected_path][0]
        paths = [x['path'] for x in tracks]
        loadplaylist(args.host, "quick album", paths)
        first_track = get_playlist(args.host, "quick album")['items'][0]['id']
        playplaylist(args.host, "quick album", first_track)

    ui = UI(albums_by_artist, change_album)

    t = threading.Thread(target=websocket_worker, args=(args.host, ui.change_playing))
    #t.start()

    ui.run()

class UI(object):
    def __init__(self, albums_by_artist, change_album_func):
        self._albums_by_artist = albums_by_artist
        self._playing = None
        self._change_album = change_album_func
        self._expanded = set()

    def _is_artist_expanded(self, artist_index):
        return (artist_index,) in self._expanded

    def _is_album_expanded(self, artist_index, album_index):
        if not self._is_artist_expanded(artist_index):
            return false
        return (artist_index, album_index) in self._expanded

    def _get_lines(self, skip, n):
        for l in self._get_lines_helper():
            if skip > 0:
                skip -= 1
                continue
            yield l
            n -= 1
            if n == 0:
                break

    def _get_lines_helper(self):
        artist = 0
        album = 0
        track = None
        line = None
        for i, album_group in enumerate(self._albums_by_artist):
            show = self._is_artist_expanded(i)
            prefix = '-' if show else '+'
            key = (i,)
            yield key, '%s %s' % (prefix, album_group[0]['artist'])
            if not show:
                continue
            for j, album in enumerate(album_group):
                show = self._is_album_expanded(i, j)
                prefix = '-' if show else '+'
                key = (i, j)
                yield key, '  %s %s' % (prefix, album['album'])
                if not show:
                    continue
                for k, track in enumerate(album['tracks']):
                    key = (i, j, k)
                    yield key, '      %s ' % (track['title'], )


    def _toggle(self, key):
        if key in self._expanded:
            self._expanded.remove( key )
        else:
            self._expanded.add( key )
        self._lines = list(self._get_lines_helper())

    def _add_item(self, key):
        if len(key) == 1:
            albums = self._albums_by_artist[key[0]]
            raise ValueError(albums)

        if len(key) == 2:
            album = self._albums_by_artist[key[0]][key[1]]
            raise ValueError(album)

        if len(key) == 3:
            track = self._albums_by_artist[key[0]][key[1]]['tracks'][key[2]]
            raise ValueError(track)

    def run(self):
        self._lines = list(self._get_lines_helper())

        t = Terminal()
        first_line = 0
        selected = 0
        with t.fullscreen():
            while True:
                sys.stdout.write(t.clear() + t.move(0,0))

                print 'Playing: %s' % self._playing
                print ''

                num_rows = t.height - 4
                first_displayed_i = max(min(selected - 5, len(self._lines) - num_rows), 0)
                for y in xrange(0, num_rows):
                    i = y + first_displayed_i
                    if i == selected:
                        sys.stdout.write(t.reverse)
                    if i < len(self._lines):
                        key, s = self._lines[i]
                        print s
                    sys.stdout.write(t.normal)

                with t.cbreak():
                    x = t.inkey(timeout=0.1)
                    selected_item = self._lines[selected][0]
                    if repr(x) == 'KEY_UP':
                        selected = max(selected - 1, 0)
                    elif repr(x) == 'KEY_DOWN':
                        selected = min(selected + 1, len(self._lines) - 1)
                    elif repr(x) == 'KEY_ENTER':
                        if len(selected_item) < 3:
                            self._toggle(selected_item)
                    elif x == ' ':
                        self._add_item(selected_item)


                #first_displayed_i = max(min(selected - 5, len(self._albums) - num_rows), 0)
                #for y in xrange(0, num_rows):
                #    i = y + first_displayed_i
                #    if i == selected:
                #        sys.stdout.write(t.reverse)
                #    if i < len(self._albums):
                #        x = self._albums[i]
                #        s = '%s - %s' % (x['artist'], x['album'])
                #        print "%d. %s" % (y + first_displayed_i, s)
                #    sys.stdout.write(t.normal)
                #with t.cbreak():
                #    x = t.inkey(timeout=0.1)
                #    if repr(x) == 'KEY_UP':
                #        selected = max(selected - 1, 0)
                #    elif repr(x) == 'KEY_DOWN':
                #        selected = min(selected + 1, len(self._albums) - 1)
                #    elif repr(x) == 'KEY_ENTER':
                #        self._change_album(self._albums[selected]['path'])

    def change_playing(self, x):
        self._playing = x


if __name__ == '__main__':
    main()

