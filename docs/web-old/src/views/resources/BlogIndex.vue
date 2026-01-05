<template>
  <main class="max-w-3xl mx-auto px-6 py-12">
    <h1 class="text-2xl font-bold mb-4">NumStore Blog</h1>
    <ul class="space-y-4 list-disc pl-6">
      <li v-for="post in posts" :key="post.slug" class="space-y-1">
        <div>
          <RouterLink :to="`/resources/blog/${post.slug}`" class="underline">
            {{ post.meta.title }}
          </RouterLink>
          <span class="text-sm text-gray-600"> — {{ post.meta.date }}</span>
        </div>
        <p class="text-sm">{{ post.summary }}</p>
      </li>
    </ul>
  </main>
</template>

<script lang="ts" setup>

interface Meta {
  title: string;
  date: string
}

interface EntryModule {
  default: any;
  meta?: Meta;
  summary?: string
}

const modules = import.meta.glob('./blog_entries/*.vue', {eager: true}) as Record<string, EntryModule>

function byDateDesc(a: string, b: string): number {
// Prefer meta.date if present; otherwise, parse leading YYYY-MM-DD from filename
  const am = modules[a].meta?.date || a.match(/(\d{4}-\d{2}-\d{2})/)?.[1] || '1970-01-01'
  const bm = modules[b].meta?.date || b.match(/(\d{4}-\d{2}-\d{2})/)?.[1] || '1970-01-01'
  return am < bm ? 1 : am > bm ? -1 : 0
}

const posts = Object.keys(modules)
    .sort(byDateDesc)
    .map((path) => {
      const slug = path.split('/').pop()!.replace('.vue', '')
      const mod = modules[path]
      return {
        slug,
        meta: mod.meta || {title: slug, date: slug.slice(0, 10)},
        summary: mod.summary || '',
      }
    })
</script>