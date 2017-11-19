import requests
import json
import random
from menu import menu

def getalbums():
    r = requests.get('http://localhost/library')
    r.raise_for_status()
    return r.json()['albums']


def loadplaylist(playlist_name, tracks):
    print '---- loading ---'
    r = requests.post('http://localhost/playlists', data=json.dumps({
        'name': playlist_name,
        'playlist': tracks,
        }))
    print r.status_code
    print r.text
    r.raise_for_status()

def get_playlist(playlist_name):
    print '---- playing ---'
    r = requests.get('http://localhost/playlists')
    r.raise_for_status()
    playlists = {x['name']: x for x in r.json()['playlists']}
    return playlists[playlist_name]

def playplaylist(playlist_name, item_id):
    print '---- playing ---'
    print (playlist_name, item_id)
    r = requests.post('http://localhost/play?playlist=%s&id=%s' % (playlist_name, item_id))
    print r.status_code
    print r.text
    r.raise_for_status()

playlist_name = 'myplaylist'
albums = getalbums()
albums_sorted = sorted([(x['artist'], x['album'], x['path']) for x in albums])

def play_album(selected):
    _, _, selected_path = albums_sorted[selected]
    tracks = [x['tracks'] for x in albums if x['path'] == selected_path][0]
    paths = [x['path'] for x in tracks]
    loadplaylist("quick album", paths)
    first_track = get_playlist("quick album")['items'][0]['id']
    playplaylist("quick album", first_track)

menu(
    ['%s - %s' % (artist, album) for (artist, album, _) in albums_sorted],
    play_album
    )

