// dashboard scripts — two concerns:
//   1. keep the logged-in index's live-grants block fresh over SSE, and reflect
//      the stream's connection state in the "live" indicator dot;
//   2. copy-to-clipboard for code snippets (install one-pasters, show-once token).

// --- 1. Live grants block over SSE --------------------------------------
// The block carries data-stream (an SSE endpoint) and data-fragment (the
// grants-list HTML partial). We open an EventSource on the stream; on each
// "chains" event we re-fetch the fragment and swap it into the block, so token
// issuance / refresh / revocation reflect without a page reload. The live dot
// (#grants-live) gets .is-live while the stream is open.
(() => {
  const block = document.getElementById("grants-block");
  if (!block || !("EventSource" in window)) return;

  const stream = block.dataset.stream;
  const fragURL = block.dataset.fragment;
  if (!stream || !fragURL) return;

  const liveDot = document.getElementById("grants-live");
  const setLive = (on) => { if (liveDot) liveDot.classList.toggle("is-live", on); };

  const es = new EventSource(stream);
  es.addEventListener("open", () => setLive(true));
  es.addEventListener("error", () => setLive(false)); // browser auto-reconnects; "open" re-fires
  es.addEventListener("chains", async () => {
    try {
      const res = await fetch(fragURL, { credentials: "same-origin" });
      if (res.ok) {
        block.innerHTML = await res.text();
      }
    } catch (_) {
      // Leave the stale block in place; the next event will try again.
    }
  });
})();

// --- 2. Copy-to-clipboard ------------------------------------------------
// Every .copy-btn lives inside a .snippet next to a <code>. Clicking it copies
// that code's text and briefly confirms. Falls back to a hidden-textarea +
// execCommand when the async Clipboard API is unavailable (e.g. plain-http
// localhost without a secure context).
(() => {
  const copyText = async (text) => {
    if (navigator.clipboard && window.isSecureContext) {
      await navigator.clipboard.writeText(text);
      return;
    }
    const ta = document.createElement("textarea");
    ta.value = text;
    ta.setAttribute("readonly", "");
    ta.style.position = "absolute";
    ta.style.left = "-9999px";
    document.body.appendChild(ta);
    ta.select();
    document.execCommand("copy");
    document.body.removeChild(ta);
  };

  document.querySelectorAll(".copy-btn").forEach((btn) => {
    const snippet = btn.closest(".snippet");
    const code = snippet && snippet.querySelector("code");
    const label = btn.querySelector(".copy-label");
    if (!code) return;

    btn.addEventListener("click", async () => {
      try {
        await copyText(code.textContent.trim());
        btn.classList.add("is-copied");
        if (label) {
          const prev = label.textContent;
          label.textContent = "Copied";
          setTimeout(() => {
            label.textContent = prev;
            btn.classList.remove("is-copied");
          }, 1600);
        }
      } catch (_) {
        // Clipboard denied/unavailable: leave the user to select manually.
      }
    });
  });
})();
