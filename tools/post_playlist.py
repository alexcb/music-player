import requests
import json
import random

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


def loadplaylist(playlist_name, tracks):
    print '---- loading ---'
    r = requests.post('http://localhost/playlists', data=json.dumps({
        'name': playlist_name,
        'playlist': tracks,
        }))
    print r.status_code
    print r.text
    r.raise_for_status()

def get_random_starting_track(playlist_name):
    print '---- playing ---'
    r = requests.get('http://localhost/playlists')
    r.raise_for_status()
    playlists = {x['name']: x for x in r.json()['playlists']}
    playlist = playlists[playlist_name]
    return random.choice(playlist['items'])['id']

def playplaylist(playlist_name, item_id):
    print '---- playing ---'
    print (playlist_name, item_id)
    r = requests.post('http://localhost/play?playlist=%s&id=%s' % (playlist_name, item_id))
    print r.status_code
    print r.text
    r.raise_for_status()

playlist_name = 'myplaylist'
tracks = gettracks()
loadplaylist(playlist_name, tracks)
random_track = get_random_starting_track(playlist_name)
playplaylist(playlist_name, random_track)
