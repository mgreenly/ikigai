#!/bin/bash
# Test: Use /dev/tty directly like ikigai does
exec 3<>/dev/tty  # Open /dev/tty for read/write on fd 3
trap 'printf "\e[?25h\e[0m\e[?1049l" >&3; stty sane <&3; exec 3>&-' EXIT
stty -brkint -icrnl -inpck -istrip -ixon -opost cs8 -echo -icanon -iexten -isig min 1 time 0 <&3
printf '\e[?1049h\e[2J\e[HScroll or press q\r\n' >&3
output="Scroll or press q"
line=2
while IFS= read -rsn1 c <&3; do
  case "$c" in
    $'\e')
      read -rsn1 -t0.1 b <&3
      read -rsn1 -t0.1 code <&3
      output="$output\r\n\e[38;5;242mgot:\e[0m $code"
      line=$((line + 1))
      ;;
    q) break ;;
    *)
      output="$output\r\n\e[38;5;242mkey:\e[0m $c"
      line=$((line + 1))
      ;;
  esac

  # Mimic ikigai's render cycle
  printf '\e[2J\e[H' >&3
  printf '%b\r\n' "$output" >&3
  printf '\e[?25h' >&3
  printf '\e[%d;1H' "$line" >&3
done
