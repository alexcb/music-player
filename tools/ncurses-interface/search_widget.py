import curses


class SearchWidget(object):
    def __init__(self, parent, onchange):
        self._parent = parent
        self._text = ''
        self._has_cursor = False
        self._onchange = onchange

    def draw(self, screen, x, y, width, height, nprint):
        s = "search: " + self._text
        nprint(0, 0, s)
        if self._has_cursor:
            nprint(len(s), 0, ' ', curses.A_REVERSE)
            

    def got_cursor(self):
        self._has_cursor = True

    def lost_cursor(self):
        self._has_cursor = False

    def handle_key(self, key):
        if isinstance(key, int):
            if key == 127:
                self._text = self._text[:-1]
            elif key == ord('\n'):
                pass
            else:
                self._text += chr(key)
            self._onchange(self._text)
        elif key == 'escape':
            self._text = ''
            self._onchange(self._text)
        elif key == 'up':
            self._parent.set_focus(self._parent._album_widget)
        elif key == 'down':
            self._parent.set_focus(self._parent._album_widget)


