<script lang="ts" setup>
import {ref} from 'vue'

const form = ref({
  name: '',
  email: '',
  subject: '',
  message: ''
})

function submitForm() {
  // placeholder: log to console
  console.log('Contact form submitted:', form.value)
  alert('Form submitted (demo only). Please send an email to hello@thecompany.com.')
  form.value = {name: '', email: '', subject: '', message: ''}
}
</script>

<template>
  <main class="max-w-3xl mx-auto px-6 py-12 space-y-10">
    <!-- Header -->
    <section class="space-y-2">
      <h1 class="text-3xl font-bold">Contact Us</h1>
      <p class="text-gray-700">
        We’d love to hear from you. For general inquiries, support, or business questions, use the form
        below or reach us directly.
      </p>
    </section>

    <!-- Direct info -->
    <section class="space-y-3">
      <h2 class="text-xl font-semibold">Direct contact</h2>
      <ul class="list-disc pl-6 space-y-1">
        <li>Email: <a class="underline" href="mailto:hello@thecompany.com">hello@thecompany.com</a></li>
        <li>Support: <a class="underline" href="mailto:support@thecompany.com">support@thecompany.com</a></li>
        <li>Security: <a class="underline" href="mailto:security@thecompany.com">security@thecompany.com</a>
        </li>
      </ul>
    </section>

    <!-- Form -->
    <section class="space-y-3">
      <h2 class="text-xl font-semibold">Send a message</h2>
      <form class="space-y-4" @submit.prevent="submitForm">
        <div>
          <label class="block text-sm font-medium">Name</label>
          <input v-model="form.name" class="w-full border rounded px-3 py-2 text-sm" required/>
        </div>

        <div>
          <label class="block text-sm font-medium">Email</label>
          <input v-model="form.email" class="w-full border rounded px-3 py-2 text-sm" required type="email"/>
        </div>

        <div>
          <label class="block text-sm font-medium">Subject</label>
          <input v-model="form.subject" class="w-full border rounded px-3 py-2 text-sm"/>
        </div>

        <div>
          <label class="block text-sm font-medium">Message</label>
          <textarea v-model="form.message" class="w-full border rounded px-3 py-2 text-sm h-32" required></textarea>
        </div>

        <button class="underline" type="submit">Send (demo only)</button>
      </form>
      <p class="text-xs text-gray-600">This form is a static demo. For real inquiries, please email us directly.</p>
    </section>
  </main>
</template>

<style scoped>
/* keep styling minimal */
</style>
