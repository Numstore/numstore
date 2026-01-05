<!-- Autocreated placeholder for Numstore/What is Numstore_/Historical Precedent_ Machine Learning -->
<script setup>
// add content for this section
import Cite from "@/components/Cite.vue";
</script>
<template>
  <p>
    In modern machine learning practice, datasets are rarely managed with traditional relational databases.
    Frameworks such as TensorFlow<Cite key-id="abadi2016-tensorflow"/> and PyTorch<Cite key-id="paszke2019-pytorch"/>
    are built around multidimensional numerical arrays (tensors) as the unit of computation.
    The surrounding ecosystem adopted file formats that favor streaming and contiguous array layout:
  </p>

  <ul class="list-disc ml-6">
    <li class="list-desc ml-6">
      <strong>TFRecords</strong> serialize protocol buffer messages into binary record files for high-throughput
      training input pipelines <Cite key-id="tfrecords"/>.
    </li>
    <li class="list-desc ml-6">
      <strong>NumPy</strong> array files (<code>.npy</code>, <code>.npz</code>) are the de facto standard for dense
      arrays
      in Python, valued for simplicity and memory mapping <Cite key-id="harris2020-numpy"/>.
    </li>
    <li class="list-desc ml-6">
      <strong>HDF5</strong> remains a common container for scientific and ML data, including model weights and
      structured
      datasets <Cite key-id="folk2011-hdf5"/>.
    </li>
  </ul>

  <p>
    These choices reflect a practical constraint: relational schemas are a poor fit for array-centric workloads.
    That raises a simple question: what DBMS would you use to store the individual pixel magnitudes for every image
    in a large dataset?
  </p>

  <ul class="list-disc ml-6">
    <li class="list-desc ml-6">
      A wide SQL table with “one column per pixel” is unwieldy and brittle; it encodes array geometry into schema, not
      data.
    </li>
    <li class="list-desc ml-6">
      A single binary blob per image discards the array’s structure for query and indexing; it hides the very properties
      that make the data useful.
    </li>
    <li class="list-desc ml-6">
      Tabular formats like Parquet (or Pandas DataFrames) handle records well but do not expose DBMS features such as
      concurrency control, user sessions, and transactional guarantees when used as files.
    </li>
  </ul>

  <p>
    Numstore addresses this gap. It treats arrays as first-class storage primitives—like ML frameworks do for
    computation—
    while also providing the core properties of a DBMS: transactions, durability, and multi-user access. The result is a
    storage model aligned with how numerical workloads actually operate, without giving up database semantics.
  </p>
</template>
