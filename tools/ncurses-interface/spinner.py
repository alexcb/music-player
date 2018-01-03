import random

class Spinner(object):
    def __init__(self):
        self._loading_state = 0
        self._loading_chars = ['/', '-', '\\', '|']

    def get_speed(self):
        return 0.1

    def get_ch(self):
        self._loading_state += 1
        if self._loading_state >= len(self._loading_chars):
            self._loading_state = 0
        return self._loading_chars[self._loading_state]

class Shark(object):
    def __init__(self):
        self._width = 20
        self._loading_pos = 0
        self._loading_dir = 1
        self._shark = {1: '/|', -1: '|\\'}

    def get_speed(self):
        return 0.01

    def get_ch(self):
        self._loading_pos += self._loading_dir
        if self._loading_pos >= self._width or self._loading_pos <= 0:
            self._loading_dir *= -1
        n = self._width - self._loading_pos
        m = self._width - n
        return ('_' * n) + self._shark[self._loading_dir] + ('_' * m)

class Dots(object):
    def __init__(self):
        self._i = 0
        self._max = 4

    def get_speed(self):
        return 0.2

    def get_ch(self):
        self._i += 1
        if self._i >= self._max:
            self._i = 0
        return '.' * self._i


class Faces(object):
    def __init__(self):
        pass

    def get_speed(self):
        return 0.8

    def get_ch(self):
        return random.choice([
            ':-)',
            '(-:',
            ':P',
            ':\'(',
            '*<:o)',
            '=):-)',
            ])

def get_random_spinner():
    return random.choice([Spinner, Shark, Dots, Faces])()
