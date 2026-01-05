<script lang="ts" setup>
/* give the component a self-name so recursion works */
defineOptions({ name: 'TypeEditor' })

import { computed, reactive, toRaw, watch } from 'vue'
import { type Field, scalarOptions, type Type } from '@/lib/types'

/* ───── props / emit ───── */
const props = defineProps<{ modelValue: Type }>()
const emit = defineEmits<{ 'update:modelValue': [Type] }>()

/* ───── safe clone util (no structuredClone) ───── */
const clone = <T,>(v: T): T => JSON.parse(JSON.stringify(v))

/* ───── local editable copy ───── */
const t = reactive<Type>(clone(toRaw(props.modelValue)))

/* emit a *plain* clone whenever t changes */
watch(
  () => t,
  () => emit('update:modelValue', clone(toRaw(t))),
  { deep: true },
)

/* ───── collapse toggle ───── */
const collapsed = reactive({ on: false })

/* ───── icons & kind helpers ───── */
const kinds = ['struct', 'union', 'enum', 'scalar'] as const
const icons: Record<(typeof kinds)[number], string> = {
  struct: 'fa-cube',
  union: 'fa-layer-group',
  enum: 'fa-list-ol',
  scalar: 'fa-hashtag',
}

function switchKind(k: (typeof kinds)[number]) {
  if (t.kind === k) return
  Object.assign(t, { kind: k }) // override kind
  if (k === 'struct') (t as any).fields = []
  if (k === 'union') (t as any).variants = []
  if (k === 'enum') (t as any).options = []
  if (k === 'scalar') (t as any).name = 'U32'
}

/* ───── shared helpers for struct/union lists ───── */
const kvArray = computed<Field[]>(() =>
  t.kind === 'struct' ? (t as any).fields : t.kind === 'union' ? (t as any).variants : [],
)

function addKV() {
  kvArray.value.push({
    key: '',
    type: { kind: 'scalar', name: 'U32' },
  })
}
</script>

<template>
  <!-- COLLAPSED SUMMARY -->
  <div v-if="collapsed.on" class="flex items-center gap-2">
    <i :class="['fas', icons[t.kind]]" />
    <span class="text-sm">
      {{
        t.kind === 'scalar'
          ? `Scalar: ${(t as any).name}`
          : t.kind === 'enum'
            ? `Enum (${(t as any).options.length})`
            : t.kind === 'struct'
              ? `Struct (${(t as any).fields.length} fields)`
              : `Union (${(t as any).variants.length} variants)`
      }}
    </span>
    <button class="text-blue-600 text-xs" @click="collapsed.on = false">Edit</button>
  </div>

  <!-- FULL EDITOR -->
  <div v-else class="border rounded p-4 space-y-4 bg-gray-50">
    <!-- Icon row -->
    <div class="flex gap-2">
      <button
        v-for="k in kinds"
        :key="k"
        :class="[
          'flex-1 py-2 rounded-full border text-sm flex items-center justify-center gap-2 transition',
          t.kind === k
            ? 'bg-blue-600 text-white border-blue-600 shadow'
            : 'bg-white text-gray-700 border-gray-300 hover:bg-gray-100',
        ]"
        type="button"
        @click="switchKind(k)"
      >
        <i :class="['fas', icons[k]]" />
        {{ k[0].toUpperCase() + k.slice(1) }}
      </button>
    </div>

    <!-- Struct / Union -->
    <template v-if="t.kind === 'struct' || t.kind === 'union'">
      <div v-for="(f, idx) in kvArray" :key="idx" class="mb-4 flex flex-col gap-2">
        <input v-model="f.key" class="border rounded px-2 py-1" placeholder="name" />
        <TypeEditor v-model="f.type" class="ml-6" />
        <button
          class="self-start text-red-600 text-xs"
          type="button"
          @click="kvArray.splice(idx, 1)"
        >
          remove
        </button>
      </div>

      <button class="bg-gray-200 rounded px-3 py-1 text-xs" type="button" @click="addKV">
        + add {{ t.kind === 'struct' ? 'field' : 'variant' }}
      </button>
    </template>

    <!-- Enum -->
    <template v-else-if="t.kind === 'enum'">
      <div v-for="(opt, idx) in (t as any).options" :key="idx" class="flex gap-2 mb-2">
        <input
          v-model="(t as any).options[idx]"
          class="flex-1 border rounded px-2 py-1"
          placeholder="option"
        />
        <button class="text-red-600" type="button" @click="(t as any).options.splice(idx, 1)">
          ×
        </button>
      </div>
      <button
        class="bg-gray-200 rounded px-3 py-1 text-xs"
        type="button"
        @click="(t as any).options.push('')"
      >
        + add option
      </button>
    </template>

    <!-- Scalar -->
    <template v-else>
      <select v-model="(t as any).name" class="border rounded px-2 py-1">
        <option v-for="s in scalarOptions" :key="s" :value="s">{{ s }}</option>
      </select>
    </template>

    <!-- Collapse -->
    <div class="text-right">
      <button class="text-blue-600 text-sm" type="button" @click="collapsed.on = true">Done</button>
    </div>
  </div>
</template>

<style scoped>
select,
input,
button {
  outline: none;
}
</style>
