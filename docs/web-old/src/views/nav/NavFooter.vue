<!-- components/SiteFooter.vue -->
<script lang="ts" setup>
import {RouterLink} from 'vue-router'

interface LinkItem {
  text: string
  href: string
}

interface Group {
  label: string
  links: LinkItem[]
}

const groups: Group[] = [
  {
    label: 'Downloads',
    links: [
      {text: 'Current', href: '/downloads/current'},
      {text: 'Archive', href: '/downloads/archive'}
    ]
  },
  {
    label: 'Resources',
    links: [
      {text: 'Documentation', href: '/resources/documentation'},
      {text: 'Blog', href: '/resources/blog'},
      {text: 'References', href: '/resources/references'}
    ]
  },
  {
    label: 'Services',
    links: [
      {text: 'Enterprise Support', href: '/services/enterprise_support'},
      {text: 'Consulting', href: '/services/consulting'},
      {text: 'Training', href: '/services/training'}
    ]
  },
  {
    label: 'Community',
    links: [
      {text: 'Bug Reporting', href: '/community/bug_reporting'},
      {text: 'Forum', href: '/community/forum'}
    ]
  },
  {
    label: 'About',
    links: [
      {text: 'Company', href: '/about/company'},
      {text: 'Contact', href: '/about/contact'}
    ]
  }
]
</script>

<template>
  <footer class="bg-gray-900 text-gray-300 text-sm">
    <div
        class="max-w-7xl mx-auto py-10 px-4 grid gap-8
             grid-cols-2 sm:grid-cols-3 md:grid-cols-5"
    >
      <div v-for="group in groups" :key="group.label">
        <h3 class="font-medium text-gray-100 mb-2">
          {{ group.label }}
        </h3>

        <ul class="space-y-1">
          <li v-for="link in group.links" :key="link.href">
            <RouterLink
                :to="link.href"
                class="hover:text-white transition-colors duration-150"
            >
              {{ link.text }}
            </RouterLink>
          </li>
        </ul>
      </div>
    </div>
    <div class="bg-green-300 text-center text-black text-sm">
      Copyright © 2001-2025, The Company LLC.
    </div>
  </footer>
</template>
