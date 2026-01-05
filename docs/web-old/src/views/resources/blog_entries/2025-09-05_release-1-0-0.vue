<!-- Directory: /blog/entries/2025-09-05_release-1-0-0.vue -->
<template>
  <article class="prose max-w-3xl mx-auto px-6 py-12">
    <h1>{{ meta.title }}</h1>
    <p>
      NumStore v1.0.0 is out. This first stable release includes the core R+ tree engine, a simple WAL with
      two-phase commit semantics, and Python/Java bindings.
    </p>
    <h2>Highlights</h2>
    <ul>
      <li>O(log n) insert/search with the R+ tree core.</li>
      <li>Write-ahead logging and rollback support.</li>
      <li>Bindings for Python and Java.</li>
    </ul>
  </article>
</template>


<script lang="ts" setup>
const meta = {
  title: 'Release: NumStore v1.0.0',
  date: '2025-09-05',
}
</script>