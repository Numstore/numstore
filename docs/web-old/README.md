# Numstore Documentation

## Quick Start with Docker (Recommended)

**Start the container:**
```bash
docker-compose up -d
```

**Stop the container:**
```bash
docker-compose down
```

**Rebuild after changes:**
```bash
docker-compose up -d --build
```

**View logs:**
```bash
docker-compose logs -f
```

### Docker directly 

**Build the image:**
```bash
docker build -t numstore-docs .
```

**Run the container:**
```bash
docker run -d -p 8080:80 --name numstore-docs numstore-docs
```

**Stop the container:**
```bash
docker stop numstore-docs
docker rm numstore-docs
```

## Local Development

### Setup

```bash
# Install dependencies
npm install

# Start development server
npm run dev
```

The dev server will start at http://localhost:5173

### Build for production

```bash
# Build the project
npm run build

# Preview the production build
npm run preview
```

## Scripts

- `npm run dev` - Start development server
- `npm run build` - Build for production
- `npm run preview` - Preview production build
- `npm run gendocs` - Generate documentation
- `npm run lint` - Lint and fix code
- `npm run format` - Format code with Prettier
