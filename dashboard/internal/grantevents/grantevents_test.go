package grantevents

import (
	"testing"
	"time"
)

// TestSubscribeReceivesOnPublish: a subscriber gets a notify when Publish is
// called for its owner.
func TestSubscribeReceivesOnPublish(t *testing.T) {
	b := New()
	ch, cancel := b.Subscribe("owner@int.ikigenba.com")
	defer cancel()

	b.Publish("owner@int.ikigenba.com")
	select {
	case <-ch:
	case <-time.After(time.Second):
		t.Fatal("expected a notify after Publish, got none")
	}
}

// TestPublishNonBlockingWhenBufferFull: a second Publish without an intervening
// receive does not block and does not enqueue a second notify (buffer is 1).
func TestPublishNonBlockingWhenBufferFull(t *testing.T) {
	b := New()
	ch, cancel := b.Subscribe("owner@int.ikigenba.com")
	defer cancel()

	b.Publish("owner@int.ikigenba.com")
	b.Publish("owner@int.ikigenba.com") // must not block, must coalesce

	// First receive succeeds.
	select {
	case <-ch:
	case <-time.After(time.Second):
		t.Fatal("expected first notify")
	}
	// No second notify is buffered.
	select {
	case <-ch:
		t.Fatal("expected only one buffered notify, got a second")
	default:
	}
}

// TestPublishOtherOwnerNotDelivered: Publish for a different owner does not
// wake this subscriber.
func TestPublishOtherOwnerNotDelivered(t *testing.T) {
	b := New()
	ch, cancel := b.Subscribe("a@int.ikigenba.com")
	defer cancel()

	b.Publish("b@int.ikigenba.com")
	select {
	case <-ch:
		t.Fatal("notify delivered to the wrong owner")
	case <-time.After(50 * time.Millisecond):
	}
}

// TestCancelUnregisters: after cancel, Publish neither panics nor delivers.
func TestCancelUnregisters(t *testing.T) {
	b := New()
	ch, cancel := b.Subscribe("owner@int.ikigenba.com")
	cancel()

	b.Publish("owner@int.ikigenba.com") // must not panic; subscriber is gone
	select {
	case <-ch:
		t.Fatal("notify delivered after cancel")
	case <-time.After(50 * time.Millisecond):
	}

	// Double cancel is safe.
	cancel()
}

// TestPublishEmptyOwnerNoop: empty owner is a no-op.
func TestPublishEmptyOwnerNoop(t *testing.T) {
	b := New()
	ch, cancel := b.Subscribe("")
	defer cancel()
	b.Publish("")
	select {
	case <-ch:
		t.Fatal("empty-owner Publish should be a no-op")
	case <-time.After(50 * time.Millisecond):
	}
}

// TestNilBusPublishNoop: a nil Bus Publish is a safe no-op.
func TestNilBusPublishNoop(t *testing.T) {
	var b *Bus
	b.Publish("owner@int.ikigenba.com")
}
