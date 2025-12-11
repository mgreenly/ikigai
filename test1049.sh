#!/bin/bash
trap 'printf "\e[?1049l"; stty sane' EXIT
stty raw -echo
printf '\e[?1049h\e[2J\e[HScroll or press q\r\n'
while IFS= read -rsn1 c; do
  case "$c" in
    $'\e')
      read -rsn1 -t0.1 b
      read -rsn1 -t0.1 code
      printf 'got: %s\r\n' "$code"
      ;;
    q) break ;;
  esac
done
