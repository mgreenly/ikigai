package script

import (
	"errors"
	"strings"
	"testing"
)

func TestValidateTriggerWellFormedness(t *testing.T) {
	// R-7UZ2-4KOT
	for _, filter := range []string{"create/bills/**", ":create/**", "*:create/**", "drop*:create/**", "github:push/**"} {
		_, err := validateTrigger(filter)
		if !errors.Is(err, ErrValidation) {
			t.Fatalf("%q: %v", filter, err)
		}
	}
	_, err := validateTrigger("github:push/**")
	for _, source := range []string{"cron", "crm", "ledger", "dropbox", "prompts"} {
		if !strings.Contains(err.Error(), source) {
			t.Fatalf("unknown source error does not name %q: %v", source, err)
		}
	}
	if strings.Contains(err.Error(), "scripts") {
		t.Fatalf("unknown source error = %v", err)
	}
	if source, err := validateTrigger("dropbox:create/bills/**"); err != nil || source != "dropbox" {
		t.Fatalf("valid filter = %q, %v", source, err)
	}
}

func TestValidateTriggerFamilies(t *testing.T) {
	// R-7W6Y-ICFI
	for _, filter := range []string{"dropbox:create/bills/**/*.pdf", "dropbox:*", "cron:tick/some-schedule-nobody-declared"} {
		if _, err := validateTrigger(filter); err != nil {
			t.Fatalf("%q: %v", filter, err)
		}
	}
	for _, filter := range []string{"dropbox:nosuchkind/**", "dropbox:create/["} {
		if _, err := validateTrigger(filter); !errors.Is(err, ErrValidation) {
			t.Fatalf("%q: %v", filter, err)
		}
	}
}
