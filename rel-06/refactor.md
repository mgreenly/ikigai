## ensure logger is first and last

  * make the logger creation the very first thing that happens
  * output a session start message as very first log event
  * output a session end message as teh very last log event
  * fix up the panic exits to try and write this entry also on the way out
  * make the logger destruction the very last thing that happens


## relocate migration files
  * relocate the migration files to ./shared/ikigai/migrations
  * install the migration files to $PREFIX/shared/ikigai/migrations
  * runtime search for the migration files relative to the binary
     - until function to find relative path from the binary
     - use readlink("/proc/self/exe", ...)
     - turn it into an absolute path


