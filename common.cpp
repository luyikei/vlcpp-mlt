
#include "common.hpp"

const char * const argv[] = {
    "--verbose=1",
    "--no-skip-frames",
    "--text-renderer",
    "--no-sub-autodetect-file",
    "--no-disable-screensaver",
    NULL,
};

VLC::Instance instance = VLC::Instance( 4, argv );
