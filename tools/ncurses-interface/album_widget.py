import sys
import curses
import re

char_re = re.compile('[^0-9a-zA-Z]+')

class AlbumsWidget(object):
    def __init__(self, parent):
        self._parent = parent

        self._expanded = set()
        self._selected = 0
        self._first_displayed = 0
        self._has_cursor = False
        self._filter = ''
        self._height = 10

        self._lines = None


    def _is_artist_expanded(self, artist_index):
        return (artist_index,) in self._expanded

    def _is_album_expanded(self, artist_index, album_index):
        if not self._is_artist_expanded(artist_index):
            return false
        return (artist_index, album_index) in self._expanded

    def _albums_by_artist(self):
        return self._parent._mc._albums_by_artist

    def _get_lines_helper(self):
        artist = 0
        album = 0
        track = None
        line = None
        text_filter = self.strip_chars(self._filter.lower())
        for i, album_group in enumerate(self._albums_by_artist()):
            show = self._is_artist_expanded(i)
            prefix = '-' if show else '+'
            key = (i,)
            if text_filter not in self.strip_chars(album_group[0]['artist'].lower()):
                continue
            yield key, '%s %s' % (prefix, album_group[0]['artist'])
            if not show:
                continue
            for j, album in enumerate(album_group):
                show = self._is_album_expanded(i, j)
                prefix = '-' if show else '+'
                key = (i, j)
                yield key, '  %s %s (%s)' % (prefix, album['album'], album['year'])
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
        playlist_name = self._parent._playlist_widget._selected_playlist
        if len(key) == 1:
            albums = self._albums_by_artist()[key[0]]
            for album in albums:
                for track in album['tracks']:
                    self._parent._mc.add_track(track, playlist_name)

        if len(key) == 2:
            album = self._albums_by_artist()[key[0]][key[1]]
            for track in album['tracks']:
                self._parent._mc.add_track(track, playlist_name)

        if len(key) == 3:
            track = self._albums_by_artist()[key[0]][key[1]]['tracks'][key[2]]
            self._parent._mc.add_track(track, playlist_name)

    def draw(self, screen, xx, yy, width, height, nprint):
        if self._lines is None:
            self._lines = list(self._get_lines_helper())

        self._height = height
        total_max_items_visible = height - 1

        tmp = max(self._selected - (total_max_items_visible-1), 0)
        if tmp > self._first_displayed:
            self._first_displayed = tmp
        if self._selected < self._first_displayed:
            self._first_displayed = self._selected

        nprint(0, 0, "Albums:")
        for i, y in enumerate(xrange(1, height)):
            i += self._first_displayed
            attr = None
            if i == self._selected and self._has_cursor:
                attr = curses.A_REVERSE
            if i < len(self._lines):
                key, s = self._lines[i]
                nprint(0, y, s, attr)
            #sys.stdout.write(terminal.normal)

    def strip_chars(self, text):
        return char_re.sub('', text)

    def filter(self, text):
        self._filter = text
        self._lines = list(self._get_lines_helper())
        self._selected = 0


    def handle_key(self, key):
        if self._lines is None:
            return
        if self._selected < len(self._lines):
            selected_item = self._lines[self._selected][0]
        else:
            selected_item = None
        if key == 'up':
            self._selected = max(self._selected - 1, 0)
        elif key == 'page-up':
            self._selected = max(self._selected - self._height/2, 0)
        elif key == 'down':
            self._selected = min(self._selected + 1, len(self._lines) - 1)
        elif key == 'page-down':
            self._selected = min(self._selected + self._height/2, len(self._lines) - 1)
        if key == 'right':
            self._parent.set_focus(self._parent._playlist_widget)
        elif key == ord('\n'):
            if len(selected_item) < 3:
                self._toggle(selected_item)
        elif key == ord(' '):
            self._add_item(selected_item)

    def got_cursor(self):
        self._has_cursor = True

    def lost_cursor(self):
        self._has_cursor = False
