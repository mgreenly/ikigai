package script

import (
	"errors"
	"testing"
)

func TestValidateTrigger(t *testing.T) {
	tests := []struct {
		name        string
		source      string
		eventFilter string
		wantErr     bool
	}{
		// crm
		{"crm exact", "crm", "contact.created", false},
		{"crm glob", "crm", "contact.*", false},
		{"crm wildcard all", "crm", "*", false},
		{"crm bad type", "crm", "contact.deleted", true},
		{"crm wrong domain", "crm", "transaction.recorded", true},

		// ledger
		{"ledger exact", "ledger", "transaction.recorded", false},
		{"ledger bad", "ledger", "transaction.voided", true},

		// dropbox
		{"dropbox exact", "dropbox", "file.modified", false},
		{"dropbox glob", "dropbox", "file.*", false},
		{"dropbox bad", "dropbox", "folder.created", true},

		// prompts
		{"prompts succeeded", "prompts", "run.succeeded", false},
		{"prompts failed", "prompts", "run.failed", false},
		{"prompts glob", "prompts", "run.*", false},
		{"prompts bad", "prompts", "run.queued", true},

		// cron is dynamic
		{"cron named", "cron", "cron.nightly", false},
		{"cron glob", "cron", "cron.*", false},
		{"cron non-cron filter", "cron", "contact.created", true},

		// unknown / self-chaining (deferred) / empty
		{"unknown source", "agent", "run.failed", true},
		{"self-chaining deferred", "scripts", "scripts.succeeded", true},
		{"empty filter", "crm", "", true},
		{"empty source", "", "contact.created", true},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			err := validateTrigger(tc.source, tc.eventFilter)
			if tc.wantErr {
				if err == nil {
					t.Fatalf("validateTrigger(%q,%q) = nil, want error", tc.source, tc.eventFilter)
				}
				if !errors.Is(err, ErrValidation) {
					t.Fatalf("error is not ErrValidation: %v", err)
				}
				return
			}
			if err != nil {
				t.Fatalf("validateTrigger(%q,%q) = %v, want nil", tc.source, tc.eventFilter, err)
			}
		})
	}
}

func TestGlobMatch(t *testing.T) {
	tests := []struct {
		pattern, s string
		want       bool
	}{
		{"contact.*", "contact.created", true},
		{"contact.*", "contact.tagged", true},
		{"contact.*", "transaction.recorded", false},
		{"*", "anything.at.all", true},
		{"run.succeeded", "run.succeeded", true},
		{"run.succeeded", "run.failed", false},
		{"file.*", "file.deleted", true},
		{"[", "anything", false}, // malformed pattern matches nothing
	}
	for _, tc := range tests {
		t.Run(tc.pattern+"~"+tc.s, func(t *testing.T) {
			if got := globMatch(tc.pattern, tc.s); got != tc.want {
				t.Errorf("globMatch(%q,%q) = %v, want %v", tc.pattern, tc.s, got, tc.want)
			}
		})
	}
}

func TestTriggerSources(t *testing.T) {
	srcs := triggerSources()
	want := map[string]bool{"cron": true, "crm": true, "ledger": true, "dropbox": true, "prompts": true}
	if len(srcs) != len(want) {
		t.Fatalf("triggerSources len = %d (%v), want %d", len(srcs), srcs, len(want))
	}
	for _, s := range srcs {
		if !want[s] {
			t.Errorf("unexpected source %q", s)
		}
	}
}
