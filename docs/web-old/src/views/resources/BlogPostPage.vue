<!-- src/views/resources/BlogPostPage.vue -->
<template>
  <main class="max-w-3xl mx-auto px-6 py-12 space-y-4">
    <!-- force re-render when slug changes -->
    <component :is="Resolved" v-if="Resolved" :key="slug"/>

    <div v-else>
      <h1 class="text-2xl font-bold">Not found</h1>
      <p>This post could not be located.</p>
      <p class="pt-2"><a class="underline" href="/resources/blog">Back to blog</a></p>

      <!-- debug -->
      <div class="mt-6 border-t pt-4 text-sm">
        <p><strong>Debug</strong></p>
        <p>slug: <code>{{ slug }}</code></p>
        <p>expected key: <code>/src/views/resources/blog_entries/{{ slug }}.vue</code></p>
        <p>glob matched {{ keys.length }} file(s):</p>
        <ul class="list-disc pl-6">
          <li v-for="k in keys" :key="k"><code>{{ k }}</code></li>
        </ul>
        <p>module keys for this slug:</p>
        <pre class="whitespace-pre-wrap text-xs">{{ JSON.stringify(entry, null, 2) }}</pre>
      </div>
    </div>
  </main>
</template>

<script lang="ts" setup>
import {computed, onMounted, watch} from 'vue'
import {useRoute} from 'vue-router'

interface Meta {
  title: string;
  date: string
}

interface EntryModule {
  default?: any;
  meta?: Meta;
  summary?: string
}

// IMPORTANT: do NOT use '@' in glob patterns.
// Keep your exact folder:
const modules = import.meta.glob(
    '/src/views/resources/blog_entries/*.vue',
    {eager: true}
) as Record<string, EntryModule>

const bySlug: Record<string, EntryModule> = {}
for (const p in modules) {
  const fname = p.split('/').pop()!            // e.g. "2025-09-05_release-1-0-0.vue"
  const key = fname.replace(/\.vue$/, '')      // -> "2025-09-05_release-1-0-0"
  bySlug[key] = modules[p]
}

const route = useRoute()
const slug = computed(() => (route.params.slug as string) || '')

// raw module
const entry = computed(() => bySlug[slug.value])

// the actual component to render; guard .default
const Resolved = computed(() => entry.value?.default ?? null)

// handy debug
const keys = Object.keys(modules)

// console logs so you can see what's going on in devtools
const log = () => {
  console.log('[BlogPostPage] slug =', slug.value)
  console.log('[BlogPostPage] keys =', keys)
  console.log('[BlogPostPage] entry =', entry.value)
  console.log('[BlogPostPage] default =', entry.value?.default)
}
onMounted(log)
watch(slug, log)
</script>
