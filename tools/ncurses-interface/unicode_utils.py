import unicodedata


# taken from https://bugs.python.org/issue12568
width_mapping = {'F':2, 'H': 1, 'W': 2, 'Na': 1, 'N': 1, 'A': 1}
def width(s):
    result = 0
    for c in s:
        w = unicodedata.east_asian_width(c)
        result += width_mapping[w]
    return result

