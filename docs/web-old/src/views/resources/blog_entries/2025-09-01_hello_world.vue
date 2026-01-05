<template>
  <article class="prose max-w-3xl mx-auto px-6 py-12">
    <h1>{{ meta.title }}</h1>
    <p>
      NumStore now has a tiny blog! This is a short hello and a note on what to expect: concise posts focused on
      releases, performance notes, and low-level implementation details.
    </p>
    <p>
      Future posts will cover the WAL internals, JNI bindings, and how we compare to columnar engines for numeric
      workloads.
    </p>
  </article>
</template>


<script lang="ts" setup>
const meta = {
  title: 'Hello, world',
  date: '2025-09-01',
}
</script>