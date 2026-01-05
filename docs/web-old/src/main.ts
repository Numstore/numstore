import './assets/main.css'
import { createApp } from 'vue'
import App from './App.vue'
import router from './router'

import { installVue3SphinxXml } from 'vue3-sphinx-xml'

import 'highlight.js/styles/xcode.css'

const app = createApp(App)

app.use(router)
app.use(installVue3SphinxXml)

app.mount('#app')
