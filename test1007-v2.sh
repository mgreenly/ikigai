#!/bin/bash
# Match ikigai cleanup: two separate writes, tcflush, then restore termios
cleanup() {
  printf "\e[?1007l"
  printf "\e[?1049l"
  # Flush input like ikigai does
  read -t 0.001 -n 10000 discard 2>/dev/null || true
  stty sane
}
trap cleanup EXIT
# Match ikigai's raw mode: disable ISIG too
stty raw -echo -isig
printf '\e[?1049h\e[?1007h\e[2J\e[HScroll or press q\r\n'
line=2
while IFS= read -rsn1 c; do
  case "$c" in
    $'\e') read -rsn2 -t0.1 r
      # Simulate ikigai render: clear + home + content + cursor show + position
      printf '\e[2J\e[H'
      if [[ $r == '[A' ]]; then
        printf 'UP %d\r\n' $line
        ((line++))
      fi
      if [[ $r == '[B' ]]; then
        printf 'DN %d\r\n' $line
        ((line++))
      fi
      printf '\e[?25h\e[5;1H'
      ;;
    q) break ;;
  esac
done
