# Makefile — local development. Self-contained: builds and runs prompts directly,
# foreground (logs to the terminal, Ctrl-C to stop). The bin/ scripts are the
# production deploy spine (setup/deploy/start/stop on the box) and are not used
# for local running.
APP     := prompts
BIN     := build/$(APP).bin
VERSION ?= dev
LDFLAGS := -X main.version=$(VERSION)

.PHONY: build run test vet fmt clean

build:
	@mkdir -p $(dir $(BIN))
	go build -trimpath -ldflags "$(LDFLAGS)" -o $(BIN) ./cmd/prompts

run: build
	./$(BIN)

test:
	go test ./...

vet:
	go vet ./...

fmt:
	gofmt -w .

clean:
	rm -rf build
