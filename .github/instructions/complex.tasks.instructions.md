## Handling Complex or Unfamiliar Problems

### Before writing any code

1. **State your interpretation** – Summarise your understanding of the problem in 2–3 sentences before starting. If anything is ambiguous, ask a clarifying question rather than assuming.
2. **Decompose** – Break the problem into sub-problems and briefly list the planned approach for each. Do not jump straight to implementation.

### While solving

3. **Self-check at each major step** – After completing each sub-task, verify the output meets the requirement before moving on. Do not proceed if the current step is broken.
4. **Use web search when stuck or in unfamiliar territory:**
   - After **3 failed attempts** on the same problem — stop, search, then try again.
   - When entering a **new domain or library** not previously seen in this codebase — search *before* writing code.
   - When **unsure about an API, SDK, or config option** — look up the official docs instead of guessing.
   - Search for: official docs for the version in use, existing libraries that fit our stack, established patterns, known bugs/workarounds.
5. **Prefer test-first on hard problems** – Write the expected behaviour (tests or assertions) before the implementation when the problem is unclear. It forces the problem to be well-defined.

### When delivering

6. **Explain non-obvious choices** – Add a short comment or PR description note for any approach that isn't immediately self-evident: what was tried, what was found, and why this solution was chosen.
7. **Flag new paradigms** – If the best solution requires introducing a pattern, library, or architectural concept not currently in the codebase, flag it explicitly rather than silently adding it. Let the reviewer decide.