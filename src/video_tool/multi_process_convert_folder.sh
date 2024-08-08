#!/bin/bash
# For all files in a directory, creates both spectator and normal replays in parallel
# use as: ./multi_process_convert_folder <folder_name> <number_of_parallell_jobs>
echo Now creating split screen videos
find "$1" -type f -name \*.lrp -print0 | parallel --no-notice -0 -j$2 ./videotool -r
echo Now creating spectator mode videos
find "$1" -type f -name \*.lrp -print0 | parallel --no-notice -0 -j$2 ./videotool -s -r
