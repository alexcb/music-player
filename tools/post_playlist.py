import requests
import json

def gettracks():
    print '---- getting tracks ---'
    r = requests.get('http://localhost/library')
    r.raise_for_status()
    albums = r.json()['albums']
    tracks = []
    for album in albums:
        for track in album['tracks'][::5]:
            tracks.append(track['path'])
    return tracks


def loadplaylist(tracks):
    print '---- loading ---'
    r = requests.post('http://localhost/playlists', data=json.dumps({
        'name': 'myplaylist',
        'playlist': tracks,
        }))
    print r.status_code
    print r.text
    r.raise_for_status()

def playplaylist():
    print '---- playing ---'
    r = requests.post('http://localhost/play?playlist=myplaylist')
    print r.status_code
    print r.text
    r.raise_for_status()

tracks = gettracks()
loadplaylist(tracks)
playplaylist()
