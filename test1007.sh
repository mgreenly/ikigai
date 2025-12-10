#!/bin/bash
trap 'printf "\e[?1007l\e[?1049l"; stty sane' EXIT
stty raw -echo
printf '\e[?1049h\e[?1007h\e[2J\e[HScroll or press q\r\n'
while IFS= read -rsn1 c; do
  case "$c" in
    $'\e') read -rsn2 -t0.1 r; [[ $r == '[A' ]] && printf 'UP\r\n'; [[ $r == '[B' ]] && printf 'DN\r\n' ;;
    q) break ;;
  esac
done
