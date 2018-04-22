import curses
import math

from unicode_utils import width as str_width
from log import log, log_duration

track_mode = object()
album_mode = object()

def clamp_range(x, min, max):
    if x < min:
        return min
    if x >= max:
        return max - 1
    return x

class PlaylistWidget(object):
    def __init__(self, parent):
        self._parent = parent

        self._selected_playlist = 'default'
        self._selected = 0

        self._first_displayed = 0
        self._has_cursor = False
        self._height = 10

        self._mode = track_mode

    def draw(self, screen, x, y, width, height, nprint):
        self._height = height
        current_track = self._parent._mc.get_current_track()
        if current_track:
            playing_id = self._parent._mc.get_current_track().get('id')
        else:
            playing_id = None
        playlist = self._parent._mc._playlists[self._selected_playlist]

        total_max_items_visible = height - 1

        tmp = max(self._selected - (total_max_items_visible-1), 0)
        if tmp > self._first_displayed:
            self._first_displayed = tmp
        if self._selected < self._first_displayed:
            self._first_displayed = self._selected

        playlist_num = sorted(self._parent._mc._playlists.keys()).index(self._selected_playlist) + 1
        total_playlists = len(self._parent._mc._playlists.keys())
        nprint(0, 0, "Playlist [%d/%d]: %s" % (playlist_num, total_playlists, self._selected_playlist))

        if self._mode == track_mode:
            tracks = playlist._tracks
            playing_index = playlist.get_playing_track_index()
            for i, y in enumerate(xrange(1, height)):
                i += self._first_displayed
                if i >= len(tracks):
                    break
                attr = 0
                if i == self._selected and self._has_cursor:
                    attr = curses.A_REVERSE
                if i == playing_index:
                    attr |= curses.color_pair(1)
                s = self._format_track(tracks[i], width)
                nprint(0, y, s, attr)
                #sys.stdout.write(terminal.normal)
        elif self._mode == album_mode:
            albums = playlist._albums
            playing_index = playlist.get_playing_album_index()
            for i, y in enumerate(xrange(1, height)):
                i += self._first_displayed
                if i >= len(albums):
                    break
                album = albums[i]
                attr = 0
                if i == self._selected and self._has_cursor:
                    attr = curses.A_REVERSE
                if i == playing_index:
                    attr |= curses.color_pair(1)
                s = self._format_album(album, width)
                nprint(0, y, s, attr)
                #sys.stdout.write(terminal.normal)

    def _format_track(self, track, width):
        num_col = 6
        year_width = 4
        track_num_width = 2
        duration_width = 5
        spacer = '  '
        remaining_width = width - year_width - track_num_width - duration_width - (num_col-1)*len(spacer)
        flex_width = remaining_width / 3

        def format(s, w):
            if not isinstance(s, unicode):
                s = unicode(s)
            width = str_width(s)
            if width > w:
                s = s[:(w-3)] + '...'
            else:
                pad = w - width
                s = s + ' ' * pad

            return s

        if 'stream' in track:
            track['artist'] = track['stream']
            track['album'] = '???'
            track['track_number'] = 0
            track['title'] = '???'
            track['year'] = 0
            track['length'] = 999

        if 'length' not in track:
            track['album'] = '???'
            track['track_number'] = 0
            track['title'] = '???'
            track['year'] = 0
            track['length'] = 999

        duration_min = math.floor(track['length'] / 60)
        duration_sec = math.floor(track['length'] - 60*duration_min)
        duration = '%d:%02d' % (duration_min, duration_sec)

        text = [
            format(track['artist'],       flex_width              ),
            format(track['album'],        flex_width              ),
            format(track['track_number'], track_num_width         ),
            format(track['title'],        flex_width              ),
            format(duration,              duration_width          ),
            format(track['year'],         year_width              ),
            ]

        return spacer.join(text)

    def _format_album(self, album, width):
        num_col = 4
        year_width = 4
        duration_width = 5
        spacer = '  '
        remaining_width = width - year_width - duration_width - (num_col-1)*len(spacer)
        flex_width = remaining_width / 2

        def format(s, w):
            if not isinstance(s, unicode):
                s = unicode(s)
            width = str_width(s)
            if width > w:
                s = s[:(w-3)] + '...'
            else:
                pad = w - width
                s = s + ' ' * pad

            return s

        duration_min = math.floor(album['length'] / 60)
        duration_sec = math.floor(album['length'] - 60*duration_min)
        duration = '%d:%02d' % (duration_min, duration_sec)

        text = [
            format(album['artist'], flex_width    ),
            format(album['album'],  flex_width    ),
            format(duration,        duration_width),
            format(album['year'],   year_width    ),
            ]

        return spacer.join(text)

    def next_playlist(self):
        self.change_playlist_rel(1)

    def prev_playlist(self):
        self.change_playlist_rel(-1)

    def change_playlist_rel(self, relative):
        keys = sorted(self._parent._mc._playlists.keys())
        i = keys.index(self._selected_playlist)
        i += relative
        if i >= len(keys):
            i = 0
        if i < 0:
            i = len(keys) - 1
        self._selected_playlist = keys[i]

    def _move_selected(self, direction):
        playlist = self._parent._mc._playlists[self._selected_playlist]

        if self._mode == track_mode:
            playlist.move_track(self._selected, direction)
            self._selected += direction
            self._selected = clamp_range(self._selected, 0, len(playlist._tracks))
        if self._mode == album_mode:
            playlist.move_album(self._selected, direction)
            self._selected += direction
            self._selected = clamp_range(self._selected, 0, len(playlist._albums))

    def switch_albums(self, i, j):
        pass

    def _remove_selected(self):
        playlist = self._parent._mc._playlists[self._selected_playlist]

        if self._mode == track_mode:
            playlist.remove_track(self._selected)
        if self._mode == album_mode:
            playlist.remove_album(self._selected)

    def handle_key(self, key):
        tracks = self._parent._mc._playlists[self._selected_playlist]._tracks
        albums = self._parent._mc._playlists[self._selected_playlist]._albums

        num_items = None
        if self._mode == track_mode:
            num_items = len(tracks)
        if self._mode == album_mode:
            num_items = len(albums)

        if key == 'up':
            self._selected = clamp_range(self._selected - 1, 0, num_items)
        elif key == 'page-up':
            self._selected = clamp_range(self._selected - self._height/2, 0, num_items)
        elif key == 'down':
            self._selected = clamp_range(self._selected + 1, 0, num_items)
        elif key == 'ctrl-left':
            self.prev_playlist()
        elif key == 'ctrl-right' or key == ord('j'):
            self.next_playlist()
        elif key == 'page-down':
            self._selected = clamp_range(self._selected + self._height/2, 0, num_items)
        elif key == 'left':
            self._parent.set_focus(self._parent._album_widget)
        elif key == ord(' '):
            self._remove_selected()
        elif key == 'ctrl-up':
            self._move_selected(-1)
        elif key == 'ctrl-down':
            self._move_selected(1)
        elif key == ord('\n'):
            track_index = self._selected if self._mode == track_mode else albums[self._selected]['start']
            tracks = [(x['path'], x.get('id')) for x in tracks]
            self._parent._mc.save_and_play_playlist(self._selected_playlist, tracks, track_index, 'immediate')
        elif key == ord('a'):
            tracks = [(x['path'], x.get('id')) for x in tracks]
            if self._mode == track_mode:
                self._parent._mc.save_and_play_playlist(self._selected_playlist, tracks, self._selected, 'next')
            else:
                assert 0, "playing an album is not supported -- must be in track mode"
        elif key == ord('s'):
            tracks = [(x['path'], x.get('id')) for x in tracks]
            self._parent._mc.save_playlist(self._selected_playlist, tracks)
        elif key == ord('g'):
            self.toggle_mode()
        elif key == ord('n'):
            asdfasdf

    def toggle_mode(self):
        playlist = self._parent._mc._playlists[self._selected_playlist]
        if self._mode == track_mode:
            self._mode = album_mode
            self._selected = playlist.get_corresponding_album_index(self._selected) or 0
            self._first_displayed = self._selected
        else:
            self._mode = track_mode
            self._selected = playlist.get_corresponding_track_index(self._selected) or 0
            self._first_displayed = self._selected

    def got_cursor(self):
        self._has_cursor = True

    def lost_cursor(self):
        self._has_cursor = False

