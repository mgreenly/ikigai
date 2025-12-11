# Ikigai 12/11 Dev Stream

  * https://github.com/mgreenly/ikigai

During the last few evenings I've gone down a rabbit hole trying to figure out the
optimal solution for the terminal emulator mode and how to handle keyboard and mouse
input from both modern and legacy terminal emulators.  Specifically not capturing
mouse control, leaving it to the terminal, while still being able to interpret mouse-
wheel events separate from arrow keys, as well as supporting shift+enter, ctrl+enter
when possible. This is all working now but there's a bunch of rendering quirks/bugs
to work through now.

So that will be tonights interactive session.
