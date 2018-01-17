import itertools

from log import log, log_duration

class Playlist(object):
    def __init__(self, tracks=None, playing_id=None):
        if tracks is None:
            tracks = []
        self._tracks = tracks

        self._playing_track_pos = None
        self._playing_album_pos = None
        self._build_album_view()
        if playing_id is not None:
            self.set_playing(playing_id)

    def append(self, track):
        self._tracks.append(track)
        self._build_album_view()

    def extend(self, tracks):
        self._tracks.extend(tracks)
        self._build_album_view()

    def set_playing(self, playing_id):
        try:
            self._playing_track_pos = (i for i, x in enumerate(self._tracks) if x.get('id') == playing_id).next()
        except StopIteration:
            self._playing_track_pos = None

        if self._playing_track_pos is not None:
            try:
                self._playing_album_pos = (i for i, x in enumerate(self._albums) if x['start'] <= self._playing_track_pos < x['end']).next()
            except StopIteration:
                self._playing_album_pos = None
        else:
            self._playing_album_pos = None

        log('playing_id: %s track: %s album: %s' % (playing_id, self._playing_track_pos, self._playing_album_pos))

    def get_playing_track_index(self):
        return self._playing_track_pos

    def get_playing_album_index(self):
        return self._playing_album_pos

    def get_corresponding_album_index(self, track_index):
        try:
            return (i for i, x in enumerate(self._albums) if x['start'] <= track_index < x['end']).next()
        except StopIteration:
            return None

    def get_corresponding_track_index(self, album_index):
        return self._albums[album_index]['start']

    def _build_album_view(self):
        self._albums = []

        album = None
        length = 0
        start = 0

        for i, track in enumerate(self._tracks):
            if album and track['album'] != album:
                self._albums.append({
                    'artist': artist,
                    'id': self._tracks[start].get('id'),
                    'album': album,
                    'length': length,
                    'length': length,
                    'year': year,
                    'start': start,
                    'end': i,
                    })
                length = 0
                start = i
            length += track['length']
            artist = track['artist']
            album = track['album']
            year = track['year']

        if album:
            self._albums.append({
                'artist': artist,
                'id': self._tracks[start].get('id'),
                'album': album,
                'length': length,
                'length': length,
                'year': year,
                'start': start,
                'end': len(self._tracks),
                })

    def move_track(self, pos, direction):
        #pos = None
        #for i, track in enumerate(self._tracks):
        #    if track['id'] == track_id:
        #        pos = i
        #        break
        #if pos is None:
        #    raise KeyError(track_id)

        if pos < 0 or pos >= len(self._tracks):
            return
        x = self._tracks.pop(pos)

        new_pos = pos + direction
        if new_pos < 0:
            new_pos = 0
        if new_pos >= len(self._tracks):
            new_pos = len(self._tracks)

        self._tracks.insert(new_pos, x)
        self._build_album_view()

    def move_album(self, album_i, direction):
        album = self._albums[album_i]
        pos = (album['start'], album['end'])

        to_move = self._tracks[pos[0]:pos[1]]

        if direction == -1:
            if album_i == 0:
                new_pos = 0
            else:
                new_pos = self._albums[album_i-1]['start']
        elif direction == 1:
            if (album_i+1) >= len(self._albums):
                new_pos = len(self._tracks)
            else:
                new_pos = self._albums[album_i+1]['end']
                new_pos -= len(to_move)
        else:
            raise ValueError(direction)

        without = self._tracks[:pos[0]] + self._tracks[pos[1]:]

        self._tracks = without[:new_pos] + to_move + without[new_pos:]
        self._build_album_view()

    def remove_track(self, pos):
        self._tracks.pop(pos)
        self._build_album_view()

    def remove_album(self, pos):
        album = self._albums[pos]
        pos = (album['start'], album['end'])

        self._tracks = self._tracks[:pos[0]] + self._tracks[pos[1]:]
        self._build_album_view()



if __name__ == '__main__':
    def get_tracks(num):
        tracks = []
        for i in range(num):
            tracks.append({
                'id': i,
                'title': str(i % 5),
                'album': str(i/5),
                'artist': 'unknown',
                'length': i//10,
                'year': 123,
                })
        return tracks

    def assert_eq(a, b):
        assert a == b, '%s == %s' % (repr(a), repr(b))

    def test_move_track_up():
        p = Playlist(get_tracks(10))
        p.move_track(5, -1)
        assert [x['id'] for x in p._tracks] == [0, 1, 2, 3, 5, 4, 6, 7, 8, 9]

    def test_move_track_down():
        p = Playlist(get_tracks(10))
        p.move_track(5, 1)
        assert [x['id'] for x in p._tracks] == [0, 1, 2, 3, 4, 6, 5, 7, 8, 9]

    def test_move_album_up():
        p = Playlist(get_tracks(20))
        p.move_album(10, 1)
        assert_eq(
            [x['id'] for x in p._tracks],
            [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16, 17, 18, 19, 10, 11, 12, 13, 14]
            )

    def test_move_album_down():
        p = Playlist(get_tracks(20))
        p.move_album(10, -1)
        assert_eq(
            [x['id'] for x in p._tracks],
            [0, 1, 2, 3, 4, 10, 11, 12, 13, 14, 5, 6, 7, 8, 9, 15, 16, 17, 18, 19]
            )

    def test_move_album_down_front():
        p = Playlist(get_tracks(15))
        p.move_album(0, -1)
        assert_eq(
            [x['id'] for x in p._tracks],
            [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14]
            )

    def test_move_album_up_end():
        p = Playlist(get_tracks(10))
        p.move_album(5, 1)
        assert_eq(
            [x['id'] for x in p._tracks],
            [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
            )

    test_move_track_up()
    test_move_track_down()
    test_move_album_up()
    test_move_album_down()
    test_move_album_down_front()
    test_move_album_up_end()




