<script lang="ts" setup>
import {ref} from 'vue'

const issueTemplate = `### Summary
A brief, one-sentence summary of the bug.

### Environment
- NumStore version: v1.0.0
- Install method: (tarball / Homebrew / APT / source)
- OS / Distro: (e.g., Ubuntu 22.04, macOS 14.5, Windows 11)
- CPU / Arch: (x86_64, arm64)
- Runtime/bindings: (CLI / Python / Java), versions
- Storage / FS: (ext4, APFS, NTFS), local/remote
- Config options: (WAL on/off, page size, cache size, etc.)

### Steps to Reproduce
1.
2.
3.

### Expected Result
What you expected to happen.

### Actual Result
What actually happened (include errors).

### Logs / Output
<copy the exact command, full output, and stacktrace if any>

### Minimal Repro (if possible)
<tiny script / dataset / config that reproduces the issue>

### Additional Context
<links, screenshots, notes>
`;

const subject = ref('Bug: <short-title-here>')
const body = ref(issueTemplate)

function copyTemplate() {
  navigator.clipboard?.writeText(issueTemplate).catch(() => {
  })
}

function openEmail() {
  const to = 'bugs@numstore.dev'
  const mailto = `mailto:${to}?subject=${encodeURIComponent(subject.value)}&body=${encodeURIComponent(body.value)}`
  window.location.href = mailto
}
</script>

<template>
  <main class="max-w-3xl mx-auto px-6 py-12 space-y-8">
    <!-- Header -------------------------------------------------------------->
    <section class="space-y-2">
      <h1 class="text-3xl font-bold">Bug Reporting</h1>
      <p class="text-gray-700">
        Thanks for helping improve NumStore. Please include enough detail so we can reproduce the issue quickly.
      </p>
    </section>

    <!-- Where to file ------------------------------------------------------->
    <section class="space-y-2">
      <h2 class="text-xl font-semibold">Where to file</h2>
      <ul class="list-disc pl-6 space-y-1">
        <li>
          GitHub Issues: <a class="underline" href="https://github.com/yourorg/numstore/issues" rel="noopener"
                            target="_blank">github.com/yourorg/numstore/issues</a>
        </li>
        <li>
          Email: <a class="underline" href="mailto:bugs@numstore.dev">bugs@numstore.dev</a>
        </li>
        <li>
          Security vulnerabilities: <a class="underline" href="mailto:security@numstore.dev">security@numstore.dev</a>
          (do not open a public issue)
        </li>
      </ul>
    </section>

    <!-- What to include ----------------------------------------------------->
    <section class="space-y-2">
      <h2 class="text-xl font-semibold">What to include</h2>
      <ul class="list-disc pl-6 space-y-1">
        <li><strong>Environment</strong>: NumStore version, OS/distro, CPU/arch, install method.</li>
        <li><strong>Exact steps to reproduce</strong>, expected vs actual behavior.</li>
        <li><strong>Full command and logs</strong> (no screenshots of text; copy/paste).</li>
        <li><strong>Minimal repro</strong> if possible (tiny script/dataset/config).</li>
        <li><strong>Config & flags</strong> (WAL, page/cache sizes, bindings/language versions).</li>
      </ul>
    </section>

    <!-- Quick template (copyable) ------------------------------------------>
    <section class="space-y-2">
      <h2 class="text-xl font-semibold">Issue template</h2>
      <div class="flex items-center gap-3">
        <button class="underline" type="button" @click="copyTemplate">Copy template</button>
        <span class="text-sm text-gray-600">or edit below and email it</span>
      </div>
      <pre class="text-sm bg-gray-50 p-3 rounded overflow-auto whitespace-pre-wrap">{{ body }}</pre>

      <!-- Simple email helper (optional) -->
      <div class="space-y-2">
        <label class="block text-sm font-medium">Email subject</label>
        <input v-model="subject" class="w-full border rounded px-3 py-2 text-sm"/>
        <label class="block text-sm font-medium mt-2">Email body</label>
        <textarea v-model="body" class="w-full border rounded px-3 py-2 text-sm h-48"></textarea>
        <div class="pt-2">
          <button class="underline" type="button" @click="openEmail">Open email draft</button>
        </div>
      </div>
    </section>

    <!-- Severity guidance --------------------------------------------------->
    <section class="space-y-2">
      <h2 class="text-xl font-semibold">Severity guidance</h2>
      <ul class="list-disc pl-6 space-y-1">
        <li><strong>P1 – Critical:</strong> production down, data loss/corruption risk, security exposure.</li>
        <li><strong>P2 – High:</strong> major feature broken or severe performance degradation; workaround exists.</li>
        <li><strong>P3 – Normal:</strong> functional bug or incomplete behavior; not blocking production.</li>
        <li><strong>P4 – Low:</strong> cosmetic/typo/docs.</li>
      </ul>
    </section>

    <!-- Data and privacy ---------------------------------------------------->
    <section class="space-y-2">
      <h2 class="text-xl font-semibold">Data & privacy</h2>
      <ul class="list-disc pl-6 space-y-1">
        <li>Redact secrets, API keys, and personally identifiable information.</li>
        <li>For private logs or datasets, share via email and mark as confidential.</li>
      </ul>
    </section>
  </main>
</template>

<style scoped>
/* minimal, readable */
</style>
