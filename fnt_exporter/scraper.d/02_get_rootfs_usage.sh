#!/bin/bash
export LC_ALL="en_US.UTF-8"
export LC_TIME="en_US.UTF-8"
#--- disk usage ---
echo '# HELP fs_usage_percent The usage of filesystem.'
echo '# TYPE fs_usage_percent gauge'
echo "fs_usage_percent{Mounted=\"/\"} `df -m|awk '{if($6=="/")print $5}'|sed 's/\%//'`"
