<!-- Autocreated placeholder for Numstore/What is Numstore_/Numstore's Positioning -->
<script setup>
// add content for this section
</script>
<template>
  <p>Numstore takes inspiration from these precedents but fills a gap:</p>
  <ul class="list-desc ml-6">
    <li class="list-desc ml-6"><span class="font-bold">File format vs. DBMS:</span>
      <p>Unlike HDF5 and NetCDF, Numstore is not just a storage specification. It provides the
        features of a DBMS — transactions, recovery, and indexing — applied to numerical arrays.</p>
    </li>
    <li class="list-desc ml-6"><span class="font-bold">Direct memory layout:</span>
      <p>By mapping C-style types (structs, arrays, unions, enums) directly
        to on-disk structures,
        Numstore minimizes impedance mismatch between computation and storage.</p>
    </li>
    <li class="list-desc ml-6"><span class="font-bold">Throughput for signal data:</span>
      <p>Sequential insert rates are engineered to match real-time acquisition demands (on
        the order of tens of MB/s), aligning with signal processing and experimental physics workloads.
      </p>
    </li>
    <li class="list-desc ml-6"><span class="font-bold">Integration with modern stacks:</span>
      <p> With bindings for Python, Java, and other languages,
        Numstore aims to slot
        naturally into both research and production ecosystems.</p>
    </li>
  </ul>
</template>
