package model

import "testing"

// TestEmbeddingPricingKnownModel verifies text-embedding-3-large carries a
// non-zero rate so the embed call's cost_usd (P0c) is real, not zero
// (R-ZZLK-I9CK).
func TestEmbeddingPricingKnownModel(t *testing.T) {
	spec, ok := EmbeddingPricing("text-embedding-3-large")
	if !ok {
		t.Fatal("EmbeddingPricing(text-embedding-3-large): not registered")
	}
	if spec.InputPerM <= 0 {
		t.Errorf("InputPerM = %v, want > 0", spec.InputPerM)
	}
	// 1M input tokens cost exactly the per-million rate.
	if got := spec.ComputeCost(1_000_000); got != spec.InputPerM {
		t.Errorf("ComputeCost(1M) = %v, want %v", got, spec.InputPerM)
	}
	if got := spec.ComputeCost(0); got != 0 {
		t.Errorf("ComputeCost(0) = %v, want 0", got)
	}
}

// TestEmbeddingPricingUnknownModel verifies an unregistered model reports
// false so a caller can refuse to bill a model with unknown pricing.
func TestEmbeddingPricingUnknownModel(t *testing.T) {
	if _, ok := EmbeddingPricing("nonesuch-embed"); ok {
		t.Error("EmbeddingPricing(nonesuch-embed) = ok, want not registered")
	}
}
