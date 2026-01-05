# Numstore Documentation

This directory contains the Numstore documentation website - a simple, static HTML site.

## Structure

- **index.html** - Main landing page
- **style.css** - Shared stylesheet for all pages
- **logo.svg** - Numstore logo
- **\*.html** - Documentation pages (getting-started, features, api-reference, etc.)

## Viewing the Documentation

### Locally

Simply open any HTML file in your web browser:

```bash
# Open the main page
open docs/index.html  # macOS
xdg-open docs/index.html  # Linux
start docs/index.html  # Windows
```

### With a Local Server

For a better experience, serve the docs with a simple HTTP server:

```bash
# Python 3
cd docs
python3 -m http.server 8000

# Python 2
cd docs
python -m SimpleHTTPServer 8000

# Node.js (with http-server)
cd docs
npx http-server

# Then visit http://localhost:8000
```

## GitHub Pages

This documentation is designed to work with GitHub Pages. The `docs` folder is configured
as the publishing source, making all pages accessible at your GitHub Pages URL.

## Pages

- **index.html** - Home page with overview and quick start
- **getting-started.html** - Installation and setup guide
- **features.html** - Complete feature list
- **api-reference.html** - API documentation for CLI, TCP, C, Python, and Java
- **types.html** - Type system documentation
- **configuration.html** - Configuration options
- **examples.html** - Code examples and tutorials
- **downloads.html** - Download links and installation methods
- **about.html** - Background, context, and contact information

## Editing

The documentation is plain HTML with CSS. To edit:

1. Open any `.html` file in a text editor
2. Make your changes
3. Refresh your browser to see updates
4. No build process needed!

## Old Vue.js Site

The previous Vue.js-based documentation site has been moved to `docs/web-old/` for reference.

## Contributing

When adding new pages:

1. Create a new `.html` file following the existing structure
2. Copy the `<header>` and `<nav>` sections from an existing page
3. Link your page in the navigation of all other pages
4. Keep the design simple and consistent
5. Test on multiple browsers

## Design Philosophy

This documentation follows the cURL documentation approach:

- **Simple** - Plain HTML, no build process
- **Fast** - Minimal CSS, no JavaScript required
- **Accessible** - Works everywhere, including older browsers
- **Maintainable** - Easy to edit and update
