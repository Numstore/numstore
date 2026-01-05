import {createRouter, createWebHistory, type RouteRecordRaw} from 'vue-router'

const routes: RouteRecordRaw[] = [
    // -------------------------- Downloads --------------------------
    {path: '/downloads', redirect: '/downloads/current'},
    {
        path: '/downloads/current',
        name: 'DownloadsCurrent',
        component: () => import('@/views/downloads/Current.vue'),
        meta: {section: 'Downloads', label: 'Current'}
    },
    {
        path: '/downloads/archive',
        name: 'DownloadsArchive',
        component: () => import('@/views/downloads/Archive.vue'),
        meta: {section: 'Downloads', label: 'Archive'}
    },

    // -------------------------- Resources --------------------------
    {path: '/resources', redirect: '/resources/documentation'},
    {
        path: '/resources/documentation',
        name: 'ResourcesDocumentation',
        component: () => import('@/views/resources/ResourcesDocumentation.vue'),
        meta: {section: 'Resources', label: 'Documentation'}
    },
    // Redirect legacy blog path to new /blog routes
    {
        path: '/resources/blog',
        name: 'Blog',
        component: () => import('@/views/resources/BlogIndex.vue'),
        meta: {section: 'Resources', label: 'Blog'}
    },
    {
        path: '/resources/blog/:slug',
        name: 'ResourcesBlogPost',
        component: () => import('@/views/resources/BlogPostPage.vue'),
        props: true,
        meta: {section: 'Resources', label: 'Blog'}
    },
    {
        path: '/resources/references',
        name: 'References',
        component: () => import('@/views/resources/ReferencesMain.vue'),
        meta: {section: 'Resources', label: 'References'}
    },

    // -------------------------- Services --------------------------
    {path: '/services', redirect: '/services/enterprise_support'},
    {
        path: '/services/enterprise_support',
        name: 'ServicesEnterpriseSupport',
        component: () => import('@/views/services/EnterpriseSupport.vue'),
        meta: {section: 'Services', label: 'Enterprise Support'}
    },
    {
        path: '/services/consulting',
        name: 'ServicesConsulting',
        component: () => import('@/views/services/Consulting.vue'),
        meta: {section: 'Services', label: 'Consulting'}
    },
    {
        path: '/services/training',
        name: 'ServicesTraining',
        component: () => import('@/views/services/Training.vue'),
        meta: {section: 'Services', label: 'Training'}
    },

    // -------------------------- Community --------------------------
    {path: '/community', redirect: '/community/bug_reporting'},
    {
        path: '/community/bug_reporting',
        name: 'CommunityBugReporting',
        component: () => import('@/views/community/BugReporting.vue'),
        meta: {section: 'Community', label: 'Bug Reporting'}
    },
    {
        path: '/community/forum',
        name: 'CommunityForum',
        component: () => import('@/views/community/CommunityForum.vue'),
        meta: {section: 'Community', label: 'Forum'}
    },

    // -------------------------- About --------------------------
    {path: '/about', redirect: '/about/company'},
    {
        path: '/about/company',
        name: 'AboutCompany',
        component: () => import('@/views/about/Company.vue'),
        meta: {section: 'About', label: 'Company'}
    },
    {
        path: '/about/contact',
        name: 'AboutContact',
        component: () => import('@/views/about/AboutContact.vue'),
        meta: {section: 'About', label: 'Contact'}
    },

    // -------------------------- Misc --------------------------
    {
        path: '/',
        redirect: {name: 'ResourcesDocumentation'},
    },
    {
        path: '/:pathMatch(.*)*',
        redirect: '/'
    }
]

const router = createRouter({
    history: createWebHistory(),
    routes,
    scrollBehavior: () => ({top: 0})
})

export default router
