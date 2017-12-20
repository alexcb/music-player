import curses


class PlaylistWidget(object):
    def __init__(self, parent, playlists, save_and_play_playlist):
        self._parent = parent
        self._playlists = playlists
        if 'default' not in playlists:
            playlists['default'] = []
        self._active_playlist = 'default'
        self._selected = 0
        self._has_cursor = False
        self._save_and_play_playlist = save_and_play_playlist

    def draw(self, screen, x, y, width, height, nprint):
        tracks = self._playlists[self._active_playlist]
        first_displayed_i = max(min(self._selected - 5, len(tracks) - height), 0)

        nprint(0, 0, "Playlist: %s" % self._active_playlist)
        for i, y in enumerate(xrange(1, height)):
            i += first_displayed_i
            attr = None
            if i == self._selected and self._has_cursor:
                attr = curses.A_REVERSE
            if i < len(tracks):
                s = self._format_track(tracks[i])
                nprint(0, y, s, attr)
            #sys.stdout.write(terminal.normal)

    def _format_track(self, track):
        return '%s - %s - %s - %s - %s' % (track['artist'], track['album'], track['track_number'], track['title'], track['year'])

    def next_playlist(self):
        self.change_playlist_rel(1)

    def prev_playlist(self):
        self.change_playlist_rel(-1)

    def change_playlist_rel(self, relative):
        keys = sorted(self._playlists.keys())
        i = keys.index(self._active_playlist)
        i += relative
        if i >= len(keys):
            i = 0
        if i < 0:
            i = len(keys) - 1
        self._active_playlist = keys[i]

    def handle_key(self, key):
        tracks = self._playlists[self._active_playlist]

        if key == 'up':
            self._selected = max(self._selected - 1, 0)
        elif key == 'page-up':
            self._selected = max(self._selected - 10, 0)
        elif key == 'down':
            self._selected = min(self._selected + 1, len(tracks) - 1)
        elif key == 'ctrl-left':
            self.prev_playlist()
        elif key == 'ctrl-right':
            self.next_playlist()
        elif key == 'page-down':
            self._selected = min(self._selected + 10, len(tracks) - 1)
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
            tracks = [x['path'] for x in tracks]
            self._save_and_play_playlist(tracks, self._selected)
        elif key == ord('n'):
            asdfasdf

    def got_cursor(self):
        self._has_cursor = True

    def lost_cursor(self):
        self._has_cursor = False

    def add_track(self, track):
        self._playlists[self._active_playlist].append(track)

    def new_playlist(self, playlist):
        if playlist not in self._playlists:
            self._playlists[playlist] = []
        self._active_playlist = playlist
