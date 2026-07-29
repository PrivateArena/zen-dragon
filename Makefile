.PHONY: all build clean run

APP=zen-dragon

all: build

build:
	CGO_ENABLED=1 go build -ldflags="-s -w" -o $(APP) ./cmd/zen-dragon/

run: build
	./$(APP)

clean:
	rm -f $(APP)
	go clean
