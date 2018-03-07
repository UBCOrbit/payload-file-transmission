project('payload-file-transmission', 'c')

include = include_directories('lib')

sha_src = ['lib/sha256.c', 'lib/sha256_utils.c']

sha = static_library('sha256', sha_src)

if get_option('build_tests')
    executable('test-sha256', ['lib/sha256.c', 'lib/sha256_utils.c'], c_args : '-DSHA256_TEST')
endif
