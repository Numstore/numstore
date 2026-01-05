<!-- File: DownloadsCurrent.vue -->
<template>
  <main class="max-w-3xl mx-auto px-6 py-12 space-y-6">
    <h1 class="text-2xl font-bold">Download NumStore</h1>
    <p>Current stable release: v{{ current.version }} (released {{ current.date }})</p>


    <ul class="list-disc pl-6 space-y-2">
      <li v-for="asset in current.assets" :key="asset.id">
        <span class="font-medium">{{ asset.label }}:</span>
        <a :href="asset.links.binary" class="underline">Download</a>
        (<a :href="asset.links.signature" class="underline">Signature</a>)
        <span class="text-sm text-gray-500">{{ asset.size }}</span>
      </li>
    </ul>


    <p class="pt-4">
      Looking for older versions? <a class="underline" href="/downloads/archive">Visit the archive</a>.
    </p>
  </main>
</template>


<script lang="ts" setup>
interface AssetLink {
  binary: string;
  signature: string
}

interface Asset {
  id: string;
  label: string;
  size: string;
  links: AssetLink
}

interface CurrentRelease {
  version: string;
  date: string;
  assets: Asset[]
}


const current: CurrentRelease = {
  version: '1.0.0',
  date: '2025-09-01',
  assets: [
    {
      id: 'linux-x64',
      label: 'Linux x86_64',
      size: '12.4 MB',
      links: {
        binary: '/downloads/current/numstore-1.0.0-linux-x64.tar.gz',
        signature: '/downloads/current/numstore-1.0.0-linux-x64.tar.gz.asc',
      },
    },
    {
      id: 'macos-arm64',
      label: 'macOS Apple Silicon (arm64)',
      size: '11.1 MB',
      links: {
        binary: '/downloads/current/numstore-1.0.0-macos-arm64.zip',
        signature: '/downloads/current/numstore-1.0.0-macos-arm64.zip.asc',
      },
    },
    {
      id: 'windows-x64',
      label: 'Windows x86_64',
      size: '14.8 MB',
      links: {
        binary: '/downloads/current/numstore-1.0.0-windows-x64.zip',
        signature: '/downloads/current/numstore-1.0.0-windows-x64.zip.asc',
      },
    },
  ],
}
</script>