project('payload-file-transmission', 'c')

include = include_directories('lib')

crc_src = ['lib/crc32.c']
sha_src = ['lib/sha256.c', 'lib/sha256_utils.c']

crc = static_library('crc32',  crc_src)
sha = static_library('sha256', sha_src)

stitch_src    = ['stitch/main.c']
send_file_src = ['send-file/main.c']
recv_pack_src = ['recieve-packets/main.c']

executable('stitch',       stitch_src,    include_directories : include, link_with : sha)
executable('send-file',    send_file_src, include_directories : include, link_with : [sha, crc])
executable('recv-packets', recv_pack_src, include_directories : include, link_with : [sha, crc])
