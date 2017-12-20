import curses


class SearchWidget(object):
    def __init__(self, parent):
        self._parent = parent
        self._mode = None
        self._text = ''
        self._has_cursor = False
        self._onchange = None
        self._onenter = None
        self._escape_focus = None

    def draw(self, screen, x, y, width, height, nprint):
        if self._mode is None:
            return
        s = "%s: %s" % (self._mode, self._text)
        nprint(0, 0, s)
        if self._has_cursor:
            nprint(len(s), 0, ' ', curses.A_REVERSE)

    def clear(self):
        self._mode = None
        self._onchange = None
        self._onenter = None
        self._escape_focus = None

    def set_mode(self, mode, onchange, onenter, escape_focus):
        self._mode = mode
        self._onchange = onchange
        self._onenter = onenter
        self._escape_focus = escape_focus
        self._text = ''

    def got_cursor(self):
        self._has_cursor = True

    def lost_cursor(self):
        self._has_cursor = False

    def handle_key(self, key):
        if isinstance(key, int):
            if key == 127:
                self._text = self._text[:-1]
            elif key == ord('\n'):
                if self._onenter:
                    self._onenter(self._text)
            else:
                self._text += chr(key)
            if self._onchange:
                self._onchange(self._text)
        elif key == 'escape':
            self._text = ''
            if self._onchange:
                self._onchange(self._text)
            self._parent.set_focus(self._escape_focus)
            self.clear()
        elif key == 'up':
            self._parent.set_focus(self._escape_focus)
        elif key == 'down':
            self._parent.set_focus(self._escape_focus)


