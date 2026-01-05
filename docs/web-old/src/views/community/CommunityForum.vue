<script lang="ts" setup>
import {computed, ref} from 'vue'
import {RouterLink} from 'vue-router'

type Category = { id: string; name: string; desc: string }
type Thread = {
  id: string
  title: string
  category: string
  author: string
  replies: number
  lastActivity: string // YYYY-MM-DD
  url?: string         // external or internal href
}

const categories: Category[] = [
  {id: 'announcements', name: 'Announcements', desc: 'Releases and important updates.'},
  {id: 'help', name: 'Help', desc: 'Ask questions and get support.'},
  {id: 'howto', name: 'How-To', desc: 'Tips, scripts, and best practices.'},
  {id: 'feedback', name: 'Feedback', desc: 'Feature requests and product feedback.'},
]

const threads = ref<Thread[]>([
  {
    id: 't1',
    title: 'NumStore v1.0.0 Released',
    category: 'announcements',
    author: 'team',
    replies: 12,
    lastActivity: '2025-09-05',
    url: '/resources/blog/2025-09-05_release-1-0-0'
  },
  {
    id: 't2',
    title: 'WAL tuning for high-ingest workloads',
    category: 'howto',
    author: 'alex',
    replies: 7,
    lastActivity: '2025-09-03'
  },
  {
    id: 't3',
    title: 'Install fails on Ubuntu 22.04',
    category: 'help',
    author: 'sam',
    replies: 4,
    lastActivity: '2025-09-02'
  },
  {
    id: 't4',
    title: 'Request: read-only role with snapshot export',
    category: 'feedback',
    author: 'jordan',
    replies: 3,
    lastActivity: '2025-08-28'
  },
])

// Filters
const q = ref('')
const cat = ref<string | 'all'>('all')

const filtered = computed(() => {
  const query = q.value.trim().toLowerCase()
  return threads.value
      .filter(t => (cat.value === 'all' ? true : t.category === cat.value))
      .filter(t => {
        if (!query) return true
        return (
            t.title.toLowerCase().includes(query) ||
            t.author.toLowerCase().includes(query)
        )
      })
      // newest activity first
      .sort((a, b) => (a.lastActivity < b.lastActivity ? 1 : -1))
})

// New topic (placeholder only)
const newTopic = ref({
  title: '',
  category: 'help' as string,
  body: ''
})

function submitTopic() {
  // placeholder: just add to local list
  if (!newTopic.value.title.trim()) return
  const id = 't' + (threads.value.length + 1)
  threads.value.unshift({
    id,
    title: newTopic.value.title.trim(),
    category: newTopic.value.category,
    author: 'you',
    replies: 0,
    lastActivity: new Date().toISOString().slice(0, 10),
  })
  newTopic.value = {title: '', category: 'help', body: ''}
}
</script>

<template>
  <main class="max-w-5xl mx-auto px-6 py-12 space-y-10">
    <!-- Header -->
    <section class="space-y-2">
      <h1 class="text-3xl font-bold">Community Forum</h1>
      <p class="text-gray-700">
        Ask questions, share tips, and discuss NumStore with the community.
      </p>
    </section>

    <!-- Categories -->
    <section class="space-y-3">
      <h2 class="text-xl font-semibold">Categories</h2>
      <ul class="grid sm:grid-cols-2 gap-4">
        <li v-for="c in categories" :key="c.id" class="border rounded p-4">
          <div class="flex items-center justify-between gap-4">
            <div>
              <strong>{{ c.name }}</strong>
              <div class="text-sm text-gray-600">{{ c.desc }}</div>
            </div>
            <button
                class="underline whitespace-nowrap"
                @click="cat = c.id"
            >View
            </button>
          </div>
        </li>
      </ul>
    </section>

    <!-- Thread list -->
    <section class="space-y-3">
      <h2 class="text-xl font-semibold">Threads</h2>

      <!-- Filters -->
      <div class="flex flex-wrap gap-3 items-center">
        <label class="text-sm">Category:</label>
        <select v-model="cat" class="border rounded px-3 py-2 text-sm">
          <option value="all">All</option>
          <option v-for="c in categories" :key="c.id" :value="c.id">{{ c.name }}</option>
        </select>

        <input
            v-model="q"
            class="border rounded px-3 py-2 text-sm flex-1 min-w-[200px]"
            placeholder="Search title or author…"
            type="search"
        />
      </div>

      <div class="border rounded">
        <table class="w-full border-collapse text-sm">
          <thead>
          <tr class="text-left border-b">
            <th class="py-2 px-3">Title</th>
            <th class="py-2 px-3">Category</th>
            <th class="py-2 px-3">Author</th>
            <th class="py-2 px-3">Replies</th>
            <th class="py-2 px-3">Last activity</th>
          </tr>
          </thead>
          <tbody>
          <tr v-for="t in filtered" :key="t.id" class="border-b">
            <td class="py-2 px-3">
              <!-- Link: use RouterLink for internal URLs, <a> otherwise -->
              <RouterLink
                  v-if="t.url && t.url.startsWith('/')"
                  :to="t.url"
                  class="underline"
              >{{ t.title }}
              </RouterLink>
              <a
                  v-else-if="t.url"
                  :href="t.url"
                  class="underline"
                  rel="noopener" target="_blank"
              >{{ t.title }}</a>
              <span v-else>{{ t.title }}</span>
            </td>
            <td class="py-2 px-3">{{ categories.find(c => c.id === t.category)?.name || t.category }}</td>
            <td class="py-2 px-3">{{ t.author }}</td>
            <td class="py-2 px-3">{{ t.replies }}</td>
            <td class="py-2 px-3 whitespace-nowrap">{{ t.lastActivity }}</td>
          </tr>
          <tr v-if="filtered.length === 0">
            <td class="py-6 px-3 text-center text-gray-600" colspan="5">No threads found.</td>
          </tr>
          </tbody>
        </table>
      </div>
    </section>

    <!-- New Topic (placeholder) -->
    <section class="space-y-3">
      <h2 class="text-xl font-semibold">Start a new topic</h2>
      <div class="space-y-2">
        <label class="block text-sm font-medium">Title</label>
        <input v-model="newTopic.title" class="w-full border rounded px-3 py-2 text-sm"/>
      </div>

      <div class="space-y-2">
        <label class="block text-sm font-medium">Category</label>
        <select v-model="newTopic.category" class="border rounded px-3 py-2 text-sm">
          <option v-for="c in categories" :key="c.id" :value="c.id">{{ c.name }}</option>
        </select>
      </div>

      <div class="space-y-2">
        <label class="block text-sm font-medium">Body (optional)</label>
        <textarea v-model="newTopic.body" class="w-full border rounded px-3 py-2 text-sm h-32"></textarea>
      </div>

      <button class="underline" type="button" @click="submitTopic">Post (local demo)</button>

      <p class="text-xs text-gray-600">
        This is a static demo. Wire this button to your backend or to an external forum (Discourse, Discord, etc.).
      </p>
    </section>
  </main>
</template>

<style scoped>
/* minimal and readable */
</style>
