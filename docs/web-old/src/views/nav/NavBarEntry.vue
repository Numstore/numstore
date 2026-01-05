<template>
  <!-- Hoverable group wrapper -->
  <div :class="['relative group', groupHoverClass]">

    <!-- Button -->
    <button class="flex items-center px-3 py-2">
      {{ label }}
      <i class="fa-solid fa-caret-down ml-2 text-sm"></i>
    </button>

    <!-- Dropdown -->
    <div
        :class="[
        'absolute left-0 w-40 ',
        'group-hover:opacity-100 invisible group-hover:visible',
        bgClass
      ]"
    >
      <RouterLink
          v-for="(item, i) in items"
          :key="i"
          :class="['block px-4 py-2', itemHoverClass]"
          :to="item.href"
      >
        {{ item.text }}
      </RouterLink>
    </div>
  </div>
</template>

<script lang="ts" setup>
import {RouterLink} from "vue-router";

interface DropdownItem {
  text: string
  href: string
}

/* ---------- props ---------- */
const props = withDefaults(
    defineProps<{
      label: string
      items: DropdownItem[]
      bgClass?: string
      groupHoverClass?: string
      itemHoverClass?: string
    }>(),
    {
      bgClass: 'bg-blue-400',
      groupHoverClass: 'hover:bg-blue-300',
      itemHoverClass: 'hover:bg-blue-500'
    }
)

const {label, items, bgClass, groupHoverClass, itemHoverClass} = props
</script>
