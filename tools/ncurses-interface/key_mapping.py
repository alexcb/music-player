def _get_escape_mapping():
    mapping_text = '''
    27 79 70 end
    27 79 72 home
    27 79 81 f2
    27 79 82 f3
    27 79 83 f4
    27 91 49 53 126 f5
    27 91 49 55 126 f6
    27 91 49 56 126 f7
    27 91 49 57 126 f8
    27 91 50 48 126 f9
    27 91 50 49 126 f10
    27 91 50 52 126 f12
    27 91 50 126 insert
    27 91 51 126 delete
    27 91 49 126 home
    27 91 52 126 end
    27 91 53 126 page-up
    27 91 54 126 page-down
    27 91 70 end
    27 91 72 home
    27 91 65 up
    27 91 66 down
    27 91 67 right
    27 91 68 left
    27 91 49 59 53 67 ctrl-right
    27 91 49 59 53 68 ctrl-left
    27 91 49 59 53 65 ctrl-up
    27 91 49 59 53 66 ctrl-down

    27 91 49 59 50 67 shift-right
    27 91 49 59 50 68 shift-left
    27 91 49 59 50 65 shift-up
    27 91 49 59 50 66 shift-down

    27 91 49 59 51 67 alt-right
    27 91 49 59 51 68 alt-left
    27 91 49 59 51 65 alt-up
    27 91 49 59 51 66 alt-down

    27 91 49 59 52 67 alt-shift-right
    27 91 49 59 52 68 alt-shift-left
    27 91 49 59 52 65 alt-shift-up
    27 91 49 59 52 66 alt-shift-down

    27 91 49 59 54 67 ctrl-shift-right
    27 91 49 59 54 68 ctrl-shift-left
    27 91 49 59 54 65 ctrl-shift-up
    27 91 49 59 54 66 ctrl-shift-down

    '''

    mapping = {}
    for l in mapping_text.splitlines():
        x = l.strip().split()
        if x:
            keyname = x.pop()
            last_code = int(x.pop())
            d = mapping
            for code in x:
                code = int(code)
                if code not in d:
                    d[code] = {}
                d = d[code]
            d[last_code] = keyname
    return mapping

key_mapping = _get_escape_mapping()
del _get_escape_mapping


MOUSE_EVENT = object()
KEY_EVENT = object()

ESCAPE = 27
MOUSE = 409
def get_ch(stdscr):
    c = stdscr.getch()
    if c == -1:
        return None
    if c == ESCAPE:
        mapping = key_mapping[27]
        c = stdscr.getch()
        if c == -1:
            return KEY_EVENT, 'escape'

        mapping = mapping[c]
        while isinstance(mapping, dict):
            c = stdscr.getch()
            if c == -1:
                raise RuntimeError("escape underflow")
            mapping = mapping[c]
        return KEY_EVENT, mapping
    if c == MOUSE:
        try:
            return MOUSE_EVENT, curses.getmouse()
        except:
            return None
    else:
        return KEY_EVENT, c
