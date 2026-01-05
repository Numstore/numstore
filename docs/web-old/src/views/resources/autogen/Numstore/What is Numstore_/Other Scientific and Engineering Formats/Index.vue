<!-- Autocreated placeholder for Numstore/What is Numstore_/Other Scientific and Engineering Formats -->
<script setup>
// add content for this section
import Cite from "@/components/Cite.vue";
</script>
<template>
  <p>Outside of the classified community, the best-known example of a numerical storage standard is HDF5<Cite
      key-id="folk2011-hdf5"/> (Hierarchical Data
    Format, version 5). HDF5 is widely used in physics, earth sciences, and machine learning frameworks such as
    TensorFlow<Cite
        key-id="abadi2016-tensorflow"/>
    and PyTorch<Cite key-id="paszke2019-pytorch"/>. Its strengths include:
    A hierarchical container model for organizing datasets.
    Built-in compression and chunked storage.
    Strong ecosystem support in scientific software.
    However, HDF5 is fundamentally a file format and not a DBMS. It lacks features like multi-user transactions,
    concurrency control, and fine-grained query planning. This distinction matters when moving from single-user analysis
    workflows to production systems.</p>

  <em>Other formats include:</em>
  <ul class="list-desc ml-6">
    <li class="list-desc ml-6"><span class="font-bold">NetCDF<Cite key-id="rew1990-netcdf"/> (Network Common Data Form):</span>
      <p>Designed for multidimensional climate and
        atmospheric data, NetCDF excels at
        gridded scientific measurements but is tailored for a narrower set of domains.</p>
    </li>
    <li class="list-desc ml-6"><span class="font-bold">MAT-files<Cite
        key-id="mathworks-mat"/> (MATLAB data files):</span>span>
      <p>Ubiquitous
        in engineering and signal processing research, MAT-files serialize
        MATLAB arrays but are poorly suited to large-scale, multi-language environments.</p>
    </li>
    <li class="list-desc ml-6"><span class="font-bold">Apache Parquet<Cite key-id="apache-parquet"/>:</span>span>
      <p>An efficient, columnar storage format common in data lakes and
        analytics stacks. Parquet is optimized for tabular records and OLAP queries, not for continuous numerical
        signals or heterogeneous C-style types.</p>
    </li>
  </ul>
</template>
