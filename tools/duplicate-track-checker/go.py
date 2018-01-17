#!/usr/bin/env python2.7
import argparse
import requests
import os

import mutagen.id3

import musicbrainzngs


def get_parser():
    parser = argparse.ArgumentParser(description='control me the hits')
    parser.add_argument('--host', default='localhost')
    return parser

def get_playlists(host):
    r = requests.get('http://%s/playlists' % host)
    r.raise_for_status()
    return r.json()['playlists']

def check_playlist(tracks):
    seen = set()
    dups = False
    for x in tracks['items']:
        p = x['path']
        if p in seen:
            print '  dup: %s' % p
            dups = True
        seen.add(p)
    if not dups:
        print '  no duplicates found'


def run(host):
    playlists = get_playlists(host)
    for k, v in playlists.iteritems():
        print k
        check_playlist(v)



if __name__ == '__main__':
    args = get_parser().parse_args()
    run(args.host)

