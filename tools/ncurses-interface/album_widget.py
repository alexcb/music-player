import sys
import curses


class AlbumsWidget(object):
    def __init__(self, parent, albums_by_artist, add_track_func):
        self._parent = parent
        self._albums_by_artist = albums_by_artist
        self._playing = None
        self._expanded = set()
        self._selected = 0
        self._has_cursor = False
        self._filter = ''

        self._lines = list(self._get_lines_helper())
        self._add_track = add_track_func


    def _is_artist_expanded(self, artist_index):
        return (artist_index,) in self._expanded

    def _is_album_expanded(self, artist_index, album_index):
        if not self._is_artist_expanded(artist_index):
            return false
        return (artist_index, album_index) in self._expanded

    def _get_lines_helper(self):
        artist = 0
        album = 0
        track = None
        line = None
        for i, album_group in enumerate(self._albums_by_artist):
            show = self._is_artist_expanded(i)
            prefix = '-' if show else '+'
            key = (i,)
            if self._filter.lower() not in album_group[0]['artist'].lower():
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
        if len(key) == 1:
            albums = self._albums_by_artist[key[0]]
            for album in albums:
                for track in album['tracks']:
                    self._add_track(track)

        if len(key) == 2:
            album = self._albums_by_artist[key[0]][key[1]]
            for track in album['tracks']:
                self._add_track(track)

        if len(key) == 3:
            track = self._albums_by_artist[key[0]][key[1]]['tracks'][key[2]]
            self._add_track(track)

    def draw(self, screen, xx, yy, width, height, nprint):
        first_displayed_i = max(min(self._selected - 5, len(self._lines) - height), 0)
        for y in xrange(0, height):
            i = y + first_displayed_i
            attr = None
            if i == self._selected and self._has_cursor:
                attr = curses.A_REVERSE
            if i < len(self._lines):
                key, s = self._lines[i]
                nprint(0, y, s, attr)
            #sys.stdout.write(terminal.normal)

    def filter(self, text):
        self._filter = text
        self._lines = list(self._get_lines_helper())


    def handle_key(self, key):
        if self._selected < len(self._lines):
            selected_item = self._lines[self._selected][0]
        else:
            selected_item = None
        if key == 'up':
            self._selected = max(self._selected - 1, 0)
        elif key == 'page-up':
            self._selected = max(self._selected - 10, 0)
        elif key == 'down':
            self._selected = min(self._selected + 1, len(self._lines) - 1)
        elif key == 'page-down':
            self._selected = min(self._selected + 10, len(self._lines) - 1)
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
