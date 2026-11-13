banner() {
  printf "\033[1;36m\n"
  printf "########################################################################\n"
  printf "# %s\n" "$1"
  printf "########################################################################\n"
  printf "\033[0m\n"
}
cd /home/ys/feedback/uidreczz28Z50mpcrxT
banner SEGMENT\ 3/5:\ CROSS-BINARY\ TRACE\ vs\ PRISTINE\ BASELINE
diff <(./output/tests/equiv_baseline tree) <(./output/tests/equiv_new hash) && echo 'PAIR TRACES IDENTICAL'
echo
echo "[segment done]"
