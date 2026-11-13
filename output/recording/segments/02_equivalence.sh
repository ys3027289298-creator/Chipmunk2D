banner() {
  printf "\033[1;36m\n"
  printf "########################################################################\n"
  printf "# %s\n" "$1"
  printf "########################################################################\n"
  printf "\033[0m\n"
}
cd /home/ys/feedback/uidreczz28Z50mpcrxT
banner SEGMENT\ 2/5:\ COLLISION\ PAIR\ EQUIVALENCE\ \(hash\ vs\ tree\)
./output/tests/equiv_new self
echo
echo "[segment done]"
