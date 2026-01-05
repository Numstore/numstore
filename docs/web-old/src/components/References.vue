<script setup lang="ts">
import { getReferences } from '@/lib/refs'

const refs = getReferences()

function fmt(r: any) {
  const parts = []
  if (r.authors) parts.push(r.authors)
  if (r.year) parts.push(`(${r.year})`)
  if (r.title) parts.push(r.title)
  if (r.venue) parts.push(r.venue)
  if (r.url) parts.push(r.url)
  if (r.accessed) parts.push(`[accessed ${r.accessed}]`)
  if (r.details) parts.push(r.details)
  return parts.filter(Boolean).join('. ')
}
</script>

<template>
  <section v-if="refs.length" class="mt-10">
    <h2 id="references" class="text-xl font-semibold">References</h2>
    <ol class="list-decimal ml-6 mt-3 space-y-1">
      <li v-for="r in refs" :key="r.n" :id="`ref-${r.n}`">
        {{ fmt(r) }}
      </li>
    </ol>
  </section>
</template>
