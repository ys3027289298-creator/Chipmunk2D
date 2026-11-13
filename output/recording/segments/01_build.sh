banner() {
  printf "\033[1;36m\n"
  printf "########################################################################\n"
  printf "# %s\n" "$1"
  printf "########################################################################\n"
  printf "\033[0m\n"
}
cd /home/ys/feedback/uidreczz28Z50mpcrxT
banner SEGMENT\ 1/5:\ BUILD\ REFACTORED\ LIBRARY
cmake --build build-new -j4 2>&1 | tail -2
echo
echo "[segment done]"
