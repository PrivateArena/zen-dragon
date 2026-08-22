package search

import (
	"context"
	"os"
	"path/filepath"
	"sort"
	"testing"
)

// TestScannerFirstWordMatch locks in the behavior the user questioned:
// a substring keyword that appears at the START of a filename (the "first
// word") must be matched, and matching is case-insensitive. This guards
// against any future regression that would make the engine skip leading
// matches (the search core is correct; results are rendered by the GUI).
func TestScannerFirstWordMatch(t *testing.T) {
	dir := t.TempDir()
	files := []string{
		"lufia_village.mid",     // keyword at the start (lowercase)
		"Lufia_2_Battle.mid",    // keyword at the start (mixed case)
		"some_lufia_middle.mid", // keyword in the middle
		"unrelated_track.mid",   // no match
	}
	for _, f := range files {
		if err := os.WriteFile(filepath.Join(dir, f), []byte("x"), 0o644); err != nil {
			t.Fatalf("write %s: %v", f, err)
		}
	}

	sc := &Scanner{Root: dir, Keyword: "lufia", Recursive: false, Regex: false}
	out := make(chan SearchResult, 64)
	if err := sc.Scan(context.Background(), out); err != nil {
		t.Fatalf("Scan: %v", err)
	}
	close(out)

	var got []string
	for r := range out {
		got = append(got, r.Name)
	}
	sort.Strings(got)

	want := []string{"Lufia_2_Battle.mid", "lufia_village.mid", "some_lufia_middle.mid"}
	if len(got) != len(want) {
		t.Fatalf("expected %d matches %v, got %d %v", len(want), want, len(got), got)
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("match %d: want %q, got %q (all=%v)", i, want[i], got[i], got)
		}
	}
}
