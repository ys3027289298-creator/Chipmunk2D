banner() {
  printf "\033[1;36m\n"
  printf "########################################################################\n"
  printf "# %s\n" "$1"
  printf "########################################################################\n"
  printf "\033[0m\n"
}
cd /home/ys/feedback/uidreczz28Z50mpcrxT
banner SEGMENT\ 5/5:\ PERFORMANCE\ AND\ MEMORY\ \(linear\ bound\)
./output/tests/bench_new_instr hash 2>/dev/null
echo
echo "[segment done]"
