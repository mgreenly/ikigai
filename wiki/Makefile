# Makefile — local development. Self-contained: builds and runs wiki directly,
# foreground (logs to the terminal, Ctrl-C to stop). The bin/ scripts are the
# production deploy spine (setup/deploy/start/stop on the box) and are not used
# for local running.
APP     := wiki
BIN     := bin/$(APP)
VERSION ?= dev
LDFLAGS := -X main.version=$(VERSION)

.PHONY: build run test vet fmt clean

build:
	@mkdir -p bin
	go build -trimpath -ldflags "$(LDFLAGS)" -o $(BIN) ./cmd/wiki
	go build -trimpath -ldflags "$(LDFLAGS)" -o bin/eval-extract ./cmd/eval-extract

run: build
	./$(BIN)

test:
	go test ./...

vet:
	go vet ./...

fmt:
	gofmt -w .

clean:
	rm -rf bin
