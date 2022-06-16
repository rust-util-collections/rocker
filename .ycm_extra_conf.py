import os

HOME = os.getenv("HOME")

def FlagsForFile( filename, **kwargs ):
   return {
     'flags': [
         '-x', 'c', '-Wall', '-Wextra', '-Werror', '-std=gnu11',
         '-isystem', '/usr/local/include',
         '-isystem', '/usr/include',
         '-I.',
         '-I' + HOME + "/rocker/librocker_client/src",
         '-I' + HOME + "/rocker/librocker_client/tests",
         ],
   }
