<script lang="ts" setup>
const tracks = [
  {
    name: "Operator Essentials", length: "1 day", audience: "SREs / DevOps", takeaways: [
      "Install, configure, and upgrade NumStore",
      "Backups, WAL, and recovery drills",
      "Observability & on-call runbooks"
    ]
  },
  {
    name: "Developer Fundamentals", length: "1 day", audience: "Backend / Data engineers", takeaways: [
      "Data modeling for numeric workloads",
      "Client libraries & transactions",
      "Performance-aware read/write patterns"
    ]
  },
  {
    name: "Performance & Internals", length: "2 days", audience: "Leads / Architects", takeaways: [
      "Pager, buffer, and WAL internals",
      "Benchmarking & profiling methodology",
      "Capacity planning and tuning"
    ]
  }
]

const formats = [
  "Remote live (Zoom/Meet) — interactive labs",
  "On-site workshop — whiteboard + hands-on",
  "Private cohort — tailored to your stack"
]

const materials = [
  "Slides & lab guides (PDF)",
  "Sample datasets and workloads",
  "Reference configs, dashboards, and scripts",
  "Certificate of completion (optional)"
]
</script>

<template>
  <main class="max-w-3xl mx-auto px-6 py-12 space-y-10">
    <section class="space-y-2">
      <h1 class="text-3xl font-bold">Training</h1>
      <p class="text-gray-700">
        Short, practical courses to ramp up teams on NumStore operations, development, and performance.
      </p>
    </section>

    <section class="space-y-3">
      <h2 class="text-xl font-semibold">Tracks</h2>
      <ul class="list-disc pl-6 space-y-3">
        <li v-for="t in tracks" :key="t.name">
          <strong>{{ t.name }}</strong> — {{ t.length }} · {{ t.audience }}
          <ul class="list-disc pl-6 mt-1 space-y-1">
            <li v-for="(x,i) in t.takeaways" :key="i">{{ x }}</li>
          </ul>
        </li>
      </ul>
    </section>

    <section class="space-y-3">
      <h2 class="text-xl font-semibold">Formats</h2>
      <ul class="list-disc pl-6 space-y-1">
        <li v-for="(f, i) in formats" :key="i">{{ f }}</li>
      </ul>
    </section>

    <section class="space-y-3">
      <h2 class="text-xl font-semibold">Materials</h2>
      <ul class="list-disc pl-6 space-y-1">
        <li v-for="(m, i) in materials" :key="i">{{ m }}</li>
      </ul>
    </section>

    <section class="space-y-2">
      <h2 class="text-xl font-semibold">Request a session</h2>
      <ul class="list-disc pl-6 space-y-1">
        <li>Email: <a class="underline" href="mailto:training@numstore.dev">training@numstore.dev</a></li>
        <li>Group size & dates: include preferred weeks and time zone</li>
        <li>Custom topics: add your tools (e.g., k8s, Prometheus, Java/Python bindings)</li>
      </ul>
    </section>
  </main>
</template>
