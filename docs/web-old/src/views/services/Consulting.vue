<script lang="ts" setup>
const offerings = [
  "Architecture reviews (storage layout, WAL, recovery, durability)",
  "Performance tuning & benchmarking (ingest, query, memory, IO)",
  "Migrations & upgrades (zero/low-downtime, rollback plans)",
  "Observability setup (logs, metrics, tracing, dashboards)",
  "Capacity planning & cost modeling",
  "Security reviews (CVE posture, supply chain, hardening)"
]

const engagementModels = [
  "Advisory (hourly/blocks)",
  "Fixed-scope engagement (statement of work)",
  "Embedded engineer (weekly/monthly allocation)"
]

const deliverables = [
  "Written report with findings and prioritized recommendations",
  "Issue backlog with task breakdown and severity/impact",
  "Benchmarks, profiles, and reproducible test scripts",
  "Runbooks and operational checklists"
]
</script>

<template>
  <main class="max-w-3xl mx-auto px-6 py-12 space-y-10">
    <section class="space-y-2">
      <h1 class="text-3xl font-bold">Consulting</h1>
      <p class="text-gray-700">
        Pragmatic help to ship, scale, and stabilize NumStore in real systems.
      </p>
    </section>

    <section class="space-y-3">
      <h2 class="text-xl font-semibold">What we do</h2>
      <ul class="list-disc pl-6 space-y-1">
        <li v-for="(item, i) in offerings" :key="i">{{ item }}</li>
      </ul>
    </section>

    <section class="space-y-3">
      <h2 class="text-xl font-semibold">Engagement models</h2>
      <ul class="list-disc pl-6 space-y-1">
        <li v-for="(m, i) in engagementModels" :key="i">{{ m }}</li>
      </ul>
    </section>

    <section class="space-y-3">
      <h2 class="text-xl font-semibold">Deliverables</h2>
      <ul class="list-disc pl-6 space-y-1">
        <li v-for="(d, i) in deliverables" :key="i">{{ d }}</li>
      </ul>
    </section>

    <section class="space-y-2">
      <h2 class="text-xl font-semibold">Get in touch</h2>
      <ul class="list-disc pl-6 space-y-1">
        <li>Email: <a class="underline" href="mailto:consulting@numstore.dev">consulting@numstore.dev</a></li>
        <li>Book a call: <a class="underline" href="/about/contact">/about/contact</a></li>
        <li>Enterprise Support: <a class="underline"
                                   href="/services/enterprise_support">/services/enterprise_support</a></li>
      </ul>
    </section>
  </main>
</template>
