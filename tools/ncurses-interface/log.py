import time
import os
from functools import wraps

the_log = open(os.path.expanduser('~/.music-ui.log'), 'w')
def log(s):
    if not isinstance(s, basestring):
        s = repr(s)
    the_log.write(str(time.time()) + ': ')
    the_log.write(s+'\n')
    the_log.flush()

def log_duration(fn):
    def _decorator(*args, **kwargs):
        started = time.time()
        response = fn(*args, **kwargs)
        duration = time.time() - started
        log('%s took %s seconds' % (fn.__name__, duration))
        return response
    return wraps(fn)(_decorator)

