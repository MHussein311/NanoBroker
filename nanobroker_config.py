import os
import sys

def get_include():
    # Return the path to the headers inside the environment
    return os.path.join(sys.prefix, 'include')
