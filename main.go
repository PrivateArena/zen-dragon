package main

import (
	"context"
	"flag"
	"fmt"
	"os"
	"runtime/debug"

	"zen-dragon/internal/gui"
	"zen-dragon/internal/search"

	"gioui.org/app"
)

func main() {
	debug.SetGCPercent(-1)

	keyword := flag.String("p", "", "Search pattern (substring, required)")
	root := flag.String("r", ".", "Root directory to search from")
	recursive := flag.Bool("R", false, "Search subdirectories recursively")
	regex := flag.Bool("e", false, "Interpret pattern as regex")
	scrollSpeed := flag.Float64("s", 5.0, "List scroll speed multiplier (1.0 = default)")
	help := flag.Bool("h", false, "Show help")

	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "Usage: zen-dragon [options] [keyword]\n")
		fmt.Fprintf(os.Stderr, "\nSearch files and drag them into other applications.\n\n")
		fmt.Fprintf(os.Stderr, "Options:\n")
		flag.PrintDefaults()
		fmt.Fprintf(os.Stderr, "\nIf keyword is provided as positional argument, -p is not needed.\n")
	}

	flag.Parse()

	if *help {
		flag.Usage()
		return
	}

	pat := *keyword
	if pat == "" {
		if flag.NArg() > 0 {
			pat = flag.Arg(0)
		}
	}
	if pat == "" {
		fmt.Fprintf(os.Stderr, "Error: search pattern required\n")
		flag.Usage()
		os.Exit(1)
	}

	cfg := &gui.Config{
		Keyword:     pat,
		Root:        *root,
		Recursive:   *recursive,
		Regex:       *regex,
		ScrollSpeed: *scrollSpeed,
	}

	results := make(chan search.SearchResult, 4096)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Start search goroutine
	go func() {
		scanner := &search.Scanner{
			Root:      *root,
			Keyword:   pat,
			Recursive: *recursive,
			Regex:     *regex,
		}
		err := scanner.Scan(ctx, results)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Search error: %v\n", err)
		}
		close(results)
	}()

	// Run GUI in a goroutine (app.Main must be on the main goroutine)
	go func() {
		err := gui.RunUI(cfg, results)
		if err != nil {
			fmt.Fprintf(os.Stderr, "GUI error: %v\n", err)
		}
		cancel()
		os.Exit(0)
	}()

	app.Main()
}
