project('payload-file-transmission', 'c')

include = include_directories('include')

crc = static_library('crc32',  'lib/crc32.c',  include_directories : include)
sha = static_library('sha256', 'lib/sha256.c', include_directories : include)

packetize_src = ['packetize/main.c']
stitch_src    = ['stitch/main.c']
send_pack_src = ['send-packets/main.c']

executable('packetize',    packetize_src, include_directories : include, link_with : sha)
executable('stitch',       stitch_src,    include_directories : include, link_with : sha)
executable('send-packets', send_pack_src, include_directories : include, link_with : crc)
