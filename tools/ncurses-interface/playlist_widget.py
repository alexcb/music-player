import curses


class PlaylistWidget(object):
    def __init__(self, parent, tracks, save_and_play_playlist):
        self._parent = parent
        self._tracks = tracks
        self._selected = 0
        self._has_cursor = False
        self._save_and_play_playlist = save_and_play_playlist

    def draw(self, screen, x, y, width, height, nprint):
        first_displayed_i = max(min(self._selected - 5, len(self._tracks) - height), 0)
        for y in xrange(0, height):
            i = y + first_displayed_i
            attr = None
            if i == self._selected and self._has_cursor:
                attr = curses.A_REVERSE
            if i < len(self._tracks):
                s = self._format_track(self._tracks[i])
                nprint(0, y, s, attr)
            #sys.stdout.write(terminal.normal)

    def _format_track(self, track):
        return '%s - %s - %s - %s - %s' % (track['artist'], track['album'], track['track_number'], track['title'], track['year'])

    def handle_key(self, key):
        try:
            selected_item = self._tracks[self._selected]
        except IndexError:
            return

        if key == 'up':
            self._selected = max(self._selected - 1, 0)
        elif key == 'page-up':
            self._selected = max(self._selected - 10, 0)
        elif key == 'down':
            self._selected = min(self._selected + 1, len(self._tracks) - 1)
        elif key == 'page-down':
            self._selected = min(self._selected + 10, len(self._tracks) - 1)
        elif key == 'left':
            self._parent.set_focus(self._parent._album_widget)
        elif key == ord(' '):
            if self._selected < len(self._tracks):
                del self._tracks[self._selected]
                if self._selected >= len(self._tracks):
                    self._selected = max(len(self._tracks) - 1, 0)
        elif key == 'ctrl-up':
            if self._selected > 0:
                i = self._selected - 1
                j = self._selected 
                self._tracks[i], self._tracks[j] = self._tracks[j], self._tracks[i]
                self._selected -= 1
        elif key == 'ctrl-down':
            if self._selected < (len(self._tracks) - 1):
                i = self._selected
                j = self._selected + 1 
                self._tracks[i], self._tracks[j] = self._tracks[j], self._tracks[i]
                self._selected += 1
        elif key == ord('\n'):
            tracks = [x['path'] for x in self._tracks]
            self._save_and_play_playlist(tracks, self._selected)

    def got_cursor(self):
        self._has_cursor = True

    def lost_cursor(self):
        self._has_cursor = False

    def add_track(self, track):
        self._tracks.append(track)
