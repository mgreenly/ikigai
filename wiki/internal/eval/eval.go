// Package eval loads extract evaluation cases and runs production extraction.
package eval

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"wiki/internal/extract"
)

// Case is one extract evaluation fixture.
type Case struct {
	Name       string
	Difficulty string
	Header     extract.DocumentHeader
	Text       string
	Gold       []GoldSubject
}

// GoldSubject is one blessed expected subject in an eval case.
type GoldSubject struct {
	Type   string
	Name   string
	Claims []string
}

// LoadCase parses and validates document.txt and gold.json from dir.
func LoadCase(dir string) (Case, error) {
	name := filepath.Base(filepath.Clean(dir))
	rawText, err := os.ReadFile(filepath.Join(dir, "document.txt"))
	if err != nil {
		return Case{}, fmt.Errorf("load case %s document.txt: %w", name, err)
	}
	text := string(rawText)
	if strings.TrimSpace(text) == "" {
		return Case{}, fmt.Errorf("load case %s: document.txt required", name)
	}

	rawGold, err := os.ReadFile(filepath.Join(dir, "gold.json"))
	if err != nil {
		return Case{}, fmt.Errorf("load case %s gold.json: %w", name, err)
	}
	var gold goldFile
	dec := json.NewDecoder(strings.NewReader(string(rawGold)))
	dec.DisallowUnknownFields()
	if err := dec.Decode(&gold); err != nil {
		return Case{}, fmt.Errorf("load case %s gold.json: %w", name, err)
	}
	if err := validateGold(name, gold); err != nil {
		return Case{}, err
	}

	receivedAt, err := time.Parse(time.RFC3339, gold.Header.ReceivedAt)
	if err != nil {
		return Case{}, fmt.Errorf("load case %s: header.received_at must be RFC3339: %w", name, err)
	}
	return Case{
		Name:       name,
		Difficulty: gold.Difficulty,
		Header: extract.DocumentHeader{
			Source:     gold.Header.Source,
			Title:      gold.Header.Title,
			Tags:       append([]string(nil), gold.Header.Tags...),
			ReceivedAt: receivedAt,
		},
		Text: text,
		Gold: copyGold(gold.Gold),
	}, nil
}

// LoadDataset loads each immediate subdirectory below root as one case.
func LoadDataset(root string) ([]Case, error) {
	entries, err := os.ReadDir(root)
	if err != nil {
		return nil, fmt.Errorf("load dataset %s: %w", root, err)
	}
	var cases []Case
	for _, entry := range entries {
		if !entry.IsDir() {
			continue
		}
		c, err := LoadCase(filepath.Join(root, entry.Name()))
		if err != nil {
			return nil, err
		}
		cases = append(cases, c)
	}
	return cases, nil
}

// Run executes production extract over a loaded case.
func Run(ctx context.Context, ex *extract.Extractor, c Case) ([]extract.ExtractedSubject, error) {
	return ex.Extract(ctx, c.Header, c.Text)
}

type goldFile struct {
	Difficulty string        `json:"difficulty"`
	Header     goldHeader    `json:"header"`
	Gold       []GoldSubject `json:"gold"`
}

type goldHeader struct {
	Source     string   `json:"source"`
	Title      string   `json:"title"`
	Tags       []string `json:"tags"`
	ReceivedAt string   `json:"received_at"`
}

func validateGold(caseName string, gold goldFile) error {
	switch gold.Difficulty {
	case "easy", "medium", "hard":
	default:
		return fmt.Errorf("load case %s: difficulty must be easy, medium, or hard", caseName)
	}
	if strings.TrimSpace(gold.Header.Source) == "" {
		return fmt.Errorf("load case %s: header.source required", caseName)
	}
	if strings.TrimSpace(gold.Header.Title) == "" {
		return fmt.Errorf("load case %s: header.title required", caseName)
	}
	if strings.TrimSpace(gold.Header.ReceivedAt) == "" {
		return fmt.Errorf("load case %s: header.received_at required", caseName)
	}
	if len(gold.Gold) == 0 {
		return fmt.Errorf("load case %s: gold subjects required", caseName)
	}
	for i, subject := range gold.Gold {
		if err := validateGoldSubject(i, subject); err != nil {
			return fmt.Errorf("load case %s: %w", caseName, err)
		}
	}
	return nil
}

func validateGoldSubject(i int, subject GoldSubject) error {
	switch subject.Type {
	case "entity", "event", "concept":
	default:
		return fmt.Errorf("gold[%d].type must be entity, event, or concept", i)
	}
	if strings.TrimSpace(subject.Name) == "" {
		return fmt.Errorf("gold[%d].name required", i)
	}
	if len(subject.Claims) == 0 {
		return fmt.Errorf("gold[%d].claims required", i)
	}
	for j, claim := range subject.Claims {
		if strings.TrimSpace(claim) == "" {
			return fmt.Errorf("gold[%d].claims[%d] required", i, j)
		}
	}
	return nil
}

func copyGold(in []GoldSubject) []GoldSubject {
	out := make([]GoldSubject, len(in))
	for i := range in {
		out[i] = GoldSubject{
			Type:   in[i].Type,
			Name:   in[i].Name,
			Claims: append([]string(nil), in[i].Claims...),
		}
	}
	return out
}
