%title: Ikigai
%author: Michael Greenly
%date: 12/20/2025

# MVP or Bust! (Day 1 of 14)

I'm using my holiday vacation to cunch and see just how close to the MVP we
can get. Today we're working on rel-07 which will add a generic interface for
the AI Provider's and concrete implementation's for OpenAI, Anthropic and Google.

The approach we're taking is to have the internal format be a true superset of
capabilities of each provider and and then use the interfaces to translate to
the over-the-wire format. We'll polyfil some behaviors for the providers that don't
support all the ones we need but we'll also store the raw responses for features
we don't support and are not using, primarily for debugging if we need it.
