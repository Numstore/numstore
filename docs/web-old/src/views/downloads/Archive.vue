<!-- File: DownloadsArchive.vue -->
<template>
  <main class="max-w-3xl mx-auto px-6 py-12 space-y-6">
    <h1 class="text-2xl font-bold">Release Archive</h1>
    <p>Older versions of NumStore.</p>

    <ul class="list-disc pl-6 space-y-4">
      <li v-for="rel in archive" :key="rel.version">
        <strong>v{{ rel.version }}</strong> (released {{ rel.date }})
        <ul class="list-disc pl-6 space-y-1 mt-1">
          <li v-for="asset in rel.assets" :key="rel.version + '-' + asset.id">
            <span class="font-medium">{{ asset.label }}:</span>
            <a :href="asset.links.binary" class="underline">Download</a>
            (<a :href="asset.links.signature" class="underline">Signature</a>)
            <span class="text-sm text-gray-500">{{ asset.size }}</span>
          </li>
        </ul>
      </li>
    </ul>


    <p class="pt-4">
      Back to <a class="underline" href="/downloads/current">current release</a>.
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

interface ArchivedRelease {
  version: string;
  date: string;
  assets: Asset[]
}


const archive: ArchivedRelease[] = [
  {
    version: '0.9.3',
    date: '2025-06-15',
    assets: [
      {
        id: 'linux-x64',
        label: 'Linux x86_64',
        size: '12.2 MB',
        links: {
          binary: '/downloads/archive/0.9.3/numstore-0.9.3-linux-x64.tar.gz',
          signature: '/downloads/archive/0.9.3/numstore-0.9.3-linux-x64.tar.gz.asc',
        },
      },
      {
        id: 'macos-arm64',
        label: 'macOS arm64',
        size: '10.9 MB',
        links: {
          binary: '/downloads/archive/0.9.3/numstore-0.9.3-macos-arm64.zip',
          signature: '/downloads/archive/0.9.3/numstore-0.9.3-macos-arm64.zip.asc',
        },
      },
    ],
  },
  {
    version: '0.9.0',
    date: '2025-05-02',
    assets: [
      {
        id: 'windows-x64',
        label: 'Windows x86_64',
        size: '14.5 MB',
        links: {
          binary: '/downloads/archive/0.9.0/numstore-0.9.0-windows-x64.zip',
          signature: '/downloads/archive/0.9.0/numstore-0.9.0-windows-x64.zip.asc',
        },
      },
    ],
  },
]
</script>