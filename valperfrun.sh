set -x

LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8 LC_CTYPE=en_US.UTF-8 valgrind --tool=massif   --pages-as-heap=yes   --time-unit=ms   --detailed-freq=1   --max-snapshots=200   --alloc-fn='operatornew'   --alloc-fn='operatornew[]'       --extra-debuginfo-path=~/.cache/debuginfod_client/ --keep-debuginfo=yes qltox/build-qt3/qltox

# ms_print massif.out.1289688 |less
