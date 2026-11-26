cmake_minimum_required (VERSION 3.12)

include ("${RELEASE_TARBALLS_READER}")
wsjt_read_release_tarballs ("${RELEASE_TARBALLS_FILE}" programs)
