package search

import (
	"context"
	"regexp"
	"strings"
)

type SearchResult struct {
	Path  string
	Name  string
	IsDir bool
}

type Scanner struct {
	Root      string
	Keyword   string
	Recursive bool
	Regex     bool
}

func (s *Scanner) Scan(ctx context.Context, out chan<- SearchResult) error {
	var re *regexp.Regexp
	if s.Regex {
		var err error
		re, err = regexp.Compile(s.Keyword)
		if err != nil {
			return err
		}
	}

	walkWithCallback(s.Root, s.Recursive, true, func(path string) {
		select {
		case <-ctx.Done():
			return
		default:
		}

		var name string
		if idx := strings.LastIndexByte(path, '/'); idx >= 0 {
			name = path[idx+1:]
		} else {
			name = path
		}

		match := false
		if s.Regex {
			match = re.MatchString(name)
		} else {
			match = strings.Contains(strings.ToLower(name), strings.ToLower(s.Keyword))
		}
		if !match {
			return
		}

		select {
		case out <- SearchResult{Path: path, Name: name}:
		case <-ctx.Done():
		}
	})

	return nil
}
