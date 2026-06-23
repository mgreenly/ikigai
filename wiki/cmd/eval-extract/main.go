// Command eval-extract runs the extract evaluation dataset.
package main

import (
	"context"
	"flag"
	"fmt"
	"io"
	"os"
	"strconv"
	"strings"

	agentkit "github.com/ikigenba/agentkit"
	"github.com/ikigenba/agentkit/anthropic"

	"wiki/internal/eval"
	"wiki/internal/extract"
	"wiki/internal/llm"
)

func main() {
	os.Exit(run(context.Background(), os.Args[1:], os.Getenv, os.Stdout, os.Stderr, runDeps{}))
}

type runDeps struct {
	newProvider func(apiKey string) agentkit.Provider
	evaluate    func(context.Context, string, *extract.Extractor, llm.CallSite, *eval.Judge, llm.CallSite) (eval.Scorecard, error)
}

type config struct {
	dataset    string
	record     string
	promptFile string
	json       bool
	extract    llm.CallSite
	judge      llm.CallSite
}

func run(ctx context.Context, args []string, getenv func(string) string, stdout, stderr io.Writer, deps runDeps) int {
	cfg, err := parseConfig(args)
	if err != nil {
		fmt.Fprintf(stderr, "eval-extract: %v\n", err)
		return 2
	}
	apiKey := strings.TrimSpace(getenv("ANTHROPIC_API_KEY"))
	if apiKey == "" {
		fmt.Fprintln(stderr, "eval-extract: ANTHROPIC_API_KEY is required")
		return 2
	}
	if deps.newProvider == nil {
		deps.newProvider = func(apiKey string) agentkit.Provider { return anthropic.New(apiKey) }
	}
	if deps.evaluate == nil {
		deps.evaluate = eval.RunDataset
	}

	promptLabel := "default"
	var extractorOpts []extract.Option
	if cfg.promptFile != "" {
		rawPrompt, err := os.ReadFile(cfg.promptFile)
		if err != nil {
			fmt.Fprintf(stderr, "eval-extract: -prompt-file %s: %v\n", cfg.promptFile, err)
			return 1
		}
		extractorOpts = append(extractorOpts, extract.WithPromptInstructions(string(rawPrompt)))
		promptLabel = cfg.promptFile
	}

	var recorder llm.Recorder
	var recordFile *os.File
	if cfg.record != "" {
		recordFile, err = os.Create(cfg.record)
		if err != nil {
			fmt.Fprintf(stderr, "eval-extract: record %s: %v\n", cfg.record, err)
			return 1
		}
		defer recordFile.Close()
		recorder = eval.NewJSONLRecorder(recordFile)
	}

	client := llm.New(deps.newProvider(apiKey), nil, recorder)
	extractor := extract.New(client, cfg.extract, extractorOpts...)
	judge := eval.NewJudge(client, cfg.judge)
	scorecard, err := deps.evaluate(ctx, cfg.dataset, extractor, cfg.extract, judge, cfg.judge)
	if err != nil {
		fmt.Fprintf(stderr, "eval-extract: %v\n", err)
		return 1
	}
	scorecard.Prompt = promptLabel
	if cfg.json {
		scorecard.WriteJSON(stdout)
		return 0
	}
	scorecard.WriteHuman(stdout)
	return 0
}

func parseConfig(args []string) (config, error) {
	cfg := config{
		dataset: "testdata/eval/extract",
		extract: extract.DefaultCallSite(),
		judge:   eval.DefaultJudgeCallSite(),
	}
	cfg.extract.Model = anthropic.ModelSonnet46

	fs := flag.NewFlagSet("eval-extract", flag.ContinueOnError)
	fs.SetOutput(io.Discard)
	model := fs.String("model", "", "extract model")
	reasoning := fs.String("reasoning", "", "extract reasoning")
	temperature := fs.String("temperature", "", "extract temperature")
	maxTokens := fs.String("max-tokens", "", "extract max tokens")
	judgeModel := fs.String("judge-model", "", "judge model")
	judgeReasoning := fs.String("judge-reasoning", "", "judge reasoning")
	dataset := fs.String("dataset", cfg.dataset, "dataset path")
	record := fs.String("record", "", "JSONL call record output")
	promptFile := fs.String("prompt-file", "", "extract prompt instruction file")
	jsonOut := fs.Bool("json", false, "emit JSON scorecard")
	if err := fs.Parse(args); err != nil {
		return config{}, err
	}
	if fs.NArg() != 0 {
		return config{}, fmt.Errorf("unexpected argument %q", fs.Arg(0))
	}

	cfg.dataset = *dataset
	cfg.record = *record
	cfg.promptFile = strings.TrimSpace(*promptFile)
	cfg.json = *jsonOut
	if strings.TrimSpace(*model) != "" {
		cfg.extract.Model = strings.TrimSpace(*model)
	}
	if strings.TrimSpace(*reasoning) != "" {
		parsed, err := parseReasoning(*reasoning)
		if err != nil {
			return config{}, fmt.Errorf("-reasoning: %w", err)
		}
		cfg.extract.Reasoning = parsed
	}
	if strings.TrimSpace(*temperature) != "" {
		parsed, err := strconv.ParseFloat(strings.TrimSpace(*temperature), 64)
		if err != nil {
			return config{}, fmt.Errorf("-temperature: %w", err)
		}
		cfg.extract.Temperature = &parsed
	}
	if strings.TrimSpace(*maxTokens) != "" {
		parsed, err := strconv.Atoi(strings.TrimSpace(*maxTokens))
		if err != nil {
			return config{}, fmt.Errorf("-max-tokens: %w", err)
		}
		if parsed <= 0 {
			return config{}, fmt.Errorf("-max-tokens: must be greater than zero")
		}
		cfg.extract.MaxTokens = parsed
	}
	if strings.TrimSpace(*judgeModel) != "" {
		cfg.judge.Model = strings.TrimSpace(*judgeModel)
	}
	if strings.TrimSpace(*judgeReasoning) != "" {
		parsed, err := parseReasoning(*judgeReasoning)
		if err != nil {
			return config{}, fmt.Errorf("-judge-reasoning: %w", err)
		}
		cfg.judge.Reasoning = parsed
	}
	return cfg, nil
}

func parseReasoning(raw string) (any, error) {
	switch strings.ToLower(strings.TrimSpace(raw)) {
	case "disabled", "off":
		return llm.DisableReasoning(), nil
	case "minimal", "low", "medium", "high", "xhigh", "max", "none":
		return agentkit.Level(strings.ToLower(strings.TrimSpace(raw))), nil
	default:
		return nil, fmt.Errorf("must be disabled, off, or a native reasoning level")
	}
}
