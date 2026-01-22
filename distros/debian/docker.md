Run a new container with bash (simpler)
cd distros/debian
UID=$(id -u) GID=$(id -g) docker-compose run --rm test bash

Once inside, you can run:
make ci          # Run the full CI suite
make check-unit  # Run just unit tests
make all         # Just build
# etc.

When you're done, clean up with:
cd distros/debian
UID=$(id -u) GID=$(id -g) docker-compose down -v
