import itertools

class Playlist(object):
    def __init__(self, tracks, playing_id=None):
        self._tracks = tracks
        self._playing_id = playing_id
        self._build_album_view()

    def _build_album_view(self):
        self._albums = []

        album = None
        playing = False
        length = 0
        start = 0

        for i, track in enumerate(self._tracks):
            if album and track['album'] != album:
                self._albums.append({
                    'artist': artist,
                    'id': self._tracks[start]['id'],
                    'album': album,
                    'length': length,
                    'length': length,
                    'year': year,
                    'playing': playing,
                    'start': start,
                    'end': i,
                    })
                playing = False
                length = 0
                start = i
            length += track['length']
            artist = track['artist']
            album = track['album']
            year = track['year']

        self._albums.append({
            'artist': artist,
            'id': self._tracks[start]['id'],
            'album': album,
            'length': length,
            'length': length,
            'year': year,
            'playing': playing,
            'start': start,
            'end': len(self._tracks),
            })

    def move_track(self, track_id, direction):
        pos = None
        for i, track in enumerate(self._tracks):
            if track['id'] == track_id:
                pos = i
                break
        if pos is None:
            raise KeyError(track_id)

        new_pos = pos + direction
        if new_pos < 0:
            new_pos = 0
        if new_pos >= len(self._tracks):
            new_pos = len(self._tracks) - 1

        self._tracks.insert(new_pos, self._tracks.pop(pos))
        self._build_album_view()

    def move_album(self, id, direction):
        pos = None
        album_i = None
        for i, album in enumerate(self._albums):
            if album['id'] == id:
                pos = (album['start'], album['end'])
                album_i = i
                break
        if pos is None:
            raise KeyError(id)

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




