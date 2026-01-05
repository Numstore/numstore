<script lang="ts" setup>
import {computed} from 'vue'
import {citeNumber} from '@/lib/refs'

const props = defineProps<{ keyId: string }>() // "key" is reserved in Vue templates
const n = computed(() => citeNumber(props.keyId))
</script>

<template>
  <sup v-if="n !== undefined">
    <a :href="`/resources/references#ref-${n}`" class="align-super no-underline text-blue-600">[{{ n }}]</a>
  </sup>
  <sup v-else>
    <span class="align-super text-red-600" title="Unknown reference key">[?]</span>
  </sup>
</template>