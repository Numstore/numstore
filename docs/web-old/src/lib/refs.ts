import refsRaw from '@/references.json'

export type RefEntry = {
    key: string
    authors?: string
    title?: string
    year?: string
    venue?: string
    url?: string
    accessed?: string
    details?: string
}

const refs = refsRaw as RefEntry[]

// Build key → number (1-based) map; warn on duplicates in dev.
const keyToNum = new Map<string, number>()
refs.forEach((r, i) => {
    if (keyToNum.has(r.key)) {
        if (import.meta.env.DEV) console.warn(`Duplicate reference key: ${r.key}`)
    }
    keyToNum.set(r.key, i + 1)
})

export function citeNumber(key: string): number | undefined {
    return keyToNum.get(key)
}

export function getReferences(): Array<RefEntry & { n: number }> {
    return refs.map((r, i) => ({ ...r, n: i + 1 }))
}
