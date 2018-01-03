import curses
import math

from unicode_utils import width as str_width


class PlaylistWidget(object):
    def __init__(self, parent):
        self._parent = parent

        self._selected_playlist = 'default'
        self._selected = 0

        self._first_displayed = 0
        self._has_cursor = False
        self._height = 10

    def draw(self, screen, x, y, width, height, nprint):
        self._height = height
        playing_id = self._parent._mc.get_current_track().get('id')
        tracks = self._parent._mc._playlists[self._selected_playlist]
        total_max_items_visible = height - 1

        tmp = max(self._selected - (total_max_items_visible-1), 0)
        if tmp > self._first_displayed:
            self._first_displayed = tmp
        if self._selected < self._first_displayed:
            self._first_displayed = self._selected

        nprint(0, 0, "Playlist: %s" % self._selected_playlist)
        for i, y in enumerate(xrange(1, height)):
            i += self._first_displayed
            if i >= len(tracks):
                break
            attr = 0
            if i == self._selected and self._has_cursor:
                attr = curses.A_REVERSE
            if tracks[i].get('id') == playing_id and playing_id:
                attr |= curses.color_pair(1)
            s = self._format_track(tracks[i], width)
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
        self._active_playlist = keys[i]

    def handle_key(self, key):
        tracks = self._parent._mc._playlists[self._selected_playlist]

        if key == 'up':
            self._selected = max(self._selected - 1, 0)
        elif key == 'page-up':
            self._selected = max(self._selected - self._height/2, 0)
        elif key == 'down':
            self._selected = min(self._selected + 1, len(tracks) - 1)
        elif key == 'ctrl-left':
            self.prev_playlist()
        elif key == 'ctrl-right':
            self.next_playlist()
        elif key == 'page-down':
            self._selected = min(self._selected + self._height/2, len(tracks) - 1)
        elif key == 'left':
            self._parent.set_focus(self._parent._album_widget)
        elif key == ord(' '):
            if self._selected < len(tracks):
                del tracks[self._selected]
                if self._selected >= len(tracks):
                    self._selected = max(len(tracks) - 1, 0)
        elif key == 'ctrl-up':
            if self._selected > 0:
                i = self._selected - 1
                j = self._selected 
                tracks[i], tracks[j] = tracks[j], tracks[i]
                self._selected -= 1
        elif key == 'ctrl-down':
            if self._selected < (len(tracks) - 1):
                i = self._selected
                j = self._selected + 1 
                tracks[i], tracks[j] = tracks[j], tracks[i]
                self._selected += 1
        elif key == ord('\n'):
            tracks = [(x['path'], x.get('id')) for x in tracks]
            self._parent._mc.save_and_play_playlist(self._selected_playlist, tracks, self._selected, 'immediate')
        elif key == ord('a'):
            tracks = [(x['path'], x.get('id')) for x in tracks]
            self._parent._mc.save_and_play_playlist(self._selected_playlist, tracks, self._selected, 'next')
        elif key == ord('s'):
            tracks = [(x['path'], x.get('id')) for x in tracks]
            self._parent._mc.save_playlist(self._selected_playlist, tracks)
        elif key == ord('n'):
            asdfasdf

    def got_cursor(self):
        self._has_cursor = True

    def lost_cursor(self):
        self._has_cursor = False

    #def add_track(self, track):
    #    self._parent._mc._playlists[self._active_playlist].append(track)

    #def new_playlist(self, playlist):
    #    if playlist not in self._parent._mc._playlists:
    #        self._parent._mc._playlists[playlist] = []
    #    self._active_playlist = playlist
