winscard_lib = cc.find_library('winscard')

aimepcsc_lib = static_library(
    'aimepcsc',
    name_prefix : '',
    include_directories: inc,
    implicit_include_directories : false,
    dependencies: [winscard_lib],
    sources : [
        'aimepcsc.c',
    ],
)

aimereader = executable(
    'aimereader',
    name_prefix : '',
    include_directories : [inc],
    implicit_include_directories : false,
    link_with: [aimepcsc_lib],
    sources : [
        'aimereader.c'
    ]
)
